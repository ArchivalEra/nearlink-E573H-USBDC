# nearlink-E573H-USBDC

> [中文版 README](README.md) · English (current)

Turn the USB `ffff:3733` ("00000000") — a **HiSilicon WS73 tri-mode dongle** (Bluetooth + WiFi 6 + NearLink SLE) — into a **tri-mode wireless adapter** for a Linux TV box: real high-speed WiFi / Bluetooth / NearLink, plus the control interface. Core output: **a fully reverse-engineered WS73 NearLink control plane + a self-written SSAP userspace protocol stack** (x86-compilable, resource-adaptive).

> **⚠️ read before pushing — doc maintenance checklist**: see [Document index & maintenance](#document-index--maintenance).
> The pre-push hooks (`scripts/check-docs.sh` + `scripts/check-harvest-archive.sh`) enforce README cross-links, doc-index completeness, English-only docs/, whitelist sanity, README zh/en sync, and block new knowledge/harvest archives unless both README files change in the same push.

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
assets/stack/ssap/                    # SSAP userspace stack (Apache-2.0 port + own code)
├── ssap_codec   PDU encode/decode (0x01-0x14, byte-level)
├── hwsle_transport  /dev/hwsle ACB frame adapter (tcid 0x0A)
├── ssap_server  service table + request dispatch (EXCHANGE/FIND/READ/WRITE/NOTIFY)
├── ssap_link    DLI connect state machine (0x1401→0x0015→0x1802/1804)
└── feature_mgr  heuristic feature switching (capacity/RAM/state/peer)
```
Tests: codec + server + feature suites all green, x86 zero-dependency build.

### 📚 Intel library (141 research docs, OKF bundle)
`knowledge/` is an OKF v0.2 knowledge bundle (root `knowledge/index.md`/`knowledge/log.md` + harvest/intel/decisions domains): 141 concepts = 66 harvest reports (Playjoy vendor-protocol reverse-port and the hispark-rs September increment: FRW ROM slot-261 ABI, NET0 RX-stop ownership contract, fwpkg 0.3.3) + 74 protocol/chip/SDK/driver deep-dives, plus recorded architecture decisions. Coverage: SSAP dialect comparison (**OHOS = same wire protocol as device firmware — port path proven**), connection manager, data plane (DTAP/SDR), security pairing, ranging (including the first public UWB-like multi-anchor SLE Channel Sounding full-stack reference), standard params (SLE 12Mbps/250µs/256 users), 6 OpenSparklink contract reports (DLI/UAPI/CONN-FSM/SSAP/PHY/USB transport), assembly/compiler/linker optimization resources with a general instruction set, and the OHOS ecosystem batches (AT framework/SLE mesh/framework/HDI/SSAP engine/client/HID remote/SA service/ranging/radar/DLI/DTAP/NAI/device-mgr/GLE/BGTP/SM-security/measure-QoS/SSAP-plan-audit/NearLinkSLE/LinkNebula).

## Repository layout

```
.
├── knowledge/           # OKF v0.2 knowledge bundle (harvest/intel/decisions + index/log)
├── assets/              # artifact plane (stack/ssap self-written stack + WS73 SDK reference)
├── scripts/             # ws73-probe×3 + load-driver/flash-dongle + check scripts
├── docs/agents/         # agent operating conventions
└── .scratch/            # wayfinder trackers (nearlink-driver / rust-ws73-tri-mode / knowledge-restructure)
```

## Document index & maintenance

> **Rule**: `docs/` is English-only; the two READMEs cross-link; update the table below after doc changes.

| File | Description | Lang |
|---|---|---|
| `README.md` / `README.en.md` | Project overview (cross-linked) | zh/en |
| `knowledge/` | **OKF v0.2 knowledge bundle**: harvest/intel/decisions domains + index/log (root index.md is the agent-facing progressive-disclosure surface) | mixed (legacy zh tagged via `language`, new docs English) |
| `knowledge/intel/DEVICE-INTEL.md` | ffff:3733 device enumeration intel | en |
| `knowledge/intel/SDK-INTEL.md` | WS73 SDK structure/build/reusable pieces | en |
| `knowledge/intel/USB-PROTOCOL.md` | HCC-over-USB protocol essentials | en |
| `knowledge/intel/ECOSYSTEM.md` | NearLink open-source ecosystem map + roadmap | en |
| `knowledge/intel/SHIFU-BUILD-LIST.md` | TV-box cross-compile list (hi3798 SDIO/USB variants) | zh |
| `knowledge/decisions/knowledge-assets-split.md` | knowledge/assets split decision record | en |
| `assets/stack/ssap/` | SSAP userspace stack (codec/transport/server/link/feature) | — |
| `scripts/` | test/verify/check scripts | — |

## Roadmap

1. **SSAP stack live verification**: dual dongle or NearLink phone → adv/scan/connect/SSAP handshake (stack ready)
2. **WiFi deadlock fix**: exclusive PM then wifi_soc → wlan0 → wpa_supplicant
3. **TV-box bring-up**: hi3798mv310 + SDIO 3.0 (shifu cross-compiles per list)
4. **SLB**: not on WS73 (needs dedicated silicon); SLE is the usable high-speed/low-power line

## License

AGPL-3.0 (SDK belongs to HiSilicon; SSAP stack includes Apache-2.0 ported parts).
