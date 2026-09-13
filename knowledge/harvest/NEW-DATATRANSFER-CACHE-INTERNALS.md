---
type: harvest
title: tethering datatransfer cache internals — per-app mapping carries tcid/transMode/frameType with a three-state transfer state machine and previous-state tracking
language: en
created: 2026-09-13
tags: [ohos, tethering, cache, tcid, transfer-state, backpressure, datatransfer, harvest]
sources:
  - url: https://github.com/xingkaiyueying/tethering_nearlink
    note: local clone (pushed 2026-09-11, current); read-only inspection of nearlink_sle_datatransfer_cache.h
trust: verified
stale_after: 2026-12-13
---

# tethering datatransfer cache internals — per-app mapping carries tcid/transMode/frameType with a three-state transfer state machine and previous-state tracking

- Inspection date: 2026-09-13 (same current clone as `NEW-OHOS-TETHERING-SERVICE.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the per-app cache structure behind the datatransfer service's flow control

## Executive findings

1. Each cached app session is an `AppConnectParamMapping`: peer `address` + `randomAddress`, local `dstPort`, **`tcid`** (transmission channel id — the dynamic-TCID concept from feature_mgr appears as a per-session routing field), `transMode`, `frameType`, and a state pair `state` + `transState`/`preTransState`. [`services/service/src/datatransfer/nearlink_sle_datatransfer_cache.h:41-51`]
2. The transfer state is a **three-value enum with a previous-state shadow**: `TRANSFER_FAIL` / `TRANSFER_AVAILABLE` / `TRANSFER_BUSY`, plus `preTransState` — transitions are auditable (freeze/restore can restore the prior state rather than a hardcoded default), which is the mechanism behind the `NotifyRssAppStateChanged(isFreeze)` freeze/restore seen in the service. [`nearlink_sle_datatransfer_cache.h:33-38`]
3. The mapping declares an `operator==` specifically "for NearlinkSafeList Insert" — dedup-on-insert is the container contract, so duplicate app mappings cannot enter the cache. [`nearlink_sle_datatransfer_cache.h:52`]

## Boundaries and gaps

- The cache header was read; the eviction on disconnect and the port-to-tcid allocation policy live in the service .c and were not re-traced.
- transMode/frameType value semantics are SDK-defined constants not present in this header.

## Reusable for our stack

- The tcid-per-session mapping is the concrete realization of dynamic channel IDs — our SSAP/transport session records should reserve a tcid-equivalent field with the same session-lifetime semantics.
- The state + previous-state pair with a freeze/restore consumer is the clean way to implement app-level backpressure without losing the pre-freeze condition.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-TETHERING-SERVICE.md`: the service level view; this closes the cache internals.
- `NEW-SSAPS-MULTI-OPERATIONS-CORRECTION.md`: the per-peer state vectors theme continues from server clientConfigs to client app caches.
- `feature_mgr` (local `assets/stack/ssap`): FEAT_DYN_TCID is implemented upstream as a per-session mapping field — validation of that feature bit's real-world usage.
