---
type: harvest
title: OpenHarmony communication_nearlink_service September Increment (544 commits, 2026-08-14 to 2026-09-11)
language: en
created: 2026-09-13
tags: [ohos, nearlink, ssap, dtap, dli, frame4, fuzzer, memory-safety, harvest]
sources:
  - url: https://github.com/openharmony/communication_nearlink_service
    note: pulled 2026-09-13, local fast-forward 7287d1c(2026-08-14) to 2ea5f65(2026-09-11), Apache-2.0
trust: verified
stale_after: 2026-12-13
---

# OpenHarmony communication_nearlink_service September Increment (544 commits, 2026-08-14 to 2026-09-11)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-11, not archived — ALIVE)
- Delta shape: 331 files changed, +8297/-1915; heaviest in `services/stack/src` (103 files), new `test/fuzztest/stack_fuzzer` harness (67 files), `services/service/src` (36 files)
- Scope: stack-layer protocol fixes, one new service manager (frame-4 antenna pinning), full-stack fuzzing harness, memory-safety remediation wave; no wire-format break observed in the sampled diffs

## Executive findings

1. New `SleFrame4AntennaMgr` pins the RF antenna while any SLE frame-4 advertising or frame-4 scanning is active, using two independent reference counts (ADV set, SCAN set); first 0→1 pins via plugin, last 1→0 unpins; all state transitions and plugin calls are serialized onto a dedicated "antenna" thread, so no locks are needed. MONITOR scan mode counts as frame-4 scan stopped. [`services/service/src/common/SleFrame4AntennaMgr.h:31-46`] [`SleFrame4AntennaMgr.cpp:33-70`]
2. Frame-4 is `SleAdvertiserPrimaryFrameType::SLE_ADV_PRI_FRAME_TYPE_4`, a distinct SLE advertising primary frame type alongside the default type-1 used by normal advertisers. The manager gates exclusively on type 4, implying frame-4 operation has an RF/antenna coexistence constraint the service layer must enforce. [`services/service/src/common/SleFrame4AntennaMgr.cpp:70`] [`services/service/src/advertiser/SleAdvertiserImpl.h:57`]
3. SSAP server reuse-link blind spot fixed with a new stack API `NLSTK_SsapServerReplayConnectedLink(appId)`: when an app registers as SSAP server AFTER the ACL link already exists (client connected first, peer then reuses the link to reach the new server), the peer sends no new link-establishment signaling, so the late-registered server got no events and the daemon connection table had no entry — Notify/Cancel were then wrongly blocked by `IsDeviceConnected`. After registration the daemon now asynchronously replays the current connected-link state to that appId. [`commit 37e477c`] [`NLSTK_SsapServerReplayConnectedLink` in `services/stack/src/cp/bsl/sle/servm/ssap/`]
4. `DTAP_CopyFrame` UAF fixed: the copy previously memcpy'd the whole `DTAP_Frame_S`, keeping `header`/`extension`/`payload` pointers aimed into the SOURCE frame's `buff`; freeing the source left dangling pointers in the copy. The fix deep-copies `buff`, then REBASES the three interior pointers onto the new buffer via `SDF_DataOffset(buff)` plus `headerLen`/`extensionLen` arithmetic, initializes the list entry, and documents the contract (caller must pass a valid parsed frame; copy owns an independent deep buffer). [`services/stack/src/dp/dtap/src/dtap_frame.c:105-133`] [`commit 09bdaaf`, 54-line unit test in `dtap_test.cpp`]
5. DLI gained two new control APIs: `DLI_SetICGAutorateParam(param, cbkParam)` to configure sync-link autorate parameters, and `DLI_RegisterSnoopSensitiveOpcodes(cmdOpcodes, cmdNum, evtOpcodes, evtNum)` to register CMD/EVT opcodes that must be anonymized when DLI snoop logs are persisted to disk (the "dli log anonymization" feature). [`services/stack/src/dli/interface/dli_cmd.h`, `commit a4837dc`]
6. A full-stack fuzzing harness landed under `test/fuzztest/stack_fuzzer/` with 25+ per-module fuzzers: actm, adv/advfree/advuapi, bas, ccp, cdsmstack, cfgdb, cm, cmicb, cmsignaling, dis/disclient, dli/dlilayer, dtap, hadm (+ evtreport/stm), hidstack, icce, mcp, micp, multiscan, and more — one GN target set covering essentially every stack module. [`test/fuzztest/stack_fuzzer/BUILD.gn`]
7. A memory-safety remediation wave ran through the service layer: Adv callbacks migrated from global raw pointers to Adapter-held `shared_ptr` + `weak_ptr` to close async UAF windows, `SDF_EvcCancelEvent` UAF fixed, "self-check" remediation commits for UAF and integer safety, and a 7-item callback-lifetime fix batch. [`commits 90c98f6/a171d5e/500b086/39ad463/962d36b`]
8. SSAP server framework dead code removed and `connectedDevices` registration logic fixed; per the refactor commit the framework no longer registers devices it cannot see. [`commit 6133698/2ea5f65`]

## Boundaries and gaps

- Wire-format stability: sampled diffs fix lifetime/state bugs and add APIs; no SSAP/DLI PDU layout change was observed in the sampled commits, but the 544-commit span was not exhaustively diffed at byte level.
- The `SleAdvertiserPrimaryFrameType` enum definition lives outside the sampled service headers (adapter/SDK side); only its usage sites were verified in-tree.
- Frame-4 semantics (what the frame carries, RF reasoning for the antenna pin) are not documented in-tree beyond the manager's comments; the "why" lives in plugin/hardware docs not present in this repo.
- Fuzzers are OHOS GN-build artifacts; they are not directly runnable on this repo's x86 host without the OHOS build system.

## Reusable for our stack

- Replay-on-register pattern: our SSAP server (`assets/stack/ssap/src/ssap_server.c`) has the same reuse-link blind spot — a server registered after a link exists never learns about it. The `NLSTK_SsapServerReplayConnectedLink` design (async replay of connected links to a fresh appId, guarded by a max-app-num check) is directly portable.
- Copy-then-rebase rule for frame structs: any struct holding interior pointers into its own buffer must rebase those pointers after a deep copy (offset+length arithmetic), never memcpy pointers verbatim. Applies to our DTAP-like frame paths and the SLE UART framing code.
- Opcode-sensitivity registry for logs: `DLI_RegisterSnoopSensitiveOpcodes` is a clean pattern for our USB/HCC snoop logging — declare which opcodes carry sensitive payloads and anonymize at persist time.
- Two-refcount antenna/resource pinning with a serialized executor thread is a reusable lock-free coexistence pattern for any shared radio resource.
- Full-stack per-module fuzzer layout (one fuzzer per stack module under a single GN BUILD) is a template for organizing future host-side fuzz tests.

## Comparison anchors (vs existing reports)

- `OHOS-SSAP-ENGINE.md` / `OHOS-SSAP-CLIENT.md`: covered the August snapshot's SSAP engine/client; this increment adds the server-side replay API and dead-code cleanup that postdate them.
- `OHOS-DLI-LAYER.md`: documented DLI as the command/event plane; the autorate and snoop-anonymization APIs extend that surface.
- `OHOS-FRAMEWORK-LAYER.md` / `OHOS-NAI-LAYER.md`: framework changes here are mostly NAPI/Taihe bindings and IPC plumbing consistent with those reports; no new layer appeared.
- `NEW-NEARLINK-UWB-LIKE-RANGING.md`: HADM fuzzers (hadm/hadmstm/hadmevtreport) confirm the ranging stack keeps evolving upstream; our ranging interests remain aligned.
