# Ecosystem Map: NearLink Open-Source Landscape (surveyed 2026-08)

> Based on deep-dives into 13 repos + the HiSilicon WS73 Linux SDK source. Goal: pin down what "HiSilicon fully open-sourced the NearLink protocol stack" actually refers to, and what our ffff:3733 driver can be assembled from.

## One-line conclusion

**The "full open source" release is OpenHarmony `communication_nearlink_service` (Apache-2.0, ~360k lines of C, the complete SLE host protocol stack source).** But it stops *above* the HCI seam — no byte transport, no kernel driver, no firmware inside. The WS73 USB bottom half (HCC, ffff:3733, firmware download) lives in the HiSilicon SDK we already have. **The two halves, joined by one self-built transport seam, form the complete solution.**

## Layer map

| Layer | Status | Source |
|---|---|---|
| SLE application profiles (SSAP service/discovery/connection/security/ranging/transport) | ✅ open (C, Apache-2.0) | `openharmony/communication_nearlink_service` → `services/stack/{cp,dp}` |
| HCI-style command set + HDLC framing (DLI layer) | ✅ open (C) | same → `services/stack/dli/` |
| Byte-transport HAL seam | ✅ open but only 6 functions | same → `SleDliLayerAdapter.cpp` (depends on OHOS HDI `ISleHciInterface`) |
| Low-level byte transport (UART/USB) | ❌ not in the open stack | OHOS HDF driver (external) / to be built |
| WS73 USB driver + firmware download (ffff:3733 / HCC) | ✅ source in hand | this repo `sdk/.../driver/platform/hcc/host/hcc_usb_host.c` |
| Chip-side firmware/protocol stack (Hi2821/BS21/WS63 etc.) | ⚠️ closed prebuilt .a + open app layer | Hi2821 SDK repos (tools+docs+headers open) |
| Host user-space stack (sparklinkd etc.) | ❌ closed ARM binaries | WS73 SDK `application/bin/` |
| Chip AT commands / SLE-Link binary protocol spec | ✅ docs open | Hi2821 SDK `docs/software/` (456KB SLE-Link + 816KB AT manual) |

## Repo inventory

### Cloned to /mnt/hdd/nearlink-stuff/

| Repo | What it is | Value to this project |
|---|---|---|
| `communication_nearlink_service` (24M) | **OpenHarmony official SLE host protocol stack, full source** | ★★★ the only open user-space stack source |
| `ili9320-i80-hi2821e-spi-bridge` (757M) | Full BearPi-Pico H2821E SDK (CFBB 0.9.0.5): RISC-V toolchain 349M + LiteOS + closed protocol libs (269 .a) + 31 Chinese docs | ★★ chip-side reference + **SLE-Link/AT protocol specs** |
| `FlashKeyboard` (246M) | Same-family CFBB SDK (BS21E) + **TIoT host driver** (W33 NearLink radio: 0x7e framing + firmware-load state machine) + SLP positioning protocol | ★★ TIoT = best porting reference for "host drives a NearLink radio" |
| `ws73v100-wifi` (189M) | Near-relative of our SDK (KO renamed for co-loading); same HCC family, ours is newer | ★ skip; only `sle_socket` netlink channel is unique |
| `NLChat` (191M) | Android serial chat app (CH34x USB-UART 115200 8N1 to NearLink boards) | ★ confirms SLE_UART ecosystem; no code value |
| `tp78_v3_open` / `NearLink_controller` / `NearLinkSLE` / `LinkNebula` / `NLChat_Web` / `SparkLink-FallDetection` | Hi2821 keyboard firmware / gamepad reference / SSAP samples / Rust firmware / web frontend / WS63 learning docs | ★ SSAP call sequence & HID-over-SLE design references |
| `NearLink-RSSI-Fingerprinting-Dataset` | positioning dataset | unrelated (3733 hits are float false-positives) |

### Not yet fetched (watch)

- `gitee.com/HiSpark/fbb_ws63` — official WS63 SDK source (HiSpark); referenced by NLChat and fall-detection repos
- `github.com/Hny0305Lin/Bearpi_Hi2821_Pico_NLChat` / `Bearpi_Hi3863_Pico` / `Hihope_WS63_NearLink_SDK` — BearPi/HiHope device firmware repos
- OpenHarmony `drivers/interface/nearlink` — HDI interface definitions the stack depends on (external repo)

## Confirmed protocol knowledge

- **WS73 USB**: boot state 2 bulk EPs → HCC patch channel ASCII commands (`WRITEM/WMEM/RMEM/FILES/QUIT`) push `ws73.bin` (SHA256, ≤200KB) → device re-enumerates with 5 EPs (BULK_IN/OUT + INT_IN + RW_REG×2) → HCC data channel (92B scatter header, credit flow control)
- **DLI layer** (OpenHarmony stack): HCI-style opcodes (`DLI_CREATE_CONNECTION=0x1401` etc.) + 5-byte-header HDLC frames + command queue with 10s timeout; correspondence with WS73's hcc **unverified** (risk)
- **TIoT** (FlashKeyboard/W33): `0x7e | subsys | len | payload | 0x7e` frames (SLE_MSG=0x15), 2M baud UART, firmware load with 3 retries
- **SLE-Link** (Hi2821 SDK docs): Header/Service ID/Command ID/TLV/MIC framing — binary protocol for host ↔ chip SLE host
- **SSAP**: SparkLink Service Access Protocol (GATT-like); sample service UUID 0x3333 / property 0x3434, MTU 520, connection interval 6.25–12.5ms
- **SLP positioning**: 6.4896–7.9872 GHz S-band, AES-128/SM4, ranging/Air-Mouse modes

## Finalized roadmap

```
Phase 0  today: ffff:3733 in boot state, no driver bound, no /dev, no net iface
   │
Phase 1  user-space handshake validation (fastest, lowest risk)
   │      libusb/gousb tool: enumerate 2EP → HCC patch push ws73.bin → watch re-enum to 5EP
   │      artifact: tools/ws73_probe (protocol validator, kept in repo)
   ▼
Phase 2  kernel transport driver
   │      new slim x86 usb_driver "ws73usb" using sdk hcc_usb_host.c as protocol reference:
   │      boot download + kernel channel + misc char device /dev/ws73hci
   │      artifact: driver/ws73usb/ (this repo)
   ▼
Phase 3  user-space protocol stack (the right way — not limited to vendor AT drivers)
   │      port OpenHarmony nearlink_service: strip OHOS deps (hilog/samgr/napi...)
   │      + wire SleDliLayerAdapter's 6-function HAL seam to /dev/ws73hci
   │      artifact: stack/ (this repo) ← Apache-2.0 compliant
   ▼
Phase 4  applications: peer-to-peer NearLink messaging / HID peripherals / UART passthrough
```

## Risks

1. **DLI opcode dialect vs WS73 hcc** interop unverified — Phase 1 probes confirm it
2. OpenHarmony stack strip-down effort (bundle.json lists ~50 OHOS deps, most stub-able)
3. WS73 firmware blobs encrypted/signed (firmware/us/ws73.bin is opaque `data`; the hcc download path includes SHA256 but encryption unknown)
4. Licensing: OpenHarmony stack Apache-2.0 ✅; HiSilicon SDK source belongs to CompanyNameMagicTag — reference only, not committed wholesale
