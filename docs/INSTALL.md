# Installing Halo CE on an H700 handheld

This guide takes a player from a release of the port to a game running
from the Ports menu, on Knulli or with PortMaster: what hardware and game data you need, where each
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
- [Installing with PortMaster](#installing-with-portmaster)
- [The first launch](#the-first-launch)
- [Playing](#playing)
- [Controls](#controls)
- [The spawn menu](#the-spawn-menu)
- [Bot Match](#bot-match)
- [Where things are](#where-things-are)
- [What the launcher changes on the handheld](#what-the-launcher-changes-on-the-handheld)
- [Playing online](#playing-online)
- [Changing settings](#changing-settings)
- [Updating](#updating)
- [Uninstalling](#uninstalling)
- [Troubleshooting](#troubleshooting)

## What you need

- An Allwinner H700 handheld running Knulli (tested with the release
  Gladiator II), or another firmware with PortMaster
  ([Supported handhelds](#supported-handhelds)).
- A disc image of the original Xbox version of Halo: Combat Evolved, made
  from your own disc, or the `maps/` folder of the Xbox game.
- About 3 GB free: the extracted `maps/` folder (1.8 GB) and the cache the
  game sets up at its first start (0.8 GB), plus room for the disc image
  until the maps are copied.
- From the [latest release](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/releases/latest), one of:
  - `halo-ce-knulli-<version>.zip`, for Knulli: a plain port, no PortMaster
    needed;
  - `halo-ce-portmaster-<version>.zip`, for PortMaster, on Knulli or another
    firmware ([Installing with PortMaster](#installing-with-portmaster)).

  Or the files a build produces in `dist/` ([Building](BUILDING.md)).

Both use the firmware's own SDL2 and graphics driver, and none of
PortMaster's runtimes. They hold the same game files and use the same
`ports/halo/` folder, so maps, saves and settings carry over from one to the
other.

## Supported handhelds

The port is made for the Allwinner H700 (4x Cortex-A53 at 1.5 GHz, Mali-G31 MP2,
1 GB RAM).

| Device | Screen | Status |
| --- | --- | --- |
| Anbernic RG35XX H | 640x480 | Tested |
| Anbernic RG35XX Plus | 640x480 | Untested, expected to work |
| Anbernic RG35XX SP | 640x480 | Untested, expected to work |
| Anbernic RG35XX 2024 | 640x480 | Untested, expected to work |
| Anbernic RG40XX H | 640x480 | Untested, expected to work |
| Anbernic RG40XX V | 640x480 | Untested, expected to work |
| Anbernic RG CubeXX | 720x720 | Tested on Knulli Scarab, online co-op included. The 4:3 picture has bars above and below, black from v2026.10.26 |
| Anbernic RG34XX | 720x480 | Untested |
| Anbernic RG353M (RK3566, Mali-G52) | 640x480 | Works on ROCKNIX, reported by a user ([#5](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/issues/5)) |
| R36S (RK3326, Mali-G31) | 640x480 | Works on dArkOSen from v2026.10.07, reported by a user ([#4](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/issues/4)) |

| Firmware | How | Status |
| --- | --- | --- |
| Knulli (Gladiator II) | the Knulli zip | Tested |
| Knulli (Gladiator II) | the PortMaster zip | Tested |
| Knulli (Scarab) | the Knulli zip | Tested (RG CubeXX) |
| muOS (2601 Jacaranda) | the PortMaster zip | Tested from v2026.10.07 (RG35XX H, offline and online). Earlier releases crashed there in menus, at respawn and in the pause menu |
| ArkOS and others with PortMaster | the PortMaster zip | Untested. The host needs glibc 2.30 or newer and SDL2 2.0.18 or newer |
| ROCKNIX | the PortMaster zip | Reported working by a user on an RG353M with Arm's Mali driver, with the default settings once ROCKNIX was up to date. On their older ROCKNIX (20260601) parts of the picture stopped updating unless `debug.instance_models = false` ([#5](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/issues/5)). ROCKNIX can also use the open Panfrost driver, which the port has not been tried with |
| dArkOSen | the PortMaster zip | Reported working by a user on an R36S from v2026.10.07; earlier releases crashed there ([#4](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/issues/4)) |

The port is made and tested for the H700's Mali-G31. Handhelds with other
Mali GPUs and Arm's driver can work too: users report the Rockchip ones
above working. TrimUI handhelds and other devices without a Mali GPU are not
supported. If you try another device or firmware, please
open an issue with the result and your `halo/log.txt`.

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

This section is for the Knulli zip; for the PortMaster zip, see
[Installing with PortMaster](#installing-with-portmaster). The Knulli zip
holds the files in the layout the launcher expects, as Knulli sees the card:

```
/userdata/roms/ports/
├── Halo.sh              the launcher, listed in Ports
└── halo/
    ├── halo             the host program
    ├── halo_guest.elf   the game
    ├── halo_extract.py  copies maps/ out of the disc image
    ├── halo_screen.py   the launcher's messages and progress on the screen
    ├── sdl_mapping.py   the handheld's controls for SDL
    ├── config.default.toml  the settings the first launch writes
    ├── brokers.txt      internet play's signalling brokers
    ├── LICENSE.txt, THIRD-PARTY-NOTICES.txt
    ├── PUT YOUR HALO DISC IMAGE HERE.txt
    └── Halo.iso         your disc image (any name ending in .iso or .xiso)
```

1. Unzip `halo-ce-knulli-<version>.zip` onto the card, into the partition
   that holds the `roms` folder (over the network, `\\KNULLI\share`).
2. Copy your disc image into `roms/ports/halo/`, or an extracted `maps/`
   folder to `roms/ports/halo/maps/`.

From a build instead, copy `halo`, `halo_guest.elf`, `halo_extract.py`,
`halo_screen.py`, `sdl_mapping.py` and `config.default.toml` from `dist/` into
`/userdata/roms/ports/halo/` (make the folder), and `Halo.sh` into
`/userdata/roms/ports/`.

Use whichever way you copy ROMs to the handheld: the card in a computer,
Knulli's network share, or SCP/SFTP. Where the file system keeps Unix
permissions, `Halo.sh` and `halo` must be executable (`chmod 755`); the zip
and `build.sh` set that.

## Installing with PortMaster

The PortMaster zip is laid out as PortMaster's own ports are: `Halo.sh`
(PortMaster's kind of launcher) and the `halo/` folder, with the same game
files as the Knulli zip, the gptokeyb2 settings `halo.ini`, `port.json`,
`gameinfo.xml` and a screenshot. It is not in PortMaster's catalogue; install
it from the file:

1. Put `halo-ce-portmaster-<version>.zip` in PortMaster's `autoinstall`
   folder and start PortMaster, which installs it. (Over SSH,
   `harbourmaster install <zip>` from PortMaster's folder does the same.)
2. Copy your disc image into the `ports/halo/` folder PortMaster made, or an
   extracted `maps/` folder to `ports/halo/maps/`.
3. Start Halo from Ports.

The launcher does what the Knulli one does at the first launch (the maps,
the settings, the notice) and raises the clocks while the game runs; the
controls come from PortMaster's controller database, and gptokeyb2 quits
the game with the hotkey and START. Installing it over the Knulli zip, or
the other way round, keeps `maps/`, `save/` and `config.toml`.

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
4. writes `halo/config.toml` with the settings for this handheld (below),
   copied from `config.default.toml`;
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
| SELECT | back | with D-pad Down, the [spawn menu](#the-spawn-menu) |

On a handheld with one analog stick or none (the RG40XX V; the RG35XX
Plus, SP and 2024, the RG34XX), the D-pad takes the missing stick's place
in a game, from v2026.10.21 (`input.dpad_controls`; not yet tried on those
handhelds, [#9](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/issues/9)):

| Handheld | D-pad | With L1 held |
| --- | --- | --- |
| One stick (it moves) | aims | aims; A crouches, B zooms |
| No sticks | Up and Down walk, Left and Right turn | Up and Down look, Left and Right strafe; A crouches, B zooms |

L1 pressed and let go by itself is still the flashlight. In the menus and
the spawn menu, and with SELECT held, the D-pad is the D-pad. The view
turns as fast as the game's look sensitivity setting says.
`input.dpad_controls = "off"` keeps the game's own D-pad (walking and
strafing); `"on"` uses the controls without sticks on any controller.

**To quit, hold the hotkey (MENU), or SELECT, and press START.** The game
exits, and the launcher restores the clocks and returns to EmulationStation.

If you have remapped the handheld's controls in EmulationStation, the game
follows that mapping. It reads Batocera's and Knulli's names for the
controls and those of ArkOS and EmuELEC. A firmware whose EmulationStation
names the face buttons by the Xbox layout gets A and B (and X and Y) swapped:
an empty file named `no_face_swap` in `roms/ports/halo/` takes them as they
are named. The PortMaster zip uses PortMaster's own mapping instead.

## The spawn menu

Hold SELECT and press D-pad Down during a game to open the spawn menu, after
[AlphaRing](https://github.com/kirklandsig/AlphaRing)'s for The Master Chief Collection on PC: the vehicles, weapons, equipment and
AI characters the loaded level has, placed in front of you. It works in
single player and for the host of a co-op or multiplayer game. A player who
joined someone else's game can't open it, so the host decides. What the host
spawns reaches the other players as the host's other objects do; nothing new
is sent over the network.

| Handheld (RG35XX H) | In the menu |
| --- | --- |
| L1, R1 | the previous or next page: vehicles, weapons, equipment, characters, cheats |
| D-pad Up and Down, or the left stick's up and down | pick |
| Left stick left and right | step to the side, the menu open |
| Right stick | turn, the menu open |
| D-pad Left and Right | a character's side: their own, an ally of yours, or an enemy |
| B (bottom) | spawn; on the cheats page, the cheat on or off |
| A (right) | close |
| START | close and pause |

The menu names the buttons as the Xbox controller does (A spawns, B
closes), as the game's own prompts do. While it is open you can step left
and right and turn, so that what you spawn lands in a new place each time
rather than on top of the last; you don't walk forward or back, fire or
jump until it closes.
Each level lists only what it has loaded, so the lists change from level to
level. An ally fights on your side, an enemy against you; "their own" keeps
the character's own side (an Elite stays Covenant).

The last page, CHEATS, turns the game's own cheats on and off: god mode,
infinite ammo, bottomless clip, one-hit kills, super jump, jetpack, Medusa
(enemies that see you die) and bump possession (walk into a character to
take it over). Active camouflage makes you invisible for a while.
Teleport everyone to me, the last row, brings every other player with a
character beside you, facing you, the bots too: in a big online game,
whoever is stuck comes to you. The players who joined follow on their own
machines, as they follow any teleport of the host's, so it works with
everyone in your game, whatever build they play. The cheats are the game's, so every player in the
game has them, the players who joined and the bots too: with god mode on in
a Bot Match no one dies, and with one-hit kills the bots kill in one hit as
well. God mode (just us), the row under God mode, keeps alive only the
players on this handheld or PC: not the bots, nor the players who joined.
A cheat stays on, from level to level, until you turn it off or quit
Halo. Only the host has the page, as the rest of the menu; a player who
joined someone else's game plays by that host's rules.

## Bot Match

MULTIPLAYER > BOT MATCH, under Create Game, plays Slayer against computer
players on the handheld alone, with no network:

1. Set the bots. BOTS: how many, 1 to 15. SKILL: Easy, Normal, Heroic or
   Legendary (how soon they react, how well they aim, how far they see).
   TEAMS, for Team Slayer: EVEN puts bots on both teams, evening them with
   you; VS YOU puts them all on the other team. DEFAULTS sets 7, Normal,
   Even. OK goes on.
2. Choose the map, then the game type. The bots play Slayer and Team
   Slayer, the built-in ones and those you made in Edit Gametypes; in
   another kind of game (Capture the Flag, Oddball, King of the Hill, Race)
   no bot joins.
3. In the lobby (Enlisted Players), press A to join; the bots join with
   you. A starts the match, X holds the countdown, B goes back.

After the match, the carnage report's A picks the next map and game type,
and B there goes back to the main menu. The settings are kept in
`config.toml` (`bots.count`, `bots.skill`, `bots.teams`:
[Configuration](CONFIGURATION.md#bots)). You are the game's host, so the
[spawn menu](#the-spawn-menu) works in a Bot Match too. Bots play only in
Bot Match: not in internet or LAN games.

## Where things are

Everything the game writes is in `/userdata/roms/ports/halo/`:

| Path | Contents |
| --- | --- |
| `log.txt` | The log of the last launch: the launcher's messages, the extraction's progress, the host's and the platform layer's messages, and at the end `exit status N`. Overwritten at each launch. |
| `debug.txt` | The game's own log. |
| `config.toml` | The settings ([Configuration](CONFIGURATION.md)). |
| `init.txt` | Optional: console commands run at start-up, one per line. For example `map_name levels\b30\b30` starts The Silent Cartographer directly. |
| `maps/` | The extracted game data. |
| `custom_maps/` | Halo Custom Edition maps and their resource maps, yours and those downloaded in online games ([Custom Edition maps](#custom-edition-maps)). |
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

## Playing online

The port has upstream's online play
([OpenCE](https://github.com/OpenCommunityEdition/OpenCE)): the campaign in
co-op and multiplayer games, over the internet or the local network (system
link), with the PC version's menus. The handheld needs a network
connection (Wi-Fi) for it.

- **Join a game:** Multiplayer, Join Game, Server Browser. It lists the
  public games of everyone playing a build of the same upstream commit,
  upstream's own or this port.
- **Host a game:** Multiplayer, Create Game, Internet (or System Link for
  the local network). A SINGLEPLAYER map is online co-op through the
  campaign; a multiplayer map is a multiplayer game. Server Setup sets whether the game is PUBLIC (in the
  server browser) or PRIVATE, friendly fire, and co-op's extra enemies. A
  co-op game starts PRIVATE; set it to PUBLIC so that others can find it.
- **Invite links:** a host's game also has an invite link, which the log
  (`halo/log.txt`) shows. On a PC, opening the link joins; the handheld has
  no way to paste one, so join through the server browser instead.

Internet play is on (`network.online = true`). It connects to anything only
while you host, join or have the server browser open: through public MQTT
brokers (`halo/brokers.txt`) the machines of a game find each other, and
public STUN servers tell each its internet address. To turn it off, set
`online = false` in `[network]` of `config.toml`, or use the Settings menu.
Updating from a version before co-op turns it on once; after that your
choice is kept.

Every machine in a game has to run the same network version, and the
server browser lists only games of its own. From v2026.10.30 the handheld
plays version 41, upstream's from 2026-10-29, with the PC and Android
builds of that day on, and with people playing OpenCE's web build in
a browser: a browser connects over WebRTC, which the handheld answers (DTLS,
with Mbed TLS). When upstream raises the version again, the handheld needs a
new release to see their games. Version 38 brings OpenCE's gametypes
(SWAT, Infection, Gun Game and Hardcore: Select Gametype's OPENCE bank,
and OPENCE OPTIONS in Edit Gametypes), co-op's ALLY FRIENDLY FIRE, and
the host's SHIELD EFFECTS in Player Options; upstream's text chat is left
out (its messages are dropped). Version 39 is upstream's dedicated servers
(a host with no player of its own, `halo --server` on a PC): the handheld
joins one as any other game, and the server browser shows those the
internet brokers list among the others. Those listed only on upstream's
master server don't show; an invite link to one still joins it. Version
40 adds a Discord account's pass to the join request, which upstream's
dedicated servers ask of a player on a VPN: the handheld links no Discord
account, so a server that requires one refuses it, shown as the game being
closed. Version 41 is the players' votes: while a vote runs (to kick a
player, or to end the game), its box shows at the left of the screen, and
the D-pad's left votes yes, its right no (only while the vote runs; on a
handheld without a right stick, whose D-pad walks and turns, the D-pad
doesn't vote). The handheld votes in the votes others start: starting one
takes a PC's mouse on the scoreboard, or `!endgame` in a PC's chat. A game
on a Custom Edition map can be
joined: the map comes from the game's players
([Custom Edition maps](#custom-edition-maps)).

Upstream's voice chat needs a microphone to talk, which the handhelds don't
have: the handheld doesn't send any voice.

## Custom Edition maps

Halo Custom Edition's maps run beside the Xbox game's, with upstream's
loader: the community's multiplayer maps, and campaign maps. They need
Custom Edition's three resource maps.

1. Copy `bitmaps.map`, `sounds.map` and `loc.map` into
   `roms/ports/halo/custom_maps/` (the game makes the folder at its first
   start). They are in the `maps` folder of a Halo Custom Edition install on
   a PC, or in `halo1/maps/custom_edition/` of Halo: The Master Chief
   Collection on PC (not the files directly in `halo1/maps`).
2. Copy the maps (`.map`) into the same folder.

The maps are listed as CUSTOM MULTIPLAYER and CUSTOM SINGLEPLAYER. Maps that
need OpenSauce don't run. Online, joining a game on a Custom Edition map you
don't have downloads it from the game's players into `custom_maps`, with the
resource maps it needs (`network.map_download = false` turns that off), and a
game you host on one is shared with the players who lack it
(`network.map_share`). Download only from players you trust. A map larger
than the stock ones takes more memory, which the handheld shares with
EmulationStation.

## Changing settings

The settings are in `halo/config.toml`. Edit it on the card or over the
network while the game is not running; the game reads it at start-up. The
launcher writes these values the first time (from `config.default.toml`):

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
online = true
```

The one most worth changing is `display.render_scale`: the 3D picture's
resolution as a fraction of the screen's, from 0.5 to 1.0. 1.0 is sharper
and slower; lower values are faster. Every setting, with its default and
effect, is in [Configuration](CONFIGURATION.md). To go back to the
launcher's values, delete `config.toml`; the next launch writes it again.

## Updating

From v2026.10.15 on, the launcher looks for a new release when you start
Halo, once in six hours at most (a few seconds, and not at all without a
network). When there is one, it says so on the screen before the game
starts:

- START downloads the new release's zip for your install (the Knulli zip,
  or the PortMaster zip with PortMaster) from this repository's releases,
  checks it against the SHA-256 GitHub gives for it, puts its files in
  place of the old ones and starts the new version. `maps/`, `save/`,
  `custom_maps/` and `config.toml` are kept. A download that fails or
  doesn't match changes nothing.
- Any other button, or 30 seconds, plays the version you have. That
  release isn't offered again; the next one is.

SETTINGS > NETWORK SETUP > CHECK FOR UPDATES turns the check off
(`update.auto`). An install built from source has no `VERSION` file and is
never offered one.

By hand, and from a version before v2026.10.15:

1. Unzip the new release's zip ([latest release](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/releases/latest)) over the old
   files, replacing them; or, from a build, copy the files as in
   [Copying the files](#copying-the-files). With PortMaster, install the new
   PortMaster zip the same way as the first.
2. Keep `maps/`, `save/` and `config.toml`: the zip has none of them.

**From a version before v2026.10.06.1 (co-op):** a checkpoint saved in the
middle of a level by the older version does not load (upstream's larger
game state changed the saved game's layout): the game refuses it, and the
level starts from its beginning. Profiles and the levels you reached are
kept. The shader programs are compiled again once (objects appear late the
first time in each place), and internet play is turned on once.

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
| The buttons are wrong | `sdl_mapping.py` builds the mapping from EmulationStation's controller configuration. Check that the handheld's controls are configured in EmulationStation. With PortMaster, the mapping is PortMaster's for the handheld. |
| Parts of the picture do not change, or the menu or another game shows through walls and floors | Another program is drawing to the screen while the game runs, often a frontend that did not stop cleanly. The frame rate drops too, and the render scale steps down at once. Restart the handheld (power it off and on) and start Halo again. |
| `GL error 0x0505 by frame N: out of memory` | The graphics driver could not make a texture or a target, which is drawn empty. Close other programs, restart the handheld, and set `high_res_text = false` and `high_res_hud = false`. |
| `memory low at frame N: M MB available` | The handheld is close to running out of memory; the game may be stopped (exit status 137). As above. |
| Other `GL error 0x... by frame N` lines | A graphics call failed. Please open an issue with `log.txt`. |
| Objects appear late the first time in a place | Each new shader combination is compiled once, beside the game, and cached in `save/shaders/`. |
| The frame rate drops after a while | At 70 °C the kernel lowers the clocks. Lower `display.render_scale` for more headroom. |
| `Internet play: no signalling brokers (network.brokers_file), so invites cannot work` | `halo/brokers.txt` is missing: copy it from the release again. |
| The server browser is empty | Check the handheld's Wi-Fi, and that `online = true` in `[network]` of `config.toml`. Only PUBLIC games are listed. |
| An old checkpoint does not load after updating | Checkpoints from before v2026.10.06.1 cannot load in it; start the level again. |
| The clocks stay high after a crash | The launcher restores them on exit; if it could not, the next start or a reboot does. |
| Something else | Open an issue with `halo/log.txt`, the device, the Knulli release, and whether you installed from a disc image or a `maps/` folder. |
