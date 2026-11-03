# TI-99/Sim for Miyoo Mini (Onion OS — Libretro Core)

[![License: GPL v2](https://img.shields.io/badge/License-GPL%20v2-blue.svg)](LICENSE)

A native **Libretro / RetroArch core** port of [TI-99/Sim](https://www.mrousseau.org/programs/ti99sim/) (Texas Instruments TI-99/4A Home Computer emulator) packaged for the **Miyoo Mini** and **Miyoo Mini Plus** running **Onion OS**.

---

## Overview

This repository provides a cycle-synced, single-threaded **Libretro core** (`ti99sim_libretro.so`) built on Marc Rousseau's TI-99/Sim emulation engine (`cTMS9900`, `cTMS9901`, `cTMS9918A`, `cTMS9919`, `cTMS5220`) and packaged for Onion OS (`ra32.ss`):

- **Native RetroArch Integration**: Full support for Onion OS Game Switcher, instant deterministic Savestates & Rewind, RetroArch Quick Menu (`Menu + Select`), hardware scalers/shaders/overlays, and custom input remapping.
- **Built-in 48-Key TI-99/4A On-Screen Keyboard (OSK)**: Press **`Select`** at any time to open a translucent virtual TI-99/4A keyboard with sticky `SHIFT`, `FCTN`, `CTRL`, and `ALPHA LOCK` modifiers plus dedicated function shortcuts (`AID`, `CLR`, `PROCEED`, `REDO`, `BACK`, `QUIT`).
- **Core Options v2**: Configure per-button TI-99/4A key bindings, D-pad mode (`TI Joystick` vs `Keyboard Cursor`), Color Palette (`Default`, `NTSC`, `RGB`, `Monochrome`), Video Border (`256×192` vs `288×216`), 32K RAM Expansion, TMS5220 Speech Synthesizer, and Alpha Lock Joystick Fix directly from the RetroArch Quick Menu.
- **Cycle-Synced 44.1 kHz Audio**: Synchronous single-threaded TMS9919 PSG + TMS5220 LPC speech synthesis mixed directly into Libretro's audio batch callback without background threads or `stop_audioserver.sh`.
- **Reproducible Cross-Compilation**: Builds inside the pinned Onion OS toolchain Docker container (`onionos/miyoo-toolchain`) or natively on macOS/Linux hosts (`make build-host`).

---

## Required System ROMs (Not Included)

> **Important:** No copyrighted TI ROMs, GROMs, or game cartridges are included in this repository. You must supply your own legally obtained TI-99/4A system ROMs.

Copy the following files into **`BIOS/ti99sim/`** on your Miyoo SD card (for backward compatibility, `Emu/TI99/console/` is also searched automatically):

| File | Status | SHA-1 | Description |
| :--- | :--- | :--- | :--- |
| `TI-994A.ctg` | **Required** | `562f9d4ef0dc2fb22da3cde6ebcf2da2ebce3eb0` | TI-99/4A Console ROM & GROMs (`0`, `1`, `2`) |
| `spchrom.bin` | Optional | `58b155f7eb6b0eed7ef6bfd3ab2eb4f1f12d6028` | TMS5220 Speech Synthesizer ROM (required for voice in *Parsec*, *Alpiner*, etc.) |
| `ti-disk.ctg` | Optional | `092bc2fc0ef2fb8bf9ebcd6fb1bd7f6ef7d22eb4` | TI Disk Controller DSR ROM |

If you only have raw ROM/GROM dumps (`994a_rom.bin`, `994a_grom0.bin`, etc.), use TI-99/Sim's `convert-ctg` utility with `ti99sim/roms/ti-994a.dat` to generate `TI-994A.ctg`.

---

## Installation on Miyoo Mini (Onion OS)

### Option A: From a Release / Built Package

1. Build or download `dist/ti99sim-miyoo.zip` (or copy the contents of `dist/SDCARD/`).
2. Extract the archive to the **root of your Miyoo Mini SD card** so that `RetroArch/`, `BIOS/`, `Emu/`, and `Roms/` merge with your existing folders:
   ```text
   SDCARD/
   ├── RetroArch/
   │   └── .retroarch/
   │       ├── config/
   │       │   └── TI-99Sim/
   │       │       └── Parsec.opt        # Preconfigured controls for Parsec
   │       └── cores/
   │           ├── ti99sim_libretro.so       # Native Libretro core
   │           └── ti99sim_libretro.info     # Core metadata & firmware definitions
   ├── BIOS/
   │   └── ti99sim/
   │       ├── TI-994A.ctg                   # <-- Put console ROM here
   │       ├── spchrom.bin                   # <-- Optional speech ROM
   │       └── ti-disk.ctg                   # <-- Optional disk DSR ROM
   ├── Emu/
   │   └── TI99/
   │       ├── config.json
   │       └── launch.sh
   └── Roms/
       └── TI99/
           └── <YourGame>.ctg                # <-- Put game cartridges here
   ```
3. Place your TI-99/4A game cartridges (`*.ctg`) in **`Roms/TI99/`**.
4. Insert the SD card into your Miyoo Mini, press **Select → Refresh Roms** on the Onion OS main menu, and open **Games → TI-99/4A**.

### Option B: Direct SD Card / Network Install via Make

```sh
make build
make package
make install SDCARD=/Volumes/SDCARD   # Copies files to SD card
# Or deploy over Wi-Fi / SSH:
make deploy IP=192.168.1.100
```

---

## Controls & Built-In On-Screen Keyboard

### Default Gamepad Mapping (Normal Gameplay)

| Miyoo Button | Default TI-99/4A Action | Typical Use |
| :--- | :--- | :--- |
| **D-Pad** | **TI Joystick 1** (or `E`/`S`/`D`/`X` in Cursor Mode) | Directional movement (Last-Input-Wins SOCD) |
| **A** | **Joystick 1 Fire** | Primary action / Shoot |
| **B** | **`1`** | Select Option 1 (1 Player / TI BASIC) / Menu Option |
| **Y** | **`2`** | Select Option 2 (2 Players / Cartridge Module) |
| **X** | **`3`** | Select Option 3 / Parsec Lift |
| **L1** | **`FCTN + 8` (`REDO`)** | Redo |
| **R1** | **`FCTN + 9` (`BACK`)** | Return to previous screen |
| **L2** | **`Y`** | "Yes" to prompts |
| **R2** | **`N`** | "No" to prompts |
| **Start** | **`Enter`** | Bypass title screen / Confirm |
| **Select** | **Toggle On-Screen Keyboard (OSK)** | Show/hide 48-key virtual TI-99/4A keyboard |
| **Menu** | **Onion OS Game Switcher / RetroArch** | Hold `Menu` + `Select` for RetroArch Quick Menu |

### On-Screen Keyboard (OSK) Controls (When Active)

Press **`Select`** to open the translucent 5-row TI-99/4A virtual keyboard overlay:

- **D-Pad**: Move highlight cursor across keys (wraps around edges with auto-repeat).
- **A**: Press the highlighted key (or toggle modifier if on `SHF`, `FCT`, `CTL`, `ALP`).
- **B**: Close OSK.
- **X**: Quick Backspace (`FCTN + S`).
- **Y**: Quick Space (`SPACE`).
- **Start**: Quick Enter (`ENTER`).
- **L1 / R1**: Toggle OSK dock position between **Bottom** and **Top** of the screen.
- **Select**: Close OSK.

---

## RetroArch Core Options

Open the RetroArch Quick Menu (**`Menu + Select` → Core Options**) to customize:

- **System**
  - **System Region / Refresh Rate**: `Auto` / `60 Hz (NTSC)` / `50 Hz (PAL)` (Restart)
  - **32K Memory Expansion**: `Enabled` (default) / `Disabled`
  - **TMS5220 Speech Synthesizer**: `Enabled` (default) / `Disabled` (Restart)
  - **Alpha Lock Joystick Up Fix**: `Enabled` (default, fixes TI-99/4A hardware diode bug so `ALPHA LOCK` never blocks Joystick Up) / `Disabled (Authentic Hardware Bug)`
- **Video**
  - **Color Palette**: `Default` / `NTSC` / `RGB` / `Monochrome`
  - **Video Border / Overscan**: `Cropped (256x192)` / `Full Border (288x216)`
  - **On-Screen Keyboard Position**: `Bottom` / `Top`
- **Audio**
  - **PSG Sound Effects Volume**: `0%` to `100%` (default `100%`)
  - **TMS5220 Speech Volume**: `0%` to `200%` (default `100%`, boost voice relative to PSG)
- **Controls & Keymap**
  - **D-Pad Directional Gate**: `4-Way (Last-Input Priority)` (default, prevents diagonal deadzones/freezing in *Parsec*, *Munch Man*, etc.) / `8-Way (Diagonals Allowed)`
  - **D-Pad Mode**: `TI Joystick 1` / `Keyboard Cursor (E/S/D/X)`
  - **Button A / B / Y / X / L1 / R1 / L2 / R2 / Start / Select Mapping**: Bind any physical button to any TI-99/4A key, number, or `FCTN` shortcut (`JOY_FIRE`, `SPACE`, `ENTER`, `OSK`, `0`–`9`, `A`–`Z`, `FCTN+1..9`, etc.).
  - **Per-Game Controls**: Use RetroArch's standard **Quick Menu → Core Options → Manage Core Options → Save Game Options** to save a `<Game>.opt` profile for any title. A bundled profile for *Parsec* (`B=1`, `Y=2`, `X=3` Lift, `L=Y`, `R=N` Play Again) is included out of the box in `.retroarch/config/TI-99Sim/Parsec.opt`.

---

## Building from Source

### Prerequisites

- **Docker** (for cross-compiling to ARMv7 Miyoo Mini via the pinned Onion OS toolchain image)
- Or **Clang / GCC** with C++20 support (for building the native host Libretro core via `make build-host`)

### Build Commands

```sh
# 1. Cross-compile build/ti99sim_libretro.so inside the Onion OS Docker container
make build

# 2. Assemble dist/SDCARD directory structure
make package

# 3. Create dist/ti99sim-miyoo.zip ready to extract onto an SD card
make zip

# Optional: Build native host core (ti99sim/bin/ti99sim_libretro.dylib or .so)
make build-host
```

---

## Architecture & Upstream Modifications

The emulator source in [`ti99sim/`](ti99sim/) is based on [christianhaitian/ti99sim](https://github.com/christianhaitian/ti99sim) at commit `e360854` (derived from Marc Rousseau's TI-99/Sim v0.16.0).

All modifications to `ti99sim/` are tracked in [`upstream-modifications.patch`](upstream-modifications.patch) (regenerate with `make patch`):

1. **Libretro Frontend (`ti99sim/src/libretro/`)**:
   - [`libretro-core.cpp`](ti99sim/src/libretro/libretro-core.cpp) & [`libretro-core-options.hpp`](ti99sim/src/libretro/libretro-core-options.hpp): Libretro API entrypoints, Core Options v2, input polling with Last-Input-Wins SOCD, and symbol visibility isolation (`link.T` / `exports.txt`).
   - [`ti994a-retro.cpp`](ti99sim/src/libretro/ti994a-retro.cpp): Synchronous frame-sliced `cRetroTI994A` machine coordinator (50,000 CPU cycles/frame at 60 Hz) and deterministic fixed-size binary savestates (`sRetroSaveState`).
   - [`tms9918a-retro.cpp`](ti99sim/src/libretro/tms9918a-retro.cpp): Software VDP scanline/tile/sprite compositor supporting `RGB565` and `XRGB8888`, cropped (`256×192`) and overscan border (`288×216`) geometries, and 4 color palettes.
   - [`tms9919-retro.cpp`](ti99sim/src/libretro/tms9919-retro.cpp): Synchronous cycle-accurate 44.1 kHz stereo audio synthesizer mixing TMS9919 4-voice PSG and TMS5220 LPC speech without background threads.
   - [`osk-retro.cpp`](ti99sim/src/libretro/osk-retro.cpp): Built-in 48-key TI-99/4A virtual keyboard overlay with embedded 5×7 bitmap font.
2. **Core Engine Enhancements (`ti99sim/src/core/`)**:
   - Dynamic Libretro `system_directory` (`BIOS/ti99sim/`) and `save_directory` path resolution in [`support.cpp`](ti99sim/src/core/support.cpp) alongside built-in SHA-1 (`-DTI_NO_OPENSSL`).
   - Configurable TI-99/4A Alpha Lock / Joystick Up hardware diode bug bypass (`SetAlphaLockFix`) in [`tms9901.cpp`](ti99sim/src/core/tms9901.cpp).

---

## Credits & License

- **TI-99/Sim**: Copyright © 1994–2020 [Marc Rousseau](https://www.mrousseau.org/programs/ti99sim/)
- **Upstream Linux/Handheld Fork**: [christianhaitian/ti99sim](https://github.com/christianhaitian/ti99sim)
- **Libretro Core & Miyoo Mini Packaging**: Aaron Szymanski

Licensed under the **GNU General Public License v2.0 or later** ([`LICENSE`](LICENSE)).
