#!/bin/sh
# Fetch the pinned third-party files this project links against and runs with, from steward-fu's SDL2 port for the
# Miyoo Mini (https://github.com/steward-fu/sdl2, LGPL-2.1):
#   deps/steward-sdl2/sdl2/include/   SDL2 headers, used at compile time
#   deps/lib/                         libSDL2-2.0.so.0, libEGL.so, libGLESv2.so, libjson-c.so.5, used at link time and
#                                     shipped to the device
# Only those paths are downloaded (a sparse, shallow fetch of one commit), not the multi-GB repository. Every library is
# checked against deps/SHA256SUMS. Needs git (2.25+), network access to github.com, and sha256sum or shasum.
set -eu

REPO=https://github.com/steward-fu/sdl2.git
COMMIT=0631abc8e8916db6f9bc7e2afd0c22913d092a29

ROOT=$(cd "$(dirname "$0")/.." && pwd)
SRC=$ROOT/deps/steward-sdl2
LIB=$ROOT/deps/lib
SUMS=$ROOT/deps/SHA256SUMS

if command -v sha256sum >/dev/null 2>&1; then SHA="sha256sum"; else SHA="shasum -a 256"; fi

verified() {
    [ -d "$SRC/sdl2/include" ] && [ -d "$LIB" ] && (cd "$LIB" && $SHA -c "$SUMS" >/dev/null 2>&1)
}

if verified; then
    echo "deps: up to date"
    exit 0
fi

echo "deps: fetching steward-fu/sdl2 @ $(echo "$COMMIT" | cut -c1-8)"
rm -rf "$SRC" "$LIB"
mkdir -p "$SRC" "$LIB"

git -C "$SRC" init -q
git -C "$SRC" remote add origin "$REPO"
git -C "$SRC" sparse-checkout set --no-cone /sdl2/include/ /prebuilt/640x480/ /examples/libjson-c.so.5
git -C "$SRC" fetch -q --depth 1 --filter=blob:none origin "$COMMIT"
git -C "$SRC" checkout -q FETCH_HEAD

cp "$SRC/prebuilt/640x480/libSDL2-2.0.so.0" "$SRC/prebuilt/640x480/libEGL.so" "$SRC/prebuilt/640x480/libGLESv2.so" \
   "$SRC/examples/libjson-c.so.5" "$LIB/"

if ! (cd "$LIB" && $SHA -c "$SUMS"); then
    echo "deps: checksum mismatch; refusing to continue" >&2
    rm -rf "$LIB"
    exit 1
fi
echo "deps: ok"
