#!/bin/bash
# Halo: Combat Evolved, the native port of the decompilation, for Knulli
# handhelds with the Allwinner H700 (Anbernic RG35XX H and its family).
# Refer to port/knulli/README.md.
#
# This script goes in /userdata/roms/ports, the game in the halo folder
# beside it. Put an Xbox disc image of the game (.iso) in that folder, or in
# ports itself: the first start copies its maps folder out (a few minutes,
# with its progress on the screen), then the image can be deleted. The log
# of each start is halo/log.txt; the settings are halo/config.toml.
#
# Hold the hotkey (MENU or SELECT) and push START to quit.

GAMEDIR="$(cd "$(dirname "$0")/halo" && pwd)"
cd "$GAMEDIR" || exit 1
# one start at a time (or development run, tools/device/run.sh), before the
# log starts again: another meanwhile would take the raised clocks for the
# ones to put back. The game inherits the lock, which is held as long as it
# runs, even past this script. (flock, or Python's where it is missing:
# without either, no start.)
exec 9> /var/run/halo-lock
if ! { flock -n 9 || python3 -c 'import fcntl; fcntl.flock(9, fcntl.LOCK_EX | fcntl.LOCK_NB)'; } 2> /dev/null; then
	echo "$(date): Halo is running already; this start was refused" >> "$GAMEDIR/log.txt"
	exit 1
fi
exec > "$GAMEDIR/log.txt" 2>&1
echo "Halo for Knulli, $(date)"
# (stopped between the steps below: through the EXIT handler)
trap 'exit 143' TERM INT HUP

# a step run with the signals the launcher gets passed on to it, to its end:
# a helper must not go on alone (holding the lock) when the launcher is
# stopped, and one asked to stop that has not ten seconds later is killed (a
# hung game must not keep the clocks and the lock). Its exit status; after a
# step the launcher was asked to stop in, the launcher stops.
stop() {
	stopping=1
	[ -n "$child" ] || return 0
	kill -TERM "$child" 2>/dev/null
	[ -n "$watchdog" ] || { (exec 9>&-; sleep 10; kill -KILL "$child" 2>/dev/null) & watchdog=$!; }
}
supervised() {
	local status

	child="" watchdog="" stopping=""
	trap stop TERM INT HUP
	"$@" &
	child=$!
	# (a stop that came as the step started)
	[ -z "$stopping" ] || stop
	# (until the step itself has ended: a signal ends a wait early)
	while :; do
		wait "$child"
		status=$?
		kill -0 "$child" 2>/dev/null || break
	done
	trap 'exit 143' TERM INT HUP
	[ -z "$watchdog" ] || kill "$watchdog" 2>/dev/null
	if [ -n "$stopping" ]; then
		echo "stopped (exit status $status)"
		exit 143
	fi
	return "$status"
}

# the fastest clocks while the maps are copied and the game runs: the CPU at
# its top frequency, the GPU held at its top step (the governor otherwise
# keeps it at the lowest). What they were is kept in /var/run (gone at a
# reboot), so that a start after a launcher that was killed outright puts
# them back first.
cpu=/sys/devices/system/cpu/cpufreq/policy0/scaling_governor
gpu=/sys/class/devfreq/gpu
saved=/var/run/halo-clocks
restore() {
	if [ -f "$saved" ]; then
		read -r cpu_governor gpu_minimum < "$saved"
		[ -n "$cpu_governor" ] && echo "$cpu_governor" > "$cpu" 2>/dev/null
		[ -n "$gpu_minimum" ] && echo "$gpu_minimum" > "$gpu/min_freq" 2>/dev/null
		rm -f "$saved"
	fi
	rm -f /var/run/battery-saver/halo.pause
}
restore
echo "$(cat "$cpu" 2>/dev/null) $(cat "$gpu/min_freq" 2>/dev/null)" > "$saved"
trap restore EXIT
echo performance > "$cpu" 2>/dev/null
tr ' ' '\n' < "$gpu/available_frequencies" 2>/dev/null | sort -n | tail -n 1 > "$gpu/min_freq" 2>/dev/null

# the battery saver must not dim or suspend the handheld meanwhile
mkdir -p /var/run/battery-saver && touch /var/run/battery-saver/halo.pause

# the maps: copied out of a disc image in this folder, or else in ports
if [ ! -s maps/ui.map ]; then
	shopt -s nullglob
	images=({.,..}/*.{iso,ISO,xiso,XISO})
	shopt -u nullglob
	if [ "${#images[@]}" -eq 0 ]; then
		echo "no maps folder and no disc image (.iso) in $GAMEDIR"
		supervised python3 halo_screen.py wait 60 "Halo needs your disc" "Copy the disc image of Halo: Combat Evolved for the Xbox (an .iso file) into roms/ports/halo on the SD card, then start Halo again.

Press a button to go back."
		exit 1
	fi
	[ "${#images[@]}" -eq 1 ] || echo "${#images[@]} disc images: the first is used"
	echo "copying the maps folder out of ${images[0]}"
	supervised python3 halo_extract.py --screen "${images[0]}" "$GAMEDIR" || exit 1
fi

# the settings for this handheld, the first time (config.default.toml's;
# config.toml keeps them, with the others' defaults, which the game writes):
# written whole, or no start (the game would write its own defaults instead,
# for good); nor with something else of that name, a folder say, in the way
if [ ! -f config.toml ]; then
	if [ -e config.toml ] || [ -L config.toml ] ||
		! rm -rf config.toml.new || ! cp config.default.toml config.toml.new ||
		! mv -f config.toml.new config.toml; then
		rm -f config.toml.new
		echo "cannot write config.toml"
		supervised python3 halo_screen.py wait 60 "Halo could not start" "Halo could not write its settings, roms/ports/halo/config.toml: the SD card may be full, or a folder has that name.

Press a button to go back."
		exit 1
	fi
fi

# internet play on, once for an install whose settings the launcher wrote
# with it off (before co-op and the server browser); the player's choice
# after that stays (save/online-on records that it was done)
if [ ! -e save/online-on ]; then
	sed -i '/^[[:space:]]*\[network\]/,/^[[:space:]]*\[/ s/^\([[:space:]]*online[[:space:]]*=[[:space:]]*\)false/\1true/' config.toml &&
		mkdir -p save && : > save/online-on
fi

# the handheld's own controls (SDL2 would take them for another pad)
SDL_GAMECONTROLLERCONFIG="$(python3 sdl_mapping.py)"
export SDL_GAMECONTROLLERCONFIG

# the first start: the game sets up its cache (save/z) before its first
# frame, about a minute with the screen black (nothing drawn before the game
# stays up as its GPU driver starts): said first
if [ ! -d save/z ]; then
	supervised python3 halo_screen.py wait 10 "Starting Halo" "The first start takes about a minute more, with a black screen, while the game sets up its cache. Later starts are quick.

Press a button to go on."
fi

supervised ./halo
status=$?
echo "exit status $status"
# (0: quit from the game; 130 and 143: stopped by a signal; 137: killed,
# most likely by the kernel when memory ran out, as this script's own kill
# comes only after a stop, which ends it above)
case "$status" in
0 | 130 | 143) ;;
137)
	supervised python3 halo_screen.py wait 30 "Halo was stopped" "The system stopped Halo (exit status 137), most likely because the handheld ran out of memory. In roms/ports/halo/config.toml, set high_res_text = false and high_res_hud = false: the high-res text alone takes about 260 MB.

Press a button to go back."
	;;
*)
	supervised python3 halo_screen.py wait 30 "Halo stopped" "Halo stopped unexpectedly (exit status $status). What happened is in roms/ports/halo/log.txt.

Press a button to go back."
	;;
esac
exit "$status"
