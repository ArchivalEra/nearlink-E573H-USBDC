---
type: harvest
title: "HADM ranging adapter bridge pinned: ParaPair gate triple, closed measure library contract, single-anchor service flow"
language: en
created: 2026-09-17
tags: [harvest, hadm, ranging, ohos, adapter, channel-sounding]
sources:
  - "https://github.com/openharmony/communication_nearlink_service/blob/7068bc43961da5e54eba1225a3a8363d85541585/frameworks/ranging_alogorithm_adapter/src/ranging_alogorithm_adapter.cpp"
  - "https://github.com/openharmony/communication_nearlink_service/blob/7068bc43961da5e54eba1225a3a8363d85541585/frameworks/ranging_alogorithm_adapter/src/ranging_alogorithm_adapter.h"
  - "https://github.com/openharmony/communication_nearlink_service/blob/7068bc43961da5e54eba1225a3a8363d85541585/frameworks/ranging_alogorithm_adapter/include/ranging_alogorithm_adapter_def.h"
  - "https://github.com/openharmony/communication_nearlink_service/blob/7068bc43961da5e54eba1225a3a8363d85541585/services/service/src/hadm/nearlink_hadm_client_service.cpp"
trust: A
stale_after: 2027-03-17
---

# HADM ranging adapter bridge pinned: ParaPair gate triple, closed measure library, single-anchor service flow

## Executive findings

This closes both halves of the queued HADM item at OHOS revision `7068bc43961da5e54eba1225a3a8363d85541585`: the `ParaPair` semantics left as "inferable" by the prior adapter report, and the multi-anchor aggregation question left unread. All anchors refer to that revision.

- **`ParaPair` is a concrete link-quality gate triple, now value-pinned.** Definition `{int8_t rssiLimit; uint8_t rStart; float thresholdCond2}` at `frameworks/ranging_alogorithm_adapter/include/ranging_alogorithm_adapter_def.h:46-50`; the adapter fills it with the single constant `PARA_LIMIT_VALUE = { -98, 2, 20.0f }` (`ranging_alogorithm_adapter.cpp:33`) and copies it into every algorithm invocation (`:111`). Semantics: RSSI floor -98 (dBm by convention with the adapter's other RSSI fields), an integer threshold of 2, and a float threshold of 20.0 consumed by the closed measure library. It is not per-frame adaptive — one static tuning triple for all sessions.
- **The distance math is a dlopen'd closed library, fully bridged.** `HadmRangingAdapter::InitHadm` opens `libnearlink_measure.z.so` with `RTLD_NOW` and resolves exactly three symbols: `measure_alg_func` (calculate), `measure_init`, and `measure_set_algo_mode` (`ranging_alogorithm_adapter.cpp:28-31,61-75`). The mode setter is optional — a nullptr result is tolerated and only the other two are mandatory (`:70-75`). Function pointer typedefs: `errcode_slem (*)(DisResult*, const MeasureAlgPara*)` for the calculator (`ranging_alogorithm_adapter.h:24-28`). The DSP math never appears in the open tree; this header set is the complete open contract of the closed library.
- **Algorithm selection is hardcoded despite the six-mode menu.** `TransferSoundingToAlgPara` always writes `algPara.flagInter = METHOD_ADJ_R_END` (dynamic-r V1, 150 m class) regardless of the mode passed to `InitHadmAlgo` (`:112`; mode only reaches the optional `measure_set_algo_mode` symbol at `:70-74`). The `DisAlgType` menu (1M/2M/1M_2M/ADJ_R_END/V2/V3 with 75/150 m budgets, `ranging_alogorithm_adapter_def.h:55-72`) is therefore library-side selection, not adapter-side per-session choice in this code path.
- **The bridge is stateful through one counter.** `totalCount_` is passed as `algPara.totalCount` and incremented after each transfer (`ranging_alogorithm_adapter.cpp:116,129`) — the only cross-frame state the open code supplies; smoothing streaks inside the closed library depend on it. `keyId` is a constant 0 (`PARA_KEYS_INFO`, `:32,115`).
- **Per-call heap lifecycle with missing failure propagation.** `iqDut`/`iqRtd` are `new[]`-allocated each call (`:99-102`). An allocation guard returns only from the void conversion helper, not from `CalculateHadmDistance`; the caller still invokes `calcHadmDis_` with incomplete parameters and then deletes both arrays (`:139-146`). Consequently the open caller does not leak the surviving array on its normal return path. Whether the closed calculator tolerates a null array is unverified; conversion should report failure before calculation.
- **Single-anchor aggregation verdict: the open tree has none.** `HadmClientService` consumes one `RawAddress` per sounding session: `SaveDutData`/`SaveRtdData` accumulate the two sides of one link, `ReportSoundingIQResult(addr, result)` emits per-address IQ results (`services/service/src/hadm/nearlink_hadm_client_service.cpp:145-148,388,441,464`). No cross-anchor fusion, position solve, or anchor-set manager exists anywhere in `services/` or `frameworks/`. Multi-anchor positioning remains the third-party hosts' job (GnUls GUI, competition LSQ tag) — the official OHOS stack deliberately ends at per-link `DisResult`.
- **Input completeness gate.** `CalculateHadmDistance` rejects non-complete sounding results with `NL_ERR_INVALID_PARAM` before touching the library (`ranging_alogorithm_adapter.cpp:135-136`), mutex-serialized, and logs `disSmoothed` even on algorithm failure (`:147-150`) — useful trace semantics.
- **Offset plumbing confirmed end-to-end.** Local/remote NV offsets and local/remote ToF offsets flow from `NearlinkHadmSoundingResult` fields into `MeasureAlgPara` and are logged (`:123-128`), matching the dual-end calibration scheme already recorded; channel maps are fixed 10-byte arrays (`HADM_CHMAP_BYTE_LEN = 10`, `utils/include/nearlink_hadm_sounding_result.h:27`).
- **SLEM error namespace extends beyond ranging.** Full enumeration (`ranging_alogorithm_adapter_def.h:116-154`): shared 0x8000A400-0x8000A450; ranging 0x8000A450-0x8000A550 (RSSI abnormal, matrix-inverse fail, ToF-IQ mismatch, IQ low energy, ToA below bound); **positioning 0x8000A550-0x8000A650 (`NOTRIGGER` hint, `POS_FAIL`)**; **door lock/unlock fusion 0x8000A650-0x8000A725**; **car in/out 0x8000A725-0x8000A800**. The official stack ships product-level ranging applications (door, vehicle) atop these codes — three downstream use classes in one namespace.

## Boundary

- The closed library's DSP math, smoothing behavior, and the internal meaning of `rStart`/`thresholdCond2` remain unobservable; the pinned values are facts, the interpretation as "gate triple" is inference from field names plus the ranging error codes.
- Adapter is a singleton with a global mutex; concurrent sessions serialize. No re-entrancy or per-link context was found.
- No build or test was run; Apache-2.0 facts recorded, no code imported.
- `measure_set_algo_mode` presence varies by library build (nullptr tolerated), so mode behavior may differ across OHOS releases.
- The service layer was sampled at call-site level (assembly, reporting, save functions), not read line-by-line; absence of aggregation is a tree-wide symbol search plus call-site reading, not an exhaustive line audit of all 487 service lines.

## Reusable

1. Treat `{rssiLimit=-98, rStart=2, thresholdCond2=20.0}` as the vendor's baseline quality gate when configuring our own CS ranging acceptance thresholds.
2. Copy the dlopen bridge shape for our WS73 host: mandatory symbols (calculate/init) versus optional ones (mode setter), singleton + mutex, per-call IQ buffers freed on both paths.
3. `totalCount_`-style monotonic frame counter as the only cross-frame state is a clean contract for a stateless per-frame algorithm — keep it in our ranging daemon.
4. Adopt the four-subrange SLEM error namespace (shared/ranging/positioning/application) for our SSAP tool error reporting; the application subranges (door, car) reveal intended product verticals.
5. Confirmed: no official multi-anchor aggregation exists to port — our host-side anchor fusion remains original work, informed by the harvested third-party solvers.

## Comparison anchors

- [NEW-OHOS-HADM-RANGING-ADAPTER](NEW-OHOS-HADM-RANGING-ADAPTER.md): resolved its two open items — `ParaPair` now value-pinned, multi-anchor aggregation now positively absent in the open tree.
- [NEW-NEARLINK-UWB-LIKE-RANGING](NEW-NEARLINK-UWB-LIKE-RANGING.md): its host-side GnUls fills exactly the aggregation gap OHOS leaves open; combined they bound the design space (per-link confidence vs per-position geometry gates).
- [NEW-DLI-ICB-QUALITY-EVENTS](NEW-DLI-ICB-QUALITY-EVENTS.md): DLI CS caps expose `phaseCaliOffsetCm`/`tofCaliOffsetM` — the same calibration-offset concept the adapter passes as NV/ToF offsets.
