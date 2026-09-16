---
type: harvest
title: "NearLink UWB-Like Ranging Suite (zhuzhengyan50-spec) — SLE Channel Sounding, Program Knowledge"
language: en
created: 2026-09-12
tags: [harvest, nearlink, like, ranging]
sources:
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging"
trust: B
stale_after: 2027-03-12
---

# NearLink UWB-Like Ranging Suite (zhuzhengyan50-spec) — SLE Channel Sounding, Program Knowledge

> Ticket: harvest of `zhuzhengyan50-spec/nearlink-uwb-like-ranging` (hot discovery: most recently pushed NearLink repo, 2026-09-12). Ask: first public multi-anchor SLE Channel Sounding implementation — what is reusable for the WS73 tri-mode stack, especially the missing ranging direction.
> Date: 2026-09-12. Scope: read-only program/source knowledge. No builds, no hardware, no PCB. English-only per `AGENTS.md`. Every factual claim cites `path:line` (harvest repo) or a local repo path (nearlink repo).
> Clone: `https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/tree/main/` (5.8M, 65 files, single commit `34c82f1` 2026-09-12 by Zhengyan Zhu). This report file only.

---

## 0. Sources checked

| Source | What it tells us |
|---|---|
| `README_EN.md:1-145` | Project identity, capability matrix, field-test claims, limitations, roadmap |
| `docs/ARCHITECTURE.md:1-43` | Three-role architecture and data path |
| `docs/SDK_INTEGRATION.md:1-70` | How the sample plugs into the HiSpark/BS2X SDK tree |
| `firmware/sle_measure_dis/README.md` | Role/address config, verified environment, build options |
| `firmware/sle_measure_dis/PROTOCOL.md:1-169` | Authoritative wire + serial protocol |
| `firmware/sle_measure_dis/sle_measure_dis_protocol.h:1-79` | Wire structs with compile-time size assertions |
| `firmware/sle_measure_dis/sle_measure_dis_client/sle_measure_dis_client.c` (1477 lines) | Client-side CS start/retry, SSAP CCCD, connection FSM |
| `firmware/sle_measure_dis/sle_measure_dis_client/sle_measure_dis_client_slem.c` (209 lines) | Per-connection IQ reassembly slots |
| `firmware/sle_measure_dis/sle_measure_dis_server/sle_measure_dis_server_alg.c` (487 lines) | Distance smoothing via SDK `slem_smooth` |
| `host/algorithm.py:1-106` | 2D/3D positioning (ULS + Gauss-Newton) |
| `host/parse_iq_raw.py` (690 lines) | Collector log/IQ decoder |
| `host/docs/positioning.md`, `host/docs/sensing.md` | Method boundaries and scoring formulas |
| Local: `sdk/ws73_sdk_linux_WS73_1.10.110/include/bsle/sle/sle_hadm_manager.h` | WS73-side API isomorphism evidence |

Identity note: the repo explicitly states it is **not** a UWB implementation — "UWB-Like" describes the multi-anchor positioning experience only; the technology is NearLink SLE Channel Sounding (`README_EN.md:5-8`). It is a genuine NearLink project (not a name collision): C firmware + Python host, Apache-2.0 for new code with upstream-SDK-derived files keeping original notices (`README_EN.md:143-144`, `NOTICE`).

---

## 1. Why this is the highest-value harvest of the cycle

Our stack has long had a standing gap: ranging. `RESEARCH-DIRECTIONS.md` direction 6 records "READ_MEASURE_CAPS/SET_MEASURE_EN verified, real-pair testing pending; OpenSparklink has no ranging implementation". Prior reports (`WS63-HADM-LL.md`, `OHOS-HADM-FULL.md`) documented DLI-level HADM plumbing from firmware dumps, but **no public end-to-end ranging reference existed** until this repo. It is:

- the first public multi-anchor SLE Channel Sounding implementation (anchor + ranging client + collector firmware, `firmware/sle_measure_dis/README.md`);
- the first public host-side SLE positioning/sensing suite (ULS + Gauss-Newton, Kalman, CFR/phase/MUSIC analysis, `host/`);
- field-tested: BearPi-Pico H2821E + external 3 dBi whip antenna, >100 m LOS range while retaining original SDK calibration values (`README_EN.md:48-51`); ~2 Hz per link, ~8 ranging results/s per client across 4 anchors (`firmware/sle_measure_dis/README.md` verified-environment table).

---

## 2. Three-role architecture (the pattern to copy)

Roles (`docs/ARCHITECTURE.md:5-12`):

| Role | Participates in Channel Sounding | Serial output |
|---|---|---|
| Anchor / Server (up to 4) | Yes — receives client IQ, captures local IQ, computes distance, forwards | start/connect/exception logs |
| Ranging Client | Yes — connects anchors, starts CS, uploads local IQ | connection status + first-frame hints |
| Collector | **No** — aggregates and prints to serial | distance metadata; optional bidirectional IQ |

Key insight (`README_EN.md:23-24`, `ARCHITECTURE.md:12`): the Collector exists so that **all experimental data does not have to exit through the Ranging Client's serial port** — the mobile terminal stays focused on connect + measure, because heavy serial logging can block the system (limitation recorded at `README_EN.md:126-127` and enforced by a firmware build option controlling whether the Collector prints IQ).

Data path (`ARCHITECTURE.md:14-21`): Client scans/connects by configured identity address → each link runs CS producing distance/RSSI/ToF/timestamps + bidirectional IQ → Anchor and Client data converge at Collector → Collector emits one `COLLECT_SAMPLE_META` then `COLLECT_LOCAL_IQ`/`COLLECT_REMOTE_IQ` fragments → GUI groups samples by metadata line, reorders by `seq`, updates positioning/analysis/sensing per anchor-client link.

Address scheme (`sle_measure_dis_protocol.h:16-19`, firmware README): anchors share a fixed prefix with last byte `1..4` distinguishing anchors; Ranging Client address byte `3`, Collector address byte `4`; each Ranging Client must use a **different configured address** or the Collector/host cannot distinguish terminals (`ARCHITECTURE.md:27-29`).

---

## 3. Wire protocol (authoritative, `PROTOCOL.md` + `sle_measure_dis_protocol.h`)

### 3.1 Application frame

8-byte header + payload (`PROTOCOL.md:5-13`): `type` u32 LE, `len` u32 LE, `data[len]`. Receiver must check total frame length before parsing payload. Struct with zero-length tail: `measure_ids_msg_t` (`sle_measure_dis_protocol.h:44-48`), size pinned by static assertion `:74`.

### 3.2 Message types (`PROTOCOL.md:17-25`, `sle_measure_dis_protocol.h:26-35`)

| Value | Name | Direction |
|---:|---|---|
| `0xFFFFFFEA` | `SLEM_PROFILE_MSG_IQ` | Ranging Client → Anchor (client IQ upload) |
| `0x10` | `SLEM_MSG_DIST_RESULT` | Anchor → Ranging Client (4 or 5 bytes: `dist_mm` u32 + optional `rssi` i8) |
| `0x11` | `SLEM_MSG_FINAL_RESULT` | Collector → Anchor (text round result, header 3 B + items 6 B, ver 0x01) |
| `0x12` | `SLEM_MSG_COLLECT_LOCAL_IQ` | Anchor → Collector (anchor-local IQ, payload = 4 B identity header + 332 B IQ struct) |
| `0x13` | `SLEM_MSG_COLLECT_REMOTE_IQ` | Anchor → Collector (client-remote IQ, same layout) |
| `0x14` | `SLEM_MSG_SERVER_LOCAL_IQ` | Anchor → Collector (reserved) |
| `0x15` | `SLEM_MSG_COLLECT_DIST_RESULT` | Anchor → Collector (32 B distance metadata) |

### 3.3 32-byte distance metadata (`PROTOCOL.md:38-56`, `sle_measure_dis_protocol.h:57-72`, size asserted `:76`)

`anchor_id` u8, `client_id` u8, `sdk_rssi` i8, reserved, `conn_id` u16, reserved2, `dist_mm` u32, `local_timestamp_sn` u32, `remote_timestamp_sn` u32, `local_tof_result` u32, `remote_tof_result` u32, `local_rssi` u8, `remote_rssi` u8, reserved3. All multi-byte fields BS21E little-endian (`sle_measure_dis_protocol.h:43`).

### 3.4 332-byte IQ struct — `sle_channel_sounding_iq_trans_t` (`PROTOCOL.md:70-87`)

Fixed layout: `samp_cnt` u8 (valid points, ≤80), `rssi` u8 (raw byte), `es_sn` u16, `timestamp_sn` u32, then 80 × (I u16 + Q u16, LE, 4 bytes/point) at offset 8, then `tof_result` u32 ("Mode 3 TOF") at offset 328. Build constants: `MEASURE_DIS_IQ_REPORT_CNT_MAX=1`, `SLE_CS_IQ_REPORT_COUNT=80`, hence `IQ_DATA_MAX=80`. Semantics to preserve: I/Q are declared `uint16_t` in the firmware struct — save raw 0..65535 losslessly; convert to `int16_t` two's complement only when an algorithm requires signed input (`PROTOCOL.md:87`).

### 3.5 Collector serial text encoding (`PROTOCOL.md:89-113`)

Per sample: `COLLECT_SAMPLE_META` then `COLLECT_DIST` (32 B → typically 2 lines), `COLLECT_LOCAL_IQ` (332 B → 14 lines), `COLLECT_REMOTE_IQ` (14 lines). Encoding: uppercase hex, 2 chars per byte, ≤24 bytes/line, `seq` restarts at 0 per block and must be contiguous. The Collector strips the 4-byte device-identity header; serial IQ bytes start at `samp_cnt`. The `COLLECT_*` labels are fragment groupings, **not** Channel Sounding measurement sequence numbers. Host parsing contract (`PROTOCOL.md:115-161`): line-by-line tag dispatch, per-block buffer with `seq=0` reset, contiguous-`seq` check, exact-length check (32/332) before struct unpack, `(client_id, anchor_id, timestamp_sn)` primary key, drop incomplete samples on line loss and resync at next `seq=0`.

---

## 4. Firmware API surface (BS21E / fbb_bs2x SDK dialect)

Verified environment: board `BearPi-Pico H2821E`, LiteOS, build target `standard-bs21e-1100e`, HiSpark Studio, 4 anchors, 2 trackable clients (`firmware/sle_measure_dis/README.md`). Roles are selected one-at-a-time in Kconfig (`SAMPLE_SUPPORT_SLE_MEASURE_DIS_SERVER=y` for Anchor) and rebuilt/burned per role; each Anchor needs a distinct `MEASURE_DIS_ANCHOR_ID` 1..4.

APIs observed (grep across `sle_measure_dis*.c/h`), with the load-bearing call sites:

- **CS control**: `sle_set_channel_sounding_param_ex(conn_id, &param)` at `sle_measure_dis_client.c:715` (param initialized `:706-712`, `is_cs_param_chg=0`), `sle_set_channel_sounding_enable(conn_id)` at `:729`.
- **CS callbacks**: `cs_caps_cb` / `cs_state_changed_cb` / `cs_param_cb` / `cs_iq_report_cb` / `cs_retry` registered via `sle_hadm_register_callbacks` (2 registration sites).
- **Retry scheduling**: `measure_dis_client_schedule_cs_retry(index, now, ret)` defined `sle_measure_dis_client.c:767`, invoked on CS failure at `:827,:837` — explicit CS-retry state machine, worth copying.
- **Standard SLE face**: `sle_connection_register_callbacks` (`:935`), `sle_dev_manager_register_callbacks` (`:1005`), `sle_announce_seek_register_callbacks` (`:1018`), `sle_connect_remote_device(&addr)` (`:984`), `sle_start_seek()` (`:1402`), `sle_start_announce`/`sle_set_adv*` on the server side.
- **SSAP face**: `ssapc_register_callbacks` (`:1256`), CCCD written to `property_handle + 1` (`:1084`, `SSAP_PROPERTY_TYPE_VALUE` `:1086`, `data_len=2`) — **same `handle+1` CCCD convention as our `stack/ssap` P0-6**; notification path `sle_server_ntf_by_addr` for the anchor→client direction.
- **Device-side distance smoothing** (server): SDK-provided `slem_smooth.h` / `slem_alg_smooth_dis.h` (`sle_measure_dis_server_alg.c:11-12`); constants `TOF_DEFAULT 2070`, `DIS_ALG_MODE 5`, `DIS_ALG_RSSI_LIMIT (-120)`, `DIS_ALG_THRESHOULD_COND2 20`, `DIS_ALG_R_START 2`, `MEASURE_DIS_ALG_KEY_NUM 2` (`:16-21`); flow `slem_posalg_set_base_para` (`:129`) → `slem_alg_calc_smoothed_dis` (`:206`) → `dist_mm = (uint32_t)(dis_smoothed * 1000)` (`:219`); global init `slem_init_smooth()` (`:418`).
- **Per-connection slot discipline**: client keeps `g_local_iq_slots[MAX_SERVERS]` with per-link `next_idx` reassembly state and clears slots on disconnect to avoid stale-fragment mixing (`sle_measure_dis_client_slem.c:22-56`); server keeps `g_measure_dis_alg_slots` with `key_id = slot % 2` for the smoothing algorithm and logs when connections share a key (`sle_measure_dis_server_alg.c:25-72`); a not-ready counter limits warnings when IQ is dropped before a link is prepared (`sle_measure_dis_client_slem.c:31-33`).

Naming trap for our notes: this "SLEM" prefix is the **SDK's built-in ranging-smoothing/algorithm library** (`slem_smooth`, `slem_alg_*`) plus a custom IQ profile message — it is **not** the OHOS SLEM network facade documented in `OHOS-NAI-LAYER.md`. Same token, different layer.

---

## 5. Host stack (`host/`)

### 5.1 Positioning — `host/algorithm.py:4-106` `GnUlsPositioning`

Two-step solver, dimension-agnostic 2D/3D:

1. **ULS initial guess** (`:42-72`): subtract squared-distance equations using anchor 0 as reference; `A[i-1,:] = 2*(a_i - a_0)`, `b[i-1] = (d0² - di²) - (a0² - ai²)`; solve with `numpy.linalg.lstsq`.
2. **One Gauss-Newton step** (`:74-106`): Jacobian row = unit direction `(t - a_i)/d_pred`; residual `r = d_meas - d_pred`; `t_new = t + delta`. The sign convention is self-consistent and documented inline (`:98-102`).

Method contract (`host/docs/positioning.md`): only anchors with distance > 0 and configured coordinates participate; 2D needs ≥3, 3D ≥4; collinear/coplanar geometry makes the matrix ill-conditioned; previous position is only a warm start; exactly **one** GN step is taken (not an iterative-to-convergence optimizer); the open-source version deliberately contains **no LSTM or heuristic ranging correction and does not modify SDK distances with IQ features**. Stated improvement directions: weighted least squares from link quality, condition-number warnings, robust loss for outlier distances, full iterative GN, calibration-based evaluation.

### 5.2 Ingestion — `host/parse_iq_raw.py` (690 lines)

Line-regex driven (`COLLECT_META_RE` `:33-40`, `COLLECT_HEX_RE` `:37-40`); `RangingResult`/`IQPacket`/`IQFeatures` dataclasses (`:37-97`); feature extraction (zero-crossing `:141`, stats `:152`); `StreamParser` with per-block state (`:259`); file + serial modes (`:451`); CSV/JSON outputs (`:494`).

### 5.3 Link sensing — `host/docs/sensing.md`

Per-link scores from paired bidirectional IQ, robustly standardized (median/IQR) then calibrated-sigmoid mapped to [0,1]:

- **Blockage score**: `magnitude_cv` w 0.35 + `local_rssi` w 0.30 + `remote_rssi` w 0.30 + `phase_rmse` w 0.05.
- **Dynamic score** (5-frame window per `(anchor, client, connection)`): `frame_mag_corr` 0.35 + `frame_mag_diff_mean` 0.20 + `mag_time_var` 0.20 + `time_fluctuation` 0.25.
- **Reliability** = `1 - (0.6 × blockage + 0.4 × dynamic)`.

Explicit caveat: research features, not certified human-presence detection (`README_EN.md:128-129`). GUI also adds Kalman position smoothing (`host/gui/services/kalman_filter_service.py`), link activity heatmaps, dataset labeling with Raw/Feature/Temporal exports (`host/gui/services/dataset_export_service.py`, `nearlink_research_export.py`), and a regression test suite (`host/tests/test_core.py`).

---

## 6. WS73 adaptation evidence — the decisive finding

The WS73 Linux SDK exposes an **isomorphic Channel Sounding API face** in `sdk/ws73_sdk_linux_WS73_1.10.110/include/bsle/sle/sle_hadm_manager.h` (342 lines):

| API | WS73 header line | BS21E usage in harvest repo |
|---|---|---|
| `sle_set_channel_sounding_param_ex` | `:282` | client `:715` |
| `sle_set_channel_sounding_enable` | `:299` | client `:729` |
| `sle_set_channel_sounding_disable` | `:316` | (client-side disable on retry path) |
| `sle_hadm_register_callbacks` | `:333` | both roles |
| `sle_read_local_channel_sounding_caps` | `:240` | capability discovery |
| `sle_read_remote_channel_sounding_caps` | `:259` | capability discovery |
| `sle_channel_sounding_iq_report_t {es_sn u16; timestamp_sn u32; tof_result u32; ...}` | `:68-83` | same-named, same-typed fields as the harvest IQ struct |
| callbacks `cs_state_changed_cb`, `cs_iq_report_cb` (`sle_hadm_callbacks_t`) | `:215,:219` (type decls `:172,:197`) | same callback family |

So the harvest repo's Ranging Client pattern maps onto a **WS73 dongle acting as Ranging Client with the Linux host doing positioning** — exactly the Collector/GUI split this repo already demonstrates. This extends the "OHOS = same wire protocol as device firmware" thesis to the CS/HADM subsystem: WS73 and BS21E are same-family FBB stacks with isomorphic CS headers (consistent with `WS63-VS-WS73.md`).

Delta to verify on real hardware (open items, not established facts):

- WS73 `sle_channel_sounding_iq_report_t` is a **single-report** callback payload; the BS21E sample reassembles up to 80 points per `sle_channel_sounding_iq_trans_t` (`PROTOCOL.md:76-87`). Whether one WS73 callback already carries the full point set or requires multi-callback reassembly must be measured.
- The WS73 SDK contains **no** `slem_smooth`/`slem_alg` headers (grep across `sdk/ws73_sdk_linux_WS73_1.10.110` = no hits). Device-side smoothing is unavailable; smoothing/positioning must run host-side — which matches the repo's host architecture anyway, and our host C/Rust layer is the natural home (`stack/ssap/`, future `rust-ws73`).
- `sle_measure*` symbols: no hits in WS73 headers — the repo's `sle_measure_dis_msg_*` helpers are sample-local, not SDK-required; our port would define its own framing (or reuse this repo's 8-byte frame as-is, Apache-2.0).
- Whether the WS73 Linux stack actually surfaces these APIs to userspace via `/dev/hwsle` + our SSAP transport, or only inside `sle_soc.ko`-internal paths, is a build/HITL question — out of scope for this read-only pass, queued as an issue.

---

## 7. Comparison anchors (what is new vs prior reports)

| Prior report | What it covered | What this repo adds |
|---|---|---|
| `SLE-MEASURE-QOS.md` | PHY 1M/2M/4M, MCS 0-10, CI 125µs units, ACB credit gating | Not QoS-focused; orthogonal — this is the CS ranging plane, no PHY/MCS tuning present |
| `WS63-HADM-LL.md` | WS63-side HADM 6 functions, 0x2005 polarity (enable=0=START), ACB-scheduled low-latency | BS21E-side dialect differs: `sle_set_channel_sounding_param_ex/enable` + `cs_*_cb` family; confirms cross-family API divergence documented at WS63-VS-WS73 level |
| `OHOS-HADM-FULL.md` | DLI 0x2003 full params (`measureConfigDirect=0x90010004`), 0x0028 IQ events unpacked 3 B / 12-bit, ToF×3/100 = cm | Chip-SDK-native `sle_channel_sounding_iq_trans_t` is one level higher: 4 B/point u16 I/Q, plus `es_sn`/`timestamp_sn`/`tof_result` metadata; plus device-side smoothing (OHOS stack has none) |
| `OHOS-NAI-LAYER.md` | OHOS "SLEM" thin network facade | Token collision only — this repo's `slem_*` is the SDK ranging-smooth library + custom IQ profile; different subsystem entirely |
| `stack/ssap` P0-6 CCCD | CCCD at `prop_handle+1` enables notify | Repo uses the identical `property_handle+1` convention (`client.c:1084`) — interop convention holds across stacks |

---

## 8. Reusable patterns for the WS73 stack (concrete takeaways)

1. **Collector-style role split** — decouple measurement (dongle/host client) from data aggregation (separate process/board) to protect ranging timing from logging pressure; maps directly onto our Linux host architecture.
2. **Bidirectional IQ pairing by `timestamp_sn`** with `(client_id, anchor_id, timestamp)` keys; drop-and-resync on serial line loss (`PROTOCOL.md:119-159`) — robustness contract our future ingest layer should adopt verbatim.
3. **Per-connection slot state** for fragment reassembly and algorithm context, cleared on disconnect (`sle_measure_dis_client_slem.c:22-56`, `sle_measure_dis_server_alg.c:25-72`) — same shape as our `ssap_link` timeout buckets; reuse the pattern, not the code.
4. **CS-retry scheduling** (`client.c:767,827,837`) — CS enablement needs a bounded retry FSM, not fire-and-forget; feed into our `hwsle_transport` pending-command design from `OHOS-DLI-LAYER` findings.
5. **Compile-time IQ-output kill switch** so serial logging cannot starve the measurement loop (`README_EN.md:126-127`).
6. **8-byte `{type u32 LE, len u32 LE}` frame** is Apache-2.0 and trivially compatible with our HCC ACB bearer; candidate for the future WS73 ranging app protocol instead of inventing framing.
7. **Host-side GnUls + optional Kalman** (`algorithm.py`) is the positioning pipeline to port to our daemon (host C or Rust); the ULS+GN math is small, self-contained, and already regression-tested upstream.
8. **Sensing scores** (blockage/dynamic/reliability weights above) are a research-grade starting point for TV-box presence/interference features — clearly labeled non-certified.

## 9. Not directly reusable / boundaries

- Build target is `standard-bs21e-1100e` (BS2X SDK); no WS63/WS73 build provenance in-repo — firmware must be re-integrated per `SDK_INTEGRATION.md` into whichever SDK we use.
- Hex-text serial protocol is bandwidth-inefficient by design (`README_EN.md:128`); versioned binary transport with CRC is still on their roadmap (`README_EN.md:139`) — do not treat the serial layer as a long-term transport.
- Field-test range (>100 m) is board/antenna/RF-specific (`README_EN.md:52-53`); not transferable to our dongle without our own tests.
- No SLE security/pairing in the sample (plain connections); our SM-layer plan from `OHOS-SM-SECURITY.md` remains separate.
- The 4-anchor ceiling is a measured stability result, not a protocol limit (`README_EN.md:126`); the host GUI does not impose it.

## 10. Actions (issue candidates)

- **Issue A (P1)**: WS73 ranging-client prototype — extend `stack/ssap` with a `sle_hadm` seam using `sle_hadm_manager.h:282,299,316,333`, port the 8-byte frame + IQ reassembly slots, host-side GnUls positioning; validate single- vs multi-report IQ on real hardware.
- **Issue B (P2)**: IQ structure delta matrix — WS73 `sle_channel_sounding_iq_report_t:68-83` vs BS21E `sle_channel_sounding_iq_trans_t` (80-point capacity): document capacity, fragmentation (`report_idx`), and RSSI semantics after first dual-dongle capture.
- **Issue C (P2)**: adopt the repo's `seq=0` resync + exact-length contract in our future Collector/ingest tooling; keep raw serial logs alongside parsed CSV/Parquet with protocol version.

---

## 11. File index

Harvest repo (all under `https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/tree/main/`):

- `README_EN.md:1-145` — identity, highlights, field test, limitations, roadmap, license
- `docs/ARCHITECTURE.md:1-43` — roles, data path, config boundaries, host structure
- `docs/SDK_INTEGRATION.md:1-70` — CMake/Kconfig integration, signing tool note (`riscv32-linux-secmain.exe`)
- `firmware/sle_measure_dis/README.md` — roles, verified environment (2 Hz/link), Kconfig per-role build
- `firmware/sle_measure_dis/PROTOCOL.md:1-169` — frames, message types, 32 B metadata, 332 B IQ, serial encoding, host parsing rules
- `firmware/sle_measure_dis/sle_measure_dis_protocol.h:1-79` — wire structs + size assertions
- `firmware/sle_measure_dis/sle_measure_dis_client/sle_measure_dis_client.c:706-729,767,827,837,935,984,1005,1018,1084-1088,1256,1402` — CS param/enable, retry, callbacks, SSAP CCCD
- `firmware/sle_measure_dis/sle_measure_dis_client/sle_measure_dis_client_slem.c:22-56` — per-link IQ slots
- `firmware/sle_measure_dis/sle_measure_dis_server/sle_measure_dis_server_alg.c:11-21,129,206,219,418` — SDK smoothing integration
- `host/algorithm.py:4-106` — GnUls positioning
- `host/parse_iq_raw.py:33-40,37,141,152,259,451,494` — ingest/decode/export
- `host/docs/positioning.md`, `host/docs/sensing.md` — method boundaries and scoring formulas
- `host/gui/services/kalman_filter_service.py`, `dataset_export_service.py`, `nearlink_research_export.py`, `host/tests/test_core.py` — research services

Local nearlink repo (isomorphism + gap evidence):

- `sdk/ws73_sdk_linux_WS73_1.10.110/include/bsle/sle/sle_hadm_manager.h:282,299,316,333,240,259,68-83,120,148,172,197,215,219` — isomorphic CS API + same-typed IQ fields
- `stack/ssap/` — SSAP stack with CCCD `handle+1` convention (P0-6), no ranging seam yet
- `.scratch/nearlink-driver/lab-notes/SLE-MEASURE-QOS.md`, `WS63-HADM-LL.md`, `OHOS-HADM-FULL.md`, `OHOS-NAI-LAYER.md`, `WS63-VS-WS73.md` — prior anchors

---

*Written 2026-09-12, read-only synthesis of a cloned public repo + local header evidence. English-only per `AGENTS.md`. Whitelist `.gitignore` respected — no binaries committed. Network use limited to anonymous GitHub clone/API for provenance.*
