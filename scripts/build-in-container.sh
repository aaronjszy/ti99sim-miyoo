#!/bin/bash
# Cross-compile ti99sim-sdl for the Miyoo Mini. Runs INSIDE the Onion toolchain container with the repository
# mounted at /work (the Makefile does this: `make build`). Output: build/ti99sim-sdl (stripped).
set -euo pipefail

ROOT=/work
TC=/opt/miyoomini-toolchain/bin/arm-linux-gnueabihf-
SDL2_INC=$ROOT/deps/steward-sdl2/sdl2/include
SDL2_LIB=$ROOT/deps/lib

# ti99sim's makefiles call sdl2-config; point them at the Miyoo SDL2 port instead
SHIM=$(mktemp -d)
trap 'rm -rf "$SHIM"' EXIT
cat > "$SHIM/sdl2-config" <<EOF
#!/bin/sh
for a in "\$@"; do
  case "\$a" in
    --cflags)  echo "-I$SDL2_INC -D_REENTRANT" ;;
    --libs)    echo "-L$SDL2_LIB -l:libSDL2-2.0.so.0 -Wl,-rpath-link,$SDL2_LIB" ;;
    --version) echo 2.0.18 ;;
  esac
done
EOF
chmod +x "$SHIM/sdl2-config"
export PATH="$SHIM:$PATH"

export CXX=${TC}g++ CC=${TC}gcc AR=${TC}ar RANLIB=${TC}ranlib

# TI_NO_OPENSSL: use the built-in SHA-1 (there is no OpenSSL on the device)
# -include cstring: the toolchain's GCC 8 headers do not pull <cstring> in implicitly
export TI_NO_OPENSSL=1
export CFLAGS="-mcpu=cortex-a7 -mfpu=neon-vfpv4 -mfloat-abi=hard -DTI_NO_OPENSSL -include cstring"
export LFLAGS="-static-libstdc++ -static-libgcc -Wl,--allow-shlib-undefined"

cd "$ROOT/ti99sim"
make -C src/core
make -C src/sdl

mkdir -p "$ROOT/build"
${TC}strip -o "$ROOT/build/ti99sim-sdl" bin/ti99sim-sdl
file "$ROOT/build/ti99sim-sdl"
