#!/usr/bin/env bash
#
# load-driver.sh — load the WS73 kernel driver stack (plat_soc + sle_soc).
#
# Order matters: plat_soc (HCC/USB/firmware-download/PM) first, then sle_soc
# (/dev/hwsle + SLE channel). Requires the built .ko files (see SDK build;
# on x86 use the ported tree — wayfinder tickets 06/08).
#
# Usage: bash scripts/load-driver.sh [--unload] [--sdk <path>] [--quiet]
#   --unload  rmmod both modules first
#   --sdk     SDK root (default: sdk/ws73_sdk_linux_WS73_1.10.110)
#   --quiet   suppress status chatter
#
# Exit: 0 both loaded · 1 preconditions missing · 2 insmod failed

set -u

REPO_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SDK="$REPO_ROOT/sdk/ws73_sdk_linux_WS73_1.10.110"
UNLOAD=0
QUIET=0

for a in "$@"; do
    case "$a" in
        --unload) UNLOAD=1 ;;
        --quiet)  QUIET=1 ;;
        --sdk)    SDK="${2:-$SDK}" ;;
    esac
done

say() { [ "$QUIET" -eq 1 ] || echo "$*"; }
die() { echo "error: $*" >&2; exit 1; }

PLAT="$SDK/driver/platform/plat_soc.ko"
SLE="$SDK/driver/bsle/sle_driver/sle_soc.ko"

[ -f "$PLAT" ] || die "plat_soc.ko not found at $PLAT (build it first, -j1!)"
[ -f "$SLE" ]  || die "sle_soc.ko not found at $SLE (build it first, -j1!)"

if [ "$UNLOAD" -eq 1 ]; then
    say "unloading..."
    sudo rmmod sle_soc 2>/dev/null
    sudo rmmod plat_soc 2>/dev/null
    sleep 1
fi

# runtime prerequisites
[ -f /etc/ws73_cfg.ini ] || say "warn: /etc/ws73_cfg.ini missing (copy build/config/ws73_cfg_default.ini)"
[ -f /etc/ws73/ws73.bin ] || say "warn: /etc/ws73/ws73.bin missing (copy firmware/us/*)"

say "insmod plat_soc..."
sudo insmod "$PLAT" || die "plat_soc insmod failed (dmesg for Unknown symbol?)"
say "insmod sle_soc..."
sudo insmod "$SLE"  || { sudo rmmod plat_soc 2>/dev/null; die "sle_soc insmod failed"; }

sleep 1
say "loaded:"
lsmod | grep -E "plat_soc|sle_soc"
say "device:"
for d in /sys/bus/usb/devices/*/; do
    [ "$(cat "$d/idVendor" 2>/dev/null)" = "ffff" ] || continue
    p="${d%/}"
    echo "  $p: $(readlink "$p/$p:1.0/driver" 2>/dev/null || echo unbind) bcdDevice=$(cat "$p/bcdDevice" 2>/dev/null)"
done
[ -e /dev/hwsle ] && say "  /dev/hwsle present" || say "  warn: /dev/hwsle missing"
exit 0
