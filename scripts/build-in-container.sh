#!/bin/bash
# Cross-compile ti99sim_libretro.so for the Miyoo Mini.
# Runs INSIDE the Onion toolchain container with the repository mounted at /work.
# Output: build/ti99sim_libretro.so (stripped).
set -euo pipefail

ROOT=/work
TC=/opt/miyoomini-toolchain/bin/arm-linux-gnueabihf-

export CXX=${TC}g++ CC=${TC}gcc AR=${TC}ar RANLIB=${TC}ranlib

# TI_NO_OPENSSL: use the built-in SHA-1 (there is no OpenSSL on the device)
# -include cstring: the toolchain's GCC 8 headers do not pull <cstring> in implicitly
export TI_NO_OPENSSL=1
export CFLAGS="-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -DTI_NO_OPENSSL -include cstring"
export LFLAGS="-static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined"

cd "$ROOT/ti99sim"
rm -rf bin src/core/Release src/libretro/Release

make -C src/core
make -C src/libretro

mkdir -p "$ROOT/build"
${TC}strip -o "$ROOT/build/ti99sim_libretro.so" bin/ti99sim_libretro.so
file "$ROOT/build/ti99sim_libretro.so"
${TC}nm -D --defined-only "$ROOT/build/ti99sim_libretro.so"
