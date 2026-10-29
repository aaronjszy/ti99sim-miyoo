#!/bin/sh
# Copy the packaged tree onto a mounted Miyoo SD card. Usage: scripts/install.sh <packaged SDCARD dir> <card mount point>
# (the Makefile calls this: `make install SDCARD=/path/to/card`).
#  - Existing files of the same name are overwritten; nothing is deleted.
#  - An existing Emu/TI99/keymap.ini is kept, so button changes survive an upgrade.
#  - Refuses to run if the target does not look like an Onion OS card (no Emu/ folder).
set -eu

SRC=${1:?usage: install.sh <packaged SDCARD dir> <card mount point>}
DEST=${2:?usage: install.sh <packaged SDCARD dir> <card mount point>}

[ -d "$SRC/Emu/TI99" ] || { echo "install: $SRC is not a packaged tree; run 'make package'" >&2; exit 1; }
if [ ! -d "$DEST/Emu" ]; then
    echo "install: $DEST does not look like an Onion OS SD card (no Emu/ folder)" >&2
    exit 1
fi

# Move an existing keymap.ini aside on the card itself (no temp files), and put it back afterwards, even on failure
KEYMAP=$DEST/Emu/TI99/keymap.ini
KEPT=$KEYMAP.keep
restore() { [ -f "$KEPT" ] && mv -f "$KEPT" "$KEYMAP"; return 0; }
trap restore EXIT
if [ -f "$KEYMAP" ]; then
    mv -f "$KEYMAP" "$KEPT"
    echo "install: keeping your existing keymap.ini"
fi

(cd "$SRC" && tar -cf - .) | (cd "$DEST" && tar -xf -)

echo "install: copied to $DEST"
echo "Eject the card, then on the Miyoo: Games -> TI-99/4A (reboot once if the Games entry is missing)."
