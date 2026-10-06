#!/bin/sh
# Builds the Knulli port (port/knulli/README.md) into build/knulli:
#   halo            the aarch64 glibc host (the loader, SDL2, OpenGL ES)
#   halo_guest.elf  the game, the Android port's guest image
#
# Needs: python configure.py run with --android-ndk (the guest build), the
# aarch64-linux-gnu cross compiler, SDL2 headers (SDL2_INCLUDE), the device's
# SDL2 to link against (SYSROOT_LIB: libSDL2-2.0.so.0, from /usr/lib of the
# device) and the sysroot to build against (GLIBC_SYSROOT: glibc 2.31, and
# libglvnd's EGL and OpenGL ES, made by port/knulli/glibc_sysroot.sh). Built
# on glibc 2.31, and linked against EGL and OpenGL ES by their usual names
# (libEGL.so.1, libGLESv2.so.2, which every system's SDL2 opens; on Knulli and
# muOS they load the Mali driver), the host runs on the other systems for
# these handhelds (muOS, ArkOS) as well as on Knulli.
set -eu

# the folders given, as absolute paths (they are used from other folders),
# without spaces (the compiler's options are split on them)
folder() {
	resolved=$(cd "$1" && pwd) || { echo "build.sh: $1: no such folder" >&2; exit 1; }
	case "$resolved" in
	*" "*) echo "build.sh: $resolved: a path with spaces is not supported" >&2; exit 1 ;;
	esac
	echo "$resolved"
}
SDL2_INCLUDE=$(folder "${SDL2_INCLUDE:?the folder that holds SDL2/SDL.h}")
SYSROOT_LIB=$(folder "${SYSROOT_LIB:?the device libraries}")
NDK=$(folder "${ANDROID_NDK:?the Android NDK (for the OpenGL ES and EGL headers)}")
GLIBC=$(folder "${GLIBC_SYSROOT:?the glibc 2.31 sysroot (port/knulli/glibc_sysroot.sh)}")
cd "$(dirname "$0")/../.."
ROOT=$(folder .)
CC=${CC:-aarch64-linux-gnu-gcc}
JOBS=${JOBS:-$(nproc)}
OUT=build/knulli
OBJ=$OUT/obj

ninja -j "$JOBS" build/android/halo_guest.elf build/android/host/host_import_table.c

mkdir -p "$OBJ" "$OUT/gl_include" "$OUT/lib"
KHRONOS=$NDK/toolchains/llvm/prebuilt/linux-x86_64/sysroot/usr/include
for name in EGL GLES2 GLES3 KHR; do
	ln -sfn "$KHRONOS/$name" "$OUT/gl_include/$name"
done
# the link name for the device's SDL2
ln -sf "$(ls "$SYSROOT_LIB"/libSDL2-2.0.so.0* | head -n 1)" "$OUT/lib/libSDL2.so"

# the C library's headers are the sysroot's alone (the cross compiler's own,
# a newer glibc's, would come first otherwise), and so are its libraries
LIBC_LIB=$GLIBC/usr/lib/aarch64-linux-gnu
LIBC_INCLUDES="-nostdinc -isystem $($CC -print-file-name=include)
	-isystem $GLIBC/usr/include/aarch64-linux-gnu -isystem $GLIBC/usr/include"
LIBC_LINK="--sysroot=$GLIBC -B$LIBC_LIB -L$LIBC_LIB -L$GLIBC/lib/aarch64-linux-gnu"

CFLAGS="-O2 -g -mcpu=cortex-a53 -fPIC -Wall -Wno-unused-function -D_GNU_SOURCE -DEGL_NO_X11 -DMESA_EGL_NO_X11_HEADERS"
# (the debug information names the tree and the SDL2 and C library headers
# by what they are, not by where they are on this computer; the OpenGL ES
# headers are found through build/knulli/gl_include, in the tree)
CFLAGS="$CFLAGS -ffile-prefix-map=$ROOT=. -ffile-prefix-map=$SDL2_INCLUDE=sdl2 -ffile-prefix-map=$GLIBC=glibc $LIBC_INCLUDES"
INCLUDES="-Iport/knulli/compat -Iport/knulli/host -Iport/android/include -Iport/android/host -Iport/linux/src
	-Iport/third_party/tomlc17 -I$OUT/gl_include -I$SDL2_INCLUDE"
MINIUPNPC="-Iport/third_party/miniupnpc/include -Iport/third_party/miniupnpc/src -DMINIUPNP_STATICLIB
	-DMINIUPNPC_SET_SOCKET_TIMEOUT -DMINIUPNPC_GET_SRC_ADDR -D_BSD_SOURCE -D_DEFAULT_SOURCE -w"

# the compiler and its options as the objects were made with them: when
# they change, every object is made again
FLAGS=$OUT/flags
printf '%s\n' "$CC $CFLAGS $INCLUDES $MINIUPNPC" > "$FLAGS.new"
cmp -s "$FLAGS.new" "$FLAGS" || mv "$FLAGS.new" "$FLAGS"
rm -f "$FLAGS.new"

objects=""
# whether an object is older than this script, the compiler's options, or its
# source or a header the compiler found it including (its .d file lists both;
# one gone, or a .d file that cannot be read, counts as newer)
stale() {
	object=$1
	[ ! -f "$object" ] || [ ! -f "$object.d" ] || [ "$0" -nt "$object" ] || [ "$FLAGS" -nt "$object" ] && return 0
	dependencies=$(sed -e 's/^[^:]*://' -e 's/\\$//' "$object.d") || return 0
	for dependency in $dependencies; do
		[ -f "$dependency" ] || return 0
		[ "$dependency" -nt "$object" ] && return 0
	done
	return 1
}
compile() {
	source=$1
	shift
	object=$OBJ/$(echo "$source" | tr / _).o
	if stale "$object"; then
		echo "CC $source"
		# shellcheck disable=SC2086
		$CC $CFLAGS $INCLUDES "$@" -MMD -MF "$object.d" -c "$source" -o "$object"
	fi
	objects="$objects $object"
}

for source in host_debug host_gl host_loader host_memory host_syscall host_thread; do
	compile port/android/host/$source.c
done
compile port/knulli/host/host_main.c
compile port/knulli/host/host_sdl2.c
compile port/knulli/host/host_profile.c
compile port/knulli/host/host_gl_timing.c
compile port/knulli/host/host_gl_timing.S
# the GL thread's recording functions, from the functions the guest imports
python3 port/knulli/glthread_gen.py build/android/guest/gen/gl_imports.list "$KHRONOS/GLES3/gl32.h" \
	"$OUT/host_glthread_gen.c.new"
cmp -s "$OUT/host_glthread_gen.c.new" "$OUT/host_glthread_gen.c" || mv "$OUT/host_glthread_gen.c.new" "$OUT/host_glthread_gen.c"
rm -f "$OUT/host_glthread_gen.c.new"
compile port/knulli/host/host_glthread.c
compile "$OUT/host_glthread_gen.c"
compile port/knulli/host/host_sdl3_events.c -Ibuild/android/third_party/SDL3/include
compile port/linux/src/posix_files.c
compile port/linux/src/posix_net.c
# shellcheck disable=SC2086
compile port/linux/src/posix_upnp.c $MINIUPNPC
for source in port/third_party/miniupnpc/src/*.c; do
	# shellcheck disable=SC2086
	compile "$source" $MINIUPNPC
done
compile port/third_party/tomlc17/tomlc17.c -w
compile build/android/host/host_import_table.c

echo "LINK $OUT/halo"
# (EGL and OpenGL ES: the sysroot's libglvnd, whose functions are the
# standard ones, so a call to one that is not fails here rather than on a
# handheld; an extension's is found through eglGetProcAddress)
# shellcheck disable=SC2086
$CC -o "$OUT/halo" $objects $LIBC_LINK -L"$OUT/lib" -Wl,-rpath-link,"$OUT/lib" -Wl,-rpath-link,"$LIBC_LIB" \
	-Wl,--allow-shlib-undefined -lSDL2 -l:libGLESv2.so.2 -l:libEGL.so.1 -lpthread -ldl -lm
cp build/android/halo_guest.elf "$OUT/halo_guest.elf"
ls -l "$OUT/halo" "$OUT/halo_guest.elf"
# the newest C library the host needs: glibc 2.31 at most, or the build fails
# (as it does when the host's symbols cannot be read)
symbols=$(${OBJDUMP:-aarch64-linux-gnu-objdump} -T "$OUT/halo") || { echo "build.sh: cannot read $OUT/halo" >&2; exit 1; }
needs=$(echo "$symbols" | grep -o 'GLIBC_[0-9.]*' | sort -Vu | tail -n 1)
echo "needs $needs"
[ -n "$needs" ] && [ "$(printf '%s\n' "$needs" GLIBC_2.31 | sort -V | tail -n 1)" = GLIBC_2.31 ] ||
	{ echo "build.sh: the host needs ${needs:-an unknown glibc}, not glibc 2.31 or older" >&2; exit 1; }
