# Building the port from source

This guide builds `halo` and `halo_guest.elf`, the two programs of the
port, on a Linux x86-64 computer: the tools and packages to install (clang 22
from apt.llvm.org, the Android NDK r28c, the aarch64 cross compiler), the
two libraries to copy from the handheld, what `build.sh` does at each of its
stages, how rebuilds stay incremental, what the output is, and the errors
you are most likely to meet with their fixes. The build was done on
Ubuntu 24.04 under WSL. Nothing here needs the Xbox SDK, and the build uses
no game files; the [releases](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/releases/latest) are built this way, from the
published commit. To install the result on the handheld, see
[Install](INSTALL.md); to change the code, see
[Contributing](CONTRIBUTING-DEV.md).

## Contents

- [Overview](#overview)
- [The computer](#the-computer)
- [Packages](#packages)
- [clang 22](#clang-22)
- [The Android NDK r28c](#the-android-ndk-r28c)
- [The SDL2 headers](#the-sdl2-headers)
- [The handheld's libraries](#the-handhelds-libraries)
- [Running the build](#running-the-build)
- [What build.sh does](#what-buildsh-does)
- [The output](#the-output)
- [Rebuilding](#rebuilding)
- [Common errors](#common-errors)

## Overview

| Program | What it is | Compiled by |
| --- | --- | --- |
| `halo_guest.elf` | The game as a static ILP32 AArch64 image, built by upstream's Android build | clang 22 for the `arm64_32` target; linked by the NDK's `ld.lld` |
| `halo` | The host: an aarch64 glibc executable for the handheld | `aarch64-linux-gnu-gcc`, on Debian 11's glibc 2.31, linked against the handheld's SDL2 and libglvnd's EGL and OpenGL ES |

`build.sh` fetches the upstream decompilation at a pinned commit, applies
this repository's patch, copies `port/knulli/` in, configures and builds,
and copies the results to `dist/`.

```sh
ANDROID_NDK=$PWD/android-ndk-r28c SYSROOT_LIB=$PWD/sysroot ./build.sh
```

The first build takes 10 to 30 minutes; later builds are incremental.

## The computer

- Linux on x86-64. The build was done on Ubuntu 24.04 under WSL 2; other
  recent distributions should work if they can install the same tools.
- A network connection: the build clones upstream and downloads musl, SDL3,
  the SDL2 headers, and Debian 11's glibc 2.31 and libglvnd packages.
- Under WSL, keep the repository in the Linux file system (for example under
  your home folder) rather than on a Windows drive mounted into WSL: the
  build creates many small files, and the Linux file system also keeps the
  executable bits.

## Packages

```sh
sudo apt update
sudo apt install python3 ninja-build git curl tar unzip wget gcc-aarch64-linux-gnu \
    lsb-release software-properties-common gnupg
```

| Package | Used for |
| --- | --- |
| `python3` | `configure.py`, upstream's build generators, `glthread_gen.py`. Only the standard library is needed. |
| `ninja-build` | The upstream build. |
| `git` | Cloning upstream and SDL3. |
| `curl`, `tar` | Downloading and unpacking musl and the SDL2 headers. |
| `unzip` | Unpacking the NDK. |
| `gcc-aarch64-linux-gnu` | The host (`aarch64-linux-gnu-gcc`, version 13 on Ubuntu 24.04). |
| `wget`, `lsb-release`, `software-properties-common`, `gnupg` | apt.llvm.org's `llvm.sh` script, below. |

CMake and a JDK are not needed: they build upstream's Android app, not the
guest image.

## clang 22

The guest is compiled for clang's `arm64_32` target, Apple's ILP32 AArch64
ABI. Install clang 22 from [apt.llvm.org](https://apt.llvm.org/):

```sh
wget https://apt.llvm.org/llvm.sh
sudo bash llvm.sh 22
clang-22 -print-targets | grep aarch64_32
```

The last command must print a line for `aarch64_32`. `build.sh` checks the
same thing. To use another clang with that target, set `GUEST_CC`.

For `port/knulli/profile.py` (the profiler's report), also install the
`llvm-22` package from the same repository, which provides
`llvm-symbolizer-22` and `llvm-nm-22` ([Profiling](PROFILING.md)).

## The Android NDK r28c

The NDK links the guest image (its `ld.lld`) and provides the OpenGL ES and
EGL headers for both the guest and the host.

```sh
curl -LO https://dl.google.com/android/repository/android-ndk-r28c-linux.zip
unzip -q android-ndk-r28c-linux.zip
export ANDROID_NDK=$PWD/android-ndk-r28c
```

`build.sh` checks that `$ANDROID_NDK/toolchains/llvm/prebuilt/linux-x86_64`
exists.

## The SDL2 headers

The host is compiled against SDL2's headers from the tag `release-2.30.12`,
the version Knulli Gladiator II ships. `build.sh` downloads the tag's archive
from GitHub and keeps only `include/`, as `work/sdl2-include/SDL2/`. Nothing
needs to be installed. Without a network connection, put the headers there
yourself: `build.sh` downloads them only when `work/sdl2-include/SDL2/SDL.h`
is missing.

## glibc 2.31 and libglvnd

The host is built against Debian 11's glibc 2.31 rather than the cross
compiler's own (Ubuntu 24.04's, 2.39), so that it loads on firmware with an
older C library than Knulli's. It links EGL and OpenGL ES by their usual
names, `libEGL.so.1` and `libGLESv2.so.2`, against Debian 11's libglvnd,
rather than by the Mali driver's name (`libmali.so.0`), which only some
firmware has; on the handheld those names are the Mali driver or load it.
Linking against libglvnd's standard functions also makes a call to one that
is not standard fail at the link, not on a handheld (extensions are found
through `eglGetProcAddress`).

`port/knulli/glibc_sysroot.sh` downloads the packages (`libc6`, `libc6-dev`,
`linux-libc-dev`, `libglvnd0`, `libegl1`, `libgles2`), checks them against
the checksums in the script, and unpacks them into `work/glibc-2.31`, once.
`port/knulli/build.sh` compiles with only that sysroot's headers
(`-nostdinc`) and links against its libraries, and fails if the host needs
a glibc newer than 2.31; its last line names the newest version the host
needs (2.30).

## The handheld's SDL2

The host links against the handheld's own `libSDL2-2.0.so.0`. Copy it from
the handheld's `/usr/lib` into a folder, for example `sysroot/`:

```sh
mkdir -p sysroot
scp 'root@<handheld>:/usr/lib/libSDL2-2.0.so.0*' sysroot/
# or, over USB:
adb pull /usr/lib/libSDL2-2.0.so.0 sysroot/
```

It is used only when linking: `port/knulli/build.sh` makes the link name
`libSDL2.so` in `build/knulli/lib/` point at it and links with
`--allow-shlib-undefined`, so its own dependencies need not be present. At
run time the host uses the library on the handheld (SDL2 2.0.18 or newer).
It is the firmware's file: never commit it (`sysroot/` is in `.gitignore`,
and the [legal notice](LEGAL.md) explains why).

## Running the build

```sh
git clone https://github.com/kirklandsig/halo-ce-anbernic-rg35xx.git
cd halo-ce-anbernic-rg35xx
ANDROID_NDK=$PWD/../android-ndk-r28c SYSROOT_LIB=$PWD/../sysroot ./build.sh
```

| Variable | Default | Meaning |
| --- | --- | --- |
| `ANDROID_NDK` | (required) | The NDK r28c folder. |
| `SYSROOT_LIB` | (required) | The folder with `libSDL2-2.0.so.0*` from the handheld. |
| `GUEST_CC` | `clang-22` | A clang with the `arm64_32` target. |
| `HOST_CC` | `aarch64-linux-gnu-gcc` | The aarch64 glibc cross compiler. |
| `WORK` | `work/` beside `build.sh` | The working folder: the upstream tree and the SDL2 headers. |
| `DIST` | `dist/` beside `build.sh` | The output folder. |
| `JOBS` | the number of processors | Parallel jobs for ninja. |
| `UPSTREAM_URL` | `https://github.com/cybersecurity/halo-ce-universal.git` | Where to fetch upstream from, for example a local mirror. |

## What build.sh does

**1. Checks the tools and inputs.** `git`, `python3`, `ninja`, `curl`, `tar`,
`dpkg-deb`, `$HOST_CC` and `$GUEST_CC` must be on the `PATH`, `$GUEST_CC`
must have the `aarch64_32` target, `ANDROID_NDK` must be an NDK, and
`SYSROOT_LIB` must hold the handheld's SDL2. Each failure stops the build with a message naming the
fix ([Common errors](#common-errors)).

**2. Prepares the upstream tree** in `work/halo-ce-universal`:

- On the first run it makes an empty repository with upstream as `origin`.
- It fetches the commit in `UPSTREAM_COMMIT` (a shallow fetch of that
  commit, or a full fetch if the server refuses).
- If the tree is not already at that commit with exactly the patch applied,
  it checks the commit out, discards local changes (`git reset --hard`,
  `git clean -fd`, which keeps ignored build output) and applies
  `patches/halo-ce-universal-knulli.patch`. The comparison ignores the
  patch's `index` lines, whose abbreviated object names differ between a
  shallow and a full clone.
- It replaces `port/knulli/` in the tree with this repository's copy.
- It computes a hash of the patch and of every file in `port/knulli/`. If it
  differs from `work/source.stamp`, it deletes the host's objects
  (`build/knulli/obj`): `port/knulli/build.sh` rebuilds an object when its
  source, a header it included, the script or the compiler's options
  change, but not when a new header would now be found before another on
  the include path.

**3. Gets the SDL2 headers** (`release-2.30.12`), if `work/sdl2-include`
lacks them, and glibc 2.31 and libglvnd into `work/glibc-2.31`
(`glibc_sysroot.sh`, which does nothing when they are there already).

**4. Configures upstream**, if `build.ninja` is missing or was made for
another upstream commit (`work/configured.commit`):

```sh
python3 configure.py --release --android-ndk "$ANDROID_NDK" \
    --android-guest-cc "$GUEST_CC" --linux-cc "$GUEST_CC"
```

`--release` builds without the game's assertions. `--android-guest-cc`
names the clang for the guest. The guest build reuses headers that the
Linux build's rules generate, and `--linux-cc` points those rules at the
same clang. At this step `configure.py` downloads musl 1.2.5 and clones SDL3
(`release-3.4.16`, used for its headers) into
`build/android/third_party/`. `build.sh` then checks that `build.ninja` has
the Android guest; without a network connection `configure.py` leaves it out.

**5. Builds**, by running `port/knulli/build.sh` in the upstream tree with
`SDL2_INCLUDE`, `SYSROOT_LIB`, `ANDROID_NDK`, `GLIBC_SYSROOT`, `CC=$HOST_CC`
and `JOBS`:

1. `ninja build/android/halo_guest.elf build/android/host/host_import_table.c`
   builds the guest image and the host's import table (the host function
   for each import name).
2. It links the NDK's `EGL`, `GLES2`, `GLES3` and `KHR` headers into
   `build/knulli/gl_include/`, and the device's SDL2 as `libSDL2.so` into
   `build/knulli/lib/`.
3. It compiles the host with
   `-O2 -g -mcpu=cortex-a53 -fPIC -Wall -Wno-unused-function -D_GNU_SOURCE`
   (and `-ffile-prefix-map`, so that the debug information names the tree,
   the SDL2 headers and the sysroot by relative names), against the glibc
   2.31 sysroot's headers alone:
   upstream's Android host files `host_debug.c`, `host_gl.c`,
   `host_loader.c`, `host_memory.c`, `host_syscall.c` and `host_thread.c`;
   the Knulli files `host_main.c`, `host_sdl2.c`, `host_profile.c`,
   `host_gl_timing.c`, `host_gl_timing.S`, `host_glthread.c` and
   `host_sdl3_events.c` (against SDL3's headers); the platform layer's
   `posix_files.c`, `posix_net.c` and `posix_upnp.c`; miniupnpc; tomlc17; and
   the import table.
4. It generates `build/knulli/host_glthread_gen.c` with
   `port/knulli/glthread_gen.py` from `build/android/guest/gen/gl_imports.list`
   and the NDK's `GLES3/gl32.h`, replacing the old file only if the new one
   differs, and compiles it.
5. It links `build/knulli/halo` against the sysroot's glibc with
   `-lSDL2 -l:libGLESv2.so.2 -l:libEGL.so.1 -lpthread -ldl -lm`, copies
   `build/android/halo_guest.elf` beside it, and checks the glibc version the
   host needs (2.31 at most).

**6. Copies the results** into `dist/`: `halo`, `halo_guest.elf`, `Halo.sh`,
`halo_extract.py`, `halo_screen.py` and `sdl_mapping.py`, with mode 755, and
`config.default.toml`, and lists them. (The PortMaster zip's own files are
in `port/knulli/portmaster/`.)

## The output

```
dist/
├── halo              the host (aarch64 ELF, with debug information)
├── halo_guest.elf    the game (static ILP32 AArch64 image)
├── Halo.sh           the launcher
├── halo_extract.py   the maps extractor
├── halo_screen.py    the launcher's messages and progress on the screen
├── sdl_mapping.py    the controller mapping
└── config.default.toml  the settings the first launch writes
```

Copy them to the handheld as [Install](INSTALL.md) describes. For
development, `tools/bench.sh` runs a build on the handheld over ADB
([Profiling](PROFILING.md)).

Intermediate files you may want:

| Path in `work/halo-ce-universal/` | Contents |
| --- | --- |
| `build/android/halo_guest.elf` | The guest image, with symbols: symbolize guest addresses from logs and profiles against it (`llvm-symbolizer --obj=...`). |
| `build/knulli/halo` | The host, before copying. |
| `build/knulli/host_glthread_gen.c` | The generated GL recording and replay functions. |
| `build/android/guest/gen/gl_imports.list` | The GL functions the guest imports. |
| `build/android/host/host_import_table.c` | The host's import table. |

## Rebuilding

Run `./build.sh` again. It is incremental:

- The upstream tree is reset only when it is not at the pinned commit with
  exactly the current patch, so unchanged files keep their times and ninja
  rebuilds only what changed.
- The guest is rebuilt by ninja from its dependencies.
- The host's objects are rebuilt when their source, a header they
  included, `port/knulli/build.sh` or the compiler's options change, and
  all of them when the patch or `port/knulli/` changed (the stamp above).
- `configure.py` runs again when `UPSTREAM_COMMIT` names another commit.

**Edits in `work/` are lost** when `build.sh` resets the tree. After changing
an upstream file in `work/halo-ce-universal`, regenerate the patch before
you run `build.sh` again, and change the Knulli host in this repository's
`port/knulli/`, not in `work/` ([Contributing](CONTRIBUTING-DEV.md)).

To build from scratch, delete `work/` (or only `work/halo-ce-universal/build`
and `work/halo-ce-universal/build.ninja` to keep the clone and the
downloads).

## Common errors

| Message or symptom | Cause | Fix |
| --- | --- | --- |
| `build.sh: <tool> not found: install ...` | A tool is missing from the `PATH`. | Install the package named in the message. |
| `clang-22 has no arm64_32 (aarch64_32) target; use clang 22 from apt.llvm.org` | The clang found lacks the target (some distribution builds). | Install clang 22 from apt.llvm.org, or set `GUEST_CC` to a clang that has it. |
| `set ANDROID_NDK to the Android NDK r28c folder ...` | `ANDROID_NDK` is not set. | Set it for the command. |
| `ANDROID_NDK=... is not an Android NDK (no toolchains/llvm/prebuilt/linux-x86_64)` | The path points at the wrong folder, often the zip's parent. | Point it at `android-ndk-r28c` itself. |
| `set SYSROOT_LIB to a folder with libSDL2-2.0.so.0* ...`, `no libSDL2-2.0.so.0* in SYSROOT_LIB=...` | The device's SDL2 is missing. | Copy it from the handheld's `/usr/lib` ([above](#the-handhelds-sdl2)). |
| `glibc_sysroot.sh: ...: download failed or checksum mismatch` | A Debian package could not be downloaded, or it changed. | Check the connection; Debian's archive keeps old packages, and the script tries `archive.debian.org` too. |
| `build.sh: the host needs GLIBC_2.xx, not glibc 2.31 or older` | The host was compiled against a newer glibc's headers. | Check that `GLIBC_SYSROOT` is set and that nothing adds the cross compiler's own include folder. |
| `commit ... is not in https://github.com/cybersecurity/halo-ce-universal.git` | The pinned commit cannot be fetched (no network, or a mirror without it). | Check the connection or `UPSTREAM_URL`. |
| `error: patch failed` from `git apply` | The patch does not apply to the pinned commit, usually after editing one of them by hand. | Regenerate the patch from a tree at the pinned commit ([Contributing](CONTRIBUTING-DEV.md)). |
| `build.ninja has no Android guest build: delete .../build.ninja and check configure's output (it downloads musl and SDL3)` | `configure.py` could not download musl or clone SDL3 (it printed `Android build disabled: cannot fetch musl/SDL3`), or did not find the NDK. | Fix the network or the NDK path, delete `work/halo-ce-universal/build.ninja` and run `build.sh` again. |
| `glthread_gen.py`: `<function>: pointer argument <name> has no rule` | A GL function the guest now imports has a pointer argument the GL thread does not know how to copy. | Add the function to `SYNC`, `PAYLOAD` or `OFFSETS` in `port/knulli/glthread_gen.py` ([Architecture](ARCHITECTURE.md#the-generated-recording-functions)). |
| An `AssertionError` in `glthread_gen.py` | A new GL function returns a value but is not in `SYNC`. | Add it to `SYNC`. |
| Link errors about SDL or GL symbols | `SYSROOT_LIB` is not the device's SDL2, the SDL2 headers differ from its version, or the host calls a GL or EGL function that is not standard. | Copy SDL2 from the handheld again; keep the headers at 2.30.12 for Knulli Gladiator II; get an extension's functions through `eglGetProcAddress`. |
| On the handheld: `cannot read the game image .../halo_guest.elf` | `halo_guest.elf` is not beside `halo`. | Copy both into `halo/`. |
