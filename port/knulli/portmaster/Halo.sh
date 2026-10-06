#!/bin/bash

XDG_DATA_HOME=${XDG_DATA_HOME:-$HOME/.local/share}

if [ -d "/opt/system/Tools/PortMaster/" ]; then
  controlfolder="/opt/system/Tools/PortMaster"
elif [ -d "/opt/tools/PortMaster/" ]; then
  controlfolder="/opt/tools/PortMaster"
elif [ -d "$XDG_DATA_HOME/PortMaster/" ]; then
  controlfolder="$XDG_DATA_HOME/PortMaster"
else
  controlfolder="/roms/ports/PortMaster"
fi

source $controlfolder/control.txt
[ -f "${controlfolder}/mod_${CFW_NAME}.txt" ] && source "${controlfolder}/mod_${CFW_NAME}.txt"
get_controls

GAMEDIR=/$directory/ports/halo
BINARY=halo

cd $GAMEDIR

> "$GAMEDIR/log.txt" && exec > >(tee "$GAMEDIR/log.txt") 2>&1

# The game's maps are copied out of the player's Xbox disc image on the first
# start (a few minutes, with the progress on the screen).
if [ ! -s maps/ui.map ]; then
  shopt -s nullglob
  discs=({.,..}/*.{iso,ISO,xiso,XISO})
  shopt -u nullglob
  if [ "${#discs[@]}" -eq 0 ]; then
    python3 halo_screen.py wait 60 "Halo needs your disc" "Copy the disc image of Halo: Combat Evolved for the Xbox (an .iso file) into ports/halo, then start Halo again.

Press a button to go back."
    exit 1
  fi
  python3 halo_extract.py --screen "${discs[0]}" "$GAMEDIR" || exit 1
fi

# Settings for these handhelds on the first start (config.default.toml); the
# game adds the rest. Written whole, or no start: the game would write its own
# defaults for good.
if [ ! -f config.toml ]; then
  if [ -e config.toml ] || [ -L config.toml ] ||
    ! rm -rf config.toml.new || ! cp config.default.toml config.toml.new ||
    ! mv -f config.toml.new config.toml; then
    rm -f config.toml.new
    python3 halo_screen.py wait 60 "Halo could not start" "Halo could not write its settings, ports/halo/config.toml: the SD card may be full, or a folder has that name.

Press a button to go back."
    exit 1
  fi
fi

# Internet play on, once, for an install whose settings had it off before
# co-op and the server browser; the player's choice after that stays.
if [ ! -e save/online-on ]; then
  sed -i '/^[[:space:]]*\[network\]/,/^[[:space:]]*\[/ s/^\([[:space:]]*online[[:space:]]*=[[:space:]]*\)false/\1true/' config.toml &&
    mkdir -p save && : > save/online-on
fi

# The first start sets up the game's cache with a black screen for about a minute.
if [ ! -d save/z ]; then
  python3 halo_screen.py wait 10 "Starting Halo" "The first start takes about a minute more, with a black screen, while the game sets up its cache. Later starts are quick.

Press a button to go on."
fi

# The CPU at its top frequency and the GPU at its top step while the game
# runs: the H700's GPU governor otherwise keeps it at its lowest.
cpu=/sys/devices/system/cpu/cpufreq/policy0/scaling_governor
gpu=/sys/class/devfreq/gpu
old_cpu=$(cat $cpu 2>/dev/null)
old_gpu=$(cat $gpu/min_freq 2>/dev/null)
restore_clocks() {
  [ -n "$old_cpu" ] && echo "$old_cpu" | $ESUDO tee $cpu > /dev/null 2>&1
  [ -n "$old_gpu" ] && echo "$old_gpu" | $ESUDO tee $gpu/min_freq > /dev/null 2>&1
}
# (also when the launcher itself is stopped)
trap restore_clocks EXIT
trap 'exit 143' TERM INT HUP
echo performance | $ESUDO tee $cpu > /dev/null 2>&1
tr ' ' '\n' 2>/dev/null < $gpu/available_frequencies | sort -n | tail -n 1 | $ESUDO tee $gpu/min_freq > /dev/null 2>&1

export SDL_GAMECONTROLLERCONFIG="$sdl_controllerconfig"

$GPTOKEYB2 "$BINARY" -c "$GAMEDIR/halo.ini" &

pm_platform_helper "$GAMEDIR/$BINARY"

./$BINARY

pm_finish
