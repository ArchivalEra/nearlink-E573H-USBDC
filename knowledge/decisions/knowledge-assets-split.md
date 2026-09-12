---
type: decision
title: Knowledge/assets plane split and OKF adoption
language: en
created: 2026-09-12
tags: [restructure, okf, architecture]
---

# Knowledge/assets plane split and OKF adoption

**Decision** (wayfinder effort `knowledge-restructure`, resolved 2026-09-12; user approved all recommendations):

1. **Planes**: top-level `knowledge/` (OKF v0.2 bundle: all intelligence, research, decision documents) decoupled from top-level `assets/` (code artifacts: `stack/`, `sdk/`). Root infrastructure (scripts, hooks, README pair, LICENSE, `.gitignore`) stays at root. Rule: *knowledge = read-to-decide; assets = run-or-referenced-by-builds.*
2. **OKF v0.2** adopted (spec: GoogleCloudPlatform/knowledge-catalog `okf/SPEC.md`). Hard gate = §11's three MUST rules, self-hosted in `scripts/check-okf.py` (no external binaries). Tolerated-list semantics (broken links to future concepts, unknown types/keys) preserved per §6/§11.
3. **Topology**: single bundle — `harvest/` (type `harvest`, external-repo digestion), `intel/` (type `intel`, protocol/chip/SDK research), `decisions/` (type `decision`); per-directory `index.md` + root `log.md`; file names kept (path is identity).
4. **Bilingual**: legacy documents migrate unchanged with `language: zh|en` frontmatter (custom extension key, spec-legal); **new** knowledge documents are English-only; the README pair remains the only bilingual surface with its hard sync gate.
5. **Wire truth** (stress case): code (`stack/ssap/include/ssap_pkt.h`) is wire ground truth; knowledge documents cite it, never restate it as an independent authority.

Why: the knowledge plane had grown to 89 documents scattered across `.scratch/` and `docs/` while the artifact plane mixed with it; agent-facing consumption needed a self-describing, link-typed, provenance-carrying layout (§5 `sources`/`stale_after` fit harvest knowledge that ages).

Consequence: every path-sensitive surface (harvest hook pathspec, README index tables, AGENTS.md, harvest skill drop paths) updated in the same migration; see `.scratch/knowledge-restructure/issues/04-migration-map-and-workflow.md` for the checklist.
