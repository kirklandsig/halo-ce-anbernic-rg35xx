/*
HOST_GLTHREAD.H

The Knulli port's GL thread (host_glthread.c): the guest's OpenGL ES calls
are recorded into a queue and made by a thread of their own, so that the
driver's work, which is most of a frame's on the handheld's Cortex-A53, runs
on another core than the game. The recording functions are generated
(glthread_gen.py); this is what they share with the queue.
*/

#ifndef __HALO_KNULLI_GLTHREAD_H
#define __HALO_KNULLI_GLTHREAD_H

#include <GLES3/gl32.h>
#include <stddef.h>
#include <stdint.h>

/* the kinds of object whose names are reserved ahead of time */
enum
{
	_glthread_textures,
	_glthread_buffers,
	_glthread_framebuffers,
	_glthread_samplers,
	_glthread_programs,
	_glthread_name_kinds
};

/* set on a thread with a context of its own that shares the GL thread's
objects (the guest's texture worker): its calls go straight to the driver,
on that context, rather than to the GL thread */
extern __thread int glthread_direct;

/* starts recording a queued call: room for its arguments (size bytes) and
payload bytes after them; glthread_end queues it */
void *glthread_begin(uint32_t function, size_t size, size_t payload);
void glthread_end(void);
/* the payload of a call glthread_begin made, whose arguments are size bytes */
void *glthread_payload(const void *call, size_t size);
/* runs run(context) on the GL thread, after everything queued, and waits */
void glthread_sync(void (*run)(void *), void *context);
/* names for glGen*, from the reserve */
void glthread_reserved_names(int kind, GLsizei n, GLuint *names);
/* glPixelStorei's state, which the size of an image's pixels depends on */
void glthread_pixel_store(GLenum name, GLint value);
size_t glthread_image_size(GLsizei width, GLsizei height, GLsizei depth, GLenum format, GLenum type);

/* what the GPU pass timer needs to know of a call */
enum
{
	_glthread_call_other,
	_glthread_call_bind_framebuffer,
	_glthread_call_draw,
	_glthread_call_clear,
	_glthread_call_geometry,
	_glthread_call_uniform,
	_glthread_call_copy,        /* copies, blits, mipmaps, invalidations */
};

/* generated (glthread_gen.py) */
void glthread_driver_finish(void);
void glthread_driver_flush(void);
GLint glthread_driver_integer(GLenum name);
int glthread_draw_count(uint32_t function, const void *data);
int glthread_call_kind(uint32_t function);
void glthread_replay(uint32_t function, const void *data);
void *glthread_record_function(const char *name, void *function, void *direct);
/* the commands queued so far told to the GL thread */
void glthread_publish(void);
void glthread_generate_names(int kind, GLsizei n, GLuint *names);
/* a queued call's name (glthread_gen.py) */
const char *glthread_function_name(uint32_t function);
void glthread_driver_use_program(GLuint program);
void glthread_driver_delete_program(GLuint program);

/* programs that threads of their own build (host_glthread.c): while the
one bound is being built, the generated replay asks whether to make a draw
or a uniform call (the uniform's is kept for later otherwise) */
extern int glthread_program_blocked;
void glthread_program_use(GLuint program);
void glthread_program_delete(GLuint program);
int glthread_program_draw(void);
int glthread_program_uniform(const void *call);

#endif
