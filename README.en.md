# nearlink-E573H-USBDC

> [中文版 README](README.md) · English (current)

Turn the USB `ffff:3733` ("00000000") — a **HiSilicon WS73 tri-mode dongle** (Bluetooth + WiFi 6 + NearLink SLE) — into a **tri-mode wireless adapter** for a Linux TV box: real high-speed WiFi / Bluetooth / NearLink, plus the control interface. Core output: **a fully reverse-engineered WS73 NearLink control plane + a self-written SSAP userspace protocol stack** (x86-compilable, resource-adaptive).

> **⚠️ read before pushing — doc maintenance checklist**: see [Document index & maintenance](#document-index--maintenance).
> The pre-push hook (`scripts/check-docs.sh`) enforces: README cross-links, doc-index completeness, English-only docs/, whitelist gitignore, and **README zh/en sync**.

## Project status (2026-08)

### ✅ Hardware side (verified on real dongles)
| Milestone | Status |
|---|---|
| Boot firmware download handshake (WRITEM/FILES/QUIT) | ✅ both dongles |
| NearLink SLE control plane (adv/scan/connect/ranging/data-links/security) | ✅ all commands accepted |
| Bluetooth (hci1, LE scan finds devices) | ✅ verified |
| WiFi (wifi_soc loads) | ⚠️ lazy-init PM deadlock (documented; needs exclusive PM) |
| SLB capability | ❌ WS73 has none (zero SLB in SDK/firmware; spec is member-only) |

### ✅ Software side (self-written)
```
stack/ssap/                          # SSAP userspace stack (Apache-2.0 port + own code)
├── ssap_codec   PDU encode/decode (0x01-0x14, byte-level)
├── hwsle_transport  /dev/hwsle ACB frame adapter (tcid 0x0A)
├── ssap_server  service table + request dispatch (EXCHANGE/FIND/READ/WRITE/NOTIFY)
├── ssap_link    DLI connect state machine (0x1401→0x0015→0x1802/1804)
└── feature_mgr  heuristic feature switching (capacity/RAM/state/peer)
```
Tests: codec + server + feature suites all green, x86 zero-dependency build.

### 📚 Intel library (17 research docs)
Full NearLink protocol deep-dives: SSAP dialect comparison (**OHOS = same wire protocol as device firmware — port path proven**), connection manager, data plane (DTAP/SDR), security pairing, ranging, standard params (SLE 12Mbps/250µs/256 users).

## Repository layout

```
.
├── stack/ssap/          # self-written SSAP userspace stack (core asset)
├── scripts/             # ws73-probe×3 + load-driver/flash-dongle + check scripts
├── docs/                # English intel (DEVICE/SDK/USB-PROTOCOL/ECOSYSTEM)
├── sdk/                 # HiSilicon WS73 SDK (sources + x86 port, binaries gitignored)
└── .scratch/            # wayfinder decision map + 17 research docs + shifu list
```

## Document index & maintenance

> **Rule**: `docs/` is English-only; the two READMEs cross-link; update the table below after doc changes.

| File | Description | Lang |
|---|---|---|
| `README.md` / `README.en.md` | Project overview (cross-linked) | zh/en |
| `docs/DEVICE-INTEL.md` | ffff:3733 device enumeration intel | en |
| `docs/SDK-INTEL.md` | WS73 SDK structure/build/reusable pieces | en |
| `docs/USB-PROTOCOL.md` | HCC-over-USB protocol essentials | en |
| `docs/ECOSYSTEM.md` | NearLink open-source ecosystem map + roadmap | en |
| `stack/ssap/` | SSAP userspace stack (codec/transport/server/link/feature) | — |
| `scripts/` | test/verify/check scripts | — |
| `.scratch/nearlink-driver/lab-notes/` | **17 research docs** (SSAP dialect/CM/DTAP/SDR/SM/HADM/standard…) | zh+en |
| `.scratch/nearlink-driver/lab-notes/SHIFU-BUILD-LIST.md` | TV-box cross-compile list (hi3798 SDIO/USB variants) | zh |

## Roadmap

1. **SSAP stack live verification**: dual dongle or NearLink phone → adv/scan/connect/SSAP handshake (stack ready)
2. **WiFi deadlock fix**: exclusive PM then wifi_soc → wlan0 → wpa_supplicant
3. **TV-box bring-up**: hi3798mv310 + SDIO 3.0 (shifu cross-compiles per list)
4. **SLB**: not on WS73 (needs dedicated silicon); SLE is the usable high-speed/low-power line

## License

AGPL-3.0 (SDK belongs to HiSilicon; SSAP stack includes Apache-2.0 ported parts).
