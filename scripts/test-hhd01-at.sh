#!/bin/bash
# test-hhd01-at.sh — HHD-01 (WS63) AT firmware smoke test
#
# Usage: ./test-hhd01-at.sh /dev/ttyUSB0
#
# Prerequisites:
#   1. HHD-01 connected via USB-C (CH340G serial, 115200 8N1)
#   2. Official AT firmware flashed (firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg)
#   3. Board powered on, serial console shows AT prompt
#
# This script:
#   - Verifies AT firmware responds
#   - Enables SLE
#   - Sets SLE address
#   - Registers SSAP server (service 0x2222, property 0x2323)
#   - Starts advertising
#   - Waits for our PC stack to connect
#
# After this runs, our PC stack (assets/stack/ssap/) can connect to
# the HHD-01's SSAP service and exchange data.

set -euo pipefail

PORT="${1:-/dev/ttyUSB0}"
BAUD=115200

log() { echo "[$(date +%H:%M:%S)] $*"; }

send_at() {
    local cmd="$1"
    local expect="${2:-}"
    log "TX: $cmd"
    echo -e "${cmd}\r" > "$PORT"
    if [ -n "$expect" ]; then
        sleep 0.5
        local resp
        resp=$(timeout 2 cat "$PORT" 2>/dev/null || true)
        log "RX: $resp"
        if echo "$resp" | grep -qi "$expect"; then
            log "OK: got expected '$expect'"
            return 0
        else
            log "WARN: expected '$expect' not found"
            return 1
        fi
    fi
}

# Open serial port
if [ ! -e "$PORT" ]; then
    echo "ERROR: $PORT not found"
    echo "Check USB connection and driver (CH340G)"
    exit 1
fi

# Configure serial
stty -F "$PORT" "$BAUD" raw -echo -echoe -echok 2>/dev/null || true
exec 3<>"$PORT"

log "=== HHD-01 AT Smoke Test ==="
log "Port: $PORT @ $BAUD"

# 1. Verify AT firmware
log "--- Step 1: Verify AT firmware ---"
send_at "AT" "OK"
send_at "AT+SYSINFO" ""

# 2. Enable SLE
log "--- Step 2: Enable SLE ---"
send_at "AT+SLEENABLE" ""

# 3. Set SLE address
log "--- Step 3: Set SLE address ---"
send_at "AT+SLESETADDR=0,0x112233445566" ""

# 4. Register server callback
log "--- Step 4: Register SSAP server callback ---"
send_at "AT+SSAPSREGCBK" ""

# 5. Add service (UUID 0x2222)
log "--- Step 5: Add service 0x2222 ---"
send_at "AT+SSAPSADDSRV=0x2222" ""

# 6. Add property (UUID 0x2323, read+write+notify)
log "--- Step 6: Add property 0x2323 ---"
send_at "AT+SSAPSADDPROPERTY=1,0x2323,5,5,2,0x1234" ""

# 7. Start service
log "--- Step 7: Start service ---"
send_at "AT+SSAPSSTARTSERV=1" ""

# 8. Set advertising parameters
log "--- Step 8: Set advertising params ---"
send_at "AT+SLESETADVPAR=1,3,200,200,0,0x112233445566,0,0x000000000000" ""

# 9. Set advertising data
log "--- Step 9: Set advertising data ---"
send_at "AT+SLESETADVDATA=1,10,4,aabbccddeeff11223344,11224455" ""

# 10. Start advertising
log "--- Step 10: Start advertising ---"
send_at "AT+SLESTARTADV=1" ""

log ""
log "=== HHD-01 SLE server ready ==="
log "Service: 0x2222, Property: 0x2323"
log "MAC: 11:22:33:44:55:66"
log "Advertising: ON"
log ""
log "Next: connect with our PC stack:"
log "  ssap_link_connect(link, mac_112233445566, NULL)"
log "  exchange_info(MTU=520, v1.3)"
log "  find_structure(PRIMARY_SERVICE, start=0x0001, end=0xFFFF)"
log "  find_structure(PROPERTY, start=0x0001, end=0xFFFF)"
log "  write CCCD (prop_handle+1 = 0x0004, value 0x0001)"
log "  read property 0x0003"
log ""

exec 3>&-
