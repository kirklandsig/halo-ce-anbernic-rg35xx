# Documentation

This folder documents the native port of Halo: Combat Evolved to the
Anbernic RG35XX H and the other Allwinner H700 handhelds under Knulli. The
[project README](../README.md) is the front page: what the port is, the
current frame rates, a short install and build, and the FAQ. The documents
below go further, for three kinds of reader: players who want to install
and set up the game, builders who compile it from source, and developers
who change the code or port a similar renderer to the same GPU. Each is
described below, followed by a suggested reading order for each kind of
reader.

## The documents

**[Install](INSTALL.md)** is the player's guide. It lists the supported
handhelds, explains which game data works and how the launcher uses your
disc image, and shows where each file goes on the card. It follows the first
launch step by step, maps the controls and the exit combination, lists where
the logs, saves and settings live, and covers updating, uninstalling and
troubleshooting. It also states exactly what the launcher does to the CPU
governor, the GPU's clock and Knulli's battery saver while the game runs,
and how it restores them.

**[FAQ](FAQ.md)** answers the common questions: whether the handheld can
run the game, the frame rates, the Xbox and PC versions, the legal position,
the PortMaster zip, other firmware (muOS, ROCKNIX) and devices, and why the
port does not use emulation.

**[Configuration](CONFIGURATION.md)** is the reference for every setting
that matters on the handheld: each key of `config.toml` with its type,
default, environment variable and effect, how the file is written and read,
the renderer switches added for the Mali GPU, and the `HALO_*` environment
variables that the host reads directly.

**[Building](BUILDING.md)** builds the port from source on Linux or WSL: the
packages, clang 22 from apt.llvm.org, the Android NDK r28c, the SDL2
headers, the two libraries to copy from the handheld, what `build.sh` does
at each stage, the output files, incremental rebuilds, and the common build
errors with their fixes.

**[How it works](HOW-IT-WORKS.md)** is the short overview of the
architecture: the pieces, why the game runs as an ILP32 guest image, the
Knulli host, the GL thread and the list of renderer changes for the
Mali-G31.

**[Architecture](ARCHITECTURE.md)** is the in-depth developer reference: the
guest image, its import table and the memory layout; the Knulli host's
start-up, SDL bridge, controls and audio; the GL thread's queue, kinds of
call, reserved names and frame pacing; the Direct3D 8 renderer on OpenGL ES
(render targets, the vertex mirror and stream ring, constants in arrays and
uniform blocks, shader translation, the program binary cache, textures,
visibility tests); the changes to the game's rasterizer (model sorting,
two-pass shadows, the water's bump map, decal ordering); a data-flow
diagram; and a source map.

**[Performance](PERFORMANCE.md)** is the history of the optimisations, from
the first working build to the current frame rates, with what each step
gained, what did not help, and where the time goes now.

**[Profiling](PROFILING.md)** is the developer's guide to every measuring
tool: the frame-rate log and histogram, the GL call timer, the GPU pass
timer and its traces, the sampling profiler and its report script, the
draw-caller statistics, the switches that remove work to measure it, and
the shader dump for Arm's Mali Offline Compiler. It shows sample output and
ends with the method used to find which of the game's thread, the GL thread
and the GPU limits a scene.

**[Mali-G31 notes](MALI-G31-NOTES.md)** collects the lessons for anyone
porting a Direct3D-era renderer to a Mali-G31 with Arm's driver, each with
the measurement behind it: the per-draw driver cost and what adds to it,
render-target switches and copies, vertex shader outputs and
`gl_PointSize`, the alpha test and forward pixel kill, uncached buffer
mappings, and what turned out not to be a limit.

**[Contributing](CONTRIBUTING-DEV.md)** explains how to change the code:
where each kind of change goes, how upstream changes are carried as a patch,
how to try a change on the handheld, how to benchmark before and after, and
the coding and comment style. The general contribution rules are in the
root [CONTRIBUTING.md](../CONTRIBUTING.md).

**[Roadmap](ROADMAP.md)** lists what limits the port now, with the
measurements, the planned work in order of expected payoff, and the known
limits.

**[Legal notice](LEGAL.md)** states what the repository contains and does
not contain, the licence, and what you need to play.

Two more READMEs sit next to the code: [port/knulli/README.md](../port/knulli/README.md),
the port's technical summary, and [tools/README.md](../tools/README.md), the
benchmark scripts.

## Where to start

**Players:**

1. The [project README](../README.md) and the [FAQ](FAQ.md).
2. [Install](INSTALL.md).
3. [Configuration](CONFIGURATION.md), for `display.render_scale` and the
   other display settings.

**Builders:**

1. [Building](BUILDING.md).
2. [Install](INSTALL.md), to put the result on the handheld.
3. [Legal notice](LEGAL.md): what must never be committed or shared.

**Developers:**

1. [How it works](HOW-IT-WORKS.md), then [Architecture](ARCHITECTURE.md).
2. [Performance](PERFORMANCE.md) and [Mali-G31 notes](MALI-G31-NOTES.md),
   for why the code is the way it is.
3. [Profiling](PROFILING.md), to measure.
4. [Contributing](CONTRIBUTING-DEV.md) and [Roadmap](ROADMAP.md), to pick
   and make a change.
