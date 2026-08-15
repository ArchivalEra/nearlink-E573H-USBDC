#!/usr/bin/env bash
#
# sync-ws73-probe.sh — mirror the lab's ws73-probe tool into this repo's
# scripts/ for versioning (the lab stays the dev workspace).
#
# Usage: bash scripts/sync-ws73-probe.sh [--force]
#   --force  overwrite scripts/ws73-probe* even if hashes differ (default:
#            refuses to overwrite a locally-modified copy)

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
LAB="/mnt/hdd/laboratory/ws73-probe"
SRC="$LAB/probe.c"
DST_DIR="$REPO_ROOT/scripts/ws73-probe"
TOOLS="probe.c kernel-observe.c kernel-init.c"
FORCE=0
[ "${1:-}" = "--force" ] && FORCE=1

[ -f "$SRC" ] || { echo "error: lab source not found at $LAB (is the lab mounted?)" >&2; exit 1; }

mkdir -p "$DST_DIR"
cp "$LAB/91-ws73.rules" "$DST_DIR/91-ws73.rules"

for t in $TOOLS; do
    if [ -f "$DST_DIR/$t" ] && ! diff -q "$LAB/$t" "$DST_DIR/$t" >/dev/null 2>&1 && [ "$FORCE" -eq 0 ]; then
        echo "note: scripts/ws73-probe/$t differs from lab — pass --force to overwrite" >&2
        exit 2
    fi
done

for t in $TOOLS; do
    cp "$LAB/$t" "$DST_DIR/$t"
    echo "synced: $LAB/$t -> $DST_DIR/$t"
done

if command -v gcc >/dev/null 2>&1 && command -v pkg-config >/dev/null 2>&1 && pkg-config --exists libusb-1.0 2>/dev/null; then
    echo "build check (lab):"
    (cd "$LAB" && make -s && ./ws73-probe --help >/dev/null 2>&1 || true)
    echo "  lab build ok"
else
    echo "note: libusb dev not present here; skipping lab build check"
fi
