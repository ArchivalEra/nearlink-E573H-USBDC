---
type: intel
title: OpenSparklink PHY / QoS / Advertising / Security / Ranging — Gap Analysis for WS73 Host Stack
language: zh
created: 2026-08-17
tags: []
---

# OpenSparklink PHY / QoS / Advertising / Security / Ranging — Gap Analysis for WS73 Host Stack

Date: 2026-08-17

Scope: what a full-speed SLE data + ranging + security host stack must implement, and how much
OpenSparklink (kernel module `net/sparklink` + userland crates `libsparklink`/`slkd`/`slctl`/`slkmon`) already implements vs. spec-only.

Primary sources (all under `/mnt/hdd/nearlink-stuff/`):
- `OpenSparklink-linux/net/sparklink/sle_phy.rs` (684 L)
- `OpenSparklink-linux/net/sparklink/sle_security.rs` (977 L)
- `OpenSparklink-linux/net/sparklink/sle_adv.rs` (863 L)
- `OpenSparklink-linux/net/sparklink/sle_crypto.rs`
- `OpenSparklink-linux/net/sparklink/sle_dli.rs` (1503 L)
- `OpenSparklink-linux/net/sparklink/sle_conn.rs`, `sle_pdu.rs`, `sle_ssap.rs`, `sle_usb.rs`,
  `sle_uart.rs`, `sle_serdev.rs`, `sle_spi.rs`, `sle_event.rs`, `sle_uapi.rs`, `sparklink_core.rs`, `sle_workers.rs`
- `sparklink/crates/slk-protocol/src/{advdata.rs,types.rs}`
- `sparklink/crates/libsparklink/src/{adapter.rs,event.rs}`
- `sparklink/crates/slkd/src/{controller.rs,security.rs,transport.rs,state.rs}`
- `sparklink/crates/slctl/src/commands.rs`

Context (our side, in this repo): `OHOS-HADM-RANGING.md`, `SLE-CONTROL-PLANE.md`, `OHOS-SM-SECURITY.md`.

Conventions: ✅ implemented and wired through DLI | 🟡 partial / plumbing only | ❌ absent | ❓ unknown
(line references are `file:line`).

---

## 1. PHY / QoS

### 1.1 PHY modes and bands
OpenSparklink models SLE PHY purely as **MCS table + bandwidth + pilot density + TX power** on the
2.4 GHz ISM band. There is **no SLE-1M/2M "PHY mode" concept and no BLE-compatibility/dual-mode PHY** —
grep for `ble`, `bluetooth`, `coexist` in `net/sparklink/*.rs` and all crates returns nothing relevant.

- MCS table 0–12 per T/XS 10002-2025 table 11: `sle_phy.rs:95-187`. MCS0–6 single-carrier (BPSK→16QAM,
  125 kbps–1 Mbps at 1 MHz), MCS7–12 OFDM (up to 256QAM 5/6, 3.33 Mbps at 1 MHz). Data rate scales
  linearly with bandwidth: `data_rate_kbps` multiplies by 1/2/4 MHz (`sle_phy.rs:196-199`).
- Bandwidth 1/2/4 MHz: `sle_phy.rs:580-588`. Pilot density 4:1/8:1/16:1: `sle_phy.rs:520,644-647`.
- Channel map: 79 channels (2402–2480 MHz, 1 MHz each) + AFH hop-sequence generator
  (`sle_phy.rs:249-408`). Hopping: `(last + hop_increment) mod 79`, remap into used set.
- TX power: -20..+20 dBm, `sle_phy.rs:591-597`; default +10 dBm `sle_phy.rs:556`.
- RX sensitivity / SINR: hard-coded `DEFAULT_SINR_THRESHOLDS` per MCS at BER=1e-5 (`sle_phy.rs:533-547`),
  used only for MCS *selection* (`mcs_select`, `sle_phy.rs:210-234`) — not read back from the radio.
- MIMO: enum SISO / SpatialMux2x2 / TxDiversity2x1 / RxDiversity1x2 / Beamforming2x2 with a
  `negotiate_mode()` helper (`sle_phy.rs:419-507`) — **modeled only, no antenna hardware behind it**.

### 1.2 How PHY is configured over DLI
- `SetCodingModulation = 0x180A` (MCS index only): `sle_dli.rs:525,1364-1367`.
- `SetPhyParam = 0x1806` with sub-types: `set_tx_power` sends `[0x01, dbm]`, `set_bandwidth` sends
  `[0x02, mhz]` (`sle_dli.rs:1369-1379`). `ReadPhyParam = 0x1805`.
- `PhyConfig::encode_dli_params()` / `decode_dli_params()` define a 7-byte PHY param record
  (MCS, BW, pilot, tx power, MIMO, antennas) `sle_phy.rs:609-656`.
- Feature bits map bandwidth/pilot/MCS to `SleFeature` bits (`sle_phy.rs:662-683`; `sle_dli.rs:131-262`):
  Bw2m=1<<8, Bw4m=1<<9, Pilot 4:1/8:1/16:1 = bits 10-12, MCS0-12 = bits 14-26. **No bit for 5 GHz**; a
  5 GHz feature constant exists (`Band5ghz = 1 << 38`, `sle_dli.rs:211`) but is never set by any driver.
- Controller drivers claim only what they statically know: USB advertises
  `SLE_MEAS_RSSI | SLE_MEAS_PATH_LOSS` (`sle_usb.rs:1360`); serdev advertises
  `SLE_MEAS_RSSI` (`sle_serdev.rs:274`). `features`/`features_ext` are **never populated** from a real
  device (see §4).

### 1.3 QoS / service types
There is **no SLE QoS class / service-type model** (no reliable/unreliable *service*, no burst / low-latency
classes). The only per-connection transport knobs are:

- `TransportMode { Unreliable, Reliable }` per logical channel — host-side bookkeeping only
  (`sle_conn.rs:54-63`).
- Connection interval / latency / timeout carried in events (`sle_dli.rs:849-871,853-855`) and
  `ConnParamUpdate = 0x1807` + `ConnParamReqReply = 0x1808` (`sle_dli.rs:518-520`).
- `SLE_TRANSPORT_UNRELIABLE / RELIABLE / FRAGMENTED` capability bits (`sle_dli.rs:307-309`); drivers set
  UNRELIABLE (SPI, `sle_spi.rs:397`) or UNRELIABLE|RELIABLE (serdev `sle_serdev.rs:273`, uart `sle_uart.rs:466`).
- Channel MTU/MPS and TX/RX credit fields exist on `TransportChannel` (`sle_conn.rs:76-96`) — see §5.

**DLI service/QoS opcodes: none.** OpenSparklink's DLI opcode map (`sle_dli.rs:404-676`) has no "set
service type"/"set QoS"/burst commands; the closest is the SLB logical-channel group (0x24xx), which is
SLB-only and modeled as wire constants with no host implementation.

### 1.4 PHY verdict
| Capability | Status | Evidence |
|---|---|---|
| MCS 0-12 table + rate calc | ✅ | `sle_phy.rs:95-199` |
| 1/2/4 MHz bandwidth | ✅ (param plumbing) | `sle_phy.rs:580-588`, `sle_dli.rs:1376-1379` |
| 79-ch AFH channel map + hopping | ✅ (host algorithm) | `sle_phy.rs:249-408` |
| TX power set/read | ✅ | `sle_dli.rs:1270-1273,1370-1373` |
| MCS selection by SINR | ✅ (host heuristic) | `sle_phy.rs:210-234` |
| RX sensitivity params | 🟡 (static table only) | `sle_phy.rs:533-547` |
| SLE 1M/2M PHY modes | ❌ (no such concept) | — |
| BLE-compatible/dual mode | ❌ | — |
| 5 GHz band | ❌ (constant only) | `sle_dli.rs:211` |
| MIMO (any mode) | 🟡 (model only) | `sle_phy.rs:419-507` |
| QoS classes / service types / burst / low-latency | ❌ | — |

---

## 2. Advertising

### 2.1 Adv data format
`sparklink/crates/slk-protocol/src/advdata.rs` implements the full TLV codec of TXS-20001-2025 §6.5
(`[type][len][value]`, little-endian multi-byte):

- 19 defined types (`advdata.rs:18-37`): DiscoveryLevel 0x01, AccessLayerCapability 0x02,
  Std/Custom ServiceData 0x03/0x04, Complete/Partial Std/Custom Service lists 0x05–0x08,
  ServiceStructureHash 0x09, Shortened/Complete LocalName 0x0A/0x0B, TxPowerLevel 0x0C,
  SlbDomainName 0x0D, SlbMacId 0x0E, SleMacId 0x0F, MultiHopInfo 0x10, Extended 0xFE, Manufacturer 0xFF.
- Parser tolerates unknown types + structured errors (`advdata.rs:178-334`); builder round-trips
  (`advdata.rs:341-447`); lookup helpers `find_discovery_level / find_local_name / find_tx_power /
  collect_service_uuids16` (`advdata.rs:454-509`). This matches the NearLink spec adv-payload TLV shape.
- Discovery levels Invisible/General/Priority/PairedOnly/Designated (0-4) in `types.rs:54-60` and
  parsed in `advdata.rs:66-75`.

### 2.2 Adv types and scan response
- `BroadcastType` (access/scan mode): NoAccessNoScan=0, AccessibleNoScan=1, NoAccessScannable=2,
  AccessibleScannable=3 (`sle_pdu.rs:50-60`). Connectable/scannable is this 3-bit "accessible" flag —
  there is no separate "connectable adv" enum; connectable = Accessible*.
- `PacketType`: BasicAdv, ExtendedAdv, ScanRequest, ScanResponse, AccessRequest, AccessResponse
  (`sle_pdu.rs:82-96`).
- Basic adv builder emits DiscoveryLevel + TX power + SLE addr + name + service lists TLVs
  (`sle_adv.rs:521-555`); scan result parser reads TLV types 0x01/0x0A/0x0B/0x0F (`sle_adv.rs:577-592`).
- Scan response data is separate from adv data: `ext_adv_set_scan_rsp` (per T/XS 10003-2025 §8.2
  SetBroadcastScanRsp / 0x0C04) `sle_adv.rs:715-733`.
- Extended adv: up to 4 sets (`sle_adv.rs:190`), data up to 1650 B (`sle_adv.rs:194`, single basic adv
  capped at `SLE_ADV_DATA_MAX = 255` `sle_pdu.rs:41`), primary/secondary PHY 1M/2M/coded
  (`sle_adv.rs:199-218`), SID 0-15, TX-power-in-header flag, `extended_adv_timing` (`sle_adv.rs:235-242`).
  Per-set duration (10 ms units) and max-events auto-disable with a ticker (`sle_adv.rs:744-760,840-862`).

### 2.3 Periodic advertising
❌ **No periodic advertising** anywhere: grep for `periodic` hits only the ext-adv *enable* parameters
(`sle_adv.rs:740`), power-save sniff (`sle_power.rs:32`), and the event pump. There is no
periodic-adv state machine, no sync-broadcast/periodic-sync event, and no DLI opcode for periodic adv
in the opcode map.

### 2.4 DLI adv opcodes present
`SetBroadcastParam 0x0C02`, `SetBroadcastData 0x0C03`, `SetBroadcastScanRsp 0x0C04`,
`EnableBroadcast 0x0C05`, `ReadMaxBcastDataLen 0x0C06`, `ReadBcastSetSize 0x0C07`,
`DeleteBcastSet 0x0C08` (`sle_dli.rs:464-477`); scan: `SetScanParam 0x1001`, `EnableScan 0x1002`,
`SetScanReqData 0x1003` (`sle_dli.rs:489-493`). High-level wrappers `enable_broadcast` /
`enable_scan` (`sle_dli.rs:1355-1362`). Extended-adv scan filtering by 16-bit service UUID
(`sle_adv.rs:78-146`).

### 2.5 Advertising verdict
| Capability | Status | Evidence |
|---|---|---|
| Spec-conformant adv TLV codec | ✅ | `advdata.rs:178-447` |
| Discovery levels 0-4 | ✅ | `advdata.rs:66-75`, `types.rs:54-60` |
| Connectable/scannable broadcast types | ✅ | `sle_pdu.rs:50-60` |
| Scan request/response PDUs | ✅ | `sle_pdu.rs:88-96`, `sle_adv.rs:715-733` |
| Basic + extended adv sets | ✅ | `sle_adv.rs:521-555,322-347` |
| Extended adv 1650 B / 4 sets / PHY select / SID | ✅ | `sle_adv.rs:190-194,199-218` |
| Periodic advertising | ❌ | — |
| SLB broadcast path | 🟡 (opcodes only) | `sle_dli.rs:480-485` |

---

## 3. Security / SM

### 3.1 Pairing model
`sle_security.rs` implements the DLI-driven (controller-side-orchestrated) SM, host participates
via events. Model is **mid-model style ECDH-P256 "NEGO→AUTH→ENCP"**, not a legacy static-key model.

- `SecurityMode { EncAndInt, IntOnly, EncOnly, None }` `sle_security.rs:34-44`.
- `PairingMethod { Unpaired, JustWorks, Psk, NumericComparison, PasskeyEntry, Oob, Password }`
  `sle_security.rs:53-73` — all six methods are declared.
- State machine: Idle → Pairing → Paired → Encrypted, plus AwaitingConfirm/AwaitingPasskey
  (`sle_security.rs:82-96`).
- Flow comments map each step to DLI opcodes/events (`sle_security.rs:185-202`):
  RequestPair 0x1C04 → PairInfoExchange evt 0x001E / reply 0x1C09 → PairOptionReport 0x0020 /
  accept 0x1C0B → PairRandom 0x0024 / 0x1C0E → PairConfirm 0x0025 / 0x1C0F → DHKeyVerify 0x0026 /
  0x1C10. Crypto lives in the controller's secure HW, not the kernel.
- Info-exchange reply advertises `io_cap = 0x04` (Keyboard+Display), max key len 16, sec distribution
  0x03, crypto caps AC1+AC2 enc/int, HA1 KDF, KE2 kex (`sle_security.rs:239-254`).
- Option report maps auth_method 0x00-0x05 → NumericComparison/JustWorks/PasskeyEntry/Password/Oob/Psk
  (`sle_security.rs:281-289`).
- **Important caveats:**
  - Confirm value is computed host-side as `SM3(local_pk[..32] || peer_nonce)[..16]`
    (`sle_security.rs:364-370`) and DHKey check as `HMAC-SM3(dhkey, local||remote nonce)[..16]`
    (`sle_security.rs:418-424`). This is **not the standard's confirm algorithm** (OHOS SM uses CMAC over
    Gpub‖Tpub keyed by randomR, and DHKey auth codes over a much larger transcript, see
    `OHOS-SM-SECURITY.md` §1). OpenSparklink's pairing would not interoperate with a spec-conformant peer.
  - ECDH failure falls back to a deterministic test key derived from nonces (`sle_security.rs:403-415`).
  - `on_dhkey_verify` ignores the received `dhkey_check` value entirely (`sle_security.rs:389-393`).
  - Numeric-comparison passkey / passkey-entry / OOB / password APIs are modeled
    (`sle_security.rs:484-607`) but rely on states (`AwaitingConfirm`/`AwaitingPasskey`) that the
    controller-driven event handlers never enter — only the user-facing ioctls reach them.

### 3.2 Crypto algorithms
`sle_crypto.rs` delegates to Linux kernel crypto via C FFI (`sle_crypto_ffi.c`):
- SM3 hash (`sle_crypto.rs:69-81`), HMAC-SM3 (`sle_crypto.rs:139-157`).
- SM4 ECB (`sle_crypto.rs:108-131`) and **SM4-CTR** (`sle_crypto.rs:166-188`).
- ECDH-P256 generate / shared secret (`sle_crypto.rs:227-267`).
- Key derivation: `enc_key = HMAC-SM3(link_key, "sparklink_enc_key")[..16]`,
  `int_key = HMAC-SM3(link_key, "sparklink_int_key")[..16]` (`sle_crypto.rs:196-206`) — **not the
  standard's CMAC-based derivation** (OHOS SM derives over `lk‖Ra‖Rb‖Gaddr‖Taddr`, see OHOS-SM-SECURITY §4).
- **AES-CCM is NOT implemented.** The capability constant `SLE_SEC_AES_CCM` exists
  (`sle_dli.rs:318`), and UART/SPI/serdev drivers advertise `SLE_SEC_AES_CCM` in `security_cap`
  (`sle_uart.rs:49`, `sle_spi.rs:51`, `sle_serdev.rs:44`), but no AES path exists in `sle_crypto.rs`.
  Data-plane encryption is SM4-CTR only, applied by the host to user data
  (`sle_security.rs:650-674`). Real SLE encryption is SM4-CCM/AES-CCM at the *controller* (see below).
- RAL / RPA: 8-entry RAL with IRKs, HMAC-SM3-based RPA generate/resolve + timeout refresh
  (`sle_security.rs:735-977`). RPA uses `hash = HMAC-SM3(IRK, prand)[..3]` and forces prand top bits 0b01
  (`sle_security.rs:877-890`) — resembles but is not guaranteed to match the spec RPA format.

### 3.3 DLI security opcodes implemented
Wire opcodes in `sle_dli.rs:545-615` and decoded in `sle_usb.rs:1259-1262`-style maps:
0x1C01 HashCompute, 0x1C02 GenSecureRandom, 0x1C03 StartEncrypt, 0x1C04 RequestPair,
0x1C05 ReplyEncParamReq, 0x1C06 RejectEncParamReq, 0x1C07 ReadLocalEncAlgo, 0x1C08 StartPairing,
0x1C09 PairInfoExchange, 0x1C0A PairOptionConfirm, 0x1C0B PairOptionAccept, 0x1C0C PairExtData,
0x1C0D PairPasskey, 0x1C0E PairRandom, 0x1C0F PairConfirm, 0x1C10 DhkeyVerify, 0x1C11 PairFail,
0x1C12-0x1C19 RAL/RPA (AddRalDevice, RemoveRalDevice, ClearRal, ReadRalSize, ReadRemoteRpa,
ReadLocalRpa, SetRpaEnable, SetRpaTimeout), plus SLB security 0x1C1A-0x1C23 (opcodes only).
High-level wrappers exist for the pairing exchange: `request_pair`, `pair_info_exchange_reply`,
`pair_option_accept`, `pair_random`, `pair_confirm`, `dhkey_verify`, `start_encrypt`
(`sle_dli.rs:1393-1431`). Security ioctl surface in kernel core `sparklink_core.rs:2394-2532`
(pair with method select, passkey get/confirm/reject/input, OOB, password, PSK, SM3/SM4 test ioctls).

### 3.4 Comparison vs OHOS SM research (`OHOS-SM-SECURITY.md`)
| Aspect | OpenSparklink | OHOS reference |
|---|---|---|
| Pairing flow | DLI event-driven NEGO→AUTH→ENCP | Same DLI 0x1Cxx + control-plane 0x0133-0x0142 handshake |
| Confirm value | SM3(localPk‖peerNonce) — non-standard | CMAC(derivedKeyAlgo, key=randomR, Gpub‖Tpub) |
| DHKey auth codes | HMAC-SM3(dhkey, nonces) — non-standard | CMAC over dk‖Ra‖Rb‖io‖method‖codeAlgo‖psk‖addrs |
| Key derivation | HMAC-SM3 with fixed labels | CMAC/DerivedKey over `lk‖Ra‖Rb‖Gaddr‖Taddr` |
| Link key transport | derived host-side, handed to controller via 0x1C03 | same (0x1C03/0x1C05 params) |
| Data encryption | SM4-CTR host-side (non-standard) | Controller-side SM4-CCM/AES-CCM (AC1/AC2) |
| Key exchange | ECDH-P256 (KE2) | KE1=SM2, KE2=ECDH-P256 negotiated |
| RAL/RPA | ✅ modeled | present |
| AES-CCM | ❌ (cap bit only) | ✅ AC2 |

**Bottom line:** OpenSparklink's security is a coherent *demo/plumbing* implementation with real
wire opcodes and event handling, but its confirm/derivation/encryption algorithms do **not** match the
NearLink standard and will not interoperate with a spec-compliant peer. For a real product the host
should hand the link key to the controller (as OHOS does) and let the controller do SM4-CCM/AES-CCM.

### 3.5 Security verdict
| Capability | Status | Evidence |
|---|---|---|
| DLI pairing command/event surface | ✅ | `sle_dli.rs:545-615,1393-1431` |
| JustWorks / PSK / numeric / passkey / OOB / password methods | 🟡 (declared; only JW/PSK fully wired) | `sle_security.rs:53-73,207-607` |
| ECDH-P256 / SM3 / HMAC-SM3 / SM4 | ✅ | `sle_crypto.rs:69-267` |
| AES-CCM | ❌ | `sle_crypto.rs` (absent) |
| Spec-conformant confirm/auth-code/key-derivation | ❌ | `sle_security.rs:364-430,196-206` |
| StartEncrypt / encryption status events | ✅ | `sle_dli.rs:550,722-724,836-837` |
| RAL / RPA (IRK-based) | ✅ | `sle_security.rs:735-977` |
| SLB security ops | 🟡 (opcodes only) | `sle_dli.rs:596-615` |

---

## 4. Ranging (verdict)

**❌ OpenSparklink has NO ranging / HADM / channel-sounding implementation.** This is the single
biggest gap.

What exists is only *plumbing surfaces*:

1. DLI measurement command opcodes (all four defined and sendable):
   `ReadLocalMeasCap 0x2001`, `SetMeasLinkParam 0x2003`, `MeasAction 0x2005`, `EnableMeas 0x200B`
   (`sle_dli.rs:618-625`).
2. Measurement capability bitmask constants — RSSI, PATH_LOSS, POWER_MONITOR, CHANNEL_MAP
   (`sle_dli.rs:312-315`). USB driver claims `SLE_MEAS_RSSI | SLE_MEAS_PATH_LOSS` (`sle_usb.rs:1360`);
   serdev/SPI claim only `SLE_MEAS_RSSI` (`sle_serdev.rs:274`, `sle_spi.rs:398`).
3. Extended feature bits **named** NB-RANGING / UWB-RANGING / NB-SENSING / UWB-SENSING
   (`sle_dli.rs:266-274`) — **but `features_ext` is never populated by any controller driver**
   (grep for `features_ext` assignment in `sle_usb/sle_serdev/sle_spi/sle_uart` = none). So the
   WS73's actual ranging capability would never be advertised through OpenSparklink.
4. Measurement event decoders for NbfhMeasInfo/StateChange/Params, Local/Peer caps, MeasStateChange,
   MeasQuantityReport (`sle_usb.rs:1041-1143`, `sle_dli.rs:765-778,983-1030`) and their uapi/ioctl
   serialization (`sle_uapi.rs:2248-2342`). `MeasQuantityReport` carries `meas_source`/`seq` but no
   distance/tof field is parsed.
5. Userland plumbing: `slctl meas {cap,on,off}` (`slctl/src/commands.rs:621-644`), D-Bus
   `MeasReadCap/MeasEnable/MeasSetLinkParam/MeasAction` (`slkd/src/controller.rs:396-449`),
   libsparklink ioctls (`libsparklink/src/adapter.rs:853-876`), ioctl structs
   `SleMeasCap/SleMeasLinkParam/SleMeasAction` (`slk-protocol/src/types.rs:901-929`), kernel dispatch
   `sparklink_core.rs:3605-3651`.
6. Nothing anywhere references HADM, channel sounding, TOF, IQ samples, or distance conversion.
   Greps for `ranging|hadm|tof|dtoa|dtof|distance|测距` across `net/sparklink/*.rs` and all crates
   return only: `sle_dli.rs:268-269` (feature bit names), the slctl "Measurement / Ranging" help text
   (`slctl/src/commands.rs:641`), and unrelated seq-distance math in `sle_conn.rs:121-276`.

Compare `OHOS-HADM-RANGING.md`: the real NearLink ranging path is 0x2002 ReadRemoteMeasCaps →
0x2003 SetMeasConfigParam → 0x2005 SetMeasureEn (enable/disable), then vendor IQ-report events with
12-bit I/Q samples, 79-ch map, `tofResult` in 0.1 ns units, distance = tof×0.03 m, cal offsets, G-T/TT
modes (`OHOS-HADM-RANGING.md` §1-6). **None of this exists in OpenSparklink.**

Ranging verdict:
| Capability | Status | Evidence |
|---|---|---|
| Ranging / HADM / channel sounding | ❌ | — |
| NB/UWB ranging feature discovery | ❌ (bits defined, never set) | `sle_dli.rs:266-274` |
| Measurement opcode plumbing (0x2001/0x2003/0x2005/0x200B) | 🟡 (send+events, no semantic engine) | `sle_dli.rs:618-625`, `sparklink_core.rs:3605-3651` |
| IQ / TOF / distance reporting | ❌ | — |
| RSSI measurement | ✅ (basic, for AFH) | `sle_usb.rs:1360`, `sle_conn.rs` AFH |

---

## 5. Data-link QoS (async/sync, ICB vs ACB, credits)

### 5.1 Host channel model
- DLI packet types: Command 0xA1, Event 0xA2, AsyncUnicast 0xA3, SyncUnicast 0xA4, AsyncMulticast 0xA5
  (`sle_dli.rs:99-110`). Data TX on a connection goes out as **AsyncUnicast only**:
  `encode_data(DliPacketType::AsyncUnicast, ...)` in `sle_uart.rs:445`, `sle_spi.rs:368`.
- Transport channels per connection (host side): SLE-CMTC 0x02 (reliable, 48 B MTU), SLE-SMTC 0x0A
  (reliable, 247 B), SLE-DUDTC 0x1F (default data, mode from caps), plus a dynamic reliable SSAP channel
  (TCID 0x80-0xBF) (`sle_conn.rs:45-52,356-385,431-450`).
- **Reliable vs unreliable = credit-flow windowing, implemented host-side for the SSAP/management
  channels only** (`sle_conn.rs:98-159`, `sle_workers.rs:109-127,234-314`): credit grants on CMTC,
  14-bit sliding window, retransmit queue, low-watermark re-grant. The DUDTC data channel is
  `Unreliable` by default (`sle_conn.rs:381`).
- ICB (isochronous) vs ACB (asynchronous) *link* concept: OpenSparklink models **sync links as a
  separate DLI group** rather than a QoS class. Sync unicast 0x2801/0x2803-0x2806, sync multicast
  0x2807-0x280C, sync data path 0x280D/0x280E (`sle_dli.rs:636-662`). Adapter + D-Bus expose
  `sync_ucast_param/create/remove`, `sync_mcast_param/create/remove`, `sync_datapath_cfg/remove`
  (`libsparklink/src/adapter.rs:547-590`, `slkd/src/controller.rs:451-477`). Sync data TX is sent via
  `send_data(cmd.sync_handle, ...)` (`sparklink_core.rs:3207`) — i.e., the same AsyncUnicast packet
  type byte, tagged with a sync handle. There is **no isochronous scheduling model** (no CIG/BIG event
  group timings, no per-stream interval/latency arbitration) beyond opaque parameter structs.

### 5.2 DLI data-path opcodes
There is **no "create data link"/"send data"/"set credits" DLI command** in OpenSparklink's opcode
map — data flows on the DLI *packet types* (0xA3/0xA4/0xA5), not commands. The only data-path
commands are the connection ones (CreateConnection 0x1401, CancelConnection 0x1402, Disconnect 0x1403,
SLB Create 0x1404 — `sle_dli.rs:499-504`). Data send = `SleController::send_data(handle, data)`
(`sle_dli.rs:1140`) → AsyncUnicast frame; host-side API `conn_send`/`conn_recv`
(`libsparklink/src/adapter.rs:209-227`, `sparklink_core.rs:1801-1829`).

### 5.3 Data-link verdict
| Capability | Status | Evidence |
|---|---|---|
| Async unicast data path | ✅ | `sle_dli.rs:105,1140`, `sle_uart.rs:445` |
| Async multicast data path | 🟡 (packet type + feature bits only) | `sle_dli.rs:109,215-229` |
| Sync unicast/multicast link mgmt | 🟡 (commands + structs, no scheduling) | `sle_dli.rs:636-662`, `adapter.rs:547-590` |
| Sync data path cfg/remove | ✅ (command plumbing) | `sle_dli.rs:659-661`, `adapter.rs:583-590` |
| Reliable channel w/ credits + windowing | ✅ (host-side, SSAP/mgmt channels) | `sle_conn.rs:98-159`, `sle_workers.rs:109-127` |
| ICB/ACB QoS classes / service types | ❌ | — |
| Credit grants on data channel (DUDTC) | 🟡 (bookkeeping; not exercised for bulk data) | `sle_conn.rs:89-92,381` |
| Max data length / MTU negotiation | ✅ (SetMaxDataLen 0x1804, events 0x0003) | `sle_dli.rs:513,707-709,857-861` |

---

## 6. Capability matrix (summary)

| Capability | OpenSparklink | We must still build |
|---|---|---|
| PHY MCS/BW/TX-power/AFH host model | ✅ | — |
| PHY param round-trip to WS73 (0x1805/0x1806/0x180A) | ✅ | — |
| QoS/service types, burst, low-latency | ❌ | whole model (spec service-type negotiation) |
| Adv TLV codec (spec TLV) | ✅ | — |
| Basic/ext adv + scan rsp | ✅ | — |
| Periodic adv | ❌ | implement (or skip if not needed) |
| Pairing wire flow (0x1C04-0x1C11) | ✅ | — |
| Spec-correct SM (confirm/auth-code/KDF, SM4-CCM/AES-CCM) | ❌ | conformant SM or lean on controller-crypto |
| Ranging / HADM / channel sounding | ❌ | **full stack** (see §7) |
| RSSI (link management) | ✅ | — |
| Async data + sync link plumbing | 🟡 | sync scheduling + isochronous path |
| Reliable credit transport | 🟡 | exercise on data channel; correct spec PDUs |

---

## 7. What we must still implement ourselves

Device context: WS73 dongle on USB (`ffff:3733`); we already have DLI-level command access (raw
`SL_IOCTL_DLI_SEND_CMD`, `sparklink_core.rs:3213-3237`, and `SleController::send_command`). OpenSparklink
gives us a skeleton; none of the below can be borrowed from it.

1. **Conformant Security/SM (must-fix).** OpenSparklink's confirm/auth-code/KDF formulas do not match
   the standard (`sle_security.rs:364-430,196-206`). For real devices: implement the standard SM
   transcript (per `OHOS-SM-SECURITY.md` §1-4), do ECDH-P256 (KE2) host-side or via controller, then
   hand the 16-byte link key to the WS73 via 0x1C03/0x1C05 and let the controller do SM4-CCM/AES-CCM.
   Also implement the encryption-parameter-request flow (0x000E event → 0x1C05/0x1C06) which
   OpenSparklink only stubs (`sle_dli.rs:722-724,887-888`).

2. **Ranging/HADM (all new).** Follow `OHOS-HADM-RANGING.md`: 0x2001/0x2002 read local/remote
   measurement caps → 0x2003 SetMeasLinkParam (configIndex, interval, duration) → 0x2005 MeasAction
   start/stop → vendor IQ report events. We must: parse the WS73 vendor measurement events (OpenSparklink
   parses only the generic NbfhMeasInfo/MeasQuantityReport skeleton, `sle_usb.rs:1041-1143`); decode
   12-bit I/Q samples and 79-ch map; convert `tofResult` (0.1 ns) → meters (`×0.03`); handle cal
   offsets (`phaseCaliOffsetCm`, `tofCaliOffsetM`); and expose distance via a userspace API. Nothing in
   the crates (slkd/slctl) has any distance semantics yet.

3. **Feature discovery.** Populate `features_ext` NB-RANGING bit (`sle_dli.rs:268`) from the WS73's
   real `ReadLocalFeatures` response and read remote features (`0x1801`, event 0x0016,
   `sle_dli.rs:509,731-733`) to gate ranging on the peer's sounding capability — OpenSparklink never
   sets or reads this.

4. **QoS / service types (all new).** No host-side QoS exists. Decide the WS73's service-type
   negotiation surface (the DLI spec's async/sync service types, ICB/ACB semantics) — you will need to
   probe the dongle with raw DLI commands; OpenSparklink provides no reference.

5. **Sync/isochronous data path.** OpenSparklink can send sync-link commands (0x280x) but has no
   scheduling model. For AV/voice-class traffic implement event-group periodicity / stream parameters
   and validate WS73's CIG/BIG behavior with raw DLI.

6. **Periodic advertising** only if the product needs broadcast-adv (OpenSparklink has none).

7. **Bulk reliable transport on the data channel** — verify the WS73's credit/sliding-window PDUs
   against T/XS 20002-2025 §3.4/§7.3.3; OpenSparklink's `sle_conn.rs` implements a plausible but
   unverified algorithm and uses it mainly for management/SSAP traffic.

---

## 8. Open questions

1. Does the WS73 controller itself run the pairing state machine (as OpenSparklink assumes,
   `sle_security.rs:185-202`), or does the host drive SM as in OHOS? This decides whether the
   non-standard host confirm code matters.
2. Does the WS73 advertise NB ranging in `ReadLocalFeatures` bits 66-72, and is 0x2002
   (ReadRemoteMeasCaps) supported? (Our DLI check notes say 0x1C01-0x1C07 respond; 0x20xx unverified.)
3. What is the vendor IQ-report event code / payload layout on the WS73 vs the OHOS
   0x0028-vendor-0x004A format?
4. Which AES/SM4 modes does the WS73 expect when it receives the link key in 0x1C03 — AC1 SM4-CCM /
   AC2 AES-CCM / EA2 AES-CTR?
5. Does the DUDTC (0x1F) data channel actually use credit flow control on the WS73, or is bulk data
   unreliable-fire-and-forget?
6. Is periodic advertising (or SLB broadcast) required for the TV-box use case at all?
7. What service types does the WS73 accept for high-throughput async links (e.g., large MTU, 4 MHz,
   MCS12 + 2×2 MIMO)? OpenSparklink cannot answer this — needs raw DLI probing.
