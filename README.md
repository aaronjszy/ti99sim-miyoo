# TI-99/Sim for the Miyoo Mini

A TI-99/4A home computer emulator for the **Miyoo Mini** running **[Onion OS](https://onionui.github.io/)**.
It is [TI-99/Sim](https://www.mrousseau.org/programs/ti99sim/) (SDL2 port), cross-compiled for the Miyoo and wired
into Onion, with a button layout for the handheld, a crisp full-screen picture, and a **Games → TI-99/4A** menu entry.

**Status:** works on the one device it has been tested on (a Miyoo Mini, Onion OS v4.3.1-1): boot to the TI title
screen, the menus, and the *Parsec* cartridge. Miyoo Mini Plus, other firmware and other cartridges are untested;
reports and fixes are welcome (see [Contributing](CONTRIBUTING.md)).

## Quick start

You need [Docker](https://docs.docker.com/get-docker/), `git`, `make`, and an Onion OS SD card (or its files).

```sh
git clone <this repository> ti99sim-miyoo
cd ti99sim-miyoo
make install SDCARD=/path/to/mounted/sd-card     # builds, packages, and copies onto the card
```

No SD card handy? `make zip` produces `dist/ti99sim-miyoo.zip`; extract it directly onto the root of the card (e.g. `unzip dist/ti99sim-miyoo.zip -d /path/to/sd-card`).
The first build downloads a few MB of dependencies and the Onion toolchain image, then compiles (a few minutes).

Then add the TI ROMs (next section), eject the card, and on the Miyoo open **Apps → TI99Sim** or
**Games → TI-99/4A** (reboot once if the Games entry does not appear).

## TI ROMs and Cartridges (not included)

This emulator does not include, distribute, or link to any copyrighted console ROMs, system software, or game cartridges.
To use this emulator, you must supply your own ROM and cartridge images dumped from physical hardware and cartridges you
legally own. ti99sim stores ROMs and cartridges in its own `.ctg` format:

| Put this on the card | Where | Needed? |
|---|---|---|
| `TI-994A.ctg` (console ROM + GROMs) | `Emu/TI99/console/` | **Yes** (dumped from your console) |
| `ti-disk.ctg` (disk controller), `spchrom.bin` (speech) | `Emu/TI99/console/` | Optional |
| Game cartridges as `.ctg` | `Roms/TI99/` (shown under **Games**) | For games (dumped from your cartridges) |

How to create `.ctg` files from ROM dumps of cartridges you legally own is covered by TI-99/Sim's own documentation
([`ti99sim/doc/README.html`](ti99sim/doc/README.html), the `convert-ctg` and `mkcart` tools). The upstream
[`ti99sim/scripts/setup`](ti99sim/scripts/setup) shows the whole procedure but is written for a Linux desktop. A
containerised helper that does this for you would be a good contribution (see [Help wanted](#help-wanted)).

## Controls

The Miyoo's buttons reach the emulator as keyboard keys; the layout below is the default and can be changed
(see [Configuration](#configuration)).

| Button | TI action | Notes |
|---|---|---|
| D-pad | Joystick | Directional controls |
| A | Joystick fire | Primary fire / confirm |
| B | `1` | Menu option 1 / 1-Player |
| X | `2` | Cartridge boot selection (`2`) / 2-Player |
| Y | `Y` | Answers "Yes" to prompts ("Play again (Y/N)?") |
| L1 | Space | Secondary trigger (Parsec refueling, jump, thrust) |
| R1 | `N` | Answers "No" to prompts |
| L2 | `FCTN` + `9` (BACK) | Secondary back/cancel |
| R2 | `FCTN` + `8` (REDO) | Secondary redo |
| Start | Enter | Menu confirm / start game |
| Select | `P` | Pause |
| Menu (tap) | **Quits the emulator** | Exits cleanly to Onion on release |
| Menu + R2 (or R1) | **Save State** | Saved per-cartridge in `Emu/TI99/saves/` |
| Menu + L2 (or L1) | **Load State** | Restores state for active cartridge |
| Menu + Select / Start | **Reset Console** | Reboots the console |

There is no on-screen keyboard yet, so programs that need typing (TI BASIC, for example) are not practical.

## Configuration

Everything is plain text files on the card, in `Emu/TI99/`; no rebuild is needed.

**`keymap.ini`** maps buttons to TI keys, one `<SDL key name> = <action>` per line. Actions are `FIRE`, `RESET`,
`SAVE`, `LOAD`, `NONE`, or a TI key (`A`-`Z`, `0`-`9`, `ENTER`, `SPACE`, `COMMA`, `PERIOD`, `SLASH`, `SEMICOLON`, `EQUALS`),
optionally with a modifier: `FCTN+9`, `SHIFT+1`, `CTRL+C`. The file's comments list which SDL key each Miyoo
button sends. `make install` keeps an existing `keymap.ini`, so your changes survive upgrades.

**`launch.sh`** sets environment variables for the emulator:

| Variable | Effect |
|---|---|
| `TI_SOFT_SCALE=640x480` | On by default. The emulator scales the 256×192 picture itself (nearest-neighbour), filling the screen with crisp pixels. Delete the line for the Miyoo's own 2× scaling (black borders, soft pixels). |
| `TI_LOG_KEYS=1` | Log the name of every key press, to find out what a button sends. |
| `TI_AUDIO_SAMPLES=<n>` | Audio buffer size. **Leave unset**: smaller values give silence on this audio driver. |
| `TI_INTEGER_SCALE=0`, `TI_WINDOW_SIZE=WxH`, `TI_SCALE_FILTER=linear` | Display experiments; ineffective on the Miyoo's SDL2 renderer, kept for other platforms. |

The emulator writes `ti99sim.log` next to the binary on every launch; it is the first place to look when something
goes wrong. Each run starts with `display:` and `audio:` lines showing what the SDL driver actually granted.

## Known issues

- **Audio lags the picture** by roughly half a second. The Miyoo's audio driver queues several buffers and the
  emulator cannot shrink that; smaller buffers silence the sound (the driver's sleep calculation goes negative for
  buffers under about 11 ms; this reading of its source is not confirmed on hardware). Some of the lag may be inherent
  to individual games.
- **Menu quits** the emulator. Onion's freeze-and-switch behaviour only exists for RetroArch (and DraStic), so it is not
  available here. However, manual save states are supported via the standard Onion hotkeys (`Menu + R2` to save, `Menu + L2` to load),
  stored per-game in `Emu/TI99/saves/`.
- **Uneven pixel widths:** at 2.5× scaling some pixel columns and rows are 3 px wide and others 2 px.
- **No on-screen keyboard**, see above.

## How it works

Why the build looks the way it does, since the Miyoo's SDL2 port is unusual:

- The Onion toolchain image only ships **SDL 1.2**, but TI-99/Sim needs **SDL2**. The build therefore links against
  [steward-fu's SDL2 port for the Miyoo Mini](https://github.com/steward-fu/sdl2), whose headers and prebuilt libraries
  `make deps` downloads at a pinned commit and checks against `deps/SHA256SUMS`.
- That port's renderer draws a window at a whole-number multiple of its size, centred, and blurs any hardware scaling.
  The emulator therefore scales the picture to 640×480 itself and hands over an image that needs no scaling.
- The Miyoo has no OpenSSL, so a small built-in SHA-1 replaces it (`TI_NO_OPENSSL`).
- Audio needs Onion's `audioserver` stopped first, which `launch.sh` does with Onion's own `stop_audioserver.sh`.

The changes to TI-99/Sim itself are small and live in `ti99sim/` as ordinary source: the configurable key map
(`src/sdl/keymap.cpp`), the software scaler and display options (`src/sdl/bitmap.cpp`, `tms9918a-sdl.cpp`), the
audio override (`tms9919-sdl.cpp`), and the built-in SHA-1 (`src/core/support.cpp`). A complete diff against upstream
commit `e360854` is kept in [`upstream-modifications.patch`](upstream-modifications.patch).

## Building and hacking

```sh
make deps       # download the pinned SDL2 headers and libraries into deps/
make build      # cross-compile in Docker -> build/ti99sim-sdl
make package    # assemble dist/SDCARD, a tree mirroring the SD card
make zip        # ... and zip it
make install SDCARD=/path/to/card
make clean      # remove build output (distclean also removes deps/)
```

The toolchain image and every downloaded file are pinned (image by digest, libraries by SHA-256), so builds are
repeatable: a clean build of this repository produced a byte-identical binary to the one that was tested on the
device. Edit anything under `ti99sim/`, run `make build`, and copy `build/ti99sim-sdl` to `Emu/TI99/` on the card
to try it. See [CONTRIBUTING.md](CONTRIBUTING.md).

```
Makefile                      build, package, install targets
scripts/                      fetch-deps, build-in-container, package, install
deps/SHA256SUMS               pinned checksums for the downloaded libraries
miyoo/SDCARD/                 hand-written files that go on the card (launchers, configs, keymap)
ti99sim/                      vendored TI-99/Sim source, with the Miyoo changes
upstream-modifications.patch  diff of all changes against upstream commit e360854
```

## TODO / Help wanted

- More carefully thought out and widely applicable button mappings
- Onion integration for the Menu button: autosave on exit and resume on launch.
- Lower audio latency, which probably means patching and rebuilding the SDL2 port's audio driver.
- Release builds (GitHub Actions) so people can download a ready-made zip.
- See if we can setup ti99sim as a proper RetroArch core
- Test on other hardware and firmware (Miyoo Mini Plus, other Onion versions) and with more cartridges.

## Credits and licences

- **TI-99/Sim** © Marc Rousseau, [mrousseau.org](https://www.mrousseau.org/programs/ti99sim/). The SDL2 handheld fork by
  [christianhaitian](https://github.com/christianhaitian/ti99sim) (commit `e360854`) is the base of `ti99sim/`; all modifications
  are documented in [`upstream-modifications.patch`](upstream-modifications.patch). One upstream file, the third-party CF7+
  firmware image `roms/cf7a+.bin`, is deliberately not included.
- The Miyoo Mini **SDL2 port** is by [steward-fu](https://github.com/steward-fu/sdl2) (LGPL-2.1). It is downloaded, not
  stored in this repository, but the packages `make zip` and `make install` produce include its libraries.
- The **TI-99/4A menu icon** is derived from a photograph of the console by Rama & Musée Bolo
  ([Wikimedia Commons](https://commons.wikimedia.org/wiki/File:Texas_Instruments_TI-99-4A_(transparent_bg).png)),
  licensed under [CC BY-SA 2.0 FR](https://creativecommons.org/licenses/by-sa/2.0/fr/deed.en).
- The cross-compile **toolchain image** comes from the [Onion OS](https://github.com/OnionUI/Onion) project.
- This project's own code, scripts and docs are under the **GNU GPL, version 2 or (at your option) any later version**;
  see [`LICENSE`](LICENSE). Every TI-99/Sim source file carries that same notice. (Upstream's `ti99sim/doc/COPYING` is
  the text of the *Library* GPL, which does not match the file headers; it is left as shipped.)
- **Trademarks & Non-Affiliation:** Texas Instruments, TI-99/4A, and related names and trademarks belong to
  Texas Instruments Incorporated. This project is an independent open-source emulator port and is not affiliated
  with, endorsed by, or sponsored by Texas Instruments or any software publishers. No TI ROMs or software are included.
