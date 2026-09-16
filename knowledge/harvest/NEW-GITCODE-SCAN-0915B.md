---
type: harvest
title: "GitCode scan round: ws63flash v4 Rust rewrite (WS63/BS21E/ESP32, Tauri GUI, fwpkg crate), xiangpenpen 1:N upstream; upstream staleness all-clear"
language: en
created: 2026-09-15
tags: [harvest, gitcode, rust, flasher, scan, sle]
sources:
  - "gitcode.com/xiaohong-ai/ws63flash"
  - "gitcode.com/fixedstarheng/ws63-xiangpenpen"
trust: B
stale_after: 2027-03-15
---

# GitCode scan round 0915b

## Executive findings

**1. `xiaohong-ai/ws63flash` v4.0.1 — the Rust rewrite of the WS63 UART flasher now exists, cross-platform with a GUI.** GitCode (updated 2026-08-26), MIT, Rust 1.78+, targets **WS63 / BS21E / ESP32** from one toolchain (macOS/Linux/Windows). Structure: `src/proto` (wire frames, `MAGIC_BYTES = [0xEF,0xBE,0xAD,0xDE]` — the same 0xdeadbeef wire magic our C/Python teardowns established), `src/fwpkg` (typed `FwpkgHeader {mgc, crc, cnt, len}` + 52-byte `FwpkgBinInfo {name[32], offset, length, burn_addr, burn_size, type}`, with file-absolute→data-relative offset conversion and the note "both C and Rust formats"), `src/ymodem`, `src/sign` (signing path — the verb the Python clone lacked), `src/chip` (`ChipType::{Ws63, Bs21e, Esp32}`; WS63/BS21E share `Ws63Flasher`, loaderboot-first + DOWNLOADI + YMODEM semantics documented in doc-comments, ESP32 excluded from fwpkg), three CLI bins (`ws63flash`, `ws63fwpkg`, `ws63sign`), and a **Tauri GUI** (`src-tauri` + frontend) — the first GUI wrapper for this flasher family. ~1945 LOC across core modules. This is the third independent implementation of the same boot-ROM protocol (C goodspeed34 → Python geekheart → Rust xiaohong-ai), each later one adding the verbs the earlier lacked.

**2. `fixedstarheng/ws63-xiangpenpen` — upstream of competition entry 14807, literal 1:N SLE acquisition.** WS63E + LiteOS, elderly-depression early-risk prediction from multimodal PPG/IMU/voice; repo split is the architecture statement: `sle_client_one/` (the one hub) + `sle_server_many/` (the many sensor nodes) — one-to-many PPG/IMU streaming with LCD UI + edge AI. Apache-2.0. Confirms the 1:N role naming we've seen across the corpus (sle_demo_1vn lineage) as the standard topology vocabulary.

**3. Upstream staleness all-clear (freshness round).** Local clones verified current against upstream: nearlink-uwb-like-ranging (2026-09-13 = local HEAD), teki128/nearlink (2026-09-14 = local, harvested at sync 78), keyboard-cli (pushed 2026-08-28, unchanged since harvest), communication_nearlink_service (fetched origin/main, **0 commits ahead** — our DLI/snoop/adapter reports are against the freshest tree). No stale knowledge this round.

**4. Search-window verdicts.** Remaining GitCode WS63 hits are mirrors/forks of known repos (HiSpark/fbb_ws63, hinearlink/*, hbu-dragon/fbb_ws63, YunZhiSheng mirror) or personal forks (liangkz/ws63_demos, jianguoxu/ws63 ESP32-OpenHarmony crossover, sanchuanhehe1). `xiaohong-ai/xiaohong-fbb_ws63` (Xiaohong AI full environment on official SDK) noted as a watch candidate — its AI runtime stack on fbb_ws63 is unexamined this round.

## Boundaries

- Rust flasher cloned for structure reading only; not built (needs Rust 1.78 toolchain build; out of harvest scope).
- xiangpenpen read at README/structure level; PPG/IMU frame formats not extracted (competition README coverage exists for 14807-family designs).
- Chip abstraction (`ChipType`) listed from doc-comments; ESP32 code path not read.

## Reusable

- Rust `fwpkg` crate shapes (`FwpkgHeader`/`FwpkgBinInfo` with offset conversion note) — directly reusable if we ever write fwpkg tooling in Rust (our rust-ws73 track).
- `src/sign` module exists — confirms the signing verb family (C `--write-program`) is implementable without vendor blobs.
- Three-CLI split (flasher/fwpkg/sign) as tooling UX precedent.
- `sle_client_one`/`sle_server_many` dir naming as topology self-documentation.

## Comparison anchors

- vs. NEW-XF-BURN (Python) and C ws63flash: Rust v4 completes the triple; the protocol story is now quadruply triangulated (C, Python, Rust, our USB handshake being a distinct transport).
- vs. 10102/batch-5 topologies: xiangpenpen is the depression-screening sibling of the health-monitor family with explicit 1:N dirs.
- vs. hinearlink/ws63flash-win (batch 15): two community flasher fronts (Windows-native shell vs Rust+Tauri) — the flasher UX space is being actively competed for.
