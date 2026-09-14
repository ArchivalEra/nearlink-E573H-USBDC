---
type: harvest
title: Daily-churn round — nearlink_service WeChat-call volume fix; StarFlash-Releases is a compiled-only Windows tool repo; ecosystem pulse
language: en
created: 2026-09-13
tags: [nearlink-service, daily-churn, starflash-releases, freshness, harvest]
sources:
  - url: https://github.com/openharmony/communication_nearlink_service
    note: pulled 2026-09-13, HEAD 59b50c4 (2026-09-14); 5 files +33/-8
  - url: https://gitcode.com/qq_45486948/StarFlash-Releases
    note: cloned 2026-09-13 from gitcode, pushed 2026-08-31, 160K (README-only)
trust: verified
stale_after: 2026-12-13
---

# Daily-churn round — nearlink_service WeChat-call volume fix; StarFlash-Releases is a compiled-only Windows tool repo; ecosystem pulse

- Inspection date: 2026-09-13
- Mode: read-only inspection; no build, network, hardware, or PCB access

## Executive findings

1. **nearlink_service moved again** (b6c6c48 → 59b50c4, 2026-09-14): 5 files +33/-8 — "WeChat incoming-call does not support volume adjustment" fix ×4 (qhz-fork-0907 merge). The hot repo's daily cadence continues with small bugfixes on the audio path — consistent with the ASC audio interfaces and the karaoke-ear-return commits from earlier increments. [git log + diff stat]
2. **`StarFlash-Releases` (gitcode, Prince)**: "" — a compiled Windows portable tool for StarFlash build+flash, **no source code** (README-only at HEAD; binaries likely in releases/tags). 160K clone. Classified as a distribution artifact, not a knowledge source — one-line verdict. [README.md]
3. **teki128/nearlink moved to 2026-09-14** (from 09-11): demo repo still active; delta not yet pulled (prior digest covers the 564-line minimal pair; re-digest only on substantive change).

## Boundaries and gaps

- The WeChat-call volume fix is a UI/audio-routing bugfix — no protocol-level change.
- StarFlash-Releases binaries not downloaded (distribution artifact, not knowledge).

## Reusable for our stack

- nearlink_service's daily cadence confirms the ecosystem is actively developing audio-call paths (WeChat integration) — the ASC/TWS/VCP IPC surfaces from `NEW-OHOS-SA1190-IPC-SURFACE.md` are in active use.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md` / `NEW-REPLAY-HARDENING-TAIHE.md`: the hot-repo follow-up chain continues; this is a minor bugfix round.
- `NEW-WEB-FLASHER-FWPKG.md`: StarFlash-Releases is a compiled-tool distribution channel for the same flashing domain.
