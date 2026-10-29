# Contributing

Thanks for helping. This is a small project; the best contributions are tested fixes and reports from hardware
the maintainers do not have.

## Setting up

You need Docker, `git` and `make`. Everything else (the cross-compiler, SDL2 port) is fetched by `make`.

```sh
make build            # cross-compile -> build/ti99sim-sdl
make package          # dist/SDCARD, a tree mirroring the SD card
```

To try a change on a device: copy `build/ti99sim-sdl` over `Emu/TI99/ti99sim-sdl` on the card (or `make install
SDCARD=...`), launch it, then read `Emu/TI99/ti99sim.log`. Add `export TI_LOG_KEYS=1` to `launch.sh` to see what
each button sends.

## What to know before you change things

- **Where the code is:** the emulator is vendored under `ti99sim/` and is ordinary source; edit it in place. Match the
  surrounding style (tabs, spaces inside parentheses, naming).
- **The Miyoo's SDL2 port is unusual** (see "How it works" in the README). Features that look like they should work
  (SDL scale hints, desktop fullscreen, integer-scale flags) often do not on its renderer, so verify on a device, and
  say in your pull request what you tested it on.
- **Never commit TI ROMs, cartridges or disk images.** They are copyrighted; `.gitignore` excludes `*.ctg`. Do not
  link to places that host them.
- **Do not commit build output or downloaded binaries.** If a library is needed, pin it by URL and SHA-256 in
  `scripts/fetch-deps.sh` and `deps/SHA256SUMS`, as the existing ones are.
- **Licence:** contributions are under GPL-2.0-or-later like the rest of the project. New source files should carry
  `SPDX-License-Identifier: GPL-2.0-or-later`.

## Pull requests

Keep them focused, explain *why* in the description, and list how you tested: device model, Onion OS version, which
cartridge or screen you tried. For display and audio changes, the `display:` and `audio:` lines from `ti99sim.log`
are useful to include. If something could not be tested, say so.

Ideas for where to start are in the README under "Help wanted".
