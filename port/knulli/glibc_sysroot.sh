#!/bin/sh
# Makes the sysroot that the host is built against (build.sh's
# GLIBC_SYSROOT): Debian 11's glibc 2.31 for aarch64, the one PortMaster's
# ports are built on, so that the host runs on the older systems for these
# handhelds (muOS, ArkOS) as well as on Knulli; and libglvnd's EGL and OpenGL
# ES, to link against by their usual names (libEGL.so.1, libGLESv2.so.2),
# which every system's SDL2 opens:
#   sh port/knulli/glibc_sysroot.sh <folder>
# The packages are checked against the checksums below. The sysroot is made
# in a folder beside <folder> and takes its place only once it is whole, so
# that a run that stops part way leaves no sysroot that looks made; one made
# from these packages already is kept (its .packages file names them).
set -eu
TARGET=${1:?the folder to make the sysroot in}
MIRRORS="https://deb.debian.org/debian https://archive.debian.org/debian"
PACKAGES="
pool/main/g/glibc/libc6_2.31-13%2Bdeb11u11_arm64.deb baaa9aa184e2f21738c5819055e6740cc5b22f198e3f416e33f82b40ff6933d8
pool/main/g/glibc/libc6-dev_2.31-13%2Bdeb11u11_arm64.deb 28d478134722dcd4b0bd2045a199301d18713bf95947b9fce66634e7aeacab2e
pool/main/l/linux/linux-libc-dev_5.10.223-1_arm64.deb 8b6374a64412d33eac61d74f77b8f932da4b8a707ea8a614791e2a35b8917618
pool/main/libg/libglvnd/libglvnd0_1.3.2-1_arm64.deb 5d7a05a966d1df43ca440245dfc7e18a51fc974f665441fc87180a605a0481d9
pool/main/libg/libglvnd/libegl1_1.3.2-1_arm64.deb 4b531a79399010d3377ce9b6094c8f5f3508bd18ea5b32008a6a1ec16e019a81
pool/main/libg/libglvnd/libgles2_1.3.2-1_arm64.deb ae3fda1556b677519ff13d43295d7df678d3a9c39a21042e87d9255de1bc78b6
"

if [ "$(cat "$TARGET/.packages" 2> /dev/null || true)" = "$(echo "$PACKAGES")" ]; then
	echo "the sysroot in $TARGET is made already"
	exit 0
fi
mkdir -p "$(dirname "$TARGET")"
SYSROOT=$(mktemp -d "$(cd "$(dirname "$TARGET")" && pwd)/.glibc-sysroot.XXXXXX")
download=$(mktemp -d)
trap 'rm -rf "$download" "$SYSROOT"' EXIT
echo "$PACKAGES" | while read -r path sum; do
	[ -n "$path" ] || continue
	file=$download/$(basename "$path")
	for mirror in $MIRRORS; do
		curl -sfL -o "$file" "$mirror/$path" && break
	done
	echo "$sum  $file" | sha256sum -c --quiet || { echo "glibc_sysroot.sh: $path: download failed or checksum mismatch" >&2; exit 1; }
	dpkg-deb -x "$file" "$SYSROOT"
done
# the packages' symbolic links name absolute paths (/lib/...), which would
# be this computer's: they are made relative, within the sysroot
find "$SYSROOT" -type l -lname '/*' | while IFS= read -r link; do
	ln -sfn "$(realpath -m --relative-to="$(dirname "$link")" "$SYSROOT$(readlink "$link")")" "$link"
done
echo "$PACKAGES" > "$SYSROOT/.packages"
chmod 755 "$SYSROOT"
rm -rf "$TARGET"
mv "$SYSROOT" "$TARGET"
echo "glibc 2.31 sysroot in $TARGET"
