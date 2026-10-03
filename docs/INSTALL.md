# Installing Halo CE on an H700 handheld

This guide takes a player from a release of the port to a game running
from Knulli's Ports menu: what hardware and game data you need, where each
file goes on the handheld, what happens at the first launch, how the
controls are mapped and how to quit, where the logs, saves and settings
live, and how to update or remove the game. It also describes exactly what
the launcher changes on the handheld while the game runs (the CPU and GPU
clocks and the battery saver) and how it puts them back. The
[releases](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/releases/latest) hold the port and no game files (it can also be built
from source: [Building](BUILDING.md)); bring your own copy of the original
Xbox game.

## Contents

- [What you need](#what-you-need)
- [Supported handhelds](#supported-handhelds)
- [The game data](#the-game-data)
- [Copying the files](#copying-the-files)
- [The first launch](#the-first-launch)
- [Playing](#playing)
- [Controls](#controls)
- [Where things are](#where-things-are)
- [What the launcher changes on the handheld](#what-the-launcher-changes-on-the-handheld)
- [Changing settings](#changing-settings)
- [Updating](#updating)
- [Uninstalling](#uninstalling)
- [Troubleshooting](#troubleshooting)

## What you need

- An Allwinner H700 handheld running Knulli. The port was tested with the
  release Gladiator II.
- A disc image of the original Xbox version of Halo: Combat Evolved, made
  from your own disc, or the `maps/` folder of the Xbox game.
- About 3 GB free: the extracted `maps/` folder (1.8 GB) and the cache the
  game sets up at its first start (0.8 GB), plus room for the disc image
  until the maps are copied.
- `halo-ce-knulli-<version>.zip` from the [latest release](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/releases/latest), or
  the six files a build produces in `dist/` ([Building](BUILDING.md)).

PortMaster is not needed. The port uses the firmware's own SDL2 and Mali
driver.

## Supported handhelds

The port needs an Allwinner H700 (4x Cortex-A53 at 1.5 GHz, Mali-G31 MP2,
1 GB RAM) running Knulli.

| Device | Screen | Status |
| --- | --- | --- |
| Anbernic RG35XX H | 640x480 | Tested |
| Anbernic RG35XX Plus | 640x480 | Untested, expected to work |
| Anbernic RG35XX SP | 640x480 | Untested, expected to work |
| Anbernic RG35XX 2024 | 640x480 | Untested, expected to work |
| Anbernic RG40XX H | 640x480 | Untested, expected to work |
| Anbernic RG40XX V | 640x480 | Untested, expected to work |
| Anbernic RG CubeXX | 720x720 | Untested |
| Anbernic RG34XX | 720x480 | Untested |

TrimUI handhelds and devices with other SoCs are not supported: they have
different GPUs and drivers. Other firmware for the H700 (muOS, ROCKNIX) is
untested. If you try another device or firmware, please open an issue with
the result and your `halo/log.txt`.

## The game data

The port plays the Xbox version's maps. The decompilation is of the Xbox
build, and the PC version's map files (Gearbox's port) do not load.

You can give the launcher either of these:

- **A disc image** in `halo/` (or in `ports/` beside it), named `*.iso` or
  `*.xiso`, in either case. The extractor (`halo_extract.py`) reads the Xbox
  file system (XDVDFS) itself and accepts both an XISO, which holds only the
  game partition, and a full disc image, in which it finds the game
  partition at the offsets used by the first Xbox discs and by later ones.
  It only reads the image, and before it copies anything it checks that the
  image has Halo's maps folder (with `ui.map` and `a10.map`), that no map
  reaches past the end of the image, and that the card has room.
- **An extracted `maps/` folder** from the Xbox game, copied to `halo/maps/`.
  The launcher then extracts nothing.

The image must come from a disc you own. The repository contains no game
data and nothing is downloaded; see the [legal notice](LEGAL.md).

## Copying the files

The release's zip holds the files in the layout the launcher expects, as
Knulli sees the card:

```
/userdata/roms/ports/
├── Halo.sh              the launcher, listed in Ports
└── halo/
    ├── halo             the host program
    ├── halo_guest.elf   the game
    ├── halo_extract.py  copies maps/ out of the disc image
    ├── halo_screen.py   the launcher's messages and progress on the screen
    ├── sdl_mapping.py   the handheld's controls for SDL
    ├── LICENSE.txt, THIRD-PARTY-NOTICES.txt
    ├── PUT YOUR HALO DISC IMAGE HERE.txt
    └── Halo.iso         your disc image (any name ending in .iso or .xiso)
```

1. Unzip `halo-ce-knulli-<version>.zip` onto the card, into the partition
   that holds the `roms` folder (over the network, `\\KNULLI\share`).
2. Copy your disc image into `roms/ports/halo/`, or an extracted `maps/`
   folder to `roms/ports/halo/maps/`.

From a build instead, copy `halo`, `halo_guest.elf`, `halo_extract.py`,
`halo_screen.py` and `sdl_mapping.py` from `dist/` into
`/userdata/roms/ports/halo/` (make the folder), and `Halo.sh` into
`/userdata/roms/ports/`.

Use whichever way you copy ROMs to the handheld: the card in a computer,
Knulli's network share, or SCP/SFTP. Where the file system keeps Unix
permissions, `Halo.sh` and `halo` must be executable (`chmod 755`); the zip
and `build.sh` set that.

## The first launch

Open Ports in EmulationStation and start Halo. If it is not listed, update
the game lists from EmulationStation's menu. The first launch:

1. writes a new `halo/log.txt` (every launch does);
2. raises the clocks and holds off the battery saver
   ([below](#what-the-launcher-changes-on-the-handheld));
3. finds no `maps/ui.map`, takes the first disc image in `halo/` (else in
   `ports/`), and copies its `maps/` folder out with `halo_extract.py`, with
   its progress on the screen: about four minutes. What is wrong, if
   anything (not an Xbox disc image, an incomplete one, not enough free
   space), the screen says instead, until a button is pressed. Each file is
   written under a temporary name and renamed when it is complete, and
   `ui.map` comes last, so a copy that was stopped goes on at the next
   launch, keeping the maps it finished (from the same image only: one of the
   same size, layout and mastering date). The log
   has a line `copying the maps folder out of <image>`, then one line per
   file with the percentage done before it and the file's name, and a last
   line `100% <count> files`;
4. writes `halo/config.toml` with the settings for this handheld (below);
5. sets up the controls;
6. says on the screen that the game's own first start takes about a minute
   more (until a button is pressed, ten seconds at most);
7. starts the game. Before its first frame the game sets up its cache in
   `save/z` (0.8 GB), for about a minute with the screen black, and it
   completes `config.toml` with every other setting at its default, with a
   comment for each.

After the first launch the disc image can be deleted.

The first time each combination of shaders is drawn, the driver compiles it
on a thread of its own, and what it draws appears a moment late. The
compiled programs are kept in `halo/save/shaders/`, so later launches load
them instead.

## Playing

The menus run at 60 fps. In the campaign, the frame rate depends on the
scene and on the render scale; the current figures are in the
[README's performance table](../README.md#performance). The 3D picture is
drawn at 0.75 of the screen's resolution (480x360 on a 640x480 screen) and
scaled up; [Changing settings](#changing-settings) explains how to trade
sharpness for speed.

The first time you enter a level, and again once two other levels have
been loaded since, its loading screen lasts 20 to 30 s: the Xbox maps are
compressed, and the game decompresses the level into its cache in
`save/z`, as the Xbox did to its hard disk. Other loads are quicker.

Bink video is not available in the upstream port, so the game skips its
movies.

## Controls

The launcher builds the SDL mapping from EmulationStation's controller
configuration, so the buttons follow the positions of an Xbox controller.
The face buttons below are named as on the RG35XX H (A on the right, B at
the bottom); what matters is the position.

| Handheld | Xbox controller | In the game |
| --- | --- | --- |
| Left stick, right stick | left stick, right stick | move, look |
| R2 | right trigger | fire |
| L2 | left trigger | throw a grenade |
| B (bottom) | A | jump, accept |
| A (right) | B | melee, back |
| Y (left) | X | action, reload |
| X (top) | Y | change the weapon |
| L1 | white | flashlight |
| R1 | black | change the grenade |
| L3, R3 (stick clicks, where the handheld has them) | left and right stick clicks | crouch, zoom |
| D-pad | D-pad | |
| START | start | pause menu |
| SELECT | back | |

**To quit, hold the hotkey (MENU), or SELECT, and press START.** The game
exits, and the launcher restores the clocks and returns to EmulationStation.

If you have remapped the handheld's controls in EmulationStation, the game
follows that mapping.

## Where things are

Everything the game writes is in `/userdata/roms/ports/halo/`:

| Path | Contents |
| --- | --- |
| `log.txt` | The log of the last launch: the launcher's messages, the extraction's progress, the host's and the platform layer's messages, and at the end `exit status N`. Overwritten at each launch. |
| `debug.txt` | The game's own log. |
| `config.toml` | The settings ([Configuration](CONFIGURATION.md)). |
| `init.txt` | Optional: console commands run at start-up, one per line. For example `map_name levels\b30\b30` starts The Silent Cartographer directly. |
| `maps/` | The extracted game data. |
| `save/` | The game's writable Xbox drives, each a folder named after its drive letter, with the saved games and player profiles. |
| `save/shaders/` | The compiled shader programs. Safe to delete; they are compiled again. |
| `profile.txt` | Only when profiling ([Profiling](PROFILING.md)). |

To back up your progress, copy `save/`.

## What the launcher changes on the handheld

`Halo.sh` changes three things for the length of the game and puts them
back when it exits.

| What | While the game runs | Why |
| --- | --- | --- |
| The CPU governor (`/sys/devices/system/cpu/cpufreq/policy0/scaling_governor`) | `performance`: the cores stay at their top frequency (1512 MHz) | The game's thread and the GL thread run at full speed. Together with the GPU's clock, this took the first working build from 15 to 18 fps (a30, 640x480). |
| The GPU's minimum frequency (`/sys/class/devfreq/gpu/min_freq`) | The highest of `available_frequencies` (648 MHz) | Under load, the GPU's governor otherwise kept it at 420 MHz. |
| Knulli's battery saver | Paused: the launcher creates `/var/run/battery-saver/halo.pause` | The battery saver must not dim or suspend the handheld during the game. |

Before it changes them, the launcher keeps the current governor and
`min_freq` in `/var/run/halo-clocks`. A shell `EXIT` trap writes both back
and removes the pause file when the script ends, whether the game quit
normally or crashed. When the system stops the launcher, it passes the
signal on to the game, and kills a game that has not quit ten seconds later.
If the launcher itself is killed outright, before it can run its trap, the
next start puts the clocks back first; a reboot restores the firmware's
defaults too. One start runs at a time: another while the game runs is
refused, and the log notes it.

The kernel's thermal governor still applies: at 70 °C it lowers the CPU from
1512 to 1416 MHz and the GPU from 648 to 600 MHz, which costs a few frames a
second. Long sessions reach that temperature.

The launcher also exports `SDL_GAMECONTROLLERCONFIG` (the controls) for the
game only; nothing is written to the firmware's configuration.

## Changing settings

The settings are in `halo/config.toml`. Edit it on the card or over the
network while the game is not running; the game reads it at start-up. The
launcher writes these values the first time:

```toml
[display]
screen_width = 0
render_scale = 0.75
interpolation = true
fast_shaders = true
fast_textures = true
high_res_hud = false
high_res_text = false

[update]
auto = false

[network]
online = false
```

The one most worth changing is `display.render_scale`: the 3D picture's
resolution as a fraction of the screen's, from 0.5 to 1.0. 1.0 is sharper
and slower; lower values are faster. Every setting, with its default and
effect, is in [Configuration](CONFIGURATION.md). To go back to the
launcher's values, delete `config.toml`; the next launch writes it again.

## Updating

1. Unzip the new release's zip ([latest release](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/releases/latest)) over the old
   files, replacing them; or, from a build, copy the six files as in
   [Copying the files](#copying-the-files).
2. Keep `maps/`, `save/` and `config.toml`: the zip has none of them.

Your `config.toml` is kept as it is. Settings that are new in a version are
added to it at their defaults at the first start, and the log says so
(`settings: added <name> (new in this version) at its default`). The
launcher's own values are written only when the file is missing, so a
change to them in a new version reaches you only if you delete the file.
`display.high_res_hud` and `display.high_res_text` are added off. v2026.10.02
added `high_res_text` on, which takes about 260 MB at the menus and with
EmulationStation in memory could run the handheld out of memory (exit
status 137): if your `config.toml` has
`high_res_text = true`, set it to `false`
([Configuration](CONFIGURATION.md#displayhigh_res_text)).

Compiled shader programs are stored under a hash of their source and of the
driver's version. Programs that a new version changes are compiled again
once; the old files stay unused in `save/shaders/`, which you can delete.

## Uninstalling

1. Copy `halo/save/` somewhere if you want to keep your progress.
2. Delete `/userdata/roms/ports/Halo.sh` and the folder
   `/userdata/roms/ports/halo/`.
3. Update the game lists in EmulationStation.

The launcher's changes to the clocks and the battery saver last only while
the game runs, so there is nothing else to undo.

## Troubleshooting

Read `halo/log.txt` first; most problems name themselves there.

| Symptom or log message | Cause and fix |
| --- | --- |
| Halo returns to the menu at once | The screen said why first (the messages below); the log has the same message. |
| `no maps folder and no disc image (.iso) in ...` | `halo/` has no `maps/ui.map`, and neither `halo/` nor `ports/` has a file ending in `.iso` or `.xiso`. Copy the image or the `maps/` folder there. |
| `This file is not an Xbox disc image` | For example the PC version's image, or a damaged file. |
| `This Xbox disc image has no maps folder`, or `its maps folder is incomplete` | The image is of another Xbox game, or not a whole copy of Halo. |
| `The disc image is incomplete`, or `damaged` | Copy the image to the card again. |
| `There is not enough free space on the SD card` | The message says how much Halo needs (the maps and the game's cache) and how much is free. |
| `The Halo game data was not found: .../maps/ui.map is missing` | The host found no maps. If you copied a `maps/` folder by hand, check that `ui.map` is in `halo/maps/`. |
| The screen stays black for about a minute at the first start | The game is setting up its cache in `save/z`, as the screen said before. Later starts take seconds. |
| `Halo is running already; this start was refused` | Halo was started while it ran. |
| `Halo was stopped` (exit status 137) | The system killed the game, most likely when memory ran out. In `config.toml`, set `high_res_text = false` and `high_res_hud = false`, and unset `HALO_HIGH_RES_TEXT` and `HALO_HIGH_RES_HUD` if you set them (they override the file). |
| `Halo stopped unexpectedly (exit status N)` | The game crashed or could not start; the details are in `log.txt`. |
| The maps do not load | The PC version's files do not work. Use the Xbox version. |
| The buttons are wrong | `sdl_mapping.py` builds the mapping from EmulationStation's controller configuration. Check that the handheld's controls are configured in EmulationStation. |
| Objects appear late the first time in a place | Each new shader combination is compiled once, beside the game, and cached in `save/shaders/`. |
| The frame rate drops after a while | At 70 °C the kernel lowers the clocks. Lower `display.render_scale` for more headroom. |
| The clocks stay high after a crash | The launcher restores them on exit; if it could not, the next start or a reboot does. |
| Something else | Open an issue with `halo/log.txt`, the device, the Knulli release, and whether you installed from a disc image or a `maps/` folder. |
