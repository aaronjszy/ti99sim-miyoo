# Native Libretro Core Architecture & Implementation Plan (`ti99sim_libretro`)

**Branch:** `feat/libretro-core`  
**Target Artifact:** `ti99sim_libretro.so` (Linux / Miyoo Mini Onion OS) & `ti99sim_libretro.dylib` (macOS host dev)  
**License:** `GPL-2.0-or-later` (with MIT-licensed official `libretro.h` API header)

---

## 1. Executive Summary & Why Libretro Solves Current Limitations

Porting `ti99sim` from a standalone SDL2 application (`ti99sim-sdl`) to a native Libretro core (`ti99sim_libretro`) transforms the emulator from a Miyoo-specific binary with platform workarounds into a portable, first-class RetroArch core.

Crucially, moving to Libretro directly resolves **every Known Issue** documented in [`README.md`](README.md):

| Current Standalone Issue (`ti99sim-sdl`) | How `ti99sim_libretro` Solves It |
|---|---|
| **~500 ms audio latency** caused by `steward-fu/sdl2` queuing multiple `mi_ao` buffers (and muting if buffer `< 11 ms`). | Core generates exact per-frame stereo PCM samples synchronously via `retro_audio_sample_batch_cb`; RetroArch's native `mi_ao` / OSS audio driver handles low-latency buffering and dynamic rate control. |
| **Menu button quits**; no Onion OS **GameSwitcher** (freeze-and-switch, auto-save on exit, auto-resume on launch). | Launching through Onion OS's `ra32.ss` RetroArch binary gives full **GameSwitcher**, auto-savestates, play-time tracking, and screenshots automatically. |
| **Uneven pixel widths** from 2.5× CPU nearest-neighbor scaling (`TI_SOFT_SCALE=640x480`) to work around SDL2 blur. | Core outputs a clean native `256×192` (or bordered `288×216`) `RGB565` buffer; RetroArch provides hardware/NEON scaling, integer scale, aspect control, overlays, and sharp-bilinear/CRT shaders. |
| **No on-screen keyboard**, making TI BASIC, text adventures, and multi-key prompts impractical on handhelds. | Core includes a **built-in framebuffer Virtual Keyboard Overlay** (for handhelds) plus `RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK` (for physical/desktop keyboards). |
| **External SDL2 binary dependency** (`deps/` download of `steward-fu/sdl2` `.so` blobs). | Zero external library downloads needed; `ti99sim_libretro.so` compiles self-contained inside the Onion toolchain container using only vendored `libretro.h`. |

---

## 2. Technical Audit: Codebase Realities & Pitfalls to Mitigate

A source-level audit of `ti99sim/` revealed **eight critical implementation details** that would break a naive port if not addressed upfront:

### 2.1 VDP Rasterizer & PSG Synthesizer Live in `src/sdl/`, Not `src/core/`
- **Finding:** [`ti99sim/src/core/tms9918a.cpp`](ti99sim/src/core/tms9918a.cpp) and [`ti99sim/src/core/tms9919.cpp`](ti99sim/src/core/tms9919.cpp) only implement register state, VRAM addressing, and sprite coincidence flags—**neither file renders pixels or synthesizes audio waveforms**. All tile/bitmap/multicolor/text/sprite rendering (~600 lines) lives in [`ti99sim/src/sdl/tms9918a-sdl.cpp`](ti99sim/src/sdl/tms9918a-sdl.cpp), and all 3-tone + LFSR periodic/white noise synthesis lives in [`ti99sim/src/sdl/tms9919-sdl.cpp`](ti99sim/src/sdl/tms9919-sdl.cpp).
- **Mitigation:** Implement `cRetroTMS9918A` and `cRetroTMS9919` in `ti99sim/src/libretro/` by adapting the pure C++ dirty-tile/sprite rasterizer and voice/LFSR synthesizer directly into SDL-free classes writing to flat `uint16_t` (`RGB565`) and `int16_t` (`PCM`) buffers.

### 2.2 Global Mutable State & Dynamic Linker Symbol Collisions
- **Finding:** `ti-core.a` relies heavily on non-namespaced process globals across [`opcodes.cpp`](ti99sim/src/core/opcodes.cpp), [`tms9900.cpp`](ti99sim/src/core/tms9900.cpp), [`tms9901.cpp`](ti99sim/src/core/tms9901.cpp), and [`ti994a.cpp`](ti99sim/src/core/ti994a.cpp):
  - Global objects/arrays: `cpuMemory`, `gplMemory`, `MemFlags[0x10000]`, `MemTrapIndex[0x10000]`, `TrapList[16]`.
  - Global registers/pointers: `WorkspacePtr`, `ProgramCounter`, `Status`, `InterruptFlag`, `InstructionCounter`, `ClockCycleCounter`, `runFlag`, `stopFlag`, `CRU_Object`, `pic`, `pTimerObj`, `TimerHook`.
  - Generic global function names: `Run()`, `Stop()`, `Step()`, `sha1()`, `ReadCRU()`, `WriteCRU()`.
- **Mitigation:**
  1. **Symbol Visibility Isolation:** Compile with `-fvisibility=hidden` and link `ti99sim_libretro.so` with a linker version script (`link.T`) exporting **only** `retro_*` symbols. This prevents `dlopen()` symbol collisions with RetroArch and allows `--gc-sections` to strip unused code from `-static-libstdc++`.
  2. **Deterministic Unload/Reload Cleanup:** In `retro_unload_game()`, explicitly destroy the `cRetroTI994A` instance and reset all core globals (`cpuMemory`, `gplMemory`, `MemFlags`, `TrapList`, `InterruptFlag = 0`, `runFlag = 0`, `stopFlag = 0`, `TimerHook = nullptr`, `CRU_Object = nullptr`, `pic = nullptr`) so switching games or restarting within the same RetroArch session is 100% clean.

### 2.3 BIOS / System Path Resolution Trap (`support.cpp`)
- **Finding:** [`GetCommonPath()`](ti99sim/src/core/support.cpp) caches `/proc/self/exe` in a function-local `static` variable, and [`LocateFiles()`](ti99sim/src/core/support.cpp) caches its search root array on the first call. Inside RetroArch, `/proc/self/exe` resolves to `/mnt/SDCARD/RetroArch/retroarch`, causing `cTI994A` (`TI-994A.ctg`), `cTMS5220` (`spchrom.bin`), `LoadDevices` (`ti-disk.ctg`), and `ParseState` to miss RetroArch's `system_directory` (`/mnt/SDCARD/BIOS`).
- **Mitigation:** Add a `SetLibretroPaths(const std::filesystem::path &systemDir, const std::filesystem::path &saveDir)` hook in [`support.hpp`](ti99sim/include/support.hpp) / [`support.cpp`](ti99sim/src/core/support.cpp) (clearing any cached path tables) so `LocateFile` and `LocateFiles` search, in priority order:
  1. `<retro_system_dir>/ti99sim/` (clean per-core subfolder, e.g., `BIOS/ti99sim/TI-994A.ctg`)
  2. `<retro_system_dir>/console/` and `<retro_system_dir>/` directly (e.g., `BIOS/TI-994A.ctg`, `BIOS/spchrom.bin`)
  3. `/mnt/SDCARD/Emu/TI99/` (zero-config backward compatibility for existing `ti99sim-miyoo` installations keeping ROMs in `Emu/TI99/console/`)

### 2.4 `TimerHook` 256-Instruction Granularity & Double-Retrace Hazard
- **Finding:** In [`opcodes.cpp`](ti99sim/src/core/opcodes.cpp), `ExecuteInstruction()` only invokes `TimerHook()` when `(char)InstructionCounter == 0` (every 256 instructions $\approx$ 3,500–5,000 clock cycles). Furthermore, `cTI994A`'s constructor automatically sets `TimerHook = _TimerHookProc`, which fires `VideoRetrace()` whenever `clockCycles - m_LastRetrace > m_RetraceInterval`. If `retro_run()` also calls `VideoRetrace()`, VDP interrupts fire **twice per frame** (2× game speed).
- **Mitigation:** Set `TimerHook = nullptr` (and override `TimerHookProc` as a no-op in `cRetroTI994A`). Drive CPU execution deterministically in `cRetroTI994A::RunFrame()` using exact cycle budgets ($3\,000\,000 / 60 = 50\,000$ cycles NTSC; $3\,000\,000 / 50 = 60\,000$ cycles PAL) while updating `m_LastRetrace` explicitly so savestates remain compatible.

### 2.5 Mono-to-Stereo Conversion, Sub-Frame Audio & Speech Spin-Loops
- **Finding:**
  1. Libretro's `retro_audio_sample_batch_cb` **strictly expects interleaved 16-bit stereo (`L, R`)** frames, whereas `cTMS9919` and `cTMS5220::AudioCallback` generate mono `INT16` samples.
  2. Rendering all audio only once at the end of a frame drops mid-frame PSG writes and penalizes games (e.g., *Parsec*, *Alpiner*) that poll `SpeechReadBreakPoint` (`TMS5220_BL` Buffer Low) in a tight CPU loop during `Speak External`.
  3. Upstream [`cTMS9919::ParseState`](ti99sim/src/core/tms9919.cpp) restores `m_Frequency` and `m_Attenuation` without recalculating the active voice period/volume table, and returns `false`.
- **Mitigation:**
  - Use **44,100 Hz** audio rate: divides evenly into both **60 Hz** ($735$ samples/frame) and **50 Hz** ($882$ samples/frame) with zero fractional drift.
  - Implement **cycle-stamped catch-up rendering** in `cRetroTMS9919::WriteData()` plus **sub-frame audio/speech slicing** (5 slices per frame = 10,000 cycles / 147 samples per slice in NTSC) so `cTMS5220` drains its FIFO progressively during the frame, then expand mono to stereo `[s, s]` at frame end.
  - Fix `cRetroTMS9919::ParseState` to rebuild voice `period` and `setting` values immediately after state load.

### 2.6 Savestate Sizing, Determinism & Heap Overhead (`stateobject.cpp`)
- **Finding:** `cTI994A::SaveState()` builds an `sStateSection` tree of `std::map<std::string, std::string>` hex dumps (~3,500 heap node allocations per call, variable ASCII output size ~60–280 KB). However, Libretro requires `retro_serialize_size()` to return a **constant upper-bound size** across frames, and RetroArch features like **Rewind** and **Runahead** call `retro_serialize()` 60–120 times per second.
- **Mitigation (Two-Stage Serialization Architecture):**
  - **Fast Binary Serializer (Primary for Libretro):** The entire mutable state of the TI-99/4A—CPU registers (`20 B`), TMS9901 (`128 B`), TMS9918A registers + 16 KB VRAM (`~16.5 KB`), TMS9919 + LFSR (`128 B`), TMS5220 FIFO + LPC filter history (`1 KB`), 256 B Scratchpad + GROM state (`300 B`), 32 KB Expansion RAM (`36 KB`), and Cartridge bank indices + optional Cartridge RAM (`0–32 KB`)—fits in a **fixed ~64–96 KB flat binary struct** with a magic/version header. Serializing takes `< 50 µs` with **zero heap allocations**, enabling smooth **GameSwitcher**, **Rewind**, and **1-frame Runahead** even on the Miyoo Mini's Cortex-A7.
  - When peripheral DSR devices (`ti-disk.ctg` / `cf7+.ctg`) are active, append a fixed-size reserved block for peripheral state.

### 2.7 The 1981 TI-99/4A Hardware "Alpha Lock + Joystick Up" Bug
- **Finding:** [`cTMS9901::ReadCRU`](ti99sim/src/core/tms9901.cpp) (line 312) faithfully emulates the original TI-99/4A console's missing keyboard diode: when `VK_CAPSLOCK` (Alpha Lock) is active and `m_CapsLock == false`, scanning `address == 7` forces `retVal = 0` **even when `m_ColumnSelect` is `6` or `7` (Joystick 1/2)**, locking Joystick Up! Conversely, leaving Alpha Lock OFF causes letter bindings (`Y`, `N`, `P`) to emit lowercase ASCII, which early TI software ignores.
- **Mitigation:** Add the classic **"Alpha Lock Diode Fix"** (`m_ColumnSelect < 6` guard on line 312 of `tms9901.cpp`, controlled by a Core Option enabled by default) and initialize `VK_CAPSLOCK` **ON**. Both uppercase key prompts (`Y`/`N`, TI BASIC) and Joystick Up then work simultaneously out of the box.

### 2.8 Do Not Rely Solely on RetroArch's Frontend OSK
- **Finding:** Onion OS's `ra32.ss` build uses a minimal RGUI setup without touch/Ozone virtual keyboards.
- **Mitigation:** Render a lightweight **In-Core Virtual Keyboard Overlay** directly onto a copy of the framebuffer when toggled on (`Select` or dedicated hotkey), matching established home-computer Libretro cores (`cap32`, `vice`, `atari800`, `freeintv`).

---

## 3. Libretro Core Specification & API Design

Designed in accordance with the [Libretro Core Development Guides](https://docs.libretro.com/development/cores/developing-cores/) and [`libretro.h`](https://github.com/libretro/RetroArch/blob/master/libretro-common/include/libretro.h).

### 3.1 System Metadata (`retro_get_system_info`)
- `library_name`: `"TI-99/Sim"`
- `library_version`: `"0.16.0-miyoo"`
- `valid_extensions`: `"ctg"` (Phase 6 adds `"dsk|dtk|hfe|cf7"` once disk drive mounting is wired)
- `need_fullpath`: `true` (`cCartridge` reads `.ctg` via file path and uses the filename stem for sidecar `<game>.ini` lookup; keeping `block_extract = false` means RetroArch still transparently extracts `.zip`/`.7z` archives to `/tmp` before passing `info->path`).
- **Boot to TI BASIC (`RETRO_ENVIRONMENT_SET_SUPPORT_NO_GAME`):** Set to `true` in `retro_set_environment()` so users can launch the core without a cartridge to enter TI BASIC.

### 3.2 Audio/Video Geometry & Timing (`retro_get_system_av_info`)
- **Pixel Format:** Request `RETRO_PIXEL_FORMAT_RGB565` in `retro_load_game()` via `RETRO_ENVIRONMENT_SET_PIXEL_FORMAT` (optimal 16-bit fast path for Miyoo Mini's `mi_gfx` / ARMv7 driver, 98 KB/frame), with fallback or Core Option for `RETRO_PIXEL_FORMAT_XRGB8888`.
- **Display Geometry (`struct retro_game_geometry`):**
  - **Cropped Mode (Default):** `base_width = 256`, `base_height = 192`
  - **Overscan Border Mode (Core Option):** `base_width = 288`, `base_height = 216` (16 px horizontal, 12 px vertical border rendered in VDP Register 7 backdrop color so border flashes in *Parsec* / *TI Invaders* are visible while preserving exact 4:3)
  - `max_width = 288`, `max_height = 216`, `aspect_ratio = 4.0f / 3.0f`
  - Dynamic switching handled at runtime via `RETRO_ENVIRONMENT_SET_GEOMETRY`.
- **Timing (`struct retro_system_timing`):**
  - `fps = 60.0` (NTSC default) or `50.0` (PAL Core Option; triggers `RETRO_ENVIRONMENT_SET_SYSTEM_AV_INFO` if changed).
  - `sample_rate = 44100.0` (exact $735$ stereo frames @ 60 Hz; $882$ stereo frames @ 50 Hz).

### 3.3 Low-Latency Frame Execution Schedule (`retro_run`)
On real TI-99/4A hardware, keyboard/joystick CRU scanning typically runs inside the VDP Vertical Blank ISR (`INT 2`). Ordering `retro_run()` around the active-display-to-VBlank transition eliminates a full frame (16.7 ms) of input lag:

```
retro_run() [50,000 CPU cycles @ 3.0 MHz NTSC]:
  1. Check RETRO_ENVIRONMENT_GET_VARIABLE_UPDATE -> apply changed Core Options
  2. input_poll_cb() -> update TMS9901 Joystick + Key Matrix (or route to Virtual Keyboard if active)
  3. Active Scanlines (~36,600 cycles across 4 slices; flush PSG/TMS5220 audio after each slice)
  4. VideoRetrace() -> latch sprite coincidence/5th-sprite, render 256x192/288x216 RGB565 frame, assert VDP INT
  5. VBlank Period (~13,400 cycles, 1 slice) -> CPU immediately services VBlank ISR with fresh input!
  6. Composite In-Core Virtual Keyboard Overlay (if toggled active) onto output buffer
  7. video_cb(frameBuffer, width, height, pitch)
  8. audio_batch_cb(stereoSamples, 735 /* or 882 PAL */)
```

### 3.4 Input Architecture: RetroPad, Presets, OSK & Physical Keyboard
1. **Controller Ports & Input Descriptors:**
   - Declare `RETRO_DEVICE_JOYPAD` (and `RETRO_DEVICE_NONE`) for Ports 1 & 2 via `RETRO_ENVIRONMENT_SET_CONTROLLER_INFO`.
   - Register human-readable labels via `RETRO_ENVIRONMENT_SET_INPUT_DESCRIPTORS` so RetroArch's *Quick Menu → Controls* shows meaningful names (`Fire / Action`, `Key 1`, `Key 2`, `Yes (Y)`, `Space`, `No (N)`, `Back (FCTN+9)`, `Redo (FCTN+8)`, `Enter`, `Virtual Keyboard`).
   - Port over the **Last-Input-Wins SOCD D-pad resolver** (just committed in `22c7069`) driving Joystick 1/2 axes + optional TI cursor keys (`FCTN+E/X/S/D`).
2. **Default RetroPad Layout (Matches Miyoo Mini Physical Controls):**
   - `D-Pad` / `Left Analog`: Joystick Directions (+ optional Arrow Keys)
   - `A` (East): Joystick Fire
   - `B` (South): `1` (Menu selection 1 / Lift 1)
   - `X` (North): `2` (Start Cartridge from Master Title Screen / Lift 2)
   - `Y` (West): `Y` ("Yes" to prompts; auto-switches to `3` / Lift 3 when *Parsec* preset is active)
   - `L1` / `R1`: `SPACE` / `N` (`Y` / `N` when *Parsec* preset is active)
   - `L2` / `R2`: `FCTN+9` (BACK) / `FCTN+8` (REDO)
   - `Start`: `ENTER`
   - `Select`: **Toggle Virtual Keyboard Overlay** (with `P` easily accessible on OSK or remappable via Core Options; note `Menu` is reserved by RetroArch / Onion OS)
3. **Three Ways to Customize Controls Per Game:**
   - **Built-in Smart Game Presets (Auto-detect by Cartridge SHA-1 / Title):** E.g., automatically maps `B/X/Y` to `1/2/3` and `L1/R1` to `Y/N` when *Parsec* is loaded (toggleable via Core Option).
   - **Core Options v2 Button Mapping Category:** Every RetroPad button (`A, B, X, Y, L1, R1, L2, R2, Start, Select`) can be bound to `Fire`, `OSK Toggle`, or any TI key/chord (`0–9`, `A–Z`, `SPACE`, `ENTER`, `FCTN+4 Clear`, `FCTN+7 Aid`, `FCTN+8 Redo`, `FCTN+9 Back`, `FCTN+= Quit`), savable per-game via RetroArch's native *Save Game Options* (`<game>.opt`).
   - **Legacy Sidecar `<game>.ini` Support:** Optionally honor `<rom_dir>/<game>.ini` if present for backward compatibility with existing `ti99sim-miyoo` setups.
4. **In-Core Virtual Keyboard Overlay (OSK):**
   - Compact translucent 5-row TI-99/4A 48-key layout drawn over the bottom half (or top, movable) of the framebuffer.
   - Sticky modifier indicators for `FCTN`, `SHIFT`, `CTRL`, and `ALPHA`.
   - While OSK is open: `D-Pad` moves cursor, `A` taps key, `B` presses `ENTER`, `X` toggles sticky `FCTN`, `Y` presses `SPACE`, `Select` closes OSK.
5. **Hardware Keyboard Passthrough (`RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK`):**
   - Translates `retro_key` (`RETROK_*`) events directly into `cTMS9901` virtual keys for desktop/Bluetooth keyboard users (recommended alongside RetroArch's *Game Focus* toggle `Scroll Lock`).

### 3.5 Core Options v2 Specification (`libretro_core_options.h`)
Implement **Core Options v2** (`RETRO_ENVIRONMENT_GET_CORE_OPTIONS_VERSION` $\ge 2$ via `RETRO_ENVIRONMENT_SET_CORE_OPTIONS_V2`) with automatic v1/v0 fallback (`RETRO_ENVIRONMENT_SET_VARIABLES`):

| Category | Option Key | Values (**Default** in bold) | Notes |
|---|---|---|---|
| **System** | `ti99sim_region` | **`NTSC (60Hz)`**, `PAL (50Hz)` | Updates VDP refresh & AV timing |
| **System** | `ti99sim_ram_32k` | **`Enabled`**, `Disabled` | Toggles console 32K RAM banks (`0x2000`, `0xA000`) |
| **System** | `ti99sim_speech` | **`Enabled`**, `Disabled` | Enables TMS5220 Speech Synthesizer (`spchrom.bin`) |
| **System** | `ti99sim_alphalock_fix` | **`Enabled`**, `Disabled` | Virtual diode mod preventing Alpha Lock from sticking Joystick Up |
| **Video** | `ti99sim_palette` | **`Default`**, `NTSC Tuned`, ` vibrant RGB`, `Monochrome` | Maps to the 3 built-in `ColorTable` palettes + BW |
| **Video** | `ti99sim_borders` | **`Cropped (256x192)`**, `Overscan (288x216)` | Calls `RETRO_ENVIRONMENT_SET_GEOMETRY` on change |
| **Input** | `ti99sim_dpad_cursor` | **`Joystick + Arrows`**, `Joystick Only` | Whether D-pad also presses `FCTN+E/X/S/D` |
| **Input** | `ti99sim_auto_preset` | **`Enabled`**, `Disabled` | Auto-applies built-in per-cartridge button presets (e.g. *Parsec*) |
| **Input** | `ti99sim_pad_a` … `pad_select` | TI Key / Chord / Fire / OSK list | Per-button TI key binding overrides |

### 3.6 Logging & Error Reporting
- Query `RETRO_ENVIRONMENT_GET_LOG_INTERFACE` in `retro_set_environment()` and route core diagnostics through `retro_log_printf_t` instead of raw `stdout`/`stderr`.
- If `TI-994A.ctg` is missing from `BIOS/` (`system_directory`), push a clear on-screen OSD alert via `RETRO_ENVIRONMENT_SET_MESSAGE_EXT` (`"Missing BIOS: Place TI-994A.ctg in RetroArch system/BIOS folder"`) before returning `false` from `retro_load_game()`.

### 3.7 Core Info File (`ti99sim_libretro.info`)
Ship a standard Libretro `.info` metadata file declaring system details and BIOS checksums so RetroArch's *Core Information* menu validates user firmware:
- `firmware0_desc = "TI-994A.ctg (TI-99/4A Console ROM + GROMs)"` (`firmware0_opt = "false"`)
- `firmware1_desc = "spchrom.bin (TMS5220 Speech Synthesizer ROM)"` (`firmware1_opt = "true"`)
- `firmware2_desc = "ti-disk.ctg (PHP1240 Disk Controller ROM)"` (`firmware2_opt = "true"`)

---

## 4. Repository Layout, Build System & Onion OS Integration

### 4.1 Planned Source & Packaging Structure
`src/sdl/` remains intact so standalone builds stay functional as a reference oracle, while `src/libretro/` houses the new core:

```
├── LIBRETRO_PLAN.md                        # This architectural plan
├── Makefile                                # Top-level targets: build, build-host, build-sdl, package, zip, deploy, patch
├── miyoo/
│   └── SDCARD/
│       ├── BIOS/
│       │   └── ti99sim/README.txt          # Instructions for placing TI-994A.ctg & spchrom.bin
│       ├── RetroArch/.retroarch/cores/
│       │   └── ti99sim_libretro.info       # Libretro core metadata & firmware manifest
│       └── Emu/TI99/
│           ├── config.json                 # Onion Games menu config (RetroArch-compatible)
│           ├── icon.png                    # Existing TI-99/4A menu icon
│           └── launch.sh                   # Invokes Onion's ra32.ss with ti99sim_libretro.so
├── scripts/
│   ├── build-in-container.sh               # Builds build/ti99sim_libretro.so inside Onion toolchain container
│   └── package.sh                          # Stages dist/SDCARD/{RetroArch,Emu,Roms,BIOS}
└── ti99sim/
    ├── include/
    │   └── libretro.h                      # Official Libretro API header (MIT)
    └── src/
        ├── core/                           # Shared ti-core.a (minimal hooks for paths & diode fix)
        ├── sdl/                            # Legacy standalone SDL2 frontend (kept working)
        └── libretro/                       # NEW: Native Libretro frontend
            ├── Makefile                    # Builds ti99sim_libretro.so / .dylib
            ├── link.T                      # Linker version script exporting only retro_*
            ├── libretro-core.cpp           # Libretro C ABI entrypoints & lifecycle
            ├── libretro-core-options.hpp   # Core Options v2 + v1 fallback definitions
            ├── ti994a-retro.hpp/.cpp       # Synchronous cycle-sliced cTI994A subclass + binary savestates
            ├── tms9918a-retro.hpp/.cpp     # RGB565 / XRGB8888 VDP rasterizer + border support
            ├── tms9919-retro.hpp/.cpp      # Cycle-stamped 44.1 kHz stereo PSG + TMS5220 mixer
            └── osk-retro.hpp/.cpp          # Built-in 48-key TI-99/4A framebuffer virtual keyboard
```

### 4.2 Onion OS Launcher & Configuration (`Emu/TI99/`)
Following [Onion OS Emulator Configuration Guidelines](https://onionui.github.io/docs/emulators/folders):

1. **`miyoo/SDCARD/Emu/TI99/config.json`:**
   - Remove `"noaudiofix": 1` (which was only required when `launch.sh` killed Onion's `audioserver` for standalone SDL2) so Onion's standard RetroArch audio/volume integration works natively.
   - Keep `"rompath": "../../Roms/TI99"`, `"imgpath": "../../Roms/TI99/Imgs"`, `"extlist": "ctg"`.
2. **`miyoo/SDCARD/Emu/TI99/launch.sh`:**
   ```sh
   #!/bin/sh
   echo $0 $*
   RA_DIR=/mnt/SDCARD/RetroArch
   cd "$RA_DIR"
   HOME="$RA_DIR/" "$RA_DIR/ra32.ss" -v -L "$RA_DIR/.retroarch/cores/ti99sim_libretro.so" "$1"
   ```
   *(Using LF line endings strictly; falls back to `./retroarch` if `ra32.ss` is absent on non-Onion setups.)*

### 4.3 Compiler & Linker Flags (Miyoo Mini ARMv7)
- `CXXFLAGS`: `-std=c++2a -O3 -mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -fPIC -fvisibility=hidden -ffunction-sections -fdata-sections -DTI_NO_OPENSSL -include cstring`
- `LDFLAGS`: `-shared -Wl,--version-script=link.T -Wl,--gc-sections -Wl,--no-undefined -static-libstdc++ -static-libgcc` (links `-lstdc++fs -lm -lpthread` statically for C++17/20 `std::filesystem` symbols without relying on the Miyoo rootfs's older `/lib/libstdc++.so.6`).

---

## 5. Phased Implementation Roadmap

### Phase 1: Core Scaffolding, Build Harness & Path Hook
- **Scope:**
  - Vendor official `libretro.h` into `ti99sim/include/libretro.h`.
  - Add `SetLibretroPaths()` search-root hook in `ti99sim/src/core/support.cpp` and clean global reset helper for `retro_unload_game()`.
  - Create `ti99sim/src/libretro/` with `Makefile`, `link.T`, stub `libretro-core.cpp`, and `ti99sim_libretro.info`.
  - Add `make build-host` (native macOS/Linux `.dylib`/`.so` for rapid desktop testing) and update Docker cross-compilation to output `build/ti99sim_libretro.so`.
- **Verification:**
  - `nm -gU` / `readelf -Ws` confirms **only** `retro_*` symbols are exported.
  - RetroArch loads the core metadata (`retroarch --features` / Core Information menu) on host and container build succeeds cleanly.

### Phase 2: Frame-Driven Machine (`cRetroTI994A`) & Video Rasterizer (`cRetroTMS9918A`)
- **Scope:**
  - Implement `cRetroTMS9918A` (`RGB565` & `XRGB8888`, `256×192` cropped and `288×216` overscan border modes, dirty-tile & sprite composition, palette selection).
  - Implement `cRetroTI994A` with `TimerHook` disabled and deterministic 5-slice frame stepping (Active Scan $\rightarrow$ `VideoRetrace()` $\rightarrow$ VBlank ISR).
  - Wire `retro_load_game()` for `.ctg` cartridges + `SUPPORT_NO_GAME` (boot to TI Title Screen / TI BASIC) + OSD message if `TI-994A.ctg` is missing.
- **Verification:**
  - Boot `TI-994A.ctg` to color title bar and Master Selection screen at 60.0 fps; boot *Parsec* `.ctg` with crisp `RGB565` graphics and zero double-retrace speed bugs.

### Phase 3: Cycle-Synced Stereo Audio (`cRetroTMS9919`) & TMS5220 Speech
- **Scope:**
  - Implement `cRetroTMS9919` at 44,100 Hz with cycle-stamped register write catch-up, 5-slice sub-frame generation, `cTMS5220` speech mixing, and mono-to-stereo interleaving (`735` frames NTSC / `882` frames PAL).
  - Fix `ParseState` voice recalculation in `cRetroTMS9919`.
  - Verify `spchrom.bin` discovery from `<system_dir>/` and legacy `Emu/TI99/console/`.
- **Verification:**
  - Console power-on beep, key clicks, *Parsec* sound effects, and *Parsec* TMS5220 speech phrases play clearly with zero 500 ms lag, crackle, or `Speak External` CPU stalls.

### Phase 4: Input System, Alpha Lock Diode Fix & Core Options v2
- **Scope:**
  - Add virtual Alpha Lock Diode Fix (`m_ColumnSelect < 6`) in `tms9901.cpp` and enable `VK_CAPSLOCK` on boot.
  - Implement `libretro-core-options.hpp` (Core Options v2 with v1 fallback).
  - Implement RetroPad polling, SOCD D-pad resolution, input descriptors, built-in SHA-1/title game presets (*Parsec*), sidecar `<game>.ini` loader, and `RETRO_ENVIRONMENT_SET_KEYBOARD_CALLBACK`.
- **Verification:**
  - Navigate TI boot menu (`Any Key` $\rightarrow$ `2`), play *Parsec* with Lift `1/2/3`, fire, refuel, and answer `Y/N` prompts with Alpha Lock ON and Joystick Up functioning simultaneously.

### Phase 5: In-Core Virtual Keyboard Overlay, Binary Savestates & SRAM
- **Scope:**
  - Implement `osk-retro.cpp` (translucent 48-key TI-99/4A virtual keyboard overlay toggled by `Select`, with sticky `FCTN`/`SHIFT`/`CTRL`/`ALPHA` indicators and built-in 5×7 bitmap font).
  - Implement deterministic fixed-size binary serialization (`retro_serialize_size`, `retro_serialize`, `retro_unserialize`) covering CPU, PIC, VDP+VRAM, PSG+LFSR, TMS5220+LPC state, Scratchpad, 32K RAM, and Cartridge banks/RAM.
  - Wire `retro_get_memory_data(RETRO_MEMORY_SAVE_RAM)` for battery-backed cartridges (e.g., *Mini Memory*).
- **Verification:**
  - Enter and run a multi-line `10 PRINT "HELLO"` program in TI BASIC using only the Miyoo gamepad + OSK.
  - Pass RetroArch Savestate save/load, **Rewind** (60 fps continuous), and **1-frame Runahead** stress tests without audio pops, visual glitches, or desync.

### Phase 6: Onion OS Packaging, Device Deployment & Stretch Goals
- **Scope:**
  - Update `miyoo/SDCARD/`, `scripts/package.sh`, `Makefile`, and `README.md` to package `RetroArch/.retroarch/cores/ti99sim_libretro.{so,info}` and the `ra32.ss`-based `Emu/TI99/launch.sh` + `config.json`.
  - Run `make patch` to update [`upstream-modifications.patch`](upstream-modifications.patch) cleanly against upstream commit `e360854`.
  - **Stretch Goal (Follow-up PR):** Wire `cDiskDevice` (`ti-disk.ctg`, `DSK1`–`DSK3`) to `retro_load_game` (`.dsk`/`.hfe`) and `RETRO_ENVIRONMENT_SET_DISK_CONTROL_EXT_INTERFACE` for multi-disk swapping.
- **Verification:**
  - Deploy to physical Miyoo Mini (`make deploy`), verify Onion **Games → TI-99/4A** launch, verify **Menu button → GameSwitcher** suspend/resume, verify audio/video sync, and confirm zero regressions.

---

## 6. Authoritative References

- **Libretro Core Development Overview:** https://docs.libretro.com/development/cores/developing-cores/
- **Libretro Canonical API Header (`libretro.h`):** https://github.com/libretro/RetroArch/blob/master/libretro-common/include/libretro.h
- **Libretro Core Options v2 Specification & Template:** https://github.com/libretro/libretro-samples/tree/master/options/core_options_v2
- **Libretro Core Info (`.info`) Format & Firmware Keys:** https://docs.libretro.com/development/cores/core-info/
- **Onion OS Emulator Folder & `launch.sh` / `config.json` Spec:** https://onionui.github.io/docs/emulators/folders
- **Onion OS Combining Systems & Custom RetroArch Cores (`ra32.ss`):** https://onionui.github.io/docs/advanced/combining-systems
