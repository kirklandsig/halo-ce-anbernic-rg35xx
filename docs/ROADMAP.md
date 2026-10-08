# Roadmap and known limits

This page says where the port stands, what limits it now, and what is
planned, in order of expected payoff. The goal is 60 fps (16.7 ms a frame)
in the opening of the a30 level at the default render scale of 0.75, the
scene looking out of the Pelican. The limits are stated with the
measurements that show them; the measurements were taken with the tools in
[Profiling](PROFILING.md), and the history that led here is in
[Performance](PERFORMANCE.md). Items move from here into the performance
history as they land.

## Where it stands

On the RG35XX H at render scale 0.75, the menus run at 60 fps. On
2026-10-02, over the minute after each level's first 30 seconds:

- The a30 opening (Halo) runs at about 58 fps, with about 90% of frames
  shown on time.
- The b30 beach battle (The Silent Cartographer) runs at about 41 fps.
- The c10 opening (343 Guilty Spark), with dynamic resolution, runs at 51
  to 58 fps for its first 45 seconds and then holds 60. It ran at 30 to 42
  fps at a fixed 0.75.

The [README's performance table](../README.md#performance) has the
published figures.

A frame is made by three workers that overlap: the game's thread, the GL
thread that runs the Mali driver, and the GPU. For 60 fps each must take
less than 16.7 ms a frame. What limits each scene
([Performance](PERFORMANCE.md#where-the-threads-frames-go-2026-10-02)):

| Scene | The limit |
| --- | --- |
| a30 opening | The game's thread. It does about 16.4 ms of work a frame, and about 19 ms in the frames that run a game tick (5.6 ms of tick, 13.3 of drawing). The GL thread runs about 13 ms a frame and holds or waits for the display for the rest. |
| b30 battle | The game's thread, at about 23 ms a frame: 61% the rendering, 26% the tick. The GL thread runs 17 to 19 ms and waits 2 to 6 ms for it. |
| c10 opening | The GPU: at 0.75 the swaps wait 9 to 20 ms. Dynamic resolution lowers the scale until it keeps up. |

The GL thread's time is the driver's CPU time per call. No lower render
scale changes it; the render scale only helps where the GPU is the limit.

## Planned work

### 1. Fewer and cheaper draws on the GL thread

The driver's per-draw cost is the largest single item. The ways to cut it:

- **Draw repeated geometry as instances.** Of 423 indexed draws a frame in
  a30, 244 draw the same geometry with the same shaders, textures and
  blending as another draw of the frame (six marines, repeated parts).
  `HALO_DEBUG_DRAW_CALLERS=4` measures the upper bound by leaving the
  repeats out.
- **Fewer uniform uploads.** Merge the per-draw uniforms into one array
  or block, so that a draw sets them with one call rather than several.
- **Fewer texture rebinds.** Sorting the models cut the binds from 594 to
  393 a frame; the remaining binds are the next target.
- **Fewer changes of render target.** 25 framebuffer binds cost 2.6 ms a
  frame. Drawing the shadow textures and the motion sensor's target before
  the primary target's first pass would stop them splitting that pass.
- **Merge the environment's lightmap and diffuse passes.** The level
  geometry is drawn in more than one pass; combining these two would cut
  its draws. This is a large change to the game's rasterizer.
- **The stream buffers' copies (done, 2026-10-02).** The game's thread now
  copies into the buffers mapped for good itself, and the GL thread only
  flushes. The GL thread's frame in a30 went from 15.1 to 13.8 ms
  ([Architecture](ARCHITECTURE.md#vertex-data-the-mirror-and-the-stream-ring)).
- **The sun glow's passes.** When the sun is in view, a few single-draw
  passes alternate between a small target and the back buffer, each a
  change of target and about 1.7 ms of the GPU's time at render scale
  0.625. Drawing them without the switches would cut both.

### 2. The game's thread

- **Cheaper per-draw work in the renderer.** `prepare_draw` rebuilds and
  compares the pixel shader key (252 bytes), the per-draw uniform inputs
  (308 bytes) and the uniform shadows on every draw. Tracking what changed
  (dirty flags set by the state functions) would avoid most of it.
- **Sound obstruction (measured, 2026-10-02).** The obstruction rays are
  5% of the game's thread in a30, about 40 sounds a tick. Upstream's
  per-tick cache had lost 10 to 14% of its queries to sounds evicting each
  other from 128 slots. With 1024 slots and a better hash, what is left is
  one ray per sound per tick. Fewer rays would mean changing how often the
  game checks.
- **The HUD's tag search (done, 2026-10-02).** The HUD checked every
  bitmap's tag by searching all tags by name, every frame: 3% of the game's
  thread in a30.
- **The game tick.** In the battle the tick is a quarter of the game's
  thread (objects 15%, AI 8%), and no single function stands out. The
  frames with a tick are the late ones in a30.
- **Profile-guided optimisation for the device.** The guest is optimised
  with upstream's profile, recorded by the x86 Linux build. A profile
  recorded on the handheld would match its code paths.

### 3. The GPU and the picture

- **Dynamic resolution (done, 2026-10-02).** The render scale drops a step
  of 1/16 at a time while the GPU falls behind
  ([Configuration](CONFIGURATION.md#displaydynamic_resolution-and-displaydynamic_resolution_min)).
  c10's opening went from 30 to 42 fps to a steady 60 after its first 45
  seconds.

- **The frame pacing below render scale 0.75.** In the a30 opening at a
  fixed 0.625 about 51 fps; with `display.frame_pacing = false` the same run
  holds 60.
  - The pacing log shows nearly every frame shown a refresh after its due
    refresh, and every sixth on screen for two refreshes.
  - At the lower scales the GL thread sleeps about 3 ms a frame in the
    driver (none without the shadows), so its frames reach their swap late
    in the refresh before their due one. Mali's last pass of a frame then
    misses its refresh, and the frames after it follow until one is due two
    refreshes on.
  - Dynamic resolution steps back up out of it.

  Next: what the shadows' changes of target wait for, and due times that
  follow the refresh frames are really shown at.
- **Dynamic resolution at render scale 1.0.** At 1.0 the a30 opening is
  limited by the GPU after its intro, but the GL thread waits for it inside
  the driver's other calls (asleep 7 to 8 ms a frame), not in its swap or at
  the stream ring, so the scale stays at 1.0 there (about 40 fps). The b30
  battle's waits do show, and it steps down. The driver has no timer
  queries.

  Next: judge from the GL thread's whole frame, its time asleep in the
  driver as the frame split works it out, rather than from two of its
  waits. The shadows' sleep at the lower scales (above) would then have to
  be told apart from the GPU's, by whether a step down cuts the sleep.
- **The HUD at full resolution.** At a render scale below 1, the HUD and
  text are drawn at the lower resolution too. Drawing them at the screen's
  resolution would keep them sharp, and would let the 3D picture go to a
  lower scale without making the HUD harder to read.
- **Vertex work.** Models are most of the GPU's vertex work;
  `display.model_detail` already switches to simpler models sooner, and
  fewer passes over the level's geometry (above) would also cut it.

### 4. Devices and firmware

- The RG35XX H is the one the port is developed on. A user reported the
  RG34XX SP (720x480) working
  ([#1](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/issues/1)).
  The other 640x480 H700 handhelds (RG35XX Plus, SP and 2024, RG40XX H and
  V) are expected to work. The RG CubeXX (720x720) and the RG34XX have other
  screen shapes, which `display.screen_width = 0` (the launcher's) fits.
- Other firmware: each release has a PortMaster zip, tested with
  PortMaster on Knulli and, from v2026.10.07, on muOS 2601 Jacaranda.
  ROCKNIX, whose Panfrost driver the port has not met, is untested.

Reports from other devices and firmware are welcome
([Contributing](../CONTRIBUTING.md)).

## Known limits

- **Frame rate.** Below 60 fps in the campaign at the default settings; the
  b30 battle varies by several frames a second from moment to moment.
- **Heat.** Long sessions reach 70 °C, where the kernel lowers the CPU from
  1512 to 1416 MHz and the GPU from 648 to 600 MHz, and the frame rate
  drops by a few frames a second.
- **Render scale.** The whole picture, the HUD included, is drawn at the
  render scale (0.75 by default) and scaled up. In the a30 opening a lower
  render scale is not faster (about 51 fps at 0.625, the frame pacing), and
  at 1.0 dynamic resolution does not lower the scale in the a30 opening
  after its intro ([above](#3-the-gpu-and-the-picture)).
- **Movies.** Bink video is not available in the upstream port, so the game
  skips its movies.
- **Network play.** Internet play is on (`network.online = true`), and a
  game on the local network needs Wi-Fi too: offline, creating one says
  "Network connection lost". The handheld plays upstream's network version
  24 and loads no Custom Edition maps, so it can't join a game on one, and
  it needs a new release each time upstream raises the version.
- **Late objects.** The first time each combination of shaders is drawn,
  it is compiled on the program builder's thread (220 to 250 ms) and what
  it draws is skipped until then; the cache in `save/shaders/` keeps it for
  later launches. Loading a cached program takes about 2 ms, also beside
  the GL thread.
- **New-area frames.** Shaders are translated, and textures decoded and
  uploaded, beside the game's thread; what they are drawn on appears a frame
  or a few late the first time. Instanced model variants are still
  translated on the game's thread.
- **Checkpoints.** A checkpoint copies the 16 MB game state in about 14 ms
  of the game's thread.
- **Late frames.** A frame not ready about 1.5 ms before its refresh is
  shown a refresh late. Where the GPU is the limit, dynamic resolution now
  lowers the scale instead. The late frames left come from the game's
  thread: in a30 the frames that run a game tick, and in the b30 battle
  most frames.
