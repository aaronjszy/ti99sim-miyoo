#!/bin/sh
# Regenerate upstream-modifications.patch against upstream commit e360854.
set -eu

# Avoid sandbox errors on protected user git configs
export HOME="${TMPDIR:-/tmp}"

UPSTREAM_COMMIT="e360854"
ROOT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
OUTPUT_PATCH="$ROOT_DIR/upstream-modifications.patch"

# Verify that the upstream commit is available in git; if missing, fetch it
if ! git -C "$ROOT_DIR" rev-parse --verify -q "$UPSTREAM_COMMIT^{commit}" >/dev/null; then
    echo "Fetching upstream commit $UPSTREAM_COMMIT from christianhaitian/ti99sim..."
    git -C "$ROOT_DIR" remote add upstream https://github.com/christianhaitian/ti99sim.git 2>/dev/null || true
    git -C "$ROOT_DIR" fetch upstream master
fi

TMP_DIR="$(mktemp -d /tmp/ti99sim-patch-XXXXXX)"
trap 'rm -rf "$TMP_DIR"' EXIT

mkdir -p "$TMP_DIR/a" "$TMP_DIR/b"

# Export the clean upstream tree
git -C "$ROOT_DIR" archive --prefix=ti99sim/ "$UPSTREAM_COMMIT" | tar -x -C "$TMP_DIR/a"

# Copy current working tree of ti99sim
cp -R "$ROOT_DIR/ti99sim" "$TMP_DIR/b/"

# Exclude build output
rm -rf "$TMP_DIR/b/ti99sim/bin" "$TMP_DIR/b/ti99sim/src"/*/Release "$TMP_DIR/b/ti99sim/src"/*/Debug

# Generate unified diff (diff returns 1 when differences exist)
(cd "$TMP_DIR" && diff -ruN a b > "$OUTPUT_PATCH") || true

echo "patch: $OUTPUT_PATCH updated against commit $UPSTREAM_COMMIT"
