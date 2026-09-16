---
type: harvest
title: "OHOS nearlink stack_fuzzer anatomy — hydra-fuzz GN targets, a shared include map that doubles as the stack's internal layer taxonomy, and module-boundary fuzz entries"
language: en
created: 2026-09-13
tags: [ohos, fuzzer, gn, hydra-fuzz, layer-taxonomy, dtap, dli, testing, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# OHOS nearlink stack_fuzzer anatomy — hydra-fuzz GN targets, a shared include map that doubles as the stack's internal layer taxonomy, and module-boundary fuzz entries

- Inspection date: 2026-09-13 (same current tree as `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`, which listed the 25+ fuzzers; this pass opens the harness structure)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: how the fuzzer fleet is built and what its include graph reveals about the closed stack's layering

## Executive findings

1. Each fuzzer is an **`ohos_fuzztest` GN target under the "hydra-fuzz" framework**: `ohos_fuzztest("DtapFuzzTest")` with `fuzz_config_file` pointing at the fuzzer directory, a per-fuzzer `corpus/` directory, a `project.xml`, and one `dtap_fuzzer.cpp/.h` pair — a uniform, replicable per-module layout. [`test/fuzztest/stack_fuzzer/dtap_fuzzer/BUILD.gn:14-18`]
2. The `include_dirs` of a single fuzz target **enumerates the stack's internal layer taxonomy** that headers alone do not show: `services/stack/src/{adapter, dli/{event,thread,dft}, sdf/{oal/{memm,sig,lock,timer,evc}, dfx/{errno,trace}, bsl/stm, sdffwk}, dp/dpfwk, cp/bsl/sle/{servm/ssap, devd, hadm}, nai/dft}` — SDF (system device framework) hosting OAL primitives (memory/lock/timer/event), DFX (diagnostics), BSL/STM, plus the DLI/DTAP data plane and the CP/BSL SLE service plane. [`dtap_fuzzer/BUILD.gn:19-40`]
3. A **shared `stack_fuzz.gni`** deduplicates the common include set (`shared_stack_include_dirs`) across all 25+ targets — layer vocabulary is declared once; per-fuzzer GN adds only module-specific dirs. [`test/utils/stack_fuzz.gni`]
4. The fuzz entry drives the stack **through its real internal boundaries**, not a shim: the DTAP fuzzer includes `cm_trans_channel_api.h`, `cm_dyn_trans_channel_api.h`, `dli_layer.h`, `dtap_scheduler.h`, `dtap_trans.h` and uses LLVM `FuzzedDataProvider` — fuzz inputs traverse channel manager → DLI → DTAP scheduler, the same path live frames take. [`dtap_fuzzer.cpp:1-40`]

## Boundaries and gaps

- OHOS GN-build specific (`build/ohos.gni`, `ohos_fuzztest`); the harness does not run outside the OHOS build without porting the GN templates.
- Corpus contents are seed files (binary blobs); their coverage value was not evaluated.
- Only the dtap fuzzer was opened; the other 24+ follow the same template per the shared GNI.

## Reusable for our stack

- The layer taxonomy from the include graph (sdf/oal primitives → sdf framework → dli → dp → cp/bsl service plane → nai) is the canonical directory skeleton for an SSAP/SLE stack — directly comparable with our `assets/stack/ssap` layout and the rust-ws73 crate split.
- Fuzz-at-module-boundaries through real internal entry points (channel manager → data-link → transport) is the harness shape our host stack fuzzing should copy when we add fuzz targets.
- One shared GNI holding the layer include map keeps 25+ targets consistent — the GN equivalent of our central Makefile header lists.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`: that report enumerated the fuzzer fleet; this one dissects the harness construction.
- `NEW-SSAP-SERVM-MODULE-MAP.md`: the include graph confirms the servm suite's place in the layer taxonomy (cp/bsl/sle/servm).
- `02-dli-hcc-dialect.md`: the DLI/DTAP fuzz entries exercise exactly the dialect plane that intel doc maps.
