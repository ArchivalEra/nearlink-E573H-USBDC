#!/usr/bin/env python3
"""
sle-hci-scan.py — systematic SLE HCI command probe via /dev/hwsle.

Opens /dev/hwsle (triggers pm_sle_enable -> SLE_OPEN), then sends candidate
HCI commands in both standard-HCI and DLI-ish formats, collecting any device
events (0xA2) per command. Goal: discover the WS73 SLE controller's command
dialect (the SDK ships no HCI opcode table; OHOS DLI values are the best
candidate set per ticket 02).

Usage: sudo python3 sle-hci-scan.py [--seconds N] [--list]
       --list  just print the candidate command table and exit
"""

import os, sys, time, select, struct

DEV = "/dev/hwsle"
DT_CMD, DT_EVENT, DT_ACB, DT_ICB = 0xA1, 0xA2, 0xA3, 0xA4

# candidate commands: (name, bytes) — format [A1][opcode LE 2B][plen LE 2B][params]
CMDS = [
    # standard-HCI style (opcodes from generic HCI / LE)
    ("HCI_RESET",              bytes([0xA1, 0x03, 0x00, 0x00, 0x00])),
    ("HCI_READ_LOCAL_VERSION", bytes([0xA1, 0x01, 0x10, 0x00, 0x00])),
    ("LE_READ_LOCAL_FEATURES", bytes([0xA1, 0x03, 0x10, 0x00, 0x00])),
    ("LE_READ_BUFFER_SIZE",    bytes([0xA1, 0x02, 0x10, 0x00, 0x00])),
    ("LE_READ_ADV_CH_TX_POWER",bytes([0xA1, 0x07, 0x10, 0x00, 0x00])),
    # DLI-style opcodes (OpenHarmony stack, ticket 02)
    ("DLI_RESET",              bytes([0xA1, 0x08, 0x04, 0x00, 0x00])),
    ("DLI_READ_LOCAL_VERSION", bytes([0xA1, 0x04, 0x04, 0x00, 0x00])),
    ("DLI_SET_ADV_PARAMS",     bytes([0xA1, 0x02, 0x0C, 0x00, 0x00])),
    ("DLI_SET_ADV_ENABLE",     bytes([0xA1, 0x05, 0x0C, 0x01, 0x01])),
    ("DLI_SET_SCAN_PARAMS",    bytes([0xA1, 0x01, 0x10, 0x00, 0x00])),
    ("DLI_SET_SCAN_ENABLE",    bytes([0xA1, 0x02, 0x10, 0x01, 0x01])),
    ("DLI_CREATE_CONNECTION",  bytes([0xA1, 0x01, 0x14, 0x00, 0x00])),
    # SLE-ish / vendor probes
    ("SLE_READ_INFO",          bytes([0xA1, 0x01, 0xFC, 0x00, 0x00])),
    ("DLI_READ_LOCAL_BUFFER",  bytes([0xA1, 0x02, 0x04, 0x00, 0x00])),
    ("DLI_READ_SUPPORT_FEATS", bytes([0xA1, 0x03, 0x04, 0x00, 0x00])),
    ("DLI_READ_LOCAL_VER_FULL",bytes([0xA1, 0x04, 0x04, 0x00, 0x00])),
    ("DLI_READ_RSSI",          bytes([0xA1, 0x0C, 0x18, 0x00, 0x00])),
    ("VENDOR_TEST",            bytes([0xA1, 0x00, 0x00, 0x00, 0x00])),
]

def hexd(b):
    return " ".join(f"{x:02x}" for x in b)

def try_open():
    try:
        fd = os.open(DEV, os.O_RDWR | os.O_NONBLOCK)
        return fd
    except OSError as e:
        print(f"open {DEV} failed: {e}")
        return None

def drain(fd, timeout_s):
    """non-blocking read for timeout_s; return list of (len, bytes)"""
    out = []
    end = time.time() + timeout_s
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.1)
        if r:
            try:
                data = os.read(fd, 4096)
                if data:
                    out.append(data)
            except (BlockingIOError, OSError):
                pass
    return out

def main():
    seconds = int(sys.argv[sys.argv.index("--seconds") + 1]) if "--seconds" in sys.argv else 3
    if "--list" in sys.argv:
        for name, cmd in CMDS:
            print(f"  {name:28s} {hexd(cmd)}")
        return

    fd = try_open()
    if fd is None:
        sys.exit(1)
    print(f"opened {DEV} (SLE_OPEN handshake triggered); draining 1s...")
    drain(fd, 1.0)

    print(f"== probing {len(CMDS)} candidate commands ==")
    for name, cmd in CMDS:
        try:
            os.write(fd, cmd)
        except OSError as e:
            print(f"  {name}: write err {e}")
            continue
        time.sleep(0.4)  # give device time to answer
        resp = drain(fd, 0.3)
        if resp:
            print(f"  {name:28s} TX {hexd(cmd)}  ->  {len(resp)} event(s):")
            for r in resp:
                t = r[0] if r else 0
                tname = {DT_EVENT: "EVENT", DT_ACB: "ACB", DT_ICB: "ICB", DT_CMD: "CMD"}.get(t, "?")
                print(f"      [{tname} {len(r)}B] {hexd(r[:32])}{'…' if len(r) > 32 else ''}")
        else:
            print(f"  {name:28s} TX {hexd(cmd)}  ->  (no response)")

    os.close(fd)  # -> SLE_CLOSE
    print("== done ==")

if __name__ == "__main__":
    main()
