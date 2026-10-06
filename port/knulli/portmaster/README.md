## Notes

Thanks to the [OpenCE](https://github.com/OpenCommunityEdition/OpenCE) team for decompiling Halo: Combat Evolved's Xbox build to C, which brings Bungie's whole campaign, with its vehicles and its AI, to a native build. This port for the Allwinner H700 handhelds is [halo-ce-anbernic-rg35xx](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx).

Copy your disc image of Halo: Combat Evolved for the Xbox (an `.iso` file) into `ports/halo`. The first start copies the game's maps out of it, which takes a few minutes; the image can then be deleted. The PC version's files do not work. The log of each start is `ports/halo/log.txt`, and the settings are in `ports/halo/config.toml`.

## Controls

| Button | Action |
|--|--|
| Left stick | Move |
| Right stick | Look |
| R2 | Fire |
| L2 | Throw a grenade |
| Bottom face button | Jump, accept |
| Right face button | Melee, back |
| Left face button | Action, reload |
| Top face button | Change the weapon |
| L1 | Flashlight |
| R1 | Change the grenade |
| L3 | Crouch |
| R3 | Zoom |
| Start | Pause menu |
| Hotkey + Start | Quit |

## Compile

The build runs on Linux x86-64 (Ubuntu 24.04; WSL works). It needs clang 22 for the game image's `arm64_32` target, the Android NDK r28c, and the aarch64 cross compiler. The handheld's `libSDL2-2.0.so.0` is linked against.

```sh
sudo apt install python3 ninja-build git curl gcc-aarch64-linux-gnu
wget https://apt.llvm.org/llvm.sh && sudo bash llvm.sh 22
curl -LO https://dl.google.com/android/repository/android-ndk-r28c-linux.zip
unzip -q android-ndk-r28c-linux.zip
git clone https://github.com/kirklandsig/halo-ce-anbernic-rg35xx.git
cd halo-ce-anbernic-rg35xx
mkdir -p sysroot
scp 'root@<handheld>:/usr/lib/libSDL2-2.0.so.0*' sysroot/
ANDROID_NDK=$PWD/../android-ndk-r28c SYSROOT_LIB=$PWD/sysroot ./build.sh
```

The result is in `dist/`.
