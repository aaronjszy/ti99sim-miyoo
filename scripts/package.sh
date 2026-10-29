#!/bin/sh
# Assemble dist/SDCARD: a directory tree that mirrors the Miyoo's SD card (copy it onto the card, or `make install`).
# Needs build/ti99sim-sdl (make build) and deps/lib (make deps).
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/dist/SDCARD
APP=$OUT/Emu/TI99

[ -f "$ROOT/build/ti99sim-sdl" ] || { echo "package: build/ti99sim-sdl is missing; run 'make build'" >&2; exit 1; }
[ -d "$ROOT/deps/lib" ]          || { echo "package: deps/lib is missing; run 'make deps'" >&2; exit 1; }

rm -rf "$ROOT/dist"
mkdir -p "$OUT"
cp -R "$ROOT/miyoo/SDCARD/." "$OUT/"

cp "$ROOT/build/ti99sim-sdl" "$APP/"
cp "$ROOT"/deps/lib/* "$APP/"
chmod +x "$APP/ti99sim-sdl" "$APP/launch.sh"

# Folders the emulator and Onion's Games menu look in; the TI ROMs themselves are not included (see README.md)
mkdir -p "$APP/console" "$APP/cartridges" "$APP/disks" "$OUT/Roms/TI99"
cat > "$APP/console/README.txt" <<'EOF'
Put the TI-99/4A console ROM cartridge here as TI-994A.ctg (see the project README, "TI ROMs").
Optional: ti-disk.ctg (disk controller) and spchrom.bin (speech ROM).
EOF

echo "package: $OUT"
find "$OUT" -type f | sed "s|^$ROOT/||" | sort
