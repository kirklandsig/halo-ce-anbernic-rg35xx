# Knulli (Allwinner H700 handhelds)

`port/knulli` runs the Android port's guest image (the game as ILP32 AArch64
code, [port/android/README.md](https://github.com/cybersecurity/halo-ce-universal/blob/9f3e8c92de7569a577c3044d05288dbd6590c5bf/port/android/README.md)) as an ordinary aarch64
Linux program on handhelds with the Allwinner H700 under Knulli: the Anbernic
RG35XX H, Plus, SP, 2024, RG40XX H/V, RG CubeXX and others. Their GPU, a
Mali-G31, has only Arm's OpenGL ES driver for the framebuffer (no X11,
Wayland, DRM or Vulkan), which the firmware's SDL2 drives.

## Install

1. Take the files from a [release](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/releases/latest),
   or build the port (below) and take `halo`, `halo_guest.elf`,
   `halo_extract.py`, `halo_screen.py` and `sdl_mapping.py`.
2. Copy them into `/userdata/roms/ports/halo/`, and `Halo.sh` into
   `/userdata/roms/ports/`.
3. Put an Xbox disc image of Halo (`.iso`) in `/userdata/roms/ports/halo/`.
   The first start copies its `maps/` folder out (a few minutes), and the
   image can then be deleted. Or copy an extracted `maps/` folder there.
4. Start Halo from the Ports list.

Hold the hotkey (MENU, or SELECT) and push START to quit. The log is
`halo/log.txt`, the settings `halo/config.toml`, the saves and the shader
cache `halo/save/`.

## Build

The guest image is the Android build's (`ninja build/android/halo_guest.elf`,
which needs the Android NDK and a clang with the `arm64_32` target). The host
is built with the `aarch64-linux-gnu` cross compiler against the device's
own SDL2 (copy `libSDL2-2.0.so.0*` from the device's `/usr/lib`), with SDL2's
headers, and against glibc 2.31 and libglvnd's EGL and OpenGL ES, Debian
11's, which `glibc_sysroot.sh` downloads and checks:

```
python configure.py --release --android-ndk <ndk> --android-guest-cc clang-22
sh port/knulli/glibc_sysroot.sh <glibc folder>
SDL2_INCLUDE=<folder holding SDL2/SDL.h> SYSROOT_LIB=<the device's libraries> \
ANDROID_NDK=<ndk> GLIBC_SYSROOT=<glibc folder> sh port/knulli/build.sh
```

The result is in `build/knulli`. Built on glibc 2.31 (the build's last line
names the newest version the host needs, and fails past 2.31), and linked
against EGL and OpenGL ES by their usual names (`libEGL.so.1`,
`libGLESv2.so.2`) rather than the Mali driver's (`libmali.so.0`), the host
also runs on the systems for these handhelds with an older C library than
Knulli's (muOS, ArkOS). It needs SDL2 2.0.18 or newer.

## How the port operates

The host is the Android host library (`port/android/host`) with these files
replaced or added:

| File | Function |
| --- | --- |
| `host/host_main.c` | The entry point: no JNI or APK; the image and the game data are files next to the executable. |
| `host/host_sdl2.c` | SDL3's calls answered by the firmware's SDL2, the only SDL with the Mali framebuffer's video driver. |
| `host/host_sdl3_events.c` | SDL3's event layout, attribute and type numbering (the guest was built against SDL3). |
| `host/host_glthread.c`, `glthread_gen.py` | The GL thread (below). |
| `host/host_profile.c`, `profile.py` | A sampling profiler (`HALO_PROFILE_HZ`). |
| `host/host_gl_timing.c`, `.S` | A timer of the driver's functions (`HALO_GL_TIMING`). |
| `compat/android/log.h` | The NDK's log functions, to the standard error stream. |
| `Halo.sh`, `sdl_mapping.py`, `halo_extract.py`, `halo_screen.py` | The launcher: the controls, the clocks, the first start's maps, and messages on the screen while the game is not running. |

### The GL thread

On the H700's Cortex-A53 the Mali driver's own work (about 30 µs for each
draw call) is most of a frame. The guest's OpenGL ES calls are recorded into
a queue and made by a thread of their own, so the driver's work runs on
another core than the game. Calls that return nothing are queued with a copy
of the memory their pointers refer to; calls that return a value wait.
`glGen*` names come from a reserve the GL thread keeps filled. The game can be
one frame ahead (`HALO_GL_THREAD_FRAMES`, 7 at most). `HALO_GL_THREAD=0` turns
it off.

Shader programs are built beside it: two threads (`halo-load`, `halo-compile`)
load them from the cache or compile them on EGL contexts that share the GL
thread's, and the GL thread skips the draws made with a program until it is
built, keeping the uniforms set for it. `HALO_ASYNC_PROGRAMS=0` builds them on
the GL thread instead, stalling the frame for each.

Textures are decoded and uploaded beside it too: a thread of the guest's own
(`xbox_textures.c`) gets a shared context (`host_gl_texture_thread`), whose
GL calls the recording functions then pass straight to the driver
(`glthread_direct`). The draws that use a texture are skipped until its first
upload is done (`debug.async_textures`). Shaders are translated on a thread
of the guest's own as well (`debug.async_shaders`), and the draws that need
one are skipped until it is in.

Frames are paced to the display. Below 60 fps Mali's swap on the
framebuffer does not wait, and a frame is shown from the next refresh; the
H700's LCD timing controller, read through `/dev/mem`, tells where that
refresh is. Each frame is given the refresh it is due at as it begins, the
game times the frame by it (`host_gl_frame_due`), and the GL thread holds a
frame that is ready early until the refresh before (`display.frame_pacing`).
A frame the GPU finishes too late is shown a refresh late, and the frames
after it follow it until one is due two refreshes after the one before:
Mali begins a frame's last pass only once the frame before it is on screen.
The host defines `ioctl` over the C library's to see Mali's pans of the
framebuffer (`HALO_PACING_LOG`).

### Changes to the renderer for this GPU

These are in `port/linux/src`, for `HALO_ANDROID` builds:

- `display.render_scale`: the 3D picture at a fraction of the screen's
  resolution, scaled up by Present.
- `display.fast_shaders`: colours and combiner arithmetic in half precision
  (Mali computes it at twice the rate); texture coordinates stay single
  precision.
- `display.fast_textures`: DXT1 and 16-bit Xbox textures go to the GPU as
  16-bit texels (the colours they hold) instead of 32-bit ones.
- Vertex programs declare only the constant registers they read (Mali
  processes a uniform array whole whenever one element changes).
- One sampler object for each sampler configuration, bound rather than
  reconfigured.
- `glDrawRangeElementsBaseVertex` with the index range the renderer already
  knows, which spares Mali its scan of the indices.
- Linked shader programs are kept in `save/shaders` as the driver's binaries,
  so a combination of shaders is compiled once, not at every start (a link
  takes 60 ms on this GPU: a visible stutter). `debug.no_program_cache`
  turns this off.
- The guest's clock reads go through the C library's vDSO instead of a
  system call.
- Visibility test results (lens flares, the lights' occlusion) are read
  without waiting for the GPU: the counters are copied at the end of each
  frame into three read-back buffers, and a copy the GPU has finished is
  read (`host_gl_visibility_frame`). Reading them directly held the game,
  the GL thread and the GPU in lockstep once a frame.
- Consecutive quad draws that change nothing but constant vertex attributes
  are drawn as one (`debug.batch_quads = false` turns this off), and decals
  whose blend function does not depend on their order are drawn grouped by
  bitmap (`rasterizer_xbox_decals.c`). In a battle, some 700 decal draws a
  frame become about 20.
- The attributes of indexed draws from the mirror point at the base vertex,
  so draws from the same vertex buffer share them (`debug.stable_streams =
  false` turns this off).
- Each program's vertex shader writes only the outputs its pixel shader
  reads, and the point size only for points (`nv2a_vertex_shader_to_glsl`).
  Mali then shades each vertex's position first and the rest only for the
  triangles it keeps; the translated shaders are bound by their outputs'
  stores, not their arithmetic.
- Objects' shadows are drawn in two passes over the objects
  (`rasterizer_xbox_shadows.c`): every shadow's texture first, each into
  targets of its own, then every shadow onto the environment without
  leaving the primary target, blurred as it is read rather than in a pass
  of its own. Switching the primary target out and back for each shadow
  stalled the driver.
- `display.model_detail` (default 0.5): objects switch to their simpler
  models sooner, relative to the rendered resolution.
- The water's bump map is built at the start of the frame, before the
  primary target's first pass (`rasterizer_xbox_water.c`), rather than at
  the first water draw, where it split that pass in two.
- A draw whose alpha test cannot fail (the alpha tested is an opaque
  texture's) is drawn without it (`debug.alpha_test_elision`): a shader that
  can discard loses Mali's hidden surface removal. Textures are known to be
  opaque from their texels, as they are decoded.
- The frame's depth and stencil are invalidated before the picture is
  scaled to the screen, so the GPU does not write them out.
- Programs that read more than 64 vertex constant registers (the skinned
  models, which index them) read them from a uniform block, whose range is
  bound per draw, rather than a uniform array the driver copies whole at
  every change.
- The streaming vertex, index and constant buffers are mapped for good
  (`GL_EXT_buffer_storage`, `host_gl_buffer_persistent`): a write is a copy
  rather than a map and an unmap.

- Opaque models are drawn sorted by shader rather than object by object
  (`debug.sort_models`, `rasterizer_xbox_models.c`): the objects that can be
  drawn in any order are kept between `rasterizer_models_begin` and `_end`
  and drawn together, which spares the driver most of its changes of
  program and textures.
- A texture whose mip levels the game draws one by one (the water's bump
  map) has them drawn into its own levels (`mip_composite_get`): copying
  them from separate targets made the driver wait for the GPU in the middle
  of the frame, some 6 ms a frame where water was in view.

### Frame rates

Anbernic RG35XX H, Knulli Gladiator II, stock thermal limits, frames per
second over the last 40 to 60 seconds of a level's opening:

| Level | 640x480 (`render_scale = 1.0`, an earlier build) | 480x360 (`render_scale = 0.75`, the default) |
| --- | --- | --- |
| Main menu | 60 | 60 |
| c10 (swamp) | 26 | 44 |
| b30 (beach, battle) | 26 | 34–42 |
| a30 (level opening) | 20 | 49 |

The tools below (`HALO_GPU_PASS_TIMING`, `HALO_GL_TIMING`) show which of the
GPU, the GL thread and the game's thread limits a scene.

### First start

`Halo.sh` looks for a disc image (`.iso` or `.xiso`) in `halo/`, or in
`ports/` beside it, when `halo/maps/ui.map` is missing. `halo_extract.py`
copies the maps folder out of it with its progress on the screen
(`halo_screen.py`, which draws on the framebuffer with Python's standard
library), after checking that the card has room for the maps and the
game's cache; the maps a stopped copy of the same image finished are kept.
What goes wrong (no image, not an Xbox one, an incomplete one, no room) is
said on the screen until a button is pressed. Before the game's own first
start, which sets up its cache in `save/z` for about a minute with the
screen black, the screen says so.

### Clocks

The launcher sets the CPU governor to `performance` and holds the GPU at its
top frequency (648 MHz): its governor otherwise keeps it at 420 MHz. The
kernel's thermal governor still lowers both at 70 °C. What they were is kept
in `/var/run/halo-clocks`, so that a start after a launcher that was killed
outright puts them back first. One start runs at a time: the launcher holds
a lock (`/var/run/halo-lock`, with `flock`) that the game inherits, so a
second start while the game runs is refused (`log.txt` notes it).

## Tools for performance work

| Environment variable | Function |
| --- | --- |
| `HALO_FPS_LOG=<seconds>` | The frame rate, the longest frame, memory, temperature and clocks in the log. |
| `HALO_GL_TIMING=1` | Each GL function's calls and time per frame (on the GL thread). |
| `HALO_GPU_PASS_TIMING=1` | Finishes the GPU at each change of render target and logs the time of each target's passes, split into the calls that made them and the GPU's work. `2` also logs one frame's passes in order (`HALO_GPU_TRACE_PASSES_AT=<frame>` another than the 750th, `HALO_GPU_TRACE_PASSES_FRAMES=<n>` that many); `3` also the GPU's time for each draw of those frames, and a slow draw's textures and state; `4` logs, for one frame, how long each change of render target waited in the driver, with the GPU running as it does (nothing finished). |
| `HALO_PROFILE_HZ=<rate>`, `HALO_PROFILE_DELAY=<seconds>` | Samples every thread (`HALO_PROFILE_THREADS=game`: the game's thread only; `HALO_PROFILE_SAMPLES` the samples kept); `profile.py` reports the result, `--frames-over <ms>` only the samples of the game thread's frames longer than that. |
| `HALO_HITCH_LOG=<ms>` | Logs every frame longer than that from the renderer, the game's thread and the GL thread: what the frame made (programs, textures, shaders, geometry), the waits, the slowest calls and the draws skipped for programs being built. |
| `HALO_PACING_LOG=1` | Every 300 frames, from the framebuffer's pans: how many refreshes the frames stayed on screen, how the time from what a frame shows to its showing changed from frame to frame, and how many frames were shown after their due refresh (frame pacing); `2` also logs each late frame. |
| `HALO_TEST_INPUT=walk[:<seed>]` | A bot that walks, turns, looks around and jumps, never firing: traversal benchmarks for the hitch log. |
| `HALO_DEBUG_DRAW_CALLERS=1` | The draws each caller of the draw functions makes, per frame (`2`: their callers' callers; `3` also counts the indexed draws that repeat another draw's geometry and state; `4` skips those, a wrong picture). |
| `HALO_GPU_DUMP_SHADERS=<folder>` | Writes the generated GLSL, to analyse with Arm's Mali Offline Compiler (`malioc -c Mali-G31`). |
| `HALO_DEBUG_FREEZE=textures,program,raster` | Draws keep the state they find, to measure what setting it costs. |
| `HALO_DEBUG_LOD_BIAS=<levels>` | Samples smaller mip levels, to measure what texture bandwidth costs. |
| `HALO_DEBUG_SKIP_GL=glA,glB` | The GL thread does not make these calls, to measure their cost. |
| `HALO_DEBUG_TINY_SCISSOR=1` | Draws one pixel of each draw: the GPU's time without the pixels. |

The renderer's switches (`HALO_DEBUG_DRAW_CALLERS`, `_FREEZE`, `_LOD_BIAS`,
`_TINY_SCISSOR`, and `HALO_BATCH_QUADS`, `HALO_STABLE_STREAMS`) are also
`debug.*` settings in `config.toml`; the variables set them for one run.

`init.txt` in the data folder runs console commands at start-up, for example
`map_name levels\b30\b30` to start a level, and the `rasterizer_*` globals
that turn the renderer's features off.
