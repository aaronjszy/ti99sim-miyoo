# RetroArch Integration Audit & Improvement Roadmap (`ti99sim_libretro`)

**Date:** 31-Oct-2026  
**Target:** `ti99sim_libretro` (Miyoo Mini / Onion OS & Desktop Libretro)  
**Status:** Architecture Solid (~Phase 5 Complete); Integration Polish & Feature Expansion Needed

---

## 1. Executive Summary

The Libretro port of TI-99/Sim is in an advanced, highly functional state. Key accomplishments already in place include:
- **Deterministic savestates (`sRetroSaveState`)**: ~85 KB flat binary layout with zero dynamic heap allocations during serialize/unserialize, enabling RetroArch Rewind, Runahead, and GameSwitcher support.
- **Cycle-sliced synchronous audio**: Multi-slice CPU catch-up with 44.1 kHz stereo audio buffer generation mixing TMS9919 PSG and TMS5220 LPC speech without threads.
- **Video subsystem**: Clean TMS9918A rasterization supporting RGB565/XRGB8888, overscan border modes (256×192 vs 288×216), and multiple color palettes.
- **In-core Virtual Keyboard Overlay (OSK)**: 48-key virtual keyboard overlay with sticky modifiers.
- **Input handling**: Last-Input-Wins SOCD resolution, Core Options v2 button remapping, and RetroArch input descriptors.

However, an audit of the RetroArch and Onion OS integration identified **three critical discrepancies**, **several missing Libretro frontend capabilities**, and **high-value feature opportunities**.

---

## 2. Critical Discrepancies & Immediate Fixes

### 2.1 Onion OS Launcher Uses `retroarch` Instead of `ra32.ss`
* **File:** [`miyoo/SDCARD/Emu/TI99/launch.sh`](miyoo/SDCARD/Emu/TI99/launch.sh)
* **Current State:**
  ```sh
  #!/bin/sh
  cd /mnt/SDCARD/RetroArch/
  HOME=/mnt/SDCARD/RetroArch/ /mnt/SDCARD/RetroArch/retroarch -v -L /mnt/SDCARD/RetroArch/.retroarch/cores/ti99sim_libretro.so "$1"
  ```
* **Issue:** On Onion OS, `ra32.ss` is the official RetroArch wrapper. It handles GameSwitcher freeze/resume workflows, automatic savestate creation on sleep/shutdown, battery monitor hooks, and display scaling. Calling `retroarch` directly bypasses these platform features.
* **Remediation:** Update `launch.sh` to check for `ra32.ss` and fallback to `retroarch` for non-Onion environments:
  ```sh
  #!/bin/sh
  RA_DIR=/mnt/SDCARD/RetroArch
  cd "$RA_DIR"
  if [ -f "$RA_DIR/ra32.ss" ]; then
      HOME="$RA_DIR/" "$RA_DIR/ra32.ss" -v -L "$RA_DIR/.retroarch/cores/ti99sim_libretro.so" "$1"
  else
      HOME="$RA_DIR/" "$RA_DIR/retroarch" -v -L "$RA_DIR/.retroarch/cores/ti99sim_libretro.so" "$1"
  fi
  ```

---

### 2.2 Major Discrepancy Between OSK Implementation and Documentation
* **Files:** 
  - [`ti99sim/src/libretro/osk-retro.cpp`](ti99sim/src/libretro/osk-retro.cpp)
  - [`ti99sim_libretro.info`](miyoo/SDCARD/RetroArch/.retroarch/cores/ti99sim_libretro.info)
  - [`README.md`](README.md)
* **Conflict Matrix:**
  | Button | `README.md` & `.info` Description | Actual `osk-retro.cpp` Behavior |
  | :--- | :--- | :--- |
  | **A** | Tap key & automatically close OSK | **Presses/holds the highlighted key** (does *not* close OSK) |
  | **B** | Hold/press the highlighted key | **Closes the OSK immediately** |
  | **X** | Quick Backspace (`FCTN+S`) | **Toggles sticky `FCTN` modifier** |
  | **Y** | Toggle sticky modifier | **Inputs `SPACE`** |
  | **Start** | Enter / Confirm | **Inputs `ENTER`** |
* **Impact:** Players following the documentation or the RetroArch in-game button descriptions will press `B` expecting to type a letter and inadvertently exit the OSK. Furthermore, typing in TI BASIC is cumbersome without a dedicated Backspace shortcut.
* **Remediation:**
  1. Add quick Backspace on `X` in `osk-retro.cpp` (`FCTN + S` or `FCTN + 3` Erase).
  2. Decide whether `B` or `Select` should be the primary OSK exit button.
  3. Reconcile `README.md`, `ti99sim_libretro.info`, and `osk-retro.cpp` so button assignments match.

---

### 2.3 Gamepad Default Layout Documentation Mismatch
* **Files:** [`README.md`](README.md), [`libretro-core.cpp`](ti99sim/src/libretro/libretro-core.cpp), [`Parsec.opt`](miyoo/SDCARD/RetroArch/.retroarch/config/TI-99Sim/Parsec.opt)
* **Issue:**
  - `README.md` states: `B = Joystick 1 Fire`, `A = Space`, `Y = 1`, `X = 2`, `L1 = 3`.
  - `libretro-core.cpp` defaults to: `A = JOY_FIRE`, `B = 1`, `Y = 2`, `X = 3`, `L = FCTN_REDO`, `R = FCTN_BACK`, `L2 = Y`, `R2 = N`.
  - `Parsec.opt` sets: `A = JOY_FIRE`, `B = 1`, `Y = 2`, `X = 3`, `L = Y`, `R = N`.
* **Remediation:** Update `README.md` to reflect that `A` is `Joystick Fire` by default (matching the code and `.info` file), and clarify the physical button mapping on the Miyoo Mini.

---

## 3. Glaring Omissions in RetroArch Integration

### 3.1 Cartridge Archive Support (`.zip` / `.7z`)
* **Files:**
  - `ti99sim/src/libretro/libretro-core.cpp` (`valid_extensions`)
  - `miyoo/SDCARD/RetroArch/.retroarch/cores/ti99sim_libretro.info` (`supported_extensions`)
  - `miyoo/SDCARD/Emu/TI99/config.json` (`extlist`)
* **Current Limitation:** Only `.ctg` files are declared.
* **Issue:** RetroArch users commonly store ROM collections in `.zip` or `.7z` format. Currently, RetroArch and Onion OS will filter these out.
* **Remediation:** 
  - Change extensions to `"ctg|zip|7z"`.
  - Because the core specifies `need_fullpath = true` and `block_extract = false`, RetroArch automatically decompresses archive contents to `/tmp` and passes the extracted `.ctg` file path to `retro_load_game`.

---

### 3.2 Disk Image Support (`.dsk`) & Libretro Disc Control Interface
* **Context:** The TI-99/4A has a significant library of software distributed exclusively on 5.25" floppy disks (e.g., *Tunnels of Doom*, *Adventure*, *Telengard*, Infocom games, TI-Writer, Assembler, and user-written Extended BASIC programs).
* **Current State:** 
  - Upstream `ti99sim` already contains complete sector, track, and image emulation in `ti99sim/src/core/` (`ti-disk.cpp`, `cDiskDevice`, `disk-image.cpp`, `disk-sector.cpp`).
  - `LoadDevices` already auto-detects `ti-disk.ctg` from `BIOS/ti99sim/`.
  - However, there is no mechanism in `libretro-core.cpp` to attach disk images to `DSK1`–`DSK3` or boot disk software.
* **Remediation:**
  1. Add `.dsk` to `valid_extensions`. When a `.dsk` is loaded, automatically mount it as `DSK1` and boot into console mode with disk support active.
  2. Implement `RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE`:
     - Exposes RetroArch's native **Disc Control** menu (*Disc Cycle / Eject / Insert / Disc Index*).
     - Allows multi-disk swapping without quitting the core.

---

### 3.3 Core Options v0 Fallback (`RETRO_ENVIRONMENT_SET_VARIABLES`)
* **File:** [`ti99sim/src/libretro/libretro-core-options.hpp`](ti99sim/src/libretro/libretro-core-options.hpp)
* **Issue:** Core Options v2 and v1 are supported, but if a frontend returns `version == 0` (older RetroArch versions, custom forks, or minimal frontends like MiniUI), no options are registered.
* **Remediation:** Add standard `RETRO_ENVIRONMENT_SET_VARIABLES` serialization fallback for `version == 0`.

---

## 4. Quality of Life & Usability Improvements

### 4.1 Expose OSK Dock Position in Core Options
* **Current State:** The OSK can be shifted between the top and bottom of the display using `L1`/`R1` while active, but always defaults to Bottom on reset.
* **Improvement:** Add `ti99sim_osk_position` (`Bottom` [default] vs `Top`) to Core Options v2 so users can persist their preferred layout.

### 4.2 PSG / Speech Audio Balance Option
* **Current State:** Master volume is clamped to 75%, and PSG and TMS5220 speech are mixed directly at unity gain.
* **Improvement:** In titles with external speech (*Parsec*, *Alpiner*), speech phrases can sometimes be overpowered by PSG sound effects. Adding independent volume sliders or a balance ratio option in Core Options improves audio clarity.

### 4.3 TI BASIC Cartridge RAM / NVRAM Persistence
* **Current State:** `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` properly inspects `FLAG_BATTERY_BACKED` and returns the cartridge RAM bank for cartridges like *Mini Memory*.
* **Improvement:** Verify SRAM write-back timing with RetroArch's auto-save interval to ensure user programs in Mini Memory or battery-backed RAM cartridges persist cleanly across sessions.

---

## 5. Prioritized Action Plan & Progress

| Priority | Task | Target Files | Status |
| :---: | :--- | :--- | :---: |
| **P0** | **Fix Onion OS `launch.sh`** | [`miyoo/SDCARD/Emu/TI99/launch.sh`](miyoo/SDCARD/Emu/TI99/launch.sh) | **Done** |
| **P0** | **Reconcile OSK Controls & Documentation** | [`README.md`](README.md), [`ti99sim_libretro.info`](miyoo/SDCARD/RetroArch/.retroarch/cores/ti99sim_libretro.info) | **Done** |
| **P0** | **Add Quick Backspace to OSK (`X` -> `FCTN+S`)** | [`ti99sim/src/libretro/osk-retro.cpp`](ti99sim/src/libretro/osk-retro.cpp) | **Done** |
| **P1** | **Enable Archive Support (`.zip`, `.7z`)** | [`libretro-core.cpp`](ti99sim/src/libretro/libretro-core.cpp), [`.info`](miyoo/SDCARD/RetroArch/.retroarch/cores/ti99sim_libretro.info), [`config.json`](miyoo/SDCARD/Emu/TI99/config.json) | **Done** |
| **P1** | **Add OSK Initial Position Core Option** | [`libretro-core-options.hpp`](ti99sim/src/libretro/libretro-core-options.hpp), [`libretro-core.cpp`](ti99sim/src/libretro/libretro-core.cpp) | **Done** |
| **P2** | **Add Core Options v0 Fallback (`SET_VARIABLES`)** | [`libretro-core-options.hpp`](ti99sim/src/libretro/libretro-core-options.hpp) | **Done** |
| **P2** | **Audio / Speech Gain Core Options** | [`libretro-core-options.hpp`](ti99sim/src/libretro/libretro-core-options.hpp), [`tms9919-retro.cpp`](ti99sim/src/libretro/tms9919-retro.cpp) | **Done** |
| **P3** | **Wire Floppy Disk (`.dsk`) & Disc Control** | [`libretro-core.cpp`](ti99sim/src/libretro/libretro-core.cpp), [`ti994a-retro.cpp`](ti99sim/src/libretro/ti994a-retro.cpp) | Pending |

---

## 6. Reviewer Notes & On-Device Findings (Onion OS v4.3)

### 6.1 Clarification on `ra32.ss` vs `retroarch` (Section 2.1)
- **What we found on the actual Miyoo Mini running Onion OS v4.3:**
  - `/mnt/SDCARD/RetroArch/ra32.ss` **does not exist** on modern Onion OS v4.2/v4.3+. `ra32.ss` was used on stock Miyoo firmware and older Onion OS v3 builds.
  - On Onion OS v4.3, `/mnt/SDCARD/RetroArch/retroarch` *is* Onion's custom-patched RetroArch binary with GameSwitcher, `Menu` button hooks, and automatic `.state.auto` save/restore compiled directly into the binary. All official `Emu/*/launch.sh` scripts on Onion OS v4.3 invoke `/mnt/SDCARD/RetroArch/retroarch` directly.
  - **Why the original `launch.sh` crashed on device:**
    1. It set `progdir=$(dirname "$0")` (which evaluated to `.` when invoked from `/mnt/SDCARD/Emu/TI99`), then ran `cd /mnt/SDCARD/RetroArch/`, and then invoked `$progdir/../../RetroArch/ra32.ss`—which resolved to `/mnt/RetroArch/ra32.ss` (`not found`).
    2. Even with an absolute path, `/mnt/SDCARD/RetroArch/ra32.ss` is absent on Onion OS v4.3.
- **Verdict on Section 2.1's `if [ -f "$RA_DIR/ra32.ss" ]` snippet:**
  - Although `ra32.ss` is legacy rather than modern Onion OS, the proposed `if [ -f "$RA_DIR/ra32.ss" ] ... else ...` check using absolute paths (`RA_DIR=/mnt/SDCARD/RetroArch`) is still a great compatibility improvement: it runs `/mnt/SDCARD/RetroArch/retroarch` on Onion OS v4.3 while remaining backward-compatible with stock Miyoo firmware and Onion v3.

### 6.2 Agreement on Sections 2.2, 2.3, 3, and 4
- **Section 2.2 & 2.3 (OSK & Button Docs Mismatch):** Confirmed. `README.md` and `ti99sim_libretro.info` still describe an earlier draft of the button layout (`B=Fire, A=Space, Y=1, X=2`) and OSK controls (`A=Tap+Close, B=Hold, X=Backspace`), whereas `osk-retro.cpp` and `libretro-core.cpp` implement `A=Fire/Press, B=1/Close OSK, Y=2/Space, X=3/FCTN`. Note also that `DetectGamePreset()` in `libretro-core.cpp` was just updated so *Parsec* maps `B=1 (Lift 1), Y=2 (Lift 2), X=3 (Lift 3)` to match `SlotDefaultValues` and keep `L1` bound to `FCTN_REDO`.
- **Section 3.1 (`.zip` / `.7z` Archive Support):** Confirmed. Since `need_fullpath = true` and `block_extract = false`, adding `zip|7z` to `valid_extensions` (`libretro-core.cpp`), `supported_extensions` (`ti99sim_libretro.info`), and `extlist` (`config.json`) will let RetroArch automatically extract compressed `.ctg` archives to `/tmp`.
- **Sections 3.2, 3.3, 4.1–4.3:** All agreed and accurate.

### 6.3 Recent Runtime Fixes Already Landed in `ti99sim/`
For context when editing `ti99sim/`:
1. **TMS9901 Level-Triggered Interrupt Masking (`ti99sim/src/core/tms9901.cpp`):** Fixed a critical bug in `cTMS9901::WriteCRU` where unmasking an interrupt pin (`SBO 2`) while its interrupt line was already asserted (`m_PinState[2][0] == -1`) failed to increment `m_ActiveInterrupts` or signal `INT1` to the CPU, permanently wedging VDP interrupts and hanging games on a solid cyan screen after menu selection.
2. **Savestate Scratchpad Restoration (`ti99sim/src/libretro/ti994a-retro.cpp`):** Fixed `cRetroTI994A::Unserialize` so `cpuMemory.SetMemory( 0x8000..0x8300, m_Scratchpad, false )` and `0x9000` are mapped *after* `UpdateMemory( MASK_CONSOLE_ALL )`, and bumped `SAVESTATE_VERSION` to `2`.
3. **Makefile Dependency Fix (`ti99sim/src/libretro/Makefile`):** Fixed `$( filter %.a, $(LIBS))` whitespace so changes to `../core/Release/ti-core.a` automatically relink `ti99sim_libretro`.

