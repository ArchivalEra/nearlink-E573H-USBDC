---
type: harvest
title: nearlink_service 67-commit follow-up — ReplayConnectedLink hardening (MTU replay, sync execution, dedup window, 165-line test) and a Taihe IDL for SSAP
language: en
created: 2026-09-13
tags: [ohos, ssap, replay, mtu, taihe, idl, ani, arkts, follow-up, harvest]
sources:
  - url: https://github.com/openharmony/communication_nearlink_service
    note: fast-forwarded 2026-09-13 (2ea5f65 → b6c6c48, 67 commits, 115 files +7215/-596, pushed 2026-09-12 — ALIVE and moving daily)
trust: verified
stale_after: 2026-12-13
---

# nearlink_service 67-commit follow-up — ReplayConnectedLink hardening (MTU replay, sync execution, dedup window, 165-line test) and a Taihe IDL for SSAP

- Inspection date: 2026-09-13 (staleness check: upstream moved within ~24h of the last digest — the hottest repo in the ecosystem)
- Method: pull + targeted diff of the replay-hardening merge (b6c6c48) and the new Taihe binding tree
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. The ReplayConnectedLink feature documented in `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md` received a **hardening round within two days**: (a) the replay now also **replays the negotiated MTU** — the fix comment states the existing link's MTU exchange happened before app registration so a late-registered app cannot observe it; the code looks the link up by address (`SSAP_FindSsapLinkByAddr`), skips missing links, and calls `cb->onMtuChanged(appId, addr, link->mtu)`. [`commit a93bc30`, review label S2]
2. (b) The replay was **merged into the server-registration task to execute synchronously, eliminating a duplicate-notification window** (review W1) — replay is no longer a separate async sweep racing with live notifications. (c) A **165-line `ssap_link_state_replay_test.cpp`** plus connectedDevices value-semantics assertions landed (review S3) — the feature now has regression cover. [`commits a123d23, 3df299c`]
3. The delta's largest single addition is a **Taihe IDL for SSAP**: `frameworks/ets/taihe/nearlink_ssap/idl/ohos.nearlink.ssap.taihe` declares `@ohos.nearlink.ssap` with `Client` / `Server` interfaces — promise-based `Connect()`/`Disconnect()`/`GetServices()` — plus generated ANI binding headers (`ani_nearlink_ssap_client.h`, `ani_nearlink_ssap_server.h`, 127-line BUILD.gn). ArkTS applications get a typed, promise-style SSAP surface. [`frameworks/ets/taihe/nearlink_ssap/`]
4. The IDL is `@!namespace("@ohos.nearlink.ssap", "ssap")` with a `loadLibrary("nearlinkSsapTaihe_native.z")` injection — the Taihe toolkit (OHOS's next-gen ETS↔native binding generator) now covers the NearLink SSAP plane, extending the NAPI bindings noted in the September increment. [`ohos.nearlink.ssap.taihe:14-30`]

## Boundaries and gaps

- 67 commits not exhaustively reviewed; the two headline threads (replay hardening, Taihe SSAP) were traced and the remainder inventoried by stat.
- The Taihe IDL's full interface body (Server methods, event subscriptions) extends beyond the head of the file; read-on-demand.
- The replay unit test's assertions were not evaluated line-by-line.

## Reusable for our stack

- The MTU-replay completion makes the replay pattern fully specified: a late-registered server needs BOTH connection-state callbacks AND the negotiated MTU replayed — our SSAP server's implementation plan should include both in `replay_connected_link()`.
- Synchronous-execution-inside-registration (to avoid replay/notify races) is the safer ordering than an async sweep — adopt it.
- A typed IDL (Taihe-style) over our SSAP stack would give script/app consumers a promise API — the direction OHOS has now taken; our rust-ws73 host crates could mirror this with a generated binding layer.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`: the parent digest; this is the 48-hour follow-up showing review-driven maturation (S2/S3/W1 labels) of the replay feature.
- `NEW-SSAP-LINK-PLANE.md`: the replay fixes operate on the link plane's state records (`SSAP_FindSsapLinkByAddr`, `link->mtu`).
- `NEW-OHOS-SA1190-IPC-SURFACE.md`: the Taihe surface is a third binding layer on top of NAPI and IPC — three distinct app-facing surfaces now documented.
