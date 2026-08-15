#!/usr/bin/env bash
#
# hwsle-probe.sh — probe the SLE channel via /dev/hwsle (kernel driver).
# Opens the device (triggers pm_sle_enable -> SLE_OPEN handshake), reads any
# unsolicited device events for N seconds, then optionally sends SLE HCI
# commands (datatype 0xA1 CMD + opcode LE) to observe the device's reaction.
#
# Wire format (from SDK sle_dev.c / sle_hcc_proc.c):
#   write: bytes pass through — prefix with 0xA1 (CMD) + opcode
#   read:  rx skb, datatype byte included (0xA2 EVENT, 0xA3 ACB, 0xA4 ICB)
#
# Opcodes are OHOS DLI values (ticket 02) — unverified against WS73 firmware;
# use to learn the device's command dialect (event / error / silence).
#
# Usage: bash scripts/hwsle-probe.sh [--seconds n] [--cmd HEX [opcode]] [--reset] [--adv]
#   --reset  send DLI_RESET (0x0408)      --adv  send SET_ADVERTISING_ENABLE (0x0C05)
#   --cmd HEX  send a raw hex byte string (e.g. A1 08 04 00)
#
# Requires plat_soc+sle_soc loaded and a dongle in kernel mode (single
# instance — only one dongle can hold the SLE channel; see ticket 09).
# NOTE: the dongle must have gone through the DRIVER-triggered firmware
# download (pm_svc_power_on initializes hcc). A libusb-preflashed dongle
# (scripts/flash-dongle.sh) sits in kernel mode but hcc/PM is not fully
# initialized — open fails with EINVAL (0x7851). Use the driver path:
#   bash scripts/load-driver.sh   (let it auto-download firmware)

set -u

DEV=/dev/hwsle
SECONDS=4
CMDS=""

for a in "$@"; do
    case "$a" in
        --seconds) SECONDS="${2:-4}" ;;
        --reset)   CMDS="$CMDS A1 08 04 00" ;;
        --adv)     CMDS="$CMDS A1 05 0c 01 01" ;;
        --cmd)     CMDS="$CMDS ${2:-}" ;;
    esac
done

[ -e "$DEV" ] || { echo "error: $DEV missing — load driver: bash scripts/load-driver.sh" >&2; exit 1; }

echo "== open $DEV (SLE_OPEN handshake) =="
exec 3<>"$DEV" || { echo "open failed" >&2; exit 1; }
sleep 0.5

echo "== read ${SECONDS}s: unsolicited events =="
end=$((SECONDS * 2))
for ((i = 0; i < end; i++)); do
    IFS= read -r -t 0 -u 3 && {
        dd bs=1 count=64 <&3 2>/dev/null | xxd -p | tr -d '\n'; echo
    }
    sleep 0.5
done

for hex in $CMDS; do
    # group hex pairs into a command
    read -ra bytes <<< "$hex"
    if [ "${#bytes[@]}" -ge 2 ]; then
        echo "== TX: ${bytes[*]} =="
        for b in "${bytes[@]}"; do printf "\\x$b"; done >&3
        sleep 0.3
    fi
done

echo "== done (fd closed -> SLE_CLOSE) =="
exec 3>&-
exit 0
