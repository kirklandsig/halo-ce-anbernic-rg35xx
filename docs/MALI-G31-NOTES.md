# Notes on the Mali-G31 for renderer porters

This page collects what the port taught about running a Direct3D-era
renderer on the Mali-G31 MP2, a small Bifrost GPU, with Arm's proprietary
OpenGL ES driver on the Cortex-A53 cores of the Allwinner H700. Halo's Xbox
renderer assumes a GPU that switches render targets freely, a driver that
costs little per draw, and shaders that write every output; on this
platform each assumption has a price. Each lesson below states the
measurement behind it, taken on an Anbernic RG35XX H under Knulli, mostly in
the opening of the a30 level. The frame rates come from different builds
along the way, so compare the numbers within a lesson, not across lessons.
How the measurements were taken is in [Profiling](PROFILING.md); the full
history is in [Performance](PERFORMANCE.md).

## Contents

- [The platform](#the-platform)
- [The cost of a draw](#the-cost-of-a-draw)
- [Render-target switches](#render-target-switches)
- [Copies between render targets](#copies-between-render-targets)
- [Reading GPU results back](#reading-gpu-results-back)
- [Vertex shader outputs and gl_PointSize](#vertex-shader-outputs-and-gl_pointsize)
- [Alpha test and forward pixel kill](#alpha-test-and-forward-pixel-kill)
- [Uncached buffer mappings](#uncached-buffer-mappings)
- [glBufferSubData and unsynchronized writes](#glbuffersubdata-and-unsynchronized-writes)
- [Depth, stencil and clears](#depth-stencil-and-clears)
- [What was not the limit](#what-was-not-the-limit)
- [Shader programs](#shader-programs)
- [Texture uploads](#texture-uploads)
- [Clocks and heat](#clocks-and-heat)
- [The CPU side](#the-cpu-side)

## The platform

| Item | On the RG35XX H under Knulli Gladiator II |
| --- | --- |
| CPU | 4x Cortex-A53 at 1.5 GHz (1512 MHz) |
| GPU | Mali-G31 MP2, top step 648 MHz |
| Driver | Arm's OpenGL ES driver on the Linux framebuffer, through SDL2's `mali` video driver. The log reports `OpenGL ES 3.2 v1.r20p0-01rel0...` |
| Useful features | `glCopyImageSubData`, border clamp, atomic counters in fragment shaders, base-vertex draws, `GL_EXT_buffer_storage` |
| Missing | S3TC (DXT textures must be decoded on the CPU), anisotropic filtering, timer queries |
| No other path | No X11, Wayland, DRM/KMS or Vulkan |

## The cost of a draw

**The driver's CPU time per draw is the first limit.** On the Cortex-A53,
the driver spent about 28 µs of CPU time on each draw call in the early
builds: validating state, building descriptors and job chains. Of that, a
texture bind cost about 8 µs and a program switch about 4.5 µs. A frame of
the a30 opening has about 450 draws, so the driver alone took 23 to 28 ms a
frame at render scale 0.75. In the current build an indexed draw costs the
driver about 17 µs: 456 calls of `glDrawRangeElementsBaseVertex` take
7.8 ms a frame (17.2 µs each) out of 15.2 ms in the driver.

**A microbenchmark separates the parts.** A standalone SDL2 and OpenGL ES
program written for this project ([tools/microbench/glbench.c](../tools/microbench/glbench.c)) made
2000 draws of two triangles each, with seven vertex attributes and four
textures, and timed their submission between two `glFinish` calls:

| What changed before each draw | CPU time to submit a draw |
| --- | --- |
| Nothing (a bare `glDrawRangeElementsBaseVertex`) | about 6.5 µs |
| Constants in a `uniform vec4 c[192]` array (one `glUniform4fv` of four registers) | about 17.6 µs |
| The same in a `uniform vec4 c[16]` array | about 7.0 µs |
| The range of a uniform block holding 192 registers (`glBindBufferRange`) | about 9.6 µs |

When one element of a uniform array changes, the driver processes the
array whole: the cost grows with the array's declared size, not with what
changed. The renderer therefore declares only the registers a vertex
program reads, and programs that read more than 64 (the skinned models,
which index theirs) read them from uniform blocks whose range is bound per
draw ([Architecture](ARCHITECTURE.md#vertex-constants)).

**Program switches are the most expensive state change.** Freezing state
for measurement (draws keep the state they find; the picture is wrong)
showed where the per-draw cost goes. In the a30 opening on a build that ran
at 40 fps, freezing the textures alone left 40 fps; freezing the textures
and the program gave 53 to 55 fps. Drawing the opaque models sorted by
shader, permutation and geometry cut the program switches from about 190 to
109 a frame and the texture binds from 594 to 393.

**Other per-draw costs, and what removed them:**

- Changing the parameters of a bound sampler makes the driver build the
  draw's sampler descriptors again. One sampler object per configuration,
  made once and bound, avoids it.
- Without an index range, the driver scans each draw's indices.
  `glDrawRangeElementsBaseVertex` with the range the renderer already knows
  spares the scan. Ranged draws, right-sized constant arrays and sampler
  objects together took the first builds from 18 to 19 fps (a30,
  640x480, when the GPU was the limit).
- Mapping and unmapping a buffer for each write cost the driver about 8 µs
  a call, some 450 calls a frame. Buffers mapped once, for good, make a
  write a copy.
- `glGetIntegerv` costs a few microseconds. Keep a shadow of the state you
  set instead of asking for it.
- Many small draws of the same state cost a draw each. Batching consecutive
  quad draws, with decals ordered by bitmap where the blend allows, turned
  about 700 decal draws a frame into about 20 in a b30 battle.

**Take the driver off the game's thread.** With the GL calls replayed on a
thread of their own, the driver's work overlaps the game's. At 640x480,
where the GPU was the limit, this gained little (19 to 20 fps); at lower
render scales, where the driver's CPU time is the limit, it is essential.
Make recording a call cheap: publishing each queued call to the other
thread with an ordered store (some 5000 calls a frame) cost about 4.4% of
the game's thread; publishing in 4 KB steps, and before any wait, removed
it.

## Render-target switches

The Mali-G31 draws a render target a tile at a time in on-chip memory.
Switching to another target and back makes it write the whole target out to
memory and read it back in. The Xbox renderer switches freely.

**Shadows.** Each object's shadow switched to the shadow targets, drew the
silhouette, blurred it in two more passes, and switched back to the primary
target to project it. In the a30 opening at render scale 0.75, the game ran
at about 24 fps with shadows and about 38 fps with them off.

| Change | a30, render scale 0.75 |
| --- | --- |
| Shadows in two passes: every silhouette first, into textures of its own, then every projection without leaving the primary target | 25.3 to 25.7 fps (the blur passes still switched targets) |
| The blur folded into the projection's pixel shader (four taps) | 25.7 to 31–32 fps |
| For comparison: no blur at all | about 34 to 35 fps |
| For comparison: no shadows | about 38 fps |

**Lesson.** Order the frame so that each target is drawn once: auxiliary
targets before the main one, and post-processing folded into the shader
that reads the result rather than done as passes of its own. The water's
bump map is now built before the primary target's first pass instead of at
the first water draw, where it split that pass in two. That was part of a step
that took a30 from 36 to 40 fps, but with the alpha test change below
turned off the same build ran at 36, so on its own it gained nothing
measurable while the GPU's shading was the limit.

## Copies between render targets

The game draws the water's bump map one mip level at a time into four
render targets, and the port copied them into one mipmapped texture with
`glCopyImageSubData` whenever the water was drawn, twice a frame. Turning
the water off took the a30 opening from 40 to 52 fps, although drawing the
water cost the GPU about 1 ms. The copies between render targets made the
driver wait for the GPU in the middle of the frame, some 6 ms a frame where
water was in view, and that took away the overlap of the CPU and the GPU.

| The water's mip levels | a30, render scale 0.75 |
| --- | --- |
| Copied with `glCopyImageSubData` | 40 fps |
| Copied with blits | 30 fps |
| Drawn directly into the sampled texture's levels (a framebuffer per level) | 49 fps |

**Lesson.** A copy or blit between render targets mid-frame can be a full
pipeline stall even when it moves little data. Render into the texture you
will sample, one framebuffer per mip level. A feature whose removal gains
far more than its GPU time is a stall; look for calls that take
milliseconds ([Profiling](PROFILING.md#halo_gpu_pass_timing)).

## Reading GPU results back

The game reads its visibility tests' results (lens flares, the lights'
occlusion) at the start of the next frame. Reading the atomic counter buffer
then waited for the GPU to finish every draw before, and for the GL thread
to empty its queue: the game, the driver and the GPU ran in lockstep once a
frame. In the b30 battle, lowering the render scale barely helped (about
21 fps at 1.0, about 23 at 0.75). Copying the counters at the end of each
frame into the next of three read-back buffers, and reading a copy the GPU
finished frames ago, removed the wait: about 31 fps at 0.75.

**Lesson.** Never make the CPU wait for the GPU in a frame. Accept results
two or three frames late; the NV2A's were late too when the GPU was behind.

## Vertex shader outputs and gl_PointSize

Arm's Mali Offline Compiler (`malioc -c Mali-G31`) showed every translated
vertex shader bound by load/store, at 21 to 33 cycles a vertex, rather than
arithmetic, at 3 to 24. The translator wrote every Xbox output register
(four texture coordinates, four colours, fog) and the point size for every
vertex.

On Bifrost, the compiler can split a vertex shader into a position shader
and a varying shader, so that the varyings are computed only for the
vertices of triangles that survive culling. Writing `gl_PointSize` prevents
the split: every vertex then pays the full cost, including those of
triangles culled as back-facing or off screen.

The translator now writes only the outputs the program's pixel shader
reads, and `gl_PointSize` only for point draws:

| Measure | Before | After |
| --- | --- | --- |
| One shader's load/store cycles a vertex (`malioc`) | 22 | 1 for the position shader, 7 for the rest |
| The GPU's vertex and tiler time, a30 | about 23 ms a frame | about 13.5 ms a frame |
| The driver's CPU time a draw | 26 µs | 19 µs |
| a30, render scale 0.75 | 31–32 fps | 36 fps |

**Lesson.** Tie each vertex shader to the pixel shader it is linked with,
and never write `gl_PointSize` unless drawing points. Declaring the colour
outputs `mediump` also halves what is stored for them.

## Alpha test and forward pixel kill

Mali removes hidden fragments before shading them (forward pixel kill),
but not for a shader that can discard. Most of the game's model pixel
shaders ended in an alpha test (a `discard`) of an opaque texture's alpha,
which can never discard, so every overlapping layer of the Pelican's parts
was shaded.

The texture decoder now records whether every texel of a texture has an
alpha of 1, and a draw whose alpha test cannot fail uses a shader without
it. On the build of the time, the a30 opening ran at 40 fps with it and at
36 without it (`HALO_ALPHA_TEST_ELISION=0`). The extra shader variants raised the program
switches from about 85 to about 190 a frame, which the driver pays for;
sorting the models by shader then brought them back to 109.

**Lesson.** Find discards that cannot happen, and remove them. Knowing a
texture's opacity when it is decoded costs almost nothing.

## Uncached buffer mappings

**Every mapping is write-combined.** A microbenchmark on the handheld
([tools/microbench/glmap.c](../tools/microbench/glmap.c)) shows that
every persistent mapping the driver gives is write-combined memory, whatever
the flags: reading 1 MB back takes about 53 ms (some 20 MB/s) for a coherent
mapping, a flushed one, and one with `GL_MAP_READ_BIT` alike. Writing it is
fast in isolation, about 4 GB/s (a 960-byte copy and its
`glFlushMappedBufferRange` take 0.31 µs). In the game, with the GPU using
memory at the same time, a write of 1.46 KB on average took 2.84 µs (about
500 MB/s) and its flush 0.42 µs, some 620 writes and 900 KB a frame. An
earlier note here said a mapping with the read bit was cached memory and
twice as fast to write; the benchmark does not bear that out, and the gain
measured then came from writing less.

**Write less.** The renderer streamed some 1 MB a frame of vertex
constants into memory the CPU did not cache, at about 370 MB/s. Skipping
the copies (a wrong picture) took the buffer writes from 2.7 ms to 0.09 ms
a frame: the time was the copying itself. Splitting the skinned models'
constants into two blocks (per part, and the object's nodes written only as
far as the nodes go and found again when the same nodes are bound again),
together with the mapping made readable, took the buffer writes from 2.9 to
1.9 ms a frame; the benchmark above puts that gain on the smaller writes.
The nodes' block is uploaded whole again since v2026.10.07: a vertex bound
to one node still reads a second one past the object's, weighted 0, and the
buffer's bytes there could read as a NaN (0 × NaN is NaN), which flung
vertices across the screen.

**The flushes are cheap.** Deferring `glFlushMappedBufferRange` to the
changes of render target, the fences and the swap left the buffer writes at
about 1.85 ms a frame and the frame rate unchanged. The change was reverted.

**Orphaning keeps memory.** Mobile drivers keep every orphaned copy of a
buffer until the GPU is done with it, so a large buffer orphaned each frame
costs its size for every frame in flight. Upstream streams each frame into
the next of three buffers, reused only after the GPU passes the fence of
the frame that last used it.

## glBufferSubData and unsynchronized writes

**Don't mix them on one buffer.** The renderer keeps the Xbox's vertex and
index memory in GL buffers of 4 MB, updated a page at a time. A page that
changed is written again with `glBufferSubData`; a page written for the
first time, which no queued draw can be reading, went in through an
unsynchronized mapping, to save the driver's copy. When queued draws still
read a buffer, Mali's driver carries out a `glBufferSubData` by giving the
buffer new storage straight away and copying the old contents into it on
the GPU's timeline. An unsynchronized write made in the meantime goes into
the new storage, and the copy then lands on top of it with the old bytes.

In the game this showed after a level change: the next level's load writes
some pages again (the previous level's) and some for the first time, in the
same buffers, and some of the new level's vertices were drawn as the old
level's. Marines in The Silent Cartographer after The Pillar of Autumn had
legs drawn as thin black lines, or no body at all. A level loaded first
writes no page twice, so it never showed there. With every page written by
`glBufferSubData`, or the buffers bypassed, the models were whole.

Since v2026.10.07 a buffer remembers the frame of its latest
`glBufferSubData`, and a first-time page goes in unsynchronized only once
the GPU is known to have finished that frame (the renderer waits for each
frame's fence three frames later, when it reuses that frame's stream
buffer); until then it goes in with `glBufferSubData` too. The frame rates
were unchanged.

## Depth, stencil and clears

- **Invalidate what you will not read.** The frame's depth and stencil are
  invalidated (`glInvalidateFramebuffer`) before the picture is scaled to
  the screen, so the GPU does not write them out to memory at the end of the
  pass. This was part of the step from 36 to 40 fps in a30 and was not
  measured on its own.
- **Clearing a target that will be covered** tells a tile-based GPU not to
  read its old contents in. For the shadows' blur target, clearing first
  (and batching the blur passes) made no measurable difference (25.7 fps
  either way): the target switches themselves were the cost.

## Instancing and uniform blocks

**A uniform block holds at most 16 KB.** Linking a program whose block was
larger failed with `L0005 Max uniform block size exceeded`: 16 KB is the
least `GL_MAX_UNIFORM_BLOCK_SIZE` OpenGL ES allows, and Mali's limit. The
instanced skinned models' node registers (132 a draw) therefore share one
block of 1024 registers, packed as far apart as each group's nodes go.

**Constants read at a fixed index are free; at a varying index they are
loads.** The Mali Offline Compiler shows it for a skinned model's vertex
shader: with its low registers read at fixed indices, the compiler keeps
them in 128 uniform registers, and the shader costs 9 load/store cycles for
the position and 25 for the varyings. The instanced variant that reads
every low register at `gl_InstanceID * 60 + k` costs 16 and 49: twice the
work per vertex, and a30 fell from about 54 to 46 fps, GPU-bound. Reading
only the registers that differ between the instances from their own block
(five to seven in the a30 opening) and the rest at fixed indices brings it
back to 9 and 27 to 34.

**The pixel shader's per-instance constants are cheap.** Read from a block
at the instance's index, they add 1 to 7 load/store cycles per pixel, but
the model shaders stay arithmetic-bound (for example 3.1 against 2.6 cycles
a pixel).

**Fewer draws help only while the driver is the limit.** Instancing cut the
GL thread's draw calls from 7.8 to 5.3 ms a frame in a30, and the GPU
became the limit: the GL thread then waits in `glClear` and
`glBindFramebuffer`. In b30 and c10, already GPU-bound, it changed nothing.

## What was not the limit

| Hypothesis | Test | Result |
| --- | --- | --- |
| Texture bandwidth | Mip levels 16 times smaller in each direction (`HALO_DEBUG_LOD_BIAS=4`), GPU-timed b30 run | about 12.5 to about 13 fps: not the limit. The 16-bit textures stay, as they halve the texture memory. |
| fp16 arithmetic | Half precision on and off, GPU-timed b30 runs | about 12.6 and 13 fps: the same within the noise. The setting stays on. |
| Pixels at 640x480 | One pixel per draw (`HALO_DEBUG_TINY_SCISSOR`), GPU-timed b30 run | about 12.5 to about 22 fps: pixels were a large part of the GPU's work at full resolution, and the rest is geometry. |
| Three framebuffers instead of two | a30, render scale 0.75 | 24.0 and 23.9 fps: no change. |
| The game two frames ahead of the GL thread | a30, render scale 0.75 | 25.7 fps either way. |
| One expensive render feature | Features turned off one at a time in b30 (fog, decals, specular lights, reflections, detail objects, lens flares, bump mapping, the motion sensor, screen effects), an earlier build | About 2 fps or less each: the cost is spread over many draws. |
| The shadows' blur and the motion sensor in a30 | `rasterizer_shadows_convolution false`, `rasterizer_hud_motion_sensor false`, instanced build | 52–57 and 50–58 fps against 50–56 in the same session: no change. Without the environment shadows (`rasterizer_environment_shadows false`) a30 holds 57.5–59.9 fps, every frame at 16.7 ms. |

## Shader programs

Compiling and linking one of the port's programs takes 220 to 250 ms of the
A53's time, most of it in `glLinkProgram` (the Mali compiler works at link
time). The driver's program binaries (`glGetProgramBinary`,
`glProgramBinary`) load in about 2 ms, so the port keeps them on disk, keyed
by a hash of both shaders' sources and the driver's version and renderer
strings; a binary the driver rejects is made again. Compile a shader only
when its program is linked, so that a cached program needs no compilation
at all.

The driver has no `GL_KHR_parallel_shader_compile`, but it has
`EGL_KHR_surfaceless_context` and `EGL_KHR_create_context`: a context that
shares the rendering context's objects can be made current on another thread
without a surface, and programs linked or loaded there are used by the
rendering context once the other one has finished (`glFinish`) and the
rendering context binds them. Building programs on such threads, and not
drawing with a program until it is built, takes their cost out of the
frame. Place every uniform, block and sampler in the shaders
(`layout(location)`, `layout(binding)`, ES 3.1) so that nothing has to be
asked of a program after it is linked.

## Texture uploads

Measured with `tools/microbench/glupload.c` (24 textures, all their mip
levels, the CPU time of the calls), uploads are cheaper than the game's
frames suggest:

| Texture | `glTexImage2D` | `glTexStorage2D` + `glTexSubImage2D` |
| --- | --- | --- |
| 256x256 RGB565 | 0.36 ms | 0.32 ms |
| 256x256 RGBA8 | 0.49 ms | 0.45 ms |
| 512x512 RGB565 | 0.87 ms | 0.92 ms |
| 512x512 RGBA8 | 1.59 ms | 1.70 ms |
| 512x512 ASTC 4x4 (compressed) | 1.39 ms | 1.06 ms |

Compressed textures are not much cheaper to upload: the driver's own work
per level dominates, not the bytes. In the game the average
`glTexImage2D` call costs 0.1 to 0.3 ms, with the occasional one at 3 to 6
ms while the GPU is busy. The CPU's decoding of the Xbox's formats (DXT has
no hardware decoder here) cost more than the uploads, until it was made to
write the 16-bit texels directly.

## Clocks and heat

- **The GPU's governor.** Under load, the GPU's devfreq governor kept it at
  420 MHz. Writing its top step (648 MHz) to `min_freq`, with the CPU's
  governor set to `performance`, took the first working build from 15 to
  18 fps (a30, 640x480).
- **The thermal limit.** At 70 °C the kernel lowers the CPU from 1512 to
  1416 MHz and the GPU from 648 to 600 MHz, which moves a result by several
  frames a second. Benchmark from a fixed starting temperature (this project
  waits until the CPU is below 50 °C), and check the clocks in the log.
- **Spinning heats.** Threads that wait for each other should spin briefly
  and then sleep; long spinning keeps a core busy and brings the throttling
  temperature closer.
- **No timer queries.** The driver cannot time GPU work, so the port times
  it by finishing the GPU at each change of render target. That serialises
  CPU and GPU: only the shares between passes are meaningful.

## The CPU side

- **The C library.** The guest's musl has no assembly `memcmp` or `memcpy`
  for its ILP32 ABI, and its C `memcmp` compares a byte at a time. The
  renderer compares a few hundred bytes of state per draw, and `memcmp` was
  6.2% of the game's thread. Versions that work eight bytes at a time,
  together with the GL queue published in 4 KB steps, took a30 from 49.5 to
  50 fps to 51.5 to 52 fps (measured with the GL timer on), and to 52 to
  54 fps with the timer off.
- **The clock.** The game reads the clock very often; reading it through
  glibc's vDSO avoids a system call each time.
