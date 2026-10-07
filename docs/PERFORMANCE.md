# Performance of Halo CE on the RG35XX H

This page records how the port got from its first working build to the
current frame rates, what was measured along the way, and what turned out not
to matter. The current numbers are in the [README's performance
table](../README.md#performance). The tools behind the measurements are
described in [Profiling](PROFILING.md), the lessons drawn from them in
[Mali-G31 notes](MALI-G31-NOTES.md), and the work still to do in
[Roadmap](ROADMAP.md).

All measurements are on an Anbernic RG35XX H (Allwinner H700: 4x Cortex-A53
at 1.5 GHz, Mali-G31 MP2, 1 GB RAM) with Knulli Gladiator II and stock
thermal limits.

## Method

- Each run loads a level through `init.txt` (`map_name levels\a30\a30`) and
  plays its opening for 60 to 150 seconds, with `HALO_FPS_LOG=5` writing the
  frame rate, the longest frame, the temperature and the clocks to the log
  every 5 seconds. The figures below are the last samples of a run.
- Runs start from the same temperature: the benchmark script waits until the
  CPU is below 50 °C. At 70 °C the kernel lowers the CPU from 1512 to 1416 MHz
  and the GPU from 648 to 600 MHz, which moves the result by several frames
  a second.
- The beach battle in b30 varies from run to run; a difference of 2 or 3 fps
  there is within the noise. The a30 opening is much more repeatable.
- The instruments are in the host and the renderer: `HALO_GL_TIMING` (the time
  of each GL function on the GL thread), `HALO_GPU_PASS_TIMING` (finishes the
  GPU at each render-target change and times each pass; it slows the game
  down, so only its relative numbers count), `HALO_DEBUG_TINY_SCISSOR` (each
  draw writes one pixel), `HALO_DEBUG_LOD_BIAS`, `HALO_DEBUG_FREEZE` and
  `HALO_DEBUG_SKIP_GL`, and a sampling profiler (`HALO_PROFILE_HZ`). They are
  listed in [port/knulli/README.md](../port/knulli/README.md#tools-for-performance-work)
  and explained, with examples of their output, in [Profiling](PROFILING.md).

## The two limits of this platform

### The Mali driver's CPU cost per draw call

On the Cortex-A53, Arm's driver spends about 28 µs of CPU time on each draw
call: validating state, building descriptors and job chains. Of that, a
texture bind costs about 8 µs and a program switch about 4.5 µs. A frame of
the a30 opening has about 450 draws. Before the GL thread this ran on the
game's own thread; with the GL thread it runs on another core, and at render
scale 0.75 that thread spends 23 to 28 ms a frame inside the driver. This is
the limit at the default settings: the driver alone allows at most about 35
to 40 fps in a busy scene.

### Render-target switches on a tile-based GPU

The Mali-G31 draws a render target a tile at a time in on-chip memory. When
the renderer switches to another target and back, the GPU writes the whole
main target out to memory and reads it back in. The Xbox renderer does this
freely. The clearest case is the object shadows: each shadow switched to the
shadow targets, drew the object's silhouette, blurred it in two more passes
and switched back. In the a30 opening at render scale 0.75 the game ran at
about 24 fps with shadows and about 38 fps with them turned off.

### GPU work at 640x480

At the full resolution the GPU itself is the limit: about 28 ms of fragment
(pixel) work and about 18 ms of vertex and tiler work per frame in a busy
scene. When every draw was limited to one pixel (`HALO_DEBUG_TINY_SCISSOR`)
the GPU-timed b30 run went from about 12.5 to about 22 fps, so pixels are a
large part of it; the rest is geometry. This is why the render scale helps
so much once the pipeline stalls were removed.

## The optimisation history

Approximate frame rates at each step. The early steps were measured at
640x480 in a30, the later ones at render scale 0.75; the build changed in
between, so compare within a step rather than across the whole list. The
batching runs in b30 predate the readback fix, when the render scale made
little difference to the frame rate.

| Step | Scene | Before | After |
| --- | --- | --- | --- |
| First working build (GPU at the governor's 420 MHz) | a30, 640x480 | — | 15 |
| CPU `performance` governor, GPU held at 648 MHz | a30, 640x480 | 15 | 18 |
| Ranged draws, right-sized constant arrays, sampler objects | a30, 640x480 | 18 | 19 |
| GL thread | a30, 640x480 | 19 | 20 |
| Program binary cache | a30, 640x480 | 20 | 20, without the 60 ms link stutters |
| Quad batching and decal ordering | b30 | about 22 | about 25 |
| Asynchronous occlusion readback | b30, 0.75 | about 23 | about 31 |
| Code-quality pass on the renderer and the GL thread | b30, 0.75 | about 31 | about 33–36 |
| Model LOD scaling (`display.model_detail` 0.5) | a30, 0.75 | 24.0 | 25.3 |
| Shadows in two passes | a30, 0.75 | 25.3 | 25.7 |
| Shadow blur folded into the projection shader | a30, 0.75 | 25.7 | 31–32 |
| Vertex shaders write only what the pixel shader reads, point size only for points | a30, 0.75 | 31–32 | 36 |
| Water bump map at the frame's start, alpha test dropped where it cannot fail, depth not written out | a30, 0.75 | 36 | 40 |
| Skinned models' constants in a uniform block; streaming buffers mapped for good | a30, 0.75 | 40 | 40 (GL thread time down, GPU now close to the limit) |
| Opaque models drawn sorted by shader | a30, 0.75 | 40 | 40 (program switches 190 to 109 a frame) |
| Water bump map levels drawn into the sampled texture, not copied | a30, 0.75 | 40 | 49 |
| Skinned models' constants in two blocks (per part, and per object's nodes, written only as far as the nodes go); streaming buffers mapped as cached memory | a30, 0.75 | 49 | 49–51 (buffer writes 2.9 ms to 1.9 ms a frame) |
| The guest's `memcmp` and `memcpy` eight bytes at a time; the GL queue published in 4 KB steps, not after every call | a30, 0.75 | 49.5–50 (with the GL timer on) | 51.5–52 with the GL timer on, 52–54 with it off |
| The skinned-constant cache searched through a compact array of keys; a per-write `getenv` removed from the GL thread | a30, 0.75 | 52–54 | 54–55 |
| Repeated skinned-model draws drawn as instances; two-sided parts sorted a side at a time | a30, 0.75 | 54–55 | 53–57 (GL thread's draw calls 7.8 to 5.3 ms; the GPU is now the limit) |

Notes on the steps:

- **Clocks.** The GPU's devfreq governor kept it at 420 MHz under load.
  Writing the top frequency to `min_freq` holds it at 648 MHz while the game
  runs (the launcher restores the old value on exit).
- **GL thread.** At 640x480 the gain was small because the GPU was the limit.
  Its value shows at lower render scales, where the driver's CPU time is the
  limit and now overlaps with the game's own work.
- **Program binary cache.** A program link takes about 60 ms on this GPU.
  Caching the driver's binaries removed the stutters when new shader
  combinations appear; the average frame rate was unchanged.
- **Quad batching and decal ordering.** In the b30 battle about 700 decal
  draws a frame become about 20, and batches of about 500 quad draws become
  21 to 65 draws. This cuts the driver's per-draw time on the GL thread.
- **Asynchronous occlusion readback.** This was the largest pipeline fix.
  Before it, lowering the render scale in b30 barely helped (about 21 fps at
  1.0, about 23 at 0.75), because the game waited for the GPU's visibility
  results once a frame and so held the game, the GL thread and the GPU in
  lockstep. Reading results the GPU finished a frame or two earlier removed
  the wait, and the render scale started to pay off (about 31 fps at 0.75).
- **Code-quality pass.** A tidy-up of the renderer and the GL thread (one
  stream reservation per batch, a static quad index buffer, the GL thread's
  producer and consumer on separate cache lines). The b30 result rose from
  about 31 to 33–36 fps and the driver time fell from 27–28 to 23–27 ms a
  frame, but the battle varies between runs: read it as no regression rather
  than a proven gain.
- **Model LOD scaling.** Switching to simpler models sooner: 24.0 fps at the
  game's own switch points (1.0), 25.3 at 0.5, 26.4 at 0.3 in a30. The default
  is 0.5.
- **Shadows.** Drawing every shadow's texture first and then projecting all
  of them without leaving the main target gained little on its own, because
  the blur passes still switched targets. Folding the blur into the shader
  that projects the shadow removed those passes, which took a30 from 25.7 to
  31–32 fps. For comparison, turning the blur off entirely gave about 34 to
  35 fps, and turning the shadows off about 38.
- **Vertex shaders sized to their pixel shaders.** Arm's Mali Offline
  Compiler (`malioc -c Mali-G31`) showed every translated vertex shader bound
  by load/store (21 to 33 cycles a vertex) rather than arithmetic (3 to 24).
  The translator wrote every Xbox output register (four texture coordinates,
  four colours, fog) and the point size for every vertex. Writing
  `gl_PointSize` stops the Mali compiler from splitting the shader into a
  position shader and a varying shader, so every vertex paid the full cost,
  including those of triangles culled as back-facing or off screen. Now each
  program's vertex shader writes only the outputs its pixel shader reads, and
  the point size only for point draws. One shader went from 22 load/store
  cycles a vertex to 1 for the position shader and 7 for the rest. The GPU's
  vertex and tiler time in a30 fell from about 23 to 13.5 ms a frame, the
  driver's CPU time a draw from 26 to 19 µs, and a30 rose from 31–32 to 36
  fps.

- **Alpha test that cannot fail.** Most model pixel shaders ended in an alpha
  test (`discard`) of an opaque texture's alpha, which can never discard but
  makes Mali give up its hidden surface removal (forward pixel kill), so
  every overlapping layer of the Pelican's parts was shaded. Textures are
  now known to be opaque from their texels as they are decoded, and such
  draws use a shader without the test: a30 went from 36 to 40 fps with it,
  and stays at 36 without it (`HALO_ALPHA_TEST_ELISION=0`). It costs program
  switches (about 85 a frame became about 190), which the driver pays for.
- **Uniform arrays against uniform blocks.** A microbenchmark on the
  handheld (`glbench`): a draw costs the driver about 6.5 µs to submit; 17.6
  µs when a constant of a 192-register array changed (it copies the array
  whole), 7.0 µs with a 16-register array, and 9.6 µs with the constants in a
  uniform block whose range is bound per draw. The skinned models index their
  constants and so declare all 192; they now read them from a block.
- **What limits the a30 opening now.** A frame-time histogram (every frame
  22 to 28 ms, with and without vsync) and the game's thread's waits (none
  for results or room, about 5.7 ms a frame for the GL thread to finish the
  frame before) show the GL thread limits it, at about 25 ms a frame. Of 423
  indexed draws a frame, 244 draw the same geometry with the same shaders and
  textures as another draw of the frame (six marines, repeated parts).
  Freezing the textures and the program (every draw keeping the state it
  finds, a wrong picture) took the frame rate to 53–55 fps: program switches
  are the driver's biggest cost per draw. Drawing the models sorted by
  shader is the next step.

- **A stall hidden in the water.** Turning the water off took a30 from 40 to
  52 fps, though its drawing costs the GPU about 1 ms. The game draws the
  water's bump map one mip level at a time into four render targets, and
  the port copied them into one mipmapped texture (`glCopyImageSubData`)
  whenever the water was drawn, twice a frame: copies between render targets
  made Mali's driver wait for the GPU in the middle of the frame, taking the
  overlap of the CPU and the GPU away. The levels are now drawn into the
  sampled texture's own levels, and the copies are gone: 49 fps. Copying by
  blits instead was worse (30 fps).
- **Models sorted by shader.** Objects are kept between the model phase's
  begin and end and their parts drawn sorted by shader, permutation and
  geometry; objects with transparent parts or decals (drawn at once) are
  drawn in place. It cut the program switches from about 190 to 109 a frame
  and the texture binds from 594 to 393; on its own the frame rate stayed
  at 40, because the water's stall was the limit. Both the game's thread and
  the GL thread are now busy all of the frame, at about 20 ms each.

- **Writing to the GPU's buffers.** Skipping the copies into the streaming
  buffers (a wrong picture) took the buffer writes from 2.7 ms to 0.09 ms a
  frame: the time was the copying itself, into memory the CPU does not cache
  (about 370 MB/s), some 1 MB a frame of vertex constants. (A later
  microbenchmark showed every mapping mode is write-combined; the gain of
  this step came from writing less.) The skinned models' constants are now
  two blocks: the registers below
  60 (per part) and the object's nodes from 60, written only as far as its
  nodes go and found again when the same nodes are bound again.
- **Where a30 stands.** At about 50 fps the game's thread and the GL thread
  are both busy all the frame (about 20 ms each). The GL thread's time is
  the driver's (about 17 µs a draw, some 460 draws), the buffer writes, and
  waits at changes of render target; the game's thread spends about a
  quarter of its time in the renderer's per-draw work (`prepare_draw`).
- **The game's thread's own overheads.** The profiler showed musl's
  `memcmp` at 6.2% of the game's thread: the guest's musl has no assembly
  version for `arm64_32`, its C version compares a byte at a time, and
  `prepare_draw` compares the 252-byte pixel shader key, 308 bytes of
  per-draw uniform inputs and uniform shadows of up to 128 bytes on every
  draw. Publishing each queued GL call to the GL thread (an ordered store
  and load on a shared cache line, some 5000 times a frame) took another
  4.4% or so. The guest now has its own `memcmp` and `memcpy` working eight
  bytes at a time (`guest_string.c`), and the queue is published every
  4 KB and before any wait. a30 went from 49.5–50 to 51.5–52 fps with the
  GL timer on, and runs at 52–54 fps with it off. The game's thread now
  waits 1.5 to 1.9 ms a frame for the GL thread, which is the limit: 15.2 ms
  a frame in the driver's functions (of which `glDrawRangeElementsBaseVertex`
  7.8 ms for 456 calls, 17.2 µs each; `glBindFramebuffer` 2.6 ms for 25
  calls; `glUniform4fv` 1.3 ms), and 1.9 to 2.4 ms of buffer writes.

- **A cache search that missed the CPU's cache.** The cache of skinned
  models' node constants (128 entries of about 2 KB) was searched by reading
  each entry's key inside the entry: 128 cache misses per search, and about
  170 of the 233 searches a frame find nothing. That loop was 2.7% of the
  game's thread. The keys (stream generation, extent and first register) now
  sit in an array of their own, 3 KB that stays in the cache. A leftover
  debug switch also called `getenv` on every buffer write on the GL thread
  (1.8% of it with `strncmp`). a30 went from 52–54 to 54–55 fps. On the same
  build c10 holds 44.4 fps and the b30 beach battle runs at 36 to 46 fps.
- **What the buffer writes are.** Timing the copy and the flush apart in the
  game: about 620 writes a frame of 1.46 KB on average, 2.84 µs for the copy
  (about 500 MB/s into write-combined memory while the GPU runs) and 0.42 µs
  for the flush. New counters (`HALO_GPU_STATS`) split them: 625 writes and
  894 KB a frame in a30, of which 337 KB are vertex constants (the skinned
  models' blocks), none are pages of the mirror (its data stays put), and
  the rest are streamed vertices and indices (quad batches, immediate
  draws, dynamic geometry). Programs that read at most 64 registers without
  `a0` still take their constants as uniform arrays (`glUniform4fv`).

- **Instancing.** 134 draws a frame in a30 repeated the draw just before
  them with other constants, all skinned models' parts. They were
  separated by the cull mode (two-sided parts are drawn front, then back,
  object by object), and by the pixel shader's constants (the change
  colours differ from marine to marine; 70% of the repeats). The model
  sorting now draws a run of two-sided parts a side at a time, and the
  renderer draws consecutive same draws as instances of a variant that
  reads each instance's constants from uniform blocks
  ([Architecture](ARCHITECTURE.md#instanced-model-draws)): 250 draws a
  frame become 57. Reading every low register per instance doubled the
  vertex shaders' load/store work, and a30 fell to 46 fps; reading only
  those that differ, and drawing a draw left alone with its own program,
  brought it to 53–57 fps. The GL thread's draw calls went from 7.8 to 5.3
  ms a frame and the GPU is now the limit. b30 (35–46 fps) and c10 (about
  44) are GPU-bound and did not change.
- **Where the GPU's time goes in a30.** Without the environment shadows,
  a30 holds 57.5–59.9 fps with every frame at 16.7 ms; without their blur or
  without the motion sensor, nothing changes. The shadows' textures are
  drawn in the middle of the primary target's pass, which splits it: Mali
  writes the whole target out and reads it back in.

- **Hitches when moving around.** A frame rate says little about what is
  felt when walking into a new area: the long frames there. A walk bot
  (`HALO_TEST_INPUT=walk:1`, 120 seconds of b30) and a hitch log
  (`HALO_HITCH_LOG`, [Profiling](PROFILING.md#the-hitch-log)) found where
  they came from, and with the profiler's frame marks (`profile.py
  --frames-over`) what the game's thread did in them:
  - Every new shader program was compiled and linked on the GL thread:
    220 to 250 ms each, five in a run with a warm cache, more with a cold
    one, and each cached one still about 2 ms there. Two threads of their
    own now build programs on contexts that share the GL thread's, and the
    GL thread skips the draws made with a program until it is built
    ([Architecture](ARCHITECTURE.md#programs-built-beside-the-gl-thread)).
  - Making a program on the game's thread cost 0.7 ms: about 60 `strstr`
    scans of its shaders' sources for the uniforms they use. The shaders
    are scanned once each now, in one pass, and a program costs 0.04 ms.
  - Translating a shader cost about 1.5 ms, most of it `vsnprintf` copying
    whole sources while placing their layouts. One pass over the places a
    source says `uniform `, and copies with `memcpy`, brought it to about
    0.8 ms.
  - Decoding a texture cost about 2.2 ms: DXT1's top level was decoded twice
    (once only to see whether it had transparent texels), every level went
    through 32-bit texels before it was packed to 16 bits, and each upload
    allocated, faulted in and freed its own buffer. DXT1 and the 16-bit
    formats are now decoded straight to their 16-bit texels, the alpha is
    read from the blocks, and the buffer is kept: about 1 ms a texture.

  Frames over 50 ms in the walk went from 25 to 10 to 13 between runs,
  their time from about 3.1 to about 0.7 seconds, and the longest during
  play from about 260 to about 110 ms; a30's frame rate did not change.

- **Textures beside the renderer, and a cheaper clock.** The walk's long
  frames were then mostly textures (about 1 ms each to decode on the game's
  thread, and 0.1 to 6 ms each in `glTexImage2D` on the GL thread, tens of
  them in a frame) and shader translation. A thread of the guest's own now
  decodes textures and uploads them on a context of its own
  ([Architecture](ARCHITECTURE.md#textures)), palettized ones (Halo's bump
  maps, 14 to 16 ms each to decode on the game's thread at the largest)
  with a copy of their palette: the walk's first 600 frames went from 46 ms
  of textures on the game's thread to 1. The shader translators format
  their text without `printf` (their own `%s`, `%c`, `%d`, `%u` and `%lu`,
  and `xgpu_format` for the pixel translator's operands) and the layouts are
  placed with one allocation and a table-driven scan of the identifiers:
  shader translation went from 138 to 74 ms in those frames. A sampled
  profile of the b30 battle showed the game's thread as its limit (it
  hardly waits for the GL thread, which has 6 to 8 ms of slack), and 3% of
  it in `QueryPerformanceCounter`, read around every texture set by the
  game's profile timers through a call into the host; it now reads the
  CPU's timer directly (21 ns). A framebuffer's completeness check, a call
  that waited for the GL thread the first time a target was drawn into (the
  sun's glow in b30), is made only with `debug.gl_debug`.

- **The checkpoint.** The walk's longest frame during play, about 100 ms,
  was the game's checkpoint: `game_state_save` writing its 16 MB game state
  to `z:\savegame.bin`, a file on Knulli's FUSE file system, in 80 ms on
  the game's thread. The checkpoint is kept in memory now
  ([Architecture](ARCHITECTURE.md#system-calls-files-and-time)): a 14 ms
  copy, into a buffer whose pages are made at start-up (the first copy into
  fresh pages took 30 ms).

- **Frames paced to the display.** With the hitches gone, what remained in
  motion was its timing. Below 60 fps a frame reached the screen at the
  first refresh after the GPU drew it, while the game blends between its
  ticks by its clock at the frame's start. The H700's LCD timing controller
  tells where the display is in its refresh, so each frame is now given the
  refresh it is due at as it begins, the game times the frame by it, and the
  GL thread holds a frame that is ready early
  ([Architecture](ARCHITECTURE.md#frame-pacing)). Measured from the
  framebuffer's pans over a two-minute walk through the b30 battle: paced,
  the time from the moment a frame shows the world at to its showing stays
  the same from one frame to the next on 83% of frames and steps by a
  refresh on 17%; unpaced, it stays within 2 ms on 52%, moves by 2 to 12 ms
  on 37% and by more on 11%. Most of the steps that remain come from the GPU
  (below). The frame rate is unchanged (49 fps in the walk either way).

- **Shaders translated beside the renderer** (`debug.async_shaders`). A new
  area's shaders, about 0.5 ms each to translate, are translated on a
  thread of their own, and what needs one is drawn once it is in: in the
  first 600 frames of the b30 walk, shader time on the game's thread went
  from 74 to 28 ms.

- **A vertex shader's variant, found again.** Every draw looked for its
  vertex shader's variant among up to 32 (0.2% of the game's thread, mostly
  cache misses); it tries the one it found last first.

## What did not help, or was not the limit

- **The shadows' textures at the start of the window.** Drawing every
  shadow's texture before the primary target's first pass (with the
  first-person weapon's update, the scene's lights and the object list done
  first) left the primary target in one pass, as intended, but a30 fell to
  40 fps: every other frame took about 30 ms of GPU time, with or without
  vsync and with three framebuffers. Not yet understood; reverted.

- **Texture bandwidth.** Sampling mip levels 16 times smaller
  (`HALO_DEBUG_LOD_BIAS=4`) changed the GPU-timed b30 run from about 12.5 to
  about 13 fps. Texture bandwidth is not what limits the GPU. The 16-bit
  textures are kept because they halve the texture memory.
- **fp16 arithmetic.** Half-precision shaders on and off gave the same GPU
  time within the noise (about 12.6 and 13 fps in the GPU-timed b30 runs).
  Shader arithmetic is not the limit either. The setting stays on, since it
  costs nothing.
- **Individual render features.** Turning the renderer's features off one at
  a time (fog, environment decals, specular lights, reflections, detail
  objects, lens flares, bump mapping, the motion sensor, screen effects)
  changed the b30 frame rate by about 2 fps or less each in an earlier build,
  close to the battle's run-to-run noise. No single feature was expensive;
  the cost is spread over many draws.
- **Three framebuffers instead of two.** No change (a30 at 0.75: 24.0 and
  23.9 fps). Measured again in the b30 battle at about 40 fps: no change
  either.
- **A swap interval of 2.** Mali's fbdev driver ignores it (still 60 fps);
  the frame pacing above does what it would have.
- **`-O3` for the guest.** No change in the b30 battle (within its noise).
- **Inlining `datum_get` and `tag_block_get_element_with_size`** (5.5% and
  1.2% of the game's thread in the b30 battle, called across files, so
  never inlined without link-time optimisation): no measurable change. Their
  cost is the cache misses on the game's data, not the calls.
- **Pacing the GPU's time too.** A fence after each swap, waited for on a
  thread of its own, told when the GPU had drawn each frame, and the due
  refresh was predicted from it, alone or with the frames in flight
  followed through the pipeline. Fewer frames were late, but a frame's due
  refresh then lay more than three refreshes after its start, and with the
  game a frame ahead of the GL thread that held the game back: 27 to 37
  fps. Holding a frame at its pan (the framebuffer's flip) rather than its
  swap, with three framebuffers, kept 49 fps but made 40 to 60% of frames
  late: Mali begins a frame's last pass only once the frame before it is
  shown.
- **The battle's GPU time, taken apart.** In the b30 battle at render scale
  0.75 the primary target's GPU time (about 18.7 ms a frame with the pass
  timer) is mostly per-pixel: 10.9 ms at scale 0.5, 8.2 ms with flat
  shading. Smaller mip levels (`HALO_DEBUG_LOD_BIAS=3`), no anisotropic or
  trilinear filtering, simpler models (`display.model_detail` 0.25) and no
  projective divide in the pixel shaders changed it by under 1.3 ms. Over 60
  frames of the per-draw trace, translucent effects (smoke of some 400
  alpha-blended triangles at 3 to 5 ms a frame, explosions and flashes of up
  to 11 ms a draw) and the shiny character models lead. Below a render scale
  of about 0.625 the game's thread limits the battle instead (51 fps at 0.5
  and at 0.625).
- **The ten-millisecond draw in the GPU's per-draw trace.** One draw of b30
  (a seven-vertex fan with additive blending) took 10 ms in the per-draw
  trace (`HALO_GPU_PASS_TIMING=3`); skipping every draw of its vertex
  shader gained 1 to 3 fps. The trace finishes the GPU after each draw, so a
  draw can carry work queued before it.
- **Letting the game run two frames ahead of the GL thread**
  (`HALO_GL_THREAD_FRAMES=2`). No change (25.7 fps either way).
- **Clearing the blur target first**, so that the tiler does not read its old
  contents, and **batching the blur passes**. No measurable change (25.7 fps);
  the target switches themselves were the cost.
- **Skipping GL calls to measure them.** Skipping every `glBindTexture` gave
  black, invalid frames, so the numbers were meaningless. Freezing the state
  (`HALO_DEBUG_FREEZE`) was used instead to estimate the cost of each kind of
  state change.
- **Deferring the buffer flushes.** Calling `glFlushMappedBufferRange` only
  at changes of render target, fences and the swap, rather than after each
  write, left the buffer writes at about 1.85 ms a frame and the frame rate
  unchanged: their cost is the copy itself. The change was reverted.

## Where the time goes now

The goal is 60 fps (16.7 ms a frame) in the a30 opening. Measured there at
the default render scale of 0.75, the GPU's vertex and tiler work is about
13.5 ms a frame and its pixel work about 16 ms; the GL thread's driver time
is about 16 ms plus waits for the GPU; the game's own thread about 19 ms. All
of them have to come down. The ideas with the most expected payoff:

1. Draw the HUD at full resolution when the render scale is below 1.
2. Cut the driver's per-draw cost further: merge uniform uploads into one
   array, reduce texture rebinds, and merge the environment's lightmap and
   diffuse passes (a large engine change).
3. Train a profile-guided optimisation profile on the device (the guest uses
   upstream's x86 Linux profile).

Since then the game's thread has become cheaper (see the last notes in the
history above), and a30 runs at 53 to 57 fps, limited by the GPU. In the
b30 battle the game's thread is the limit: of its time, about 48% is
drawing (the game's renderer and the Direct3D translation), 23% the game's
tick (objects, AI, collision) and 14% waiting for the GL thread. On
2026-10-02 the GL thread turned out to be limited by the driver's CPU time,
not the GPU, and a30 reached about 58 fps
([Where the threads' frames go](#where-the-threads-frames-go-2026-10-02)).
[Roadmap](ROADMAP.md) keeps the current list of limits and planned work.

## Upstream at c55e4e2b (2026-10-01)

Rebasing the port onto upstream's latest (62 commits) cost a30 2 to 3 fps
(51.3 and 52.0 against 53.8), all of it on the game's thread. Under the same
host the previous game image ran a30 at 54.5 fps, so the cost was in the
game: profiles of the two images showed `tag_block_get_element_with_size`,
`object_get_and_verify_type` and `tag_get` taking 7% more of the game's
thread. Upstream's release builds now check the assertions and log the
failed ones (`release_assert_failed`, commit f15c1e26). Before, the
expressions were only evaluated, and the compiler dropped the checks in
those accessors, which the game calls thousands of times a frame. The
Knulli build keeps the retail behaviour (nothing checked, the expressions
still evaluated): a30 is back at 53.4 fps, b30's walk at 48.8, and the game
image is 400 KB smaller. Upstream's high-res HUD (`display.high_res_hud`)
took 77 MB more memory and about 1 fps in a30; the launcher writes it off
in a new `config.toml`.

## Upstream at 9f3e8c92 (2026-10-02)

Three commits on from c55e4e2b: upstream's high-resolution text and the
menus' redrawn titles (`display.high_res_text`), and players' names above
their heads in multiplayer. Rebased, a30 and the b30 battle run as before
(a30 about 53 fps over its first minute, the battle 36 to 43). The
high-resolution text cost no frame rate once a level ran, but the level's
first five seconds ran at 20 to 25 fps while its glyphs were drawn, and the
game held about 100 MB more memory; the launcher writes it off
([Configuration](CONFIGURATION.md#displayhigh_res_text)). The game image is
2.3 MB larger with the fonts and titles embedded.

## Upstream at 76addf66: co-op, the PC menus (2026-10-06)

The port merged upstream's online co-op, server browser and PC menus, its
per-pixel lighting and anti-aliasing (both off by default, and not used by
the port's draws on the handheld), 64-bit vertex constant serials (the
32-bit ones wrapped within minutes to an hour at a high frame rate, after
which a program could draw with another object's matrices for a frame), and
its new audio (a windowed-sinc resampler and I3DL2 reverb). The game image
is 17.2 MB (10.8 before), and the game holds about 40 MB more: 268 MB at
the PC main menu, 425 to 435 MB in the b30 battle.

On the RG35XX H, 120 s, EmulationStation stopped (and so no sound),
render scale 0.75, the second run of each:

| Scene, last 50 s | v2026.10.06 | v2026.10.06.1 |
| --- | --- | --- |
| a30 | 56.4 | 57.8 |
| b30 battle | 43.0 / 43.9 | 43.9 |

Launched from EmulationStation with sound, a30's last 50 s: v2026.10.06
53.7 fps; v2026.10.06.1 52.5 with the reverb on (the default) and 53.3 with
it off (`audio.reverb = false`).

The first merge left upstream's per-pixel lights uploaded at every draw to
uniform location 0 of the programs the host builds (whose record had no
location for them): GL_INVALID_OPERATION at every draw and a30 held at 50
fps, until the location was marked absent. The GL thread's health check
([Architecture](ARCHITECTURE.md#the-gl-thread)) showed the error; a trace of each command's error found the call.

## Upstream's game logic for many enemies (2026-10-06)

Upstream's commit 197c1994 sped up its many-enemy co-op games (484 actors,
53 to about 97 fps on a PC). Its game-logic half is taken: cluster lists
that remove a reference where it was put instead of searching for it,
object lighting that keeps each cluster's lights, a data array that stops
walking a full table for every new entry, and a texture timer that runs
only while something reads it. Its renderer half, partly for Mesa's GL
thread, and its companions for co-op's larger actor pool are not.

On the RG35XX H, release v2026.10.02.1 against this build, 120 s each with
EmulationStation stopped, render scale 0.75, cooled to 55 C first:

| Scene, last 50 s | v2026.10.02.1 | v2026.10.06 |
| --- | --- | --- |
| a30 | 55.0 | 56.4 |
| b30 battle, run 1 | 42.2 | 43.0 |
| b30 battle, run 2 | 41.9 | 43.9 |

About 3% in the battle, which has far fewer actors than upstream's test;
memory is the same. Benchmarks need EmulationStation stopped:
with it idle in the background, its frames go to the screen between the
game's, every run is held at 30 fps, and the menu shows through the game.

## Text, sound and memory (2026-10-07)

**Text.** Text was drawn a character at a time, each character its own
draw. The server browser made about 1,225 draws a frame and ran at 18 fps.
Characters and other small quads drawn one after another are now gathered
into one draw: the browser holds 60 fps, and the b30 battle gained about 8%
(42.4 and 43.0 fps against 39.7 and 39.1, alternating runs).

**Sound on muOS.** Firing into shielded enemies in the b30 beach battle
dropped muOS to 9 to 23 fps, with 100 to 160 ms frames. A profile of the
game's thread in those frames put half its time waiting for the sound
mixer's lock. The mixer, upstream's since 76addf66, resampled every voice
with a 32-tap windowed sinc while holding that lock, and the battle had 53
to 57 voices: the mixer held the lock 79 to 87% of the time. On the
handheld it now uses 8 taps, skips voices too quiet to hear (under -60
dB) and lets go of the lock every 256 frames instead of 1024. The same
fight then ran at 33 to 46 fps, the mixer held the lock 19 to 41% of the
time, and sound was 5% of the slow frames. On Knulli the old mixer never
stalled like this (39 to 60 fps in the same fight), and the new one runs it
at 45 to 58 fps.

**Memory over a session.** Each level played kept its shaders, about 25 MB
of the GPU driver's memory a level. A program not drawn with since the
current level loaded is now deleted a minute after the load. After six
levels in one session, about 50 MB more memory was free (390 MB against
339 MB).

## Where the threads' frames go (2026-10-02)

**Not the GPU, in a30 and the b30 battle.** The plan was dynamic resolution:
a render scale that drops while the GPU falls behind. Measured first, the
b30 battle ran the same at render scales 0.75, 0.625 and 0.5, at 40 to 46
fps each, so its pixels were not what limited it. A new log of the GL
thread's frame (`HALO_GL_FRAME_LOG`,
[Profiling](PROFILING.md#halo_gl_frame_log)) showed what did. In a30 the GL
thread was running for about 15 ms of its 17.5 ms frames. In the battle it
ran 17 to 19 ms of 22 to 26 ms, and waited another 2 to 6 ms for the game's
thread. In neither was it asleep in the driver for more than a millisecond
or two. Its time is the driver's CPU time, which the render scale does not
change.

c10 is the exception. There the GL thread's swaps take 9 to 20 ms a frame,
waiting for the GPU, and the frames alternate between one and two
refreshes:

| c10 opening | Frame rate |
| --- | --- |
| Render scale 0.75 | 30 to 42 fps |
| Render scale 0.625 | about 56 fps |
| Render scale 0.5 | a steady 60 fps |

So the render scale now follows the GPU (`display.dynamic_resolution`,
[Configuration](CONFIGURATION.md#displaydynamic_resolution-and-displaydynamic_resolution_min)).
In c10 it went from 0.75 to 0.5 in the first four seconds. The level ran at
51 to 58 fps until about 45 seconds in, then held 60: 99% of the frames in
the run's last 50 seconds were on time.

a30 showed what the controller must not do. The GPU does fall behind in
the first seconds of its intro, and the scale stepped down there. But at
render scales 0.6875 and 0.625, one stretch of the level then locked into
frames of one refresh and two, about 50 fps, where 0.75 held 60.

The cause is the frame pacing. At a fixed 0.625, a30 ran at about 51 fps.
The GL thread's frame was 13.2 ms running, about 3.5 ms asleep in the
driver (none without the shadows), and a 2.4 ms hold. With the pacing off
(`display.frame_pacing = false`), the same run held 60 fps with no hold.

So a window of 30 frames at a lowered scale that has 3 or more late frames,
none of which waited for the GPU, now steps straight back up. Like any step
up, it is tried again after a longer wait each time the GPU's lateness takes
it back.

a30 now steps down in its first seconds and is back at 0.75 about 20
seconds in. The pacing at lower scales is next on the
[Roadmap](ROADMAP.md#3-the-gpu-and-the-picture).

At render scale 1.0 the controller holds the b30 battle at 480x360 to
520x390 for much of the fight (37 to 41 fps over the last 50 s of two runs,
about as at 0.75), but does not help a30 after its intro. a30 is limited by the GPU
there too, but the GL thread waits for it inside the driver's other calls
(asleep 7 to 8 ms a frame, the swap 0.16 ms), where the controller does not
look. The driver has no timer queries to measure the GPU instead.

**Buffer writes on the game's thread.** Of the GL thread's 15 ms in a30,
about 1.8 ms were copies. The renderer streams about 460 writes and 900 KB
a frame into buffers mapped for good, and each write was copied twice:
into the queue on the game's thread, then out of it into the buffer on the
GL thread. Now the game's thread copies into the buffer itself and queues
only the flush, once the GPU has passed the fence of the frame that last
used the ring slot ([Architecture](ARCHITECTURE.md#vertex-data-the-mirror-and-the-stream-ring)).

The same build, with and without it (`HALO_DIRECT_WRITES=0`), in a30:

| | Without | With |
| --- | --- | --- |
| Frame rate | 56.3 fps | 58.4 fps |
| The GL thread's CPU time | 15.1 ms a frame | 13.8 ms a frame |
| Frames over 22 ms in 50 s | 306 | 114 |

The b30 battle did not change (about 41 fps): the game's thread limits it.

**The game's thread.** With the GL thread now holding its frames for 2 to 3
ms in a30 for the frame pacing, the game's thread was the limit there too.
It averaged about 16.4 ms of work a frame. Frames that run a game tick took
about 19 ms (5.6 ms of tick and 13.3 of drawing), against a budget of 16.7.
Sampled at 1000 a second:

- In a30, 74% of the thread was the rendering: models 36%, the HUD 5.5%,
  shadows 5.3%. Sounds took 5.3%, most of it collision rays for their
  obstruction, and the tick 5%.
- In the b30 battle, 61% was the rendering and 26% the tick (objects 15%,
  AI 8%).
- No single function took more than 6.3%. The largest were `datum_get`, the
  engine's array accessor, and `prepare_draw`, the Direct3D translation's
  per-draw work.

Two things were plain waste:

- **The HUD's tag search.** For every bitmap it drew, every frame, the HUD
  checked the bitmap's tag reference by searching every tag by name
  (`verify_tag_reference`, `tag_loaded`, `_stricmp`). That was about 3% of
  the game's thread in a30. The reference's own index now answers when the
  tag there has its group and name.
- **The sound obstruction cache.** Upstream caches each sound's
  obstruction for a tick, but in 128 slots chosen by the low bits of
  products of the position's coordinates. Those bits come from the floats'
  low mantissa bits, which positions placed by hand often have all zero.
  10% of the queries in a30 (14% in the battle) found another sound in their
  slot and cast their ray again. The cache now has 1024 slots, and the hash
  mixes the high bits in.

**Where a30 stands.** About 58 fps over the minute after the first 30 s,
with 88 to 96% of frames shown on time (15 to 18 ms) from run to run. The
GL thread now waits 2 to 4 ms a frame for the display (the pacing's hold,
or the final blit waiting for the framebuffer still shown). The late frames
are the game thread's frames with a tick. Without the object shadows
(`render_shadows false`) a30 runs at 59.5 fps with 99% of frames on time;
without the water, 59.7 fps and 99%. The b30 battle runs at about 41 fps,
limited by the game's thread.

## Entering a level (2026-10-01)

The maps on the Xbox disc are compressed. Before a level is played, the
game decompresses it into one of its cache files (`save/z/cache000.map` to
`cache005.map`: two campaign levels, the main menu and three multiplayer
maps) behind the loading screen, as the Xbox did to its hard disk
([Architecture](ARCHITECTURE.md#system-calls-files-and-time)). Measured on
the RG35XX H's card with a per-second log of `/proc/diskstats` and the
kernel's dirty memory: about 150 MB read and 215 to 280 MB written, 20 to
25 s of loading screen. The kernel keeps at most 50 to 70 MB of those
writes in memory, and writes the last of them in the level's first
seconds. On a card in good shape that costs nothing visible: the level
starts at its usual frame rate.

Benchmarks that entered one level after another right after a large copy
to the card (the maps folder copied out and compared, 1.8 GB each way) saw
the level start at 1 to 8 fps for 30 s or more, with the processors idle:
the card was still taking in the earlier writes, at a fraction of its usual
speed, and the level's reads waited behind them. A player can meet this on
the first level after installing (the maps, then the game's 760 MB of cache
files, are all written just before).

Flushing the cache file at the end of the copy (`fdatasync` through the
FUSE exFAT driver), so that the loading screen would wait for the card,
was tried and is not in the port: with it, on a card in good shape, the
level started at 1 to 9 fps for 5 to 20 s (three levels, against none
without), with the disk idle and the kernel reclaiming memory.

A plain `sync()` at the end of the copy (the kernel's dirty pages written
out, without the driver's flush) is in the port (2026-10-02). It was
measured with three levels the cache did not hold (a30, b30, c10), each
entered with and without it: on a card made busy before each level (1 GB
written and deleted with `dd`), and on the same card after five idle
minutes. The 5 s samples of the first 50 s after each level loaded:

| Card | Level | Without `sync()` | With it |
| --- | --- | --- | --- |
| Busy | a30 | 16, then 2 to 3 to the end | 20 and 10, then 39 to 56 |
| Busy | b30 | 32, then 3 to 8 for 35 s, then 26 and 60 | 39 and 48, then 38 to 60 (as usual) |
| Busy | c10 | 33, then 3 to 11 to the end | 30 to 47 (as usual) |
| In good shape | a30 | 37 to 60 | 35 to 59 |
| In good shape | b30 | 39 to 60 | 38 to 60 |
| In good shape | c10 | 20, then 31 to 43 | 30 to 44 |

The loading screen took 18 to 24 s without and 22 to 29 s with, 3 s longer
on average over the six pairs: the copy's last writes are made behind it
instead of during the level.

The rebase onto c55e4e2b had also turned off the checkpoint kept in memory
([Architecture](ARCHITECTURE.md#system-calls-files-and-time)): it was under `HALO_LINUX`,
which upstream no longer defines (4adc3a87), so each checkpoint wrote its
16 MB to `save/z/savegame.bin` again, on the game's thread. It is back
under `HALO_ANDROID`, and `tools/upstream_check.sh` now lists the macros the
patch's preprocessor conditions test and warns about any that no `#define`
in the sources and no `-D` in the build's scripts defines.

The release's numbers (2026-10-01; the 5 s samples of the first minute
after each level loaded, the level in the cache; the README's table):

| Scene | 0.75 | 1.0 |
| --- | --- | --- |
| c10, opening | 31–44 (44 after the first minute) | 19–28 |
| b30, beach battle | 35–44 | 28–34 |
| a30, opening | 39–60 | 26–47 |

The 640x480 column's previous measurement, on 2026-09-30's first builds,
gave 20 to 26 fps; a30 at 1.0 has since doubled, with the GPU work the
history above took out.

The release of 2026-10-02, with dynamic resolution on (the default), after
the frame pacing's fix ([below](#frame-pacing-after-a-long-frame-2026-10-02)),
and with the level in the cache:

| Scene | 0.75 | 1.0 |
| --- | --- | --- |
| c10, opening | 46–60 (60 after the first 45 s) | 23–60 (60 after the first 45 s) |
| b30, beach battle | 35–46 | 28–47 |
| a30, opening | 39–60 | 26–60 (40 once past the intro) |

Over the last 50 s of two-minute runs: a30 57.7 fps at 0.75 and 39 to 40
at 1.0, c10 60.0 at both, and the b30 battle 41.0 to 41.7 at 0.75 and 36.5
to 41.3 at 1.0.

## Shaders on a fresh install (2026-10-02)

The port keeps the driver's compiled programs in `save/shaders`
([Architecture](ARCHITECTURE.md#the-program-binary-cache)). Each program is
compiled once, the first time the game draws with it, on threads beside the
GL thread. Meanwhile the GL thread skips the draws that need it rather than
wait ([Architecture](ARCHITECTURE.md#programs-built-beside-the-gl-thread)).

With that cache emptied, a30's first 10 seconds compiled 91 programs and
skipped 23,207 draws over 602 frames:

| | First 5 s | Next 5 s |
| --- | --- | --- |
| Cache empty (fresh install) | 35 fps | 47 fps |
| Cache in place | 51 fps | 60 fps |

After the first seconds, frames took up to 100 ms. The cost comes the first
time each scene is drawn after an install, and is gone the next time.

The port does not compile every program behind the first start's notice.
That would need the list of programs before the game draws. A pixel
program's key is the game's own combiner state, worked out from the map's
shaders as they are drawn. A list made by playing every level would be
derived from the game's data, and this project ships none of it.

## Frame pacing after a long frame (2026-10-02)

At render scale 1.0, the a30 opening ran its first 10 seconds at 8 to 19
fps. The hitch log showed what happened after the level's first frames (325
and 367 ms):
- The game's thread made frames of whole refreshes: 266 ms, then a refresh
  less every four frames, down to 100 ms.
- Each time, it was waiting for the frame before.

The pacing gives each frame a due refresh from the time frames lately took
from their start to their swap
([Architecture](ARCHITECTURE.md#frame-pacing)). That time also counted the
wait of a frame's commands while the GL thread held the frame before to
its own due refresh. So one long frame made the frames after it due as far
out and held as long, and their own times stayed as long. Leaving that wait
out:

| a30 at 1.0, 5 s samples | First | Second | Third | Fourth |
| --- | --- | --- | --- | --- |
| Before | 30.1 | 12.4 | 48.2 | 56.6 |
| After | 45.4 | 59.7 | 57.1 | 58.2 |

The same loop could follow any long frame, not only a level's first.
