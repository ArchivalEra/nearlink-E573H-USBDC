---
type: harvest
title: "Daily churn 0915b: teki128 pair-cleanup increment + three SparkLink-name verdicts (TCP chat, Flutter LAN, NFC badge)"
language: en
created: 2026-09-15
tags: [harvest, verdict, sle, naming-squat, ssap]
sources:
  - ""github.com/teki128/nearlink (local clone https://github.com/teki128/nearlink)""
  - ""github.com/seantran-dev/SparkLink""
  - ""github.com/Heebu/NearLinkChat""
  - ""github.com/xypasolini-droid/SparkLink""
trust: B
stale_after: 2027-03-15
---

# Daily churn 0915b: teki128 increment + name-squat verdicts

## Executive findings

**1. teki128/nearlink keeps evolving — pair-cleanup hygiene now in the canonical minimal SLE client/server.** Two new commits since our last harvest (2a06264 2026-09-11 → 98fa110 2026-09-14): connection-parameter tuning, thread-safe property read/write fixes, and consistent `status=0x%x` (hex, not decimal) across every callback log — the third team this month to settle on hex status printing. Functionally: `sle_pair_complete_cb` now calls `sle_remove_paired_remote_device(addr)` on pair failure (nearlink_client.c:123-127), and property-length calculation got a fix in the communication thread. This independently confirms the SparkSafe (batch 4) discovery: **stale pairing keys must be removed before re-pair or SLE reconnect gets rejected** — now seen in both WS63 sample derivatives and competition firmware. Also the client boot sequence gained an explicit "This is client." marker — trivial, but it explains why NLChat_Web's ChatUI greps for that exact string.

**2. Name-squat verdicts — three "SparkLink/NearLink"-named repos contain zero NearLink.** GitHub search hygiene finding: the brand name attracts unrelated projects.
- `seantran-dev/SparkLink` (pushed 2026-09-13, 1.7MB Python): TCP LAN chat with 4-byte length-prefix framing (`protocol.py`) — plain sockets, no SLE/NearLink anywhere. The framing is the standard TCP delimiter pattern we already document in USB-PROTOCOL; not NearLink.
- `Heebu/NearLinkChat` (2026-09-06, Flutter/Dart): code search for sparklink|sle|nearlink returns **0 hits** — a same-network Flutter call/message app using the name only.
- `xypasolini-droid/SparkLink` ("RALLY Jijie", 2026-08-29): hackathon team-up AI badge product; connection entry is **NFC card-tap + QR fallback** with an optional e-ink AI Passport — "SparkLink" is the brand metaphor of "tap-to-connect", the transport is NFC. Product-planning docs (PRD, 96h schedule, e-ink BOM research) are unusually complete but contain no SLE.

**3. GitHub fresh-scan snapshot 2026-09-15:** our repo tops recency (pushed today); `openharmony/communication_nearlink_service` pushed 2026-09-14 (already under local harvest); `zhuzhengyan50-spec/nearlink-uwb-like-ranging` 2026-09-13 (our mirror); `yanlinkos/fbb_ws63` 2026-08-28 (harvested). No new genuinely-NearLink repos this window — competition corpus remains the freshest material.

## Boundaries

- teki128 repo remains the bare sle_uart derivative — no SSAP service model, binary "test" payload only; its value is lifecycle hygiene, not protocol.
- Verdict repos inspected at clone/README/search level only (shallow clone for two); deep-dive would only repeat the non-NearLink verdict.
- The GitHub window scanned is 2026-08-20 → 09-15; older dormant squatters not re-litigated.

## Reusable

- `sle_remove_paired_remote_device()` on pair-failure + `sle_remove_all_pairs()` on disconnect — combined rule now confirmed twice; encode in our SSAP reconnect manager.
- Hex-format status logging (`status=0x%x`) in all SLE callbacks — adopt in our tools; decimal status hid the errcode bit layout in past debugging.
- Search hygiene: require code-level evidence (header include, ioctl, API call) before classifying a "SparkLink/NearLink"-named repo as NearLink; name match alone is wrong ~half the time.

## Comparison anchors

- vs. SparkSafe batch 4: same remove-pairs rule from the opposite role (server-sample derivative vs. competition product) — convergent operational wisdom.
- vs. NLChat_Web: the "This is client." log marker closes the loop on its ChatUI detection heuristics.
- vs. earlier verdict batches (StarFlash-Releases): third confirmed squatter family; pattern is now systematic, not anecdotal.
