# Halo CE on the RG35XX H: frequently asked questions

The first nine questions are the same as in the [README](../README.md#faq).

## Can the Anbernic RG35XX H run Halo: Combat Evolved?

Yes. This port runs Halo CE natively on the RG35XX H under Knulli, as AArch64
code on the H700's Cortex-A53 cores with the Mali-G31 GPU. It is built from
the halo-ce-universal decompilation; you supply the Xbox game's data.

## What frame rate does Halo CE get on the RG35XX H?

At the default render scale of 0.75: 60 fps in the menus, about 40 fps on
343 Guilty Spark (c10, 31 to 44), about 40 fps in the beach battle of The
Silent Cartographer (b30), and about 55 fps at the opening of Halo (a30).
At the full 640x480 it is about 24 to 40 fps. The numbers are improving;
see the [performance table](../README.md#performance).

## Does it work with the Xbox version or the PC version of Halo?

The Xbox version. The decompilation is of the Xbox build, and it reads the
Xbox maps. The PC version (Gearbox's port) has different map files that this
code cannot load. Use a disc image of the original Xbox game.

## Is this legal?

The repository contains only source code and documentation, and its
releases only the port's programs built from that source: the upstream
decompilation's authors released their code under CC0, and so does this
port. Neither contains game files, disc images or maps.
You need your own copy of the Xbox game. Whether decompilation projects are
lawful depends on where you live; this is not legal advice. See the
[legal notice](LEGAL.md).

## Does it need PortMaster?

Not on Knulli: the Knulli zip is a plain port, a launcher script in
`roms/ports` and a folder. Each release also has a PortMaster zip, for
PortMaster on Knulli or on other firmware
([Install](INSTALL.md#installing-with-portmaster)). Both use the firmware's
own SDL2 and graphics driver and none of PortMaster's runtimes.

## Does it work on muOS or ROCKNIX?

With the PortMaster zip, muOS is expected to work: its system has the same
Mali driver as Knulli and what else the port needs, checked from its image,
but it has not been tested on a handheld yet. ROCKNIX is untested, and can
use a different graphics driver (Panfrost). The host is built on glibc 2.31
and links EGL and OpenGL ES by their usual names, so that it loads on
firmware older than Knulli. Reports are welcome.

## Why not run the PC version with Box64 and Wine?

The PC version is a 32-bit x86 Windows program that draws with Direct3D 9.
Running it on the H700 would need x86 translation, Wine, and a Direct3D to
OpenGL ES translation layer, all on four Cortex-A53 cores and 1 GB of RAM.
Each layer costs CPU time the device does not have. The native port runs the
game's own logic as ARM64 code and draws with OpenGL ES directly.

## Why not use xemu?

xemu emulates the whole original Xbox, its Pentium III CPU and its NV2A GPU,
and needs a fast desktop CPU and a desktop OpenGL or Vulkan GPU. The H700's
Cortex-A53 cores and its OpenGL ES-only Mali driver are far below that. A
native port of the decompiled code avoids the emulation entirely.

## Which other handhelds does it work on?

It should work on the other Allwinner H700 handhelds that run Knulli: the
Anbernic RG35XX Plus, SP and 2024, RG40XX H and V, RG CubeXX and RG34XX.
Only the RG35XX H has been tested. TrimUI handhelds and other SoCs are not
supported.

## How do I quit the game?

Hold the hotkey (MENU, or SELECT) and press START. The launcher then
restores the CPU and GPU clocks and returns to EmulationStation.

## Where are the settings, the saves and the log?

In the game's folder, `/userdata/roms/ports/halo/`: the settings in
`config.toml`, the saved games and the shader cache in `save/`, and the log
of the last launch in `log.txt`.

## How do I change the resolution?

Set `display.render_scale` in `config.toml`, from 0.5 to 1.0. The default,
0.75, draws the 3D picture at 480x360 and scales it to the 640x480 screen.
1.0 is sharper and slower (about 24 to 40 fps in the campaign).

## Why is the first launch slow?

The first launch copies the `maps/` folder out of your disc image, which
takes a few minutes. After that, each new combination of shaders is compiled
once, beside the game (what it draws appears a moment late), and kept in
`save/shaders` for later launches.

## Do I need to keep the disc image?

No. Once `maps/` has been extracted, the image can be deleted. You can also
skip the image and copy an extracted Xbox `maps/` folder into `halo/`.
