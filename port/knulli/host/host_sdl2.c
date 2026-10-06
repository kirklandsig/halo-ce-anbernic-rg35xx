/*
HOST_SDL2.C

SDL on behalf of the guest (guest/runtime/guest_sdl.c) for the Knulli
port. The guest calls SDL3's functions; Knulli's SDL2 answers them,
because only its SDL2 carries the video driver for the Mali GPU's
framebuffer ("mali"). The replaced file, port/android/host/host_sdl.c, is
the model: SDL objects are 64-bit pointers, which the guest cannot hold, so
it gets small handles into the table here instead.

The guest's values are SDL3's. Most agree with SDL2's (the init flags,
scancodes, keycodes, key modifiers, gamepad buttons and axes, audio
formats, GL profile and flag bits); host_sdl3_events.c translates the
others (the GL attributes, gamepad types and the layout of events).

SDL2's audio callback runs on SDL's thread, which has no guest stack: as on
Android, each request goes to a thread that has one.
*/

#include "host.h"
#include "host_knulli.h"

#include <SDL2/SDL.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#define HANDLE_COUNT 256

enum handle_type
{
	_handle_free,
	_handle_window,
	_handle_context,
	_handle_gamepad,
	_handle_audio,
};

struct handle
{
	int type;
	void *object;
};

static struct handle handles[HANDLE_COUNT];
static pthread_mutex_t handle_lock = PTHREAD_MUTEX_INITIALIZER;

static uint32_t handle_new(int type, void *object)
{
	uint32_t index;

	if (!object)
		return 0;
	pthread_mutex_lock(&handle_lock);
	/* an object that already has a handle keeps it */
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type == type && handles[index].object == object)
		{
			pthread_mutex_unlock(&handle_lock);
			return index;
		}
	}
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type == _handle_free)
		{
			handles[index].type = type;
			handles[index].object = object;
			pthread_mutex_unlock(&handle_lock);
			return index;
		}
	}
	pthread_mutex_unlock(&handle_lock);
	host_logf(HOST_LOG_ERROR, "out of SDL handles");
	return 0;
}

/* the handle of an object that is going away freed */
static void handle_release(void *object)
{
	int index;

	pthread_mutex_lock(&handle_lock);
	for (index = 1; index < HANDLE_COUNT; index++)
	{
		if (handles[index].type != _handle_free && handles[index].object == object)
		{
			handles[index].type = _handle_free;
			handles[index].object = NULL;
		}
	}
	pthread_mutex_unlock(&handle_lock);
}

static void *handle_get(uint32_t handle, int type)
{
	void *object = NULL;

	if (handle == 0 || handle >= HANDLE_COUNT)
		return NULL;
	pthread_mutex_lock(&handle_lock);
	if (handles[handle].type == type)
		object = handles[handle].object;
	pthread_mutex_unlock(&handle_lock);
	return object;
}

/* ---------- general */

int host_sdl_init(uint32_t flags)
{
	/* SDL3's subsystem bits are SDL2's (SDL_INIT_GAMEPAD is
	SDL_INIT_GAMECONTROLLER) */
	if (SDL_Init(flags) != 0)
	{
		host_logf(HOST_LOG_ERROR, "SDL_Init(%#x): %s", flags, SDL_GetError());
		return 0;
	}
	{
		const char *video = SDL_GetCurrentVideoDriver();
		const char *audio = SDL_GetCurrentAudioDriver();

		host_logf(HOST_LOG_INFO, "SDL %d.%d.%d, video %s, audio %s", SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
			SDL_PATCHLEVEL, video ? video : "none", audio ? audio : "none");
	}
	return 1;
}

int host_sdl_set_hint(const char *name, const char *value)
{
	return SDL_SetHint(name, value) == SDL_TRUE;
}

void host_sdl_get_error(char *buffer, uint32_t size)
{
	SDL_strlcpy(buffer, SDL_GetError(), size);
}

/* SDL2's scancodes as SDL3's: the same up to SDL_SCANCODE_MODE (257, the
keyboard's usages); past it the two number media and system keys apart
(Codex M27), which are left unknown here rather than misnamed */
static int32_t scancode_sdl3(int32_t scancode)
{
	return scancode >= 0 && scancode <= SDL_SCANCODE_MODE ? scancode : SDL_SCANCODE_UNKNOWN;
}

void host_sdl_scancode_name(int32_t scancode, char *buffer, uint32_t size)
{
	SDL_strlcpy(buffer, SDL_GetScancodeName((SDL_Scancode)scancode_sdl3(scancode)), size);
}

int32_t host_sdl_scancode_from_name(const char *name)
{
	return scancode_sdl3((int32_t)SDL_GetScancodeFromName(name));
}

int64_t host_sdl_ticks(void)
{
	return (int64_t)SDL_GetTicks64();
}

int64_t host_sdl_thread_id(void)
{
	return (int64_t)SDL_ThreadID();
}

/* ---------- video */

/* SDL3's window flags */
#define SDL3_WINDOW_FULLSCREEN 0x1ull
#define SDL3_WINDOW_OPENGL 0x2ull

uint32_t host_sdl_create_window(const char *title, int width, int height, int64_t flags)
{
	Uint32 host_flags = SDL_WINDOW_FULLSCREEN;
	SDL_Window *window;

	if ((uint64_t)flags & SDL3_WINDOW_OPENGL)
		host_flags |= SDL_WINDOW_OPENGL;
	/* the framebuffer is the window: its size is the display's */
	window = SDL_CreateWindow(title, SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, width, height, host_flags);
	if (!window)
	{
		host_logf(HOST_LOG_ERROR, "SDL_CreateWindow: %s", SDL_GetError());
		return 0;
	}
	SDL_GetWindowSize(window, &width, &height);
	host_logf(HOST_LOG_INFO, "window %dx%d", width, height);
	SDL_ShowCursor(SDL_DISABLE);
	return handle_new(_handle_window, window);
}

void host_sdl_window_size_in_pixels(uint32_t window, int *width, int *height)
{
	SDL_Window *object = handle_get(window, _handle_window);

	*width = 0;
	*height = 0;
	if (object)
		SDL_GL_GetDrawableSize(object, width, height);
}

int host_sdl_set_relative_mouse(uint32_t window, int enabled)
{
	(void)window;
	(void)enabled;
	return 1;
}

int host_sdl_gl_set_attribute(int attribute, int value)
{
	static const SDL_GLattr attributes[] =
	{
		[_host_gl_red_size] = SDL_GL_RED_SIZE,
		[_host_gl_green_size] = SDL_GL_GREEN_SIZE,
		[_host_gl_blue_size] = SDL_GL_BLUE_SIZE,
		[_host_gl_alpha_size] = SDL_GL_ALPHA_SIZE,
		[_host_gl_buffer_size] = SDL_GL_BUFFER_SIZE,
		[_host_gl_doublebuffer] = SDL_GL_DOUBLEBUFFER,
		[_host_gl_depth_size] = SDL_GL_DEPTH_SIZE,
		[_host_gl_stencil_size] = SDL_GL_STENCIL_SIZE,
		[_host_gl_multisamplebuffers] = SDL_GL_MULTISAMPLEBUFFERS,
		[_host_gl_multisamplesamples] = SDL_GL_MULTISAMPLESAMPLES,
		[_host_gl_context_major_version] = SDL_GL_CONTEXT_MAJOR_VERSION,
		[_host_gl_context_minor_version] = SDL_GL_CONTEXT_MINOR_VERSION,
		[_host_gl_context_flags] = SDL_GL_CONTEXT_FLAGS,
		[_host_gl_context_profile_mask] = SDL_GL_CONTEXT_PROFILE_MASK,
	};
	int index = host_sdl3_gl_attribute(attribute);

	if (index < 0 || index >= (int)(sizeof(attributes) / sizeof(attributes[0])))
		return 0;
	return SDL_GL_SetAttribute(attributes[index], value) == 0;
}

uint32_t host_sdl_gl_create_context(uint32_t window)
{
	SDL_Window *object = handle_get(window, _handle_window);
	SDL_GLContext context;

	if (!object)
		return 0;
	context = SDL_GL_CreateContext(object);
	if (!context)
	{
		host_logf(HOST_LOG_ERROR, "SDL_GL_CreateContext: %s", SDL_GetError());
		return 0;
	}
	return handle_new(_handle_context, context);
}

int host_sdl_gl_make_current(uint32_t window, uint32_t context)
{
	return SDL_GL_MakeCurrent(handle_get(window, _handle_window), handle_get(context, _handle_context)) == 0;
}

/* the swap interval in effect (host_sdl_gl_swap_interval) */
static int swap_interval = 1;

int host_sdl_gl_set_swap_interval(int interval)
{
	/* HALO_SWAP_INTERVAL overrides the game's display.vsync */
	const char *setting = getenv("HALO_SWAP_INTERVAL");

	if (setting && *setting)
		interval = atoi(setting);
	if (SDL_GL_SetSwapInterval(interval) != 0)
		return 0;
	swap_interval = interval;
	return 1;
}

/* whether swaps wait for the display, for frame pacing (host_glthread.c):
the interval set last, the override included */
int host_sdl_gl_swap_interval(void)
{
	return swap_interval;
}

/* the first number in a sysfs file, or 0 */
static long sysfs_number(const char *path)
{
	FILE *file = fopen(path, "r");
	long value = 0;

	if (file)
	{
		if (fscanf(file, "%ld", &value) != 1)
			value = 0;
		fclose(file);
	}
	return value;
}

/* HALO_FPS_LOG=<seconds>: the frame rate, the longest frame, the resident
memory, and the temperature and clocks the thermal governor allows, in the
log at this interval */
static void frame_statistics(void)
{
	static double interval = -1.0;
	static uint64_t start, previous;
	static uint64_t longest;
	static uint32_t frames, frame_buckets[6];
	uint64_t now;

	if (interval < 0.0)
	{
		const char *setting = getenv("HALO_FPS_LOG");

		interval = setting && *setting ? atof(setting) : 0.0;
	}
	if (interval <= 0.0)
		return;
	now = SDL_GetPerformanceCounter();
	if (!start)
	{
		start = previous = now;
		return;
	}
	frames++;
	if (now - previous > longest)
		longest = now - previous;
	{
		/* how the frames' times spread: vsync puts them on multiples of
		16.7 ms, a slower thread or the GPU anywhere */
		double milliseconds = (double)(now - previous) * 1000.0 / (double)SDL_GetPerformanceFrequency();
		int bucket = milliseconds < 15.0 ? 0 : milliseconds < 18.0 ? 1 : milliseconds < 22.0 ? 2 :
			milliseconds < 28.0 ? 3 : milliseconds < 35.0 ? 4 : 5;

		frame_buckets[bucket]++;
	}
	previous = now;
	if (now - start >= (uint64_t)(interval * (double)SDL_GetPerformanceFrequency()))
	{
		double frequency = (double)SDL_GetPerformanceFrequency();
		long resident_pages = 0;
		FILE *statm = fopen("/proc/self/statm", "r");

		if (statm)
		{
			long size;

			if (fscanf(statm, "%ld %ld", &size, &resident_pages) != 2)
				resident_pages = 0;
			fclose(statm);
		}
		host_logf(HOST_LOG_INFO, "fps %.1f, longest frame %.1f ms, resident %ld MB, %ld C, cpu %ld MHz, gpu %ld MHz",
			frames * frequency / (double)(now - start), longest * 1000.0 / frequency,
			resident_pages * (long)(getpagesize() / 1024) / 1024,
			sysfs_number("/sys/class/thermal/thermal_zone0/temp") / 1000,
			sysfs_number("/sys/devices/system/cpu/cpu0/cpufreq/scaling_cur_freq") / 1000,
			sysfs_number("/sys/class/devfreq/gpu/cur_freq") / 1000000);
		host_logf(HOST_LOG_INFO, "frame times: %u under 15 ms, %u 15-18, %u 18-22, %u 22-28, %u 28-35, %u over 35",
			frame_buckets[0], frame_buckets[1], frame_buckets[2], frame_buckets[3], frame_buckets[4], frame_buckets[5]);
		memset(frame_buckets, 0, sizeof(frame_buckets));
		host_gl_timing_report(frames);
		start = now;
		frames = 0;
		longest = 0;
	}
}

int host_sdl_gl_swap_window(uint32_t window)
{
	SDL_Window *object = handle_get(window, _handle_window);

	if (!object)
		return 0;
	SDL_GL_SwapWindow(object);
	frame_statistics();
	return 1;
}

/* ---------- events */

/* the buttons held on each gamepad, for the exit combination (hotkey and
start on the same gamepad, as the firmware's other ports), from its button
events in their order: SDL's own state of a gamepad is the latest, and a
press and release queued together would never show both buttons held */
#define HELD_GAMEPADS 8
static struct
{
	SDL_JoystickID gamepad;
	uint32_t buttons;
} held[HELD_GAMEPADS];

/* the buttons held on a gamepad, a new entry if it has none (one reused in
turn when all are taken) */
static uint32_t *buttons_held(SDL_JoystickID gamepad)
{
	static int next;
	int index;

	for (index = 0; index < HELD_GAMEPADS; index++)
	{
		if (held[index].buttons && held[index].gamepad == gamepad)
			return &held[index].buttons;
	}
	for (index = 0; index < HELD_GAMEPADS; index++)
	{
		if (!held[index].buttons)
			break;
	}
	if (index == HELD_GAMEPADS)
		index = next++ % HELD_GAMEPADS;
	held[index].gamepad = gamepad;
	held[index].buttons = 0;
	return &held[index].buttons;
}

static int exit_combination(uint32_t buttons)
{
	uint32_t hotkey = (1u << SDL_CONTROLLER_BUTTON_GUIDE) | (1u << SDL_CONTROLLER_BUTTON_BACK);

	return (buttons & hotkey) && (buttons & (1u << SDL_CONTROLLER_BUTTON_START));
}

static int translate(const SDL_Event *event, struct host_event *result)
{
	memset(result, 0, sizeof(*result));
	result->timestamp_ns = (uint64_t)event->common.timestamp * 1000000ull;
	switch (event->type)
	{
	case SDL_QUIT:
		result->kind = _host_event_quit;
		return 1;
	case SDL_KEYDOWN:
	case SDL_KEYUP:
		result->kind = _host_event_key;
		result->scancode = scancode_sdl3((int32_t)event->key.keysym.scancode);
		result->keycode = event->key.keysym.sym;
		result->modifiers = event->key.keysym.mod;
		result->down = event->type == SDL_KEYDOWN;
		result->repeat = event->key.repeat != 0;
		return 1;
	case SDL_CONTROLLERDEVICEADDED:
		/* SDL2 names the new device by its index, SDL3 by its instance */
		result->kind = _host_event_gamepad_added;
		result->which = (uint32_t)SDL_JoystickGetDeviceInstanceID(event->cdevice.which);
		return 1;
	case SDL_CONTROLLERDEVICEREMOVED:
	{
		SDL_GameController *gamepad = SDL_GameControllerFromInstanceID(event->cdevice.which);

		/* its buttons let go, its handle and the controller freed (the
		game's handle then reads nothing) */
		*buttons_held(event->cdevice.which) = 0;
		if (gamepad)
		{
			handle_release(gamepad);
			SDL_GameControllerClose(gamepad);
		}
		result->kind = _host_event_gamepad_removed;
		result->which = (uint32_t)event->cdevice.which;
		return 1;
	}
	case SDL_CONTROLLERBUTTONDOWN:
	case SDL_CONTROLLERBUTTONUP:
	{
		uint32_t *buttons = buttons_held(event->cbutton.which);

		if (event->cbutton.button < 32)
		{
			if (event->type == SDL_CONTROLLERBUTTONDOWN)
				*buttons |= 1u << event->cbutton.button;
			else
				*buttons &= ~(1u << event->cbutton.button);
		}
		if (event->type == SDL_CONTROLLERBUTTONDOWN && exit_combination(*buttons))
		{
			host_logf(HOST_LOG_INFO, "exit combination pressed");
			result->kind = _host_event_quit;
			return 1;
		}
		return 0;
	}
	case SDL_WINDOWEVENT:
		if (event->window.event == SDL_WINDOWEVENT_FOCUS_GAINED)
		{
			result->kind = _host_event_focus_gained;
			return 1;
		}
		if (event->window.event == SDL_WINDOWEVENT_FOCUS_LOST)
		{
			result->kind = _host_event_focus_lost;
			return 1;
		}
		return 0;
	default:
		return 0;
	}
}

int host_sdl_poll_event(void *event)
{
	SDL_Event host_event;

	while (SDL_PollEvent(&host_event))
	{
		struct host_event translated;

		if (translate(&host_event, &translated))
		{
			host_event_to_sdl3(&translated, event);
			return 1;
		}
	}
	return 0;
}

/* ---------- gamepads */

static int device_index_of(SDL_JoystickID id)
{
	int count = SDL_NumJoysticks(), index;

	for (index = 0; index < count; index++)
	{
		if (SDL_JoystickGetDeviceInstanceID(index) == id)
			return index;
	}
	return -1;
}

int host_sdl_get_gamepads(uint32_t *ids, int capacity)
{
	int count = SDL_NumJoysticks(), index, found = 0;

	for (index = 0; index < count && found < capacity; index++)
	{
		if (SDL_IsGameController(index))
			ids[found++] = (uint32_t)SDL_JoystickGetDeviceInstanceID(index);
	}
	return found;
}

uint32_t host_sdl_open_gamepad(uint32_t id)
{
	int index = device_index_of((SDL_JoystickID)id);
	SDL_GameController *gamepad;

	if (index < 0)
		return 0;
	gamepad = SDL_GameControllerFromInstanceID((SDL_JoystickID)id);
	if (!gamepad)
	{
		gamepad = SDL_GameControllerOpen(index);
		if (gamepad)
			host_logf(HOST_LOG_INFO, "gamepad %u: %s (%04x:%04x)", (unsigned)id, SDL_GameControllerName(gamepad),
				SDL_GameControllerGetVendor(gamepad), SDL_GameControllerGetProduct(gamepad));
		else
			host_logf(HOST_LOG_WARN, "cannot open gamepad %u: %s", (unsigned)id, SDL_GetError());
	}
	return handle_new(_handle_gamepad, gamepad);
}

uint32_t host_sdl_gamepad_from_id(uint32_t id)
{
	SDL_GameController *gamepad = SDL_GameControllerFromInstanceID((SDL_JoystickID)id);

	/* SDL3 opens nothing here, but a gamepad SDL2 listed and nobody has
	opened yet (the handheld's own, present at start-up) is opened now */
	if (!gamepad)
		return host_sdl_open_gamepad(id);
	return handle_new(_handle_gamepad, gamepad);
}

int host_sdl_gamepad_axis(uint32_t gamepad, int axis)
{
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object ? SDL_GameControllerGetAxis(object, (SDL_GameControllerAxis)axis) : 0;
}

int host_sdl_gamepad_button(uint32_t gamepad, int button)
{
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object ? SDL_GameControllerGetButton(object, (SDL_GameControllerButton)button) : 0;
}

int host_sdl_gamepad_type(uint32_t gamepad)
{
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);
	int kind;

	if (!object)
		return 0; /* SDL_GAMEPAD_TYPE_UNKNOWN */
	switch (SDL_GameControllerGetType(object))
	{
	case SDL_CONTROLLER_TYPE_XBOXONE: kind = _host_gamepad_xboxone; break;
	case SDL_CONTROLLER_TYPE_PS3: kind = _host_gamepad_ps3; break;
	case SDL_CONTROLLER_TYPE_PS4: kind = _host_gamepad_ps4; break;
	case SDL_CONTROLLER_TYPE_PS5: kind = _host_gamepad_ps5; break;
	case SDL_CONTROLLER_TYPE_NINTENDO_SWITCH_PRO: kind = _host_gamepad_switch_pro; break;
	default: kind = _host_gamepad_xbox360; break;
	}
	return host_sdl3_gamepad_type(kind);
}

int host_sdl_rumble_gamepad(uint32_t gamepad, uint32_t low, uint32_t high, uint32_t milliseconds)
{
	SDL_GameController *object = handle_get(gamepad, _handle_gamepad);

	return object ? SDL_GameControllerRumble(object, (Uint16)low, (Uint16)high, milliseconds) == 0 : 0;
}

/* ---------- audio

SDL2 pulls audio with a callback on its own thread; the guest's stream
callback (SDL3's model) pushes it. Each SDL2 request asks the guest, on a
thread with a guest stack (audio_thread), for what the stream lacks; what
the guest puts into the stream collects in the binding's buffer, and the
SDL2 callback copies it out. */
struct audio_binding
{
	uint32_t handle;
	uint32_t callback;
	uint32_t userdata;
	SDL_AudioDeviceID device;
	pthread_mutex_t lock;
	pthread_cond_t requested;
	pthread_cond_t done;
	int pending;
	int additional;
	unsigned char *buffer;
	int buffer_length;
	int buffer_size;
};

static void *audio_thread(void *context)
{
	struct audio_binding *binding = context;

	pthread_mutex_lock(&binding->lock);
	for (;;)
	{
		int additional;

		while (!binding->pending)
			pthread_cond_wait(&binding->requested, &binding->lock);
		additional = binding->additional;
		pthread_mutex_unlock(&binding->lock);
		host_call_guest(binding->callback, binding->userdata, binding->handle, (uint32_t)additional,
			(uint32_t)additional);
		pthread_mutex_lock(&binding->lock);
		binding->pending = 0;
		pthread_cond_signal(&binding->done);
	}
	return NULL;
}

/* appends to the binding's buffer; the caller holds its lock */
static int audio_keep(struct audio_binding *binding, const void *data, int length)
{
	if (length < 0)
		return 0;
	if (binding->buffer_length + length > binding->buffer_size)
	{
		int size = (binding->buffer_length + length) * 2;
		unsigned char *buffer = SDL_realloc(binding->buffer, (size_t)size);

		if (!buffer)
			return 0;
		binding->buffer = buffer;
		binding->buffer_size = size;
	}
	memcpy(binding->buffer + binding->buffer_length, data, (size_t)length);
	binding->buffer_length += length;
	return 1;
}

static void SDLCALL audio_callback(void *userdata, Uint8 *stream, int length)
{
	struct audio_binding *binding = userdata;
	int copied;

	pthread_mutex_lock(&binding->lock);
	if (binding->callback && binding->buffer_length < length)
	{
		binding->additional = length - binding->buffer_length;
		binding->pending = 1;
		pthread_cond_signal(&binding->requested);
		while (binding->pending)
			pthread_cond_wait(&binding->done, &binding->lock);
	}
	copied = binding->buffer_length < length ? binding->buffer_length : length;
	memcpy(stream, binding->buffer, (size_t)copied);
	if (copied < length)
		memset(stream + copied, 0, (size_t)(length - copied));
	binding->buffer_length -= copied;
	if (binding->buffer_length)
		memmove(binding->buffer, binding->buffer + copied, (size_t)binding->buffer_length);
	pthread_mutex_unlock(&binding->lock);
}

/* SDL3's SDL_AudioSpec */
struct sdl3_audio_spec
{
	int32_t format;
	int32_t channels;
	int32_t freq;
};

uint32_t host_sdl_open_audio_stream(uint32_t device, const void *spec, uint32_t callback, uint32_t userdata)
{
	const struct sdl3_audio_spec *guest_spec = spec;
	struct audio_binding *binding = SDL_calloc(1, sizeof(*binding));
	SDL_AudioSpec wanted, obtained;
	const char *setting = getenv("HALO_AUDIO_SAMPLES");

	(void)device;
	binding->callback = callback;
	binding->userdata = userdata;
	pthread_mutex_init(&binding->lock, NULL);
	pthread_cond_init(&binding->requested, NULL);
	pthread_cond_init(&binding->done, NULL);
	SDL_zero(wanted);
	/* SDL3's audio format values are SDL2's */
	wanted.format = (SDL_AudioFormat)guest_spec->format;
	wanted.channels = (Uint8)guest_spec->channels;
	wanted.freq = guest_spec->freq;
	wanted.samples = setting && *setting ? (Uint16)atoi(setting) : 1024;
	wanted.callback = audio_callback;
	wanted.userdata = binding;
	/* SDL converts to what the device takes */
	binding->device = SDL_OpenAudioDevice(NULL, 0, &wanted, &obtained, 0);
	if (!binding->device)
	{
		host_logf(HOST_LOG_ERROR, "SDL_OpenAudioDevice: %s", SDL_GetError());
		SDL_free(binding);
		return 0;
	}
	host_logf(HOST_LOG_INFO, "audio %d Hz, %d channels, %d frames a callback", obtained.freq, obtained.channels,
		obtained.samples);
	/* the device starts paused, so no callback can run before this */
	binding->handle = handle_new(_handle_audio, binding);
	if (!binding->handle)
	{
		SDL_CloseAudioDevice(binding->device);
		SDL_free(binding);
		return 0;
	}
	if (callback && host_native_thread_create(audio_thread, binding, 256 * 1024) != 0)
		host_fatal("cannot start the audio thread");
	return binding->handle;
}

int host_sdl_put_audio_stream_data(uint32_t stream, const void *data, int length)
{
	struct audio_binding *binding = handle_get(stream, _handle_audio);
	int result;

	if (!binding)
		return 0;
	/* the guest's callback runs while the SDL2 callback waits for it
	without holding the lock */
	pthread_mutex_lock(&binding->lock);
	result = audio_keep(binding, data, length);
	pthread_mutex_unlock(&binding->lock);
	return result;
}

int host_sdl_resume_audio_stream_device(uint32_t stream)
{
	struct audio_binding *binding = handle_get(stream, _handle_audio);

	if (!binding)
		return 0;
	SDL_PauseAudioDevice(binding->device, 0);
	return 1;
}

/* ---------- the clipboard (internet play's invite links): the process's own */

static char clipboard[1024];

int host_sdl_set_clipboard_text(const char *text)
{
	SDL_strlcpy(clipboard, text, sizeof(clipboard));
	host_logf(HOST_LOG_INFO, "clipboard: %s", clipboard);
	return 1;
}

void host_sdl_get_clipboard_text(char *buffer, uint32_t size)
{
	SDL_strlcpy(buffer, clipboard, size);
}

int host_sdl_show_toast(const char *message, int duration, int gravity, int x, int y)
{
	(void)duration;
	(void)gravity;
	(void)x;
	(void)y;
	host_logf(HOST_LOG_INFO, "notice: %s", message);
	return 1;
}

/* ---------- a message for the player: the log (the framebuffer has no
message boxes) */

int host_sdl_show_simple_message_box(uint32_t flags, const char *title, const char *message)
{
	(void)flags;
	host_logf(HOST_LOG_WARN, "%s: %s", title, message);
	return 1;
}
