#!/usr/bin/env python3
"""Generates the OpenGL ES side of the Knulli port's GL thread
(port/knulli/host/host_glthread.c):

    glthread_gen.py <gl_imports.list> <GLES3/gl32.h> <output.c>

For each GL function the guest imports it writes a recording function with
the function's own prototype, which the import table points at instead of
the driver's function, and the code that replays the recorded call on the
GL thread:

- functions that return a value or write through a pointer are
  synchronous: the calling thread waits while the GL thread makes the call;
- the others are queued, with a copy of the memory their pointer arguments
  refer to (PAYLOAD gives its size), and the calling thread goes on;
- glGenTextures, glGenBuffers, glGenFramebuffers, glGenSamplers and
  glCreateProgram take names reserved ahead of time by the GL thread
  (host_glthread.c), so that making an object mid-frame does not wait either;
- glUseProgram (REPLACED), the draws and the calls that set the bound
  program's uniforms ask host_glthread.c first: a program stays unbound while
  threads of its own build it, the draws made with it are skipped and the
  uniforms set for it kept.

A thread with a context of its own (glthread_direct, the guest's texture
worker) calls the driver's function directly instead.
"""

import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), "..", "..", "tools"))
from android_gl_stubs import prototypes, split_parameter  # noqa: E402

# functions the calling thread waits for
SYNC = {
    "glGetUniformBlockIndex", "glGetActiveUniformBlockiv",
    "glGetIntegerv", "glGetError", "glReadPixels", "glFinish", "glCheckFramebufferStatus",
    "glGenVertexArrays", "glCreateShader", "glGetShaderiv", "glGetShaderInfoLog",
    "glGetProgramiv", "glGetProgramInfoLog", "glGetUniformLocation", "glGenQueries",
    "glGetQueryObjectuiv", "glShaderSource", "glBindAttribLocation", "glCompileShader", "glLinkProgram",
    "glProgramBinary", "glGetProgramBinary", "glGenRenderbuffers",
}
# names reserved ahead of time (host_glthread.c)
RESERVED = {"glGenTextures": "textures", "glGenBuffers": "buffers", "glGenFramebuffers": "framebuffers",
            "glGenSamplers": "samplers"}
# objects made one at a time (GLuint f(void)) whose names are reserved too
CREATED = {"glCreateProgram": "programs"}
# queued functions' pointer arguments: the bytes to copy (a C expression of the arguments)
PAYLOAD = {
    "glDeleteTextures": ("textures", "(size_t)n * sizeof(GLuint)"),
    "glDeleteFramebuffers": ("framebuffers", "(size_t)n * sizeof(GLuint)"),
    "glDeleteBuffers": ("buffers", "(size_t)n * sizeof(GLuint)"),
    "glDrawBuffers": ("bufs", "(size_t)n * sizeof(GLenum)"),
    "glInvalidateFramebuffer": ("attachments", "(size_t)numAttachments * sizeof(GLenum)"),
    "glTexImage2D": ("pixels", "pixels ? glthread_image_size(width, height, 1, format, type) : 0"),
    "glTexImage3D": ("pixels", "pixels ? glthread_image_size(width, height, depth, format, type) : 0"),
    "glTexSubImage2D": ("pixels", "pixels ? glthread_image_size(width, height, 1, format, type) : 0"),
    "glCompressedTexImage2D": ("data", "data ? (size_t)imageSize : 0"),
    "glCompressedTexImage3D": ("data", "data ? (size_t)imageSize : 0"),
    "glTexParameteriv": ("params", "4 * sizeof(GLint)"),
    "glTexParameterfv": ("params", "4 * sizeof(GLfloat)"),
    "glSamplerParameterfv": ("param", "4 * sizeof(GLfloat)"),
    "glBufferData": ("data", "data ? (size_t)size : 0"),
    "glBufferSubData": ("data", "(size_t)size"),
    "glVertexAttrib4fv": ("v", "4 * sizeof(GLfloat)"),
    "glUniform1iv": ("value", "(size_t)count * sizeof(GLint)"),
    "glUniform4fv": ("value", "(size_t)count * 4 * sizeof(GLfloat)"),
}
# C statements the recording functions run first: the state the payload sizes depend on
HOOKS = {
    "glPixelStorei": "glthread_pixel_store(pname, param);",
}
# the draws
DRAWS = ("glDrawArrays", "glDrawElements", "glDrawElementsBaseVertex", "glDrawRangeElementsBaseVertex",
         "glDrawElementsInstancedBaseVertex")
# queued calls that host_glthread.c makes instead of the driver, with the same arguments
REPLACED = {"glUseProgram": "glthread_program_use", "glDeleteProgram": "glthread_program_delete"}
# pointer arguments that are offsets into a bound buffer, passed as they are
OFFSETS = {
    ("glVertexAttribPointer", "pointer"), ("glVertexAttribIPointer", "pointer"),
    ("glDrawElements", "indices"), ("glDrawElementsBaseVertex", "indices"),
    ("glDrawRangeElementsBaseVertex", "indices"), ("glDrawElementsInstancedBaseVertex", "indices"),
}


def parse_prototypes(header):
    """name: (result type, [(type, name)]) of the header's functions"""
    return {name: (result, [split_parameter(parameter) for parameter in parameters.split(",")]
                   if parameters and parameters != "void" else [])
            for name, (result, parameters) in prototypes(header).items()}


def is_pointer(kind):
    return "*" in kind


def current_program_call(arguments):
    """whether a call sets the bound program's state: its first parameter is
    a uniform's location (glUniform*)"""
    return bool(arguments) and arguments[0][1] == "location"


def direct_call(name, result, arguments, pointer_type):
    """the start of a recording function: on a thread with a context of its
    own, the driver's call itself"""
    invocation = f"(({pointer_type})direct_driver[glthread_{name}])({', '.join(argument for _, argument in arguments)})"
    if result != "void":
        return f"\tif (glthread_direct)\n\t\treturn {invocation};"
    return f"\tif (glthread_direct)\n\t{{\n\t\t{invocation};\n\t\treturn;\n\t}}"


def main():
    imports_path, header, output = sys.argv[1:4]
    names = [line.strip()[len("hostgl_"):] for line in open(imports_path) if line.startswith("hostgl_")]
    functions = parse_prototypes(header)
    out = []
    emit = out.append
    emit("/* generated by port/knulli/glthread_gen.py; do not edit */")
    emit("#include \"host_glthread.h\"\n")
    emit("#include <string.h>\n")
    emit("enum\n{")
    for name in names:
        emit(f"\tglthread_{name},")
    emit("\tglthread_function_count\n};\n")
    emit("static void *driver[glthread_function_count];\n")
    emit("/* the same, not timed (HALO_GL_TIMING times the GL thread's calls only) */")
    emit("static void *direct_driver[glthread_function_count];\n")
    cases = []
    for name in names:
        result, arguments = functions[name]
        parameters = ", ".join(f"{kind} {argument}" for kind, argument in arguments) or "void"
        call_arguments = ", ".join(argument for _, argument in arguments)
        pointer_type = f"{result} (GL_APIENTRY *)({', '.join(kind for kind, _ in arguments) or 'void'})"
        if name in SYNC:
            fields = "".join(f"\t{kind} {argument};\n" for kind, argument in arguments)
            if result != "void":
                fields += f"\t{result} result;\n"
            emit(f"struct sync_{name}\n{{\n{fields or chr(9) + 'char unused;' + chr(10)}}};\n")
            emit(f"static void sync_run_{name}(void *context)\n{{")
            emit(f"\tstruct sync_{name} *call = context;\n")
            invocation = f"(({pointer_type})driver[glthread_{name}])({', '.join('call->' + a for _, a in arguments)})"
            emit(f"\t(void)call;\n\t{'call->result = ' if result != 'void' else ''}{invocation};\n}}\n")
            emit(f"static {result} GL_APIENTRY record_{name}({parameters})\n{{")
            emit(direct_call(name, result, arguments, pointer_type))
            initial = ", ".join(f".{a} = {a}" for _, a in arguments)
            emit(f"\tstruct sync_{name} call = {{ {initial} }};\n" if initial else f"\tstruct sync_{name} call = {{ 0 }};\n")
            emit(f"\tglthread_sync(sync_run_{name}, &call);")
            if result != "void":
                emit("\treturn call.result;")
            emit("}\n")
            continue
        if name in CREATED:
            emit(f"static GLuint GL_APIENTRY record_{name}(void)\n{{")
            emit(f"\tGLuint name;\n\n{direct_call(name, result, arguments, pointer_type)}")
            emit(f"\tglthread_reserved_names(_glthread_{CREATED[name]}, 1, &name);\n\treturn name;\n}}\n")
            emit(f"static void glthread_generate_{CREATED[name]}(GLsizei n, GLuint *names)\n{{")
            emit(f"\tGLsizei index;\n\n\tfor (index = 0; index < n; index++)")
            emit(f"\t\tnames[index] = ((GLuint (GL_APIENTRY *)(void))driver[glthread_{name}])();\n}}\n")
            continue
        if name in RESERVED:
            # (GLsizei n, GLuint *names)
            emit(f"static void GL_APIENTRY record_{name}(GLsizei n, GLuint *names)\n{{")
            emit(direct_call(name, result, [("GLsizei", "n"), ("GLuint *", "names")], pointer_type))
            emit(f"\tglthread_reserved_names(_glthread_{RESERVED[name]}, n, names);\n}}\n")
            emit(f"static void glthread_generate_{RESERVED[name]}(GLsizei n, GLuint *names)\n{{")
            emit(f"\t((void (GL_APIENTRY *)(GLsizei, GLuint *))driver[glthread_{name}])(n, names);\n}}\n")
            continue
        assert result == "void", name
        payload = PAYLOAD.get(name)
        for kind, argument in arguments:
            if is_pointer(kind) and (name, argument) not in OFFSETS and (not payload or payload[0] != argument):
                raise SystemExit(f"{name}: pointer argument {argument} has no rule")
        fields = "".join(
            f"\t{'uintptr_t' if is_pointer(kind) else kind} {argument};\n" for kind, argument in arguments)
        emit(f"struct queued_{name}\n{{\n{fields or chr(9) + 'char unused;' + chr(10)}}};\n")
        emit(f"static void GL_APIENTRY record_{name}({parameters})\n{{")
        emit(direct_call(name, result, arguments, pointer_type))
        if name in HOOKS:
            emit(f"\t{HOOKS[name]}")
        size = payload[1] if payload else "0"
        emit(f"\tsize_t payload = {size};")
        emit(f"\tstruct queued_{name} *call = glthread_begin(glthread_{name}, sizeof(*call), payload);\n")
        for kind, argument in arguments:
            if is_pointer(kind):
                emit(f"\tcall->{argument} = (uintptr_t){argument};")
            else:
                emit(f"\tcall->{argument} = {argument};")
        if payload:
            emit(f"\tif (payload)\n\t\tmemcpy(glthread_payload(call, sizeof(*call)), {payload[0]}, payload);")
        # (a flush is told to the GL thread at once, not with the next 4 KB)
        emit("\tglthread_end();\n\tglthread_publish();\n}\n" if name == "glFlush" else "\tglthread_end();\n}\n")
        replay = []
        for kind, argument in arguments:
            if payload and argument == payload[0]:
                replay.append(f"call->{argument} ? ({kind})glthread_payload(call, sizeof(*call)) : NULL")
            elif is_pointer(kind):
                replay.append(f"({kind})call->{argument}")
            else:
                replay.append(f"call->{argument}")
        # while the bound program is being built (host_glthread.c)
        gate = ""
        if name in DRAWS:
            gate = "\t\tif (glthread_program_blocked && !glthread_program_draw())\n\t\t\tbreak;\n"
        elif current_program_call(arguments):
            gate = "\t\tif (glthread_program_blocked && !glthread_program_uniform(data))\n\t\t\tbreak;\n"
        target = REPLACED.get(name, f"(({pointer_type})driver[glthread_{name}])")
        cases.append(
            f"\tcase glthread_{name}:\n\t{{\n\t\tconst struct queued_{name} *call = data;\n\n\t\t(void)call;\n"
            f"{gate}\t\t{target}({', '.join(replay)});\n\t\tbreak;\n\t}}")
    emit("void glthread_replay(uint32_t function, const void *data)\n{\n\tswitch (function)\n\t{")
    out.extend(cases)
    emit("\tdefault:\n\t\tbreak;\n\t}\n}\n")
    emit("/* the functions' names and recording functions, in the order of the enum */")
    emit("static const struct\n{\n\tconst char *name;\n\tvoid *record;\n} functions[glthread_function_count] =\n{")
    for name in names:
        emit(f"\t{{ \"{name}\", (void *)record_{name} }},")
    emit("};\n")
    emit("void *glthread_record_function(const char *name, void *function, void *direct)\n{")
    emit("\tuint32_t index;\n")
    emit("\tfor (index = 0; index < glthread_function_count; index++)\n\t{")
    emit("\t\tif (!strcmp(functions[index].name, name))\n\t\t{")
    emit("\t\t\tdriver[index] = function;\n\t\t\tdirect_driver[index] = direct;\n\t\t\treturn functions[index].record;\n\t\t}\n\t}")
    emit("\treturn function;\n}\n")
    emit("const char *glthread_function_name(uint32_t function)\n{")
    emit("\treturn function < glthread_function_count ? functions[function].name : \"?\";\n}\n")
    emit("void glthread_generate_names(int kind, GLsizei n, GLuint *names)\n{\n\tswitch (kind)\n\t{")
    for name, kind in list(RESERVED.items()) + list(CREATED.items()):
        if name in names:
            emit(f"\tcase _glthread_{kind}:\n\t\tglthread_generate_{kind}(n, names);\n\t\tbreak;")
    emit("\t}\n}")
    # what the GPU pass timer (host_glthread.c) needs to know of the calls
    emit("\nvoid glthread_driver_finish(void)\n{")
    emit("\t((void (GL_APIENTRY *)(void))driver[glthread_glFinish])();\n}\n")
    emit("void glthread_driver_use_program(GLuint program)\n{")
    emit("\t((void (GL_APIENTRY *)(GLuint))driver[glthread_glUseProgram])(program);\n}\n")
    emit("void glthread_driver_delete_program(GLuint program)\n{")
    emit("\t((void (GL_APIENTRY *)(GLuint))driver[glthread_glDeleteProgram])(program);\n}\n")
    emit("void glthread_driver_flush(void)\n{")
    emit("\t((void (GL_APIENTRY *)(void))driver[glthread_glFlush])();\n}\n")
    emit("GLint glthread_driver_integer(GLenum name)\n{")
    emit("\tGLint value = 0;\n")
    emit("\t((void (GL_APIENTRY *)(GLenum, GLint *))driver[glthread_glGetIntegerv])(name, &value);")
    emit("\treturn value;\n}\n")
    emit("int glthread_draw_count(uint32_t function, const void *data)\n{\n\tswitch (function)\n\t{")
    for name in DRAWS:
        if name in names:
            emit(f"\tcase glthread_{name}:\n\t\treturn (int)((const struct queued_{name} *)data)->count;")
    emit("\tdefault:\n\t\treturn 0;\n\t}\n}\n")
    emit("int glthread_call_kind(uint32_t function)\n{\n\tswitch (function)\n\t{")
    emit("\tcase glthread_glBindFramebuffer:\n\t\treturn _glthread_call_bind_framebuffer;")
    for name in DRAWS:
        if name in names:
            emit(f"\tcase glthread_{name}:")
    emit("\t\treturn _glthread_call_draw;\n\tcase glthread_glClear:\n\t\treturn _glthread_call_clear;")
    for name in ("glVertexAttribPointer", "glVertexAttribIPointer", "glBindBuffer", "glEnableVertexAttribArray",
                 "glDisableVertexAttribArray", "glVertexAttrib4fv", "glVertexAttribI4ui"):
        if name in names:
            emit(f"\tcase glthread_{name}:")
    emit("\t\treturn _glthread_call_geometry;")
    for name in names:
        if current_program_call(functions[name][1]):
            emit(f"\tcase glthread_{name}:")
    emit("\t\treturn _glthread_call_uniform;")
    for name in ("glCopyImageSubData", "glBlitFramebuffer", "glCopyTexSubImage2D", "glReadPixels",
                 "glGenerateMipmap", "glInvalidateFramebuffer"):
        if name in names:
            emit(f"\tcase glthread_{name}:")
    emit("\t\treturn _glthread_call_copy;")
    emit("\tdefault:\n\t\treturn _glthread_call_other;\n\t}\n}")
    open(output, "w").write("\n".join(out) + "\n")


if __name__ == "__main__":
    main()
