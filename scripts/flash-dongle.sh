#!/usr/bin/env bash
#
# flash-dongle.sh — one-shot firmware download to a WS73 dongle via the lab
# ws73-probe tool (libusb). Wraps the 3-file sequence used and verified on
# real hardware (wayfinder tickets 01/05): ws73.bin@0x400000,
# wifi_cali.bin@0x430000, btc_cali.bin@0x440000.
#
# Usage: bash scripts/flash-dongle.sh <bus-port> [--skip-verify] [--probe PATH]
#   <bus-port>   e.g. 1-4 (from lsusb / sysfs devpath)
#   --skip-verify  skip the SHA-256 header check (not recommended)
#   --probe PATH   path to the ws73-probe binary (default: lab build, or
#                  scripts/ws73-probe/ via make)
#
# Writes to the device (firmware download + re-enumeration). Keep the second
# dongle as a control.

set -eu

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="$REPO_ROOT/sdk/ws73_sdk_linux_WS73_1.10.110"
FW="$SDK/firmware/us"
PORT="${1:-}"
SKIP=""
PROBE=""

for a in "$@"; do
    case "$a" in
        --skip-verify) SKIP="--skip-verify" ;;
        --probe)       PROBE="${2:-}" ;;
    esac
done

[ -n "$PORT" ] || { echo "usage: $0 <bus-port> [--skip-verify] [--probe PATH]" >&2; exit 3; }
[ -f "$FW/ws73.bin" ] || { echo "error: $FW/ws73.bin missing" >&2; exit 1; }

# probe binary: --probe > lab build > repo build
if [ -n "$PROBE" ]; then
    BIN="$PROBE"
elif [ -x /mnt/hdd/laboratory/ws73-probe/ws73-probe ]; then
    BIN=/mnt/hdd/laboratory/ws73-probe/ws73-probe
else
    BIN="$REPO_ROOT/scripts/ws73-probe/ws73-probe"
    [ -x "$BIN" ] || { echo "build it: cd scripts/ws73-probe && make" >&2; exit 1; }
fi

# must run with the dongle NOT bound by the kernel driver (boot mode needs
# libusb access); check + advise.
BOUND="$(readlink "/sys/bus/usb/devices/$PORT/$PORT:1.0/driver" 2>/dev/null || true)"
if [ -n "$BOUND" ]; then
    echo "note: $PORT is bound to $BOUND — unload first: bash scripts/load-driver.sh --unload" >&2
fi

echo "== flashing $PORT (3 files, SHA-256 verified) =="
sudo timeout 120 "$BIN" --port "$PORT" \
    --fw="$FW/ws73.bin@0x400000" \
    --fw="$FW/wifi_cali.bin@0x430000" \
    --fw="$FW/btc_cali.bin@0x440000" \
    $SKIP
rc=$?
[ "$rc" -eq 0 ] && echo "== $PORT now in kernel mode ==" || echo "== flash failed (rc=$rc) =="
exit "$rc"
