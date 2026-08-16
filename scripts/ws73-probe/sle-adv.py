#!/usr/bin/env python3
"""
sle-adv.py — start SLE advertising on the WS73 dongle via /dev/hwsle.

Sequence: SET_ADV_PARAMS(0x0C02) -> SET_ADV_DATA(0x0C03) -> SET_ADV_ENABLE(0x0C05).
Wire format: [A1][opcode u16 LE][plen u16 LE][params]; events: [A2][hdr][status][opcode echo][status][data].

DLI_AdvParam (49 B, pack(1)) per OHOS stack dli_def.h. Interval 100 ms (800*125us),
all 3 channels, public addr, no conn params (0x001E default interval 7.5ms).
"""

import os, sys, time, select, struct

DEV = "/dev/hwsle"
DT_CMD, DT_EVENT = 0xA1, 0xA2
OP_ADV_PARAMS, OP_ADV_DATA, OP_ADV_ENABLE = 0x0C02, 0x0C03, 0x0C05

def hx(b):
    return " ".join(f"{x:02x}" for x in b)

def frame(opcode, params):
    return bytes([DT_CMD]) + struct.pack("<HH", opcode, len(params)) + params

def drain(fd, timeout_s=0.4):
    out = []
    end = time.time() + timeout_s
    while time.time() < end:
        r, _, _ = select.select([fd], [], [], 0.05)
        if r:
            try:
                d = os.read(fd, 4096)
                if d: out.append(d)
            except (BlockingIOError, OSError):
                pass
    return out

OP_SCAN_PARAMS, OP_SCAN_ENABLE = 0x1001, 0x1002
def send(fd, name, params):
    f = frame(OP_ADV_PARAMS if name == "PARAMS" else OP_ADV_DATA if name == "DATA"
              else OP_ADV_ENABLE if name == "ENABLE"
              else OP_SCAN_PARAMS if name == "SCANP" else OP_SCAN_ENABLE, params)
    os.write(fd, f)
    time.sleep(0.3)
    resp = drain(fd, 2.5)   # wait for async command-complete
    print(f"  {name:7s} TX {hx(f)}")
    if resp:
        for r in resp:
            # parse: a2 hdr(2) plen(2) [00 01 status] opcode(2) status data
            if len(r) >= 9:
                op = struct.unpack("<H", r[5:7])[0]
                status = r[7]
                status_txt = {0: "OK", 1: "PENDING", 6: "PARAM_ERR"}.get(status, f"0x{status:02x}")
                print(f"         RX {len(r)}B op=0x{op:04x} status={status_txt} data={hx(r[8:])[:48]}")
            else:
                print(f"         RX {len(r)}B {hx(r)}")
    else:
        print("         (no response)")
    return resp

def adv_params():
    """49-byte DLI_AdvParam: handle=0, mode=1(connectable?), interval 100ms, ch=all, no pref power"""
    p = bytearray(49)
    p[0] = 0                 # advHandle
    p[1] = 1                 # advMode (1 = connectable undirected)
    p[2] = 0                 # advGtRole
    iv = 800                 # 100 ms in 125us units
    p[3:6] = iv.to_bytes(3, "little")   # primAdvIntervalMin
    p[6:9] = iv.to_bytes(3, "little")   # primAdvIntervalMax
    p[9] = 0b111             # primAdvChannelMap: ch 76/77/78
    p[10] = 0                # ownAddrType public
    p[11] = 0                # peerAddrType
    # ownAddr[6] zeros, peerAddr[6] zeros
    p[24] = 0                # advFilterPolicy
    p[25] = 127              # advTxPower no preference
    p[26] = 0                # primAdvFrameFormat
    # rest zeros: second phy params, sid, scan params, conn params
    return bytes(p)

def main():
    fd = os.open(DEV, os.O_RDWR | os.O_NONBLOCK)
    if fd < 0:
        print("open failed"); sys.exit(1)
    print(f"opened {DEV}; SLE_OPEN handshake...")
    drain(fd, 1.0)

    print("== step 1: SET_ADV_PARAMS ==")
    send(fd, "PARAMS", adv_params())

    print("== step 2: SET_ADV_DATA ==")
    # advData: advHandle, operation(3=frag complete), selection(0), len, payload
    adv_data = b"\x02\x01\x06\x03\x03\x41\x42\x43"  # flags + 3-byte name "ABC"
    p2 = bytes([0, 3, 0, len(adv_data)]) + adv_data
    send(fd, "DATA", p2)

    print("== step 3: SET_ADV_ENABLE ==")
    p3 = bytes([1, 0]) + struct.pack("<HB", 0, 0)   # enable=1, handle=0, duration=0, maxEvents=0
    send(fd, "ENABLE", p3)

    print("== step 4: SET_SCAN_PARAMS ==")
    # DLI_SetScanningParameter: ownAddrType, scanFilterPolicy, scanPhys, scanInterval, scanWindow
    sp = bytes([0, 0, 1]) + (400).to_bytes(2, "little") + (200).to_bytes(2, "little")
    send(fd, "SCANP", sp)

    print("== step 5: SET_SCAN_ENABLE ==")
    se = bytes([1, 0])  # enable=1, filterdup=0
    send(fd, "SCANE", se)

    print("== post-enable: drain 2s for adv events ==")
    time.sleep(2.0)
    resp = drain(fd, 0.5)
    for r in resp:
        print(f"  post RX {len(r)}B {hx(r)[:64]}")

    os.close(fd)
    print("done (fd closed -> SLE_CLOSE)")

if __name__ == "__main__":
    main()
