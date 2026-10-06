# Legal notice

This is an unofficial, non-commercial fan project. It is not affiliated with,
endorsed by or sponsored by Microsoft, Xbox Game Studios, Halo Studios or
Bungie. Halo, Halo: Combat Evolved and Xbox are trademarks of Microsoft
Corporation. Other names are the trademarks of their owners. They are used
here only to describe what the software is compatible with.

## What this repository contains

- Source code: the Knulli host in `port/knulli/`, a patch against the
  upstream decompilation in `patches/`, a build script and development
  tools.
- Documentation and screenshots of the game running on the handheld.

Its [releases](https://github.com/kirklandsig/halo-ce-anbernic-rg35xx/releases/latest) hold the port built from this source at the
release's commit: the programs `halo` and `halo_guest.elf`, the launcher and
its helpers, and the licences (`LICENSE.txt`, `THIRD-PARTY-NOTICES.txt`).

## What this repository does not contain

- No Halo game files: no disc images (ISO or XISO), no maps, no XBE, no
  extracted assets such as sounds, textures or models. The releases have
  none either.
- No build output: `work/` and `dist/` are not committed. The programs are
  published only in the releases, built from the published source.
- No Arm Mali driver (`libmali`) and no other libraries from the handheld's
  firmware. The build links against copies you take from your own device;
  they are never committed, and the releases use the handheld's own.

Please keep it that way: pull requests that add any of these files will not
be accepted.

## Your own copy of the game

To play, you need your own copy of the original Xbox version of Halo: Combat
Evolved. The launcher extracts the `maps/` folder from your disc image on
your own handheld; nothing is downloaded.

## Licence of the code

The upstream project, [halo-ce-universal](https://github.com/cybersecurity/halo-ce-universal),
is released under CC0 1.0 Universal by its authors. This port's code and
documentation are released under the same terms; see [LICENSE](../LICENSE).
CC0 waives copyright in the contributors' own work only. It grants no rights
to Microsoft's or Bungie's game, data or trademarks.

The build downloads third-party code under its own licences: SDL's headers
(zlib licence) and, through the upstream build, musl (MIT) and SDL3 (zlib).
The upstream tree also includes tomlc17, kcp and miniupnpc under their
licences. The releases' programs contain musl (MIT), tomlc17 (MIT), kcp
(MIT), miniupnpc (BSD-3-Clause), code from SDL3's headers (zlib) and the
game's own zlib 1.1.3; `THIRD-PARTY-NOTICES.txt` in each release has their
notices. The launcher's font is font8x8, in the public domain.

## Decompilation and your jurisdiction

Whether a decompilation project may be used, modified or distributed depends
on the law where you live. Nothing in this repository is legal advice. If
you are unsure, ask a lawyer before using it.

## Contact

If you believe something in this repository infringes your rights, open an
issue on the repository, and it will be looked at promptly.
