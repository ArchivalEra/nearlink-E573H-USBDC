# nearlink-E573H-USBDC

> [中文版 README](README.md) · English (current)

Drive the NearLink (SparkLink / SLE) USB dongle that shows up on the bus as `ffff:3733` ("00000000") from Linux.

## What this is

A **HiSilicon WS73 NearLink dongle** on the USB bus. It currently enumerates as:

```
Bus 001 Device 005: ID ffff:3733 00000000 00000000
```

- **VID:PID** `ffff:3733` — the HiSilicon WS73 family (WiFi/BT/SLE combo chip) USB identity, hardcoded in the SDK as `DEIVICE_VENDOR_ID/PRODUCT_ID`
- **Interface**: single interface, class `0xE0/0x02/0x02` (WUSB Wire Adapter masquerade), 2 bulk endpoints
- **State**: **boot mode** — firmware not yet downloaded, hence no kernel driver binds it, no /dev node, no network interface. Before anything works, `ws73.bin` must be pushed over the HCC patch channel; the device then re-enumerates with 5 endpoints (kernel mode) and only then can a protocol stack talk to it.

## Repository layout

```
.
├── .gitignore                 # whitelist-style: ignore everything, only !pattern allowlists sources/docs/config
├── docs/
│   ├── DEVICE-INTEL.md        # ffff:3733 device enumeration intel (sysfs/lsusb/descriptors/binding)
│   ├── SDK-INTEL.md           # WS73 SDK 1.10.110 structure, build flow, reusable pieces
│   ├── USB-PROTOCOL.md        # HCC-over-USB protocol essentials (boot/kernel state machine, EP layout, FW download)
│   └── ECOSYSTEM.md           # ★ NearLink open-source ecosystem map (13 repos surveyed) + finalized roadmap
└── sdk/ws73_sdk_linux_WS73_1.10.110/   # HiSilicon WS73 Linux SDK (sources tracked, binaries gitignored)
```

> The SDK's original distribution is `ws73_sdk_linux_WS73_1.10.110.zip` (48MB, present at the project root on this machine, excluded by the `*.zip` rule). Firmware blobs like `firmware/us/ws73.bin` also stay **out of git** — the driver reads them from the SDK extraction dir or `/etc/ws73/` at runtime.

## Driver strategy (finalized roadmap — see docs/ECOSYSTEM.md)

**Core insight**: HiSilicon's 2026 "full open source" release of the NearLink stack = OpenHarmony `communication_nearlink_service` (Apache-2.0, ~360k lines of C SLE host stack), which contains *no* byte transport / kernel driver / firmware. The WS73 USB bottom half (HCC, ffff:3733, firmware download) is in the SDK we hold. **Two halves + one self-built transport seam = the complete solution** — no need to stay limited by the vendor's closed AT drivers.

```
Phase 1  user-space handshake validation: libusb/gousb push ws73.bin (HCC patch) → watch re-enum to 5EP
Phase 2  kernel transport driver: slim x86 usb_driver (reference hcc_usb_host.c) → /dev/ws73hci
Phase 3  user-space protocol stack: port OpenHarmony nearlink_service (strip OHOS deps, wire HAL seam to /dev/ws73hci)
Phase 4  applications: peer-to-peer messaging / HID / UART passthrough
```

Side references: the HiSilicon SDK ships a `wireless_usb` driver (ARM prebuilt, needs x86 rebuild); the TIoT host driver in the FlashKeyboard repo (0x7e framing + firmware load) is another "host drives a NearLink radio" porting template.

## Quick start

```bash
# firmware present (already in the SDK extraction)
ls sdk/ws73_sdk_linux_WS73_1.10.110/firmware/us/ws73.bin

# device intel
lsusb -v -d ffff:3733
```

(Driver code not yet landed — see the roadmap above and docs/.)

## Document index & maintenance

> **Rule**: `docs/` is English-only; the two READMEs link to each other. When you change docs, update both columns below before pushing (the pre-push hook auto-verifies via `scripts/check-docs.sh`).

| File | Description | Language |
|---|---|---|
| `README.md` | Project overview (Chinese) | zh |
| `README.en.md` | Project overview (English, cross-linked) | en |
| `docs/DEVICE-INTEL.md` | ffff:3733 device enumeration intel | en |
| `docs/SDK-INTEL.md` | WS73 SDK structure/build/reusable pieces | en |
| `docs/USB-PROTOCOL.md` | HCC-over-USB protocol essentials | en |
| `docs/ECOSYSTEM.md` | NearLink open-source ecosystem map + roadmap | en |
| `.gitignore` | Whitelist rules (ignore all by default, allowlist only) | — |
| `sdk/` | HiSilicon WS73 Linux SDK source tree (binaries gitignored) | — |

## License

AGPL-3.0 (the SDK belongs to HiSilicon/CompanyNameMagicTag and is referenced for driver development only).
