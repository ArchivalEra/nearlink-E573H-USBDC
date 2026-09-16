---
type: harvest
title: "nearlink-vip knowledge ecosystem — two agent-facing HiSilicon knowledge bases (OKF-style wiki + JSONL peripheral graph) and the HiDiTing chipset constellation"
language: en
created: 2026-09-13
tags: [knowledge-graph, agents, hispark, hiditing, chipset, jsonl, okf, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/hs-wiki"
trust: verified
stale_after: 2026-12-13
---

# nearlink-vip knowledge ecosystem — two agent-facing HiSilicon knowledge bases (OKF-style wiki + JSONL peripheral graph) and the HiDiTing chipset constellation

- Inspection date: 2026-09-13 (staleness check: pushed 2026-07-01/07-05, not archived — ALIVE, ~2 months quiet)
- Source roots: `/mnt/hdd/nearlink-stuff/hs-wiki`, `/mnt/hdd/nearlink-stuff/hs-peripheral-knowledge`
- Mode: read-only inspection; discovered via GitCode search with the user's private-token
- Scope: two third-party agent-readable knowledge bases and their data models

## Executive findings

1. **`hs-wiki` is an OKF-style markdown knowledge base for the HiSpark ecosystem**: frontmatter-indexed (`title/type/lang/status/updated/translations` — structurally near-identical to our OKF bundle's conventions), organized into four product domains (Wearable / AIoT / Vision / Media), shared runbooks (build-flash-run, serial-debug-loop), troubleshooting indexes, and a CLI knowledge section. Agent-facing by design (AGENTS.md at root, "This is the starting point for developers and agents"). [`hs-wiki/index.md`, `shared/runbooks/`]
2. **`data/products.yaml` discloses the HiDiTing chipset constellation**: product `hiditing` (type module, domain wearable, display name Di-ting, primary_chip **hi3322**) with managed chips — **hi2871 for GNSS** and **hi2131e for cellular CAT1**, both `control_model: controlled-by-hi3322`. This is the wearable SoC constellation behind the SLE 2.0 platform, structured with type/domain/capability/control-model fields. [`hs-wiki/data/products.yaml`]
3. **`hs-peripheral-knowledge` is a JSONL knowledge graph**: 169 peripherals, 102 facts, 151 documents in `data/*.jsonl`; the fact model is `{id, peripheral_id, fact_type: bus_interface|command|register, key, conditions, confidence, extraction_method: human, revision}` — a provenance-carrying fact graph (confidence + extraction method per fact). [`data/facts.jsonl`, `data/peripherals.jsonl`]
4. The graph's **verification layer is designed but empty** (`verifications.jsonl` 0 lines, plus evidence/driver-specs stubs) — knowledge claims carry confidence but no independent verification yet; the schema anticipates it.
5. Both repos carry AGENTS.md + zh/en translations + CONTRIBUTING — third parties are actively building agent-readable HiSilicon knowledge, converging on the same shape as our `knowledge/` bundle (frontmatter, domains, runbooks, structured data), via GitCode.

## Boundaries and gaps

- Both are `status: draft` with ~2 months quiet; hs-wiki's per-domain pages beyond the index were not audited.
- hs-peripheral-knowledge covers **generic embedded peripherals** (DS18B20, DS3231…) — NearLink-specific facts were not found in the sampled records.
- The relationship between the two repos (same org, similar scaffolding) suggests a shared tooling origin; the fact/paper pipeline was not traced.

## Reusable for our stack

- The **HiDiTing chipset constellation** (hi3322 + hi2871 GNSS + hi2131e CAT1) extends our HiDiTing knowledge with the full SoC family — our wearable/SLE 2.0 planning should track all three.
- Their **products.yaml schema** (type/domain/aliases/managed_chips/control_model) is a compact catalog model worth comparing with our index conventions.
- The fact-graph model with confidence + extraction_method provenance is a candidate enrichment for our own harvest metadata (we currently track `trust` at file level; per-fact confidence is finer).
- An empty `verifications.jsonl` layer is a designed-but-unfilled state our knowledge plane deliberately avoids (our reports ARE the verification layer) — a comparative note for OKF community alignment.

## Comparison anchors (vs existing reports)

- `NEW-HIDITING-SLE2-EVIDENCE.md` / `NEW-HIDITING-LWIP-SLE-NETIF.md`: the hi3322 constellation names the SoC behind those API surfaces.
- `knowledge/` OKF bundle (our own): format kinship confirmed — frontmatter conventions, domain indexes, runbooks, structured data all parallel; their products.yaml is the piece our bundle lacks.
- `AGENTS.md` (our repo): both repos demonstrate the AGENTS.md-as-entry-point pattern for agent consumption.
