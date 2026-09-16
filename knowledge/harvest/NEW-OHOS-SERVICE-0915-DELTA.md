---
type: harvest
title: "OHOS NearLink Service 0915 Delta: Autorate Downgrade 5s, Explicit appexecfwk_base Dep, Hidden-Visibility Mock Isolation"
language: en
created: 2026-09-16
tags: [harvest, ohos, nearlink-service, autorate, qosm, build, mock, tdd]
sources:
  - "https://github.com/openharmony/communication_nearlink_service"
  - "/mnt/hdd/nearlink-stuff/communication_nearlink_service (HEAD 7068bc4, delta 59b50c4..HEAD)"
trust: A
stale_after: 2027-03-16
---

# OHOS NearLink Service 0915 Delta: Autorate Downgrade 5s, Explicit appexecfwk_base Dep, Hidden-Visibility Mock Isolation

Delta digest of `openharmony/communication_nearlink_service` between local HEAD
59b50c4 (2026-09-14, covered by NEW-OHOS-NEARLINK-SEPT-INCREMENT.md) and new
HEAD 7068bc4 (pushed 2026-09-15T21:01:28Z). Three merge commits landed:
e88a5a3 (!254, br1 branch), 3be1afa (!256, qhz-fork-0911 branch),
7068bc4 (!257, appexecfwk-base-deps fix branch). Total: 4 files, +34/-1.

## Executive findings

- Autorate downgrade hysteresis widened: `QOSM_DOWNGRADE_LEVEL_TIMEOUT_MS`
  4000 ms to 5000 ms while upgrade stays at 1000 ms
  [services/stack/src/cp/bsl/sle/qosm/autorate/include/qosm_icg_types.h:60-61]
  (commit 7e07624). Upgrade path reacts in 1 s, downgrade path now waits 5 s:
  asymmetric hysteresis (fast-up / slow-down) against rate flapping, same
  autorate subsystem whose autorate-plus-concealment work was recorded in the
  September increment report.
- `nearlink_service_common` gains an explicit GN dep on
  `bundle_framework:appexecfwk_base` [services/common/BUILD.gn:115]
  (commit fb85722). The dep previously resolved implicitly via
  `appexecfwk_core`; the fix makes the base-framework edge explicit. Prior art
  for dependency-hygiene fixes in large GN trees: implicit transitive edges
  break first when the intermediate target trims its own exports.
- ASC audio-service unit tests isolate mock symbols with hidden visibility:
  every `ServiceManagerPluginLoader` mock method now carries
  `__attribute__((visibility("hidden")))`
  [test/unittest/services_test/service_test/asc_test/mock_ServiceManagerPluginLoader.cpp:28-99]
  plus a new mocked destructor (:36-40). Root cause is documented in-file:
  `libnearlink_service_impl(.so)` dynamically resolves
  `ServiceManagerPluginLoader::GetInstance` at startup, so an exported mock
  symbol hijacks the real plugin loader inside the .so and crashes the chain;
  white-box references from the same test executable still bind the mock.
  The ASC test target also gains a `nearlink_socket` unit-test dep
  [test/unittest/services_test/service_test/asc_test/BUILD.gn:99] and lists
  the mock source explicitly (:150). Two follow-up audio-TDD commits by the
  same author (5bc950d, d95e10b, 2026-09-14) extend this mock file (+21/+12
  lines): the ASC (audio spatial control) test surface is under active
  construction this week.
- Fork-merge rhythm continues: two of the three merges (!254 br1, !256
  qhz-fork-0911) land personal-fork branches into master, matching the
  fork-evolution pattern already recorded for this repo (services-500 split
  verdict in NEW-SIG-UPSTREAM-NEARLINKKIT.md). Vendor-fork branches remain
  the normal contribution vehicle, not a hostile fork.

## Boundaries

- Read the full text diff of all 4 changed files; did not read the
  pre-existing bodies of `qosm` autorate logic, the ASC service
  implementation, or the audio TDD case semantics beyond the mock layer.
- Did not build or run any test target; GN edit correctness is by inspection.
- License is the repo-declared OpenHarmony license set (self-declared,
  not re-verified file by file this round).
- No firmware binaries, PCB data, or hardware design material touched.
- Upstream pushed_at (2026-09-15T21:01:28Z) matches pulled HEAD 7068bc4;
  freshness confirmed, no further lag.

## Reusable

- Autorate tuning anchor for WS73 dongle host work: adopt fast-upgrade /
  slow-downgrade hysteresis (1 s up / 5 s down) as the starting constant pair
  for any SLE link-rate adaptation on the dongle side; the 4 s to 5 s bump
  shows upstream itself fighting downgrade flapping.
- GN hygiene rule: always declare framework-base deps explicitly next to
  framework-core deps; add a lint that flags targets depending on
  `*_core` without `*_base` in this codebase family.
- Test-isolation pattern directly reusable for dongle host-stack unit tests:
  when a mock and a plugin-loading .so coexist in one process, mark ALL mock
  symbols hidden so dynamic resolution inside the .so cannot bind the mock,
  while same-executable white-box references keep working. Copy the
  destructor-mock habit too: an unmocked dtor on a mocked singleton is a
  teardown crash waiting to happen.
- Watch the ASC test directory for the next week: audio-over-SLE service
  API is being pinned down by tests right now; the next delta may expose
  stable ASC entry points worth porting.

## Comparison anchors

- NEW-OHOS-NEARLINK-SEPT-INCREMENT.md: this delta continues that report's
  autorate thread (there: DLI autorate plus concealment; here: QOSM
  timeout-constant tuning) and its fuzzer/test-infra thread (there: full-stack
  fuzzer scaffolding; here: ASC mock isolation). No contradiction; same
  direction, smaller step.
- NEW-FRESH-SCAN-0916.md: freshness method reused (pushed_at vs local HEAD
  date, pull-then-diff). teki128/nearlink showed a same-window 3-line delta
  and is queued as the next freshness item.
