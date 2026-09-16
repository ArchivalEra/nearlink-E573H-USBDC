---
type: harvest
title: "Freshness round — uwb-like-ranging refines its Technical Boundary; sle_mesh family naming clarified; a SparkLink-named TCP chat skipped"
language: en
created: 2026-09-13
tags: [freshness, boundary, sle-mesh, name-squat, tcp, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# Freshness round — uwb-like-ranging refines its Technical Boundary; sle_mesh family naming clarified; a SparkLink-named TCP chat skipped

- Inspection date: 2026-09-13; library freshness round (no new substantive repos)
- Mode: read-only inspection; no build, network, hardware, or PCB access

## Executive findings

1. **uwb-like-ranging authored an explicit Technical Boundary section** (commit 5d6382a): contributions are scoped to "multi-node connection and state orchestration, bidirectional observation aggregation, the Collector data path, host-side positioning from SDK distance results, and IQ-based research analysis" — explicitly NOT reimplementing the vendor's low-level ranging algorithm, and "UWB-Like" is declared a usage-model term, not waveform equivalence. Also: a **systematic positioning-accuracy benchmark has not yet been published** (stated gap). This refines `NEW-NEARLINK-UWB-LIKE-RANGING.md` and `NEW-IQ-FEATURES-SCHEMA.md` without contradicting them. [`README_EN.md` boundary section]
2. **sle_mesh family naming clarified**: the local `sle_mesh` clone carries the same `sle_team_*` module taxonomy as `sle_mesh_new` (v1.2.2 in versions/, roles MEMBER=0/LEADER=1, callbacks: joined/position/alert/relay_offline/member_timeout_defer) — the two repos are one project lineage at different versions, so `NEW-SLEMESH-RUST.md` (tier routing) and `NEW-SLE-TEAM-MESH-V456.md` (team packets) describe the same lineage's evolution, not two independent designs. [local sle_mesh include/sle_team_node.h]
3. **`seantran-dev/SparkLink` — skipped**: `protocol.py` is a plain 4-byte length-prefixed UTF-8 TCP chat (`struct.pack("!I", len)`), no NearLink SDK anywhere; the SparkLink name is branding. One-line verdict, not harvested. [raw protocol.py]

## Boundaries and gaps

- No new NearLink-bearing repos appeared in this freshness window (GitHub + GitCode scans both clean).
- The uwb-like README's positioning-accuracy benchmark remains unpublished — a watch item.

## Reusable for our stack

- The uwb-like boundary statement is a model for our own reports: scoping "what we do" against "what the vendor already does" pre-empts both overclaiming and duplicated effort.
- The sle_mesh lineage note corrects our comparison anchors: treat `NEW-SLEMESH-RUST.md` ↔ `NEW-SLE-TEAM-MESH-V456.md` as one design's timeline.

## Comparison anchors (vs existing reports)

- `NEW-NEARLINK-UWB-LIKE-RANGING.md` / `NEW-IQ-FEATURES-SCHEMA.md`: refined by the upstream boundary section.
- `NEW-SLEMESH-RUST.md` / `NEW-SLE-TEAM-MESH-V456.md`: lineage relationship corrected.
- `NEW-DS10-SLE-DTU.md`: the DTU-vs-DS10 comparisons remain valid (independent designs).
