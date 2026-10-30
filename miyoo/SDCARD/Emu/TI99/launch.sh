#!/bin/sh
# TI-99/4A launcher for Onion OS
cd "$(dirname "$0")"
export HOME="$PWD"
export LD_LIBRARY_PATH="$PWD:/config/lib:/customer/lib:$LD_LIBRARY_PATH"

# Display: the Miyoo's SDL2 renderer scales a small window itself (whole-number steps, centred, blurred),
# which leaves black borders and soft pixels. TI_SOFT_SCALE makes the emulator scale the 256x192 picture to
# 640x480 itself with nearest-neighbour, so the screen gets a 1:1 image (both are 4:3, so no distortion).
# At 2.5x some pixel columns/rows are 3 px wide and others 2 px; that is the cost of crisp pixels.
# Delete the next line to go back to the old 2x picture with black borders.
export TI_SOFT_SCALE=640x480

# Audio buffer size (TI_AUDIO_SAMPLES) is left at the emulator default of 512. Do NOT set it lower: the Miyoo's
# SDL2 audio driver sleeps (buffer ms - 10) * 1000 us after each buffer, which goes negative (about 71 minutes) for
# buffers under ~11 ms, so 128 gave silence. ti99sim.log has an "audio:" line showing what the driver granted.
# Sound lags the picture a little because the driver queues several buffers; that can't be changed from here.
#export TI_AUDIO_SAMPLES=512

# SDL2 audio (mi_ao) needs the device free; same approach as Onion's NDS/ScummVM launchers
. /mnt/SDCARD/.tmp_update/script/stop_audioserver.sh

{
  echo "== $(date)"
  ./ti99sim-sdl "$@" 2>&1
  echo "exit: $?"
} >> ti99sim.log 2>&1
