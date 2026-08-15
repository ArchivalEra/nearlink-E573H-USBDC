#!/usr/bin/env bash
#
# check-fw.sh — verify WS73 firmware blobs before flashing:
#   1. file exists and size within SDK limits (≤ 200 KiB incl. 64-byte header)
#   2. the 64-byte ASCII header is the lowercase-hex SHA-256 of the content
#      (as the boot handshake expects — see docs/USB-PROTOCOL.md)
#
# Usage: bash scripts/check-fw.sh <path-to-ws73.bin> [more files...]
# Exit: 0 all ok · 1 any check failed

set -u

fail=0
for f in "$@"; do
    echo "== $f"
    [ -f "$f" ] || { echo "  ✘ not a file"; fail=1; continue; }

    size=$(stat -c %s "$f")
    if [ "$size" -le 64 ]; then
        echo "  ✘ too small ($size B)"; fail=1; continue
    fi
    if [ "$size" -gt $((200 * 1024)) ]; then
        echo "  ✘ exceeds SDK FIRMWARE_FILESIZE_MAX (200 KiB)"; fail=1; continue
    fi
    echo "  size $size B ($((size - 64)) B content)"

    if command -v python3 >/dev/null 2>&1; then
        ok=$(python3 - "$f" <<'PY'
import hashlib, sys
path = sys.argv[1]
data = open(path, "rb").read()
head, body = data[:64], data[64:]
try:
    head_txt = head.decode("ascii").strip()
except Exception:
    print(0); raise SystemExit
calc = hashlib.sha256(body).hexdigest()
print(1 if head_txt == calc else 0)
PY
)
        if [ "$ok" = "1" ]; then
            echo "  ✔ SHA-256 header matches content"
        else
            echo "  ✘ SHA-256 header mismatch (see --skip-verify in ws73-probe if you intend to force)"
            fail=1
        fi
    else
        echo "  (python3 not found; skipped header check)"
    fi
done

[ "$fail" -eq 0 ] && echo "ALL OK" || echo "FAILED"
exit "$fail"
