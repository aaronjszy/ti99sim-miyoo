# Contributing

Thanks for helping. This is a small project; the best contributions are tested fixes and reports from hardware
the maintainers do not have.

## Setting up

You need Docker (for ARMv7 cross-compilation), `git` and `make`. Alternatively, you can build natively on macOS/Linux with a C++17 compiler via `make build-host`.

```sh
make build            # cross-compile -> build/ti99sim_libretro.so
make package          # dist/SDCARD, a tree mirroring the SD card
make zip              # dist/ti99sim-miyoo.zip, ready to extract to SD card root
```

To try a change on a device: deploy over Wi-Fi (`make deploy IP=...`) or copy the packaged tree to your mounted SD card (`make install SDCARD=/path/to/card`).

## What to know before you change things

- **Where the code is:** the emulator engine and Libretro frontend are vendored under `ti99sim/` and are ordinary C++ source; edit them in place. Match the
  surrounding style (tabs, spaces inside parentheses, naming). All changes made relative to upstream commit `e360854` are
  tracked in [`upstream-modifications.patch`](upstream-modifications.patch) (run `make patch` after editing code in `ti99sim/`).
- **Libretro architecture:** display composing (`tms9918a-retro.cpp`), audio synthesis (`tms9919-retro.cpp`), input handling (`libretro-core.cpp`), and savestates (`ti994a-retro.cpp`) run within standard Libretro callbacks. All settings are exposed as RetroArch Core Options v2 (`libretro-core-options.hpp`).
- **Never commit, host, or link to copyrighted files.** Do not commit TI console ROMs, GROMs, cartridges (`*.ctg`),
  or disk images; `.gitignore` excludes `*.ctg`. Do not post links to ROM-sharing sites or unauthorized downloads in
  issues, pull requests, or commit messages.
- **Do not commit build output or downloaded binaries.**
- **Licence:** contributions are under GPL-2.0-or-later like the rest of the project. New source files should carry
  `SPDX-License-Identifier: GPL-2.0-or-later`.

## Pull requests

Keep them focused, explain *why* in the description, and list how you tested: device model, Onion OS version, which
cartridge or feature you tried. If something could not be tested on real hardware, please state that in the PR.
