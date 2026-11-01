#!/bin/sh
# Assemble dist/SDCARD: a directory tree that mirrors the Miyoo's SD card (copy it onto the card, or `make install`).
# Needs build/ti99sim_libretro.so (make build).
set -eu

ROOT=$(cd "$(dirname "$0")/.." && pwd)
OUT=$ROOT/dist/SDCARD
APP=$OUT/Emu/TI99
CORES=$OUT/RetroArch/.retroarch/cores
BIOS=$OUT/BIOS/ti99sim

[ -f "$ROOT/build/ti99sim_libretro.so" ] || { echo "package: build/ti99sim_libretro.so is missing; run 'make build'" >&2; exit 1; }

rm -rf "$ROOT/dist"
mkdir -p "$OUT" "$CORES" "$BIOS" "$APP/console" "$APP/disks" "$OUT/Roms/TI99"
cp -R "$ROOT/miyoo/SDCARD/." "$OUT/"

cp "$ROOT/build/ti99sim_libretro.so" "$CORES/ti99sim_libretro.so"
chmod +x "$APP/launch.sh"

# Folders the core and Onion's Games menu look in; the TI ROMs themselves are not included (see README.md)
cat > "$BIOS/README.txt" <<'EOF'
Put the TI-99/4A console ROM cartridge here as TI-994A.ctg (see the project README, "Required System ROMs").
Optional: spchrom.bin (speech ROM) and ti-disk.ctg (disk controller).
EOF

cat > "$APP/console/README.txt" <<'EOF'
Put the TI-99/4A console ROM cartridge in BIOS/ti99sim/ (recommended) or here as TI-994A.ctg.
Optional: ti-disk.ctg (disk controller) and spchrom.bin (speech ROM).
EOF

echo "package: $OUT"
find "$OUT" -type f | sed "s|^$ROOT/||" | sort
