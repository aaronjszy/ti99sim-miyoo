#!/bin/sh
echo $0 $*
RA_DIR=/mnt/SDCARD/RetroArch
cd "$RA_DIR"
if [ -f "$RA_DIR/ra32.ss" ]; then
    HOME="$RA_DIR/" "$RA_DIR/ra32.ss" -v -L "$RA_DIR/.retroarch/cores/ti99sim_libretro.so" "$1"
else
    HOME="$RA_DIR/" "$RA_DIR/retroarch" -v -L "$RA_DIR/.retroarch/cores/ti99sim_libretro.so" "$1"
fi
