# Map: Knowledge/Assets Restructure (Open Knowledge Format)

> Effort: knowledge-restructure · Tracker: local markdown (`docs/agents/issue-tracker.md` — Wayfinding operations)

## Destination

The repo's knowledge plane and artifact plane are decoupled: a top-level `knowledge/` domain organized as a Google Open Knowledge Format (v0.2) bundle carries all intelligence/research/decision documents, while `assets/` holds the actual artifacts (code, scripts, SDK, hooks). Every index, gate, and workflow that references the old paths — `check-docs.sh` doc-index, `check-harvest-archive.sh` REPORT_PATHSPEC, bilingual README sync, `AGENTS.md`, the harvest skill's drop paths, existing wayfinder trackers — is updated in the same movement, and all 78 existing research documents migrate losslessly (git history preserved). The way is clear when every decision below is resolved, a step-by-step migration spec exists, and gates pass — execution rides inside this map (see Notes).

## Notes

- **Domain**: repository information-architecture restructure; the largest refactor this repo has had so far. The user named the target shape: knowledge plane decoupled from artifact plane, split as `knowledge` vs `assets`, knowledge authored in Google's Open Knowledge Format.
- **Skills every session should consult**: `grilling` + `domain-modeling` (charting/resolution), `docs/agents/issue-tracker.md` for tracker ops.
- **Standing preferences for this effort** (user-issued, standing):
  - **No subagents.** Research tickets are resolved in the main session or a dedicated later session; never dispatch agents (user instruction, context-compression budget).
  - **No option questions.** The user is busy and pushes autonomously; grilling-type tickets are worked by the agent self-grilling the domain and recording the decision, which the user can veto in review. HITL is honored by review-after-the-fact, not live exchange (effort-level override of ticket type).
  - **Docs plane rules ride along**: English-only rule, bilingual README hard gate, pre-push hooks (`check-docs.sh`, `check-harvest-archive.sh`) must all stay green through every migration batch.
- **OKF quick facts for orientation** (to be verified & expanded in the research ticket):
  - Spec: OKF v0.2 — GoogleCloudPlatform `knowledge-catalog` repo `okf/SPEC.md`; a bundle is a directory of `.md` files; every concept document opens with parseable YAML frontmatter carrying a non-empty `type`; reserved files (`index.md` — bundle-root one may declare `okf_version`, no frontmatter; `log.md` with ISO 8601 date headings) follow fixed structures; **path is identity**; files only, no runtime (conformance M1–M6).
  - Deliberately permissive (§11): broken cross-links, unknown types/keys, missing optional fields, missing `index.md` never fail conformance; quality (internal-link resolution, no orphan concepts, tags lists, `sources`/`verified`/`stale_after` provenance fields) is a separate `--strict` layer (SHOULD S1–S6). A link to a not-yet-existing concept "may simply represent not-yet-written knowledge" (§6.1).
  - Tooling landscape: Rust CLI `okf` (validate/lint/serve MCP+web), Node conformance suite with two-oracle design, GitHub Actions for CI validation, graph/visualize tooling, MCP serving of concepts as `okf://` resources.
- **Standing technical constraints** (from repo memory): whitelist gitignore (`.scratch/` allowlisted), pre-push archive gate couples new `NEW-*.md` harvest notes to bilingual README updates — its pathspec will need to follow the knowledge plane's new location; `docs/` English-only rule origin is `AGENTS.md`.

## Decisions so far

<!-- one line per closed ticket: [ticket name](path): gist -->

## Not yet specified

<!-- fog: in-scope, not yet sharp enough to ticket -->

- Batch frontmatter/metadata plan for the 78 existing documents: hangs on the OKF field set (ticket 01) and the split boundary (ticket 02). Suspected work: map every existing file to an OKF concept type, invent per-type frontmatter templates, decide what `index.md`(s) enumerate.
- Relationship of the two existing wayfinder trackers (`.scratch/nearlink-driver/`, `.scratch/rust-ws73-tri-mode/`) to the new knowledge plane: stay in place as working trackers, migrate, or re-expressed as OKF concepts/log entries.
- Consumption mode for agents: pure file reads vs a serving layer (`okf serve --mcp` candidates) — may become a prototype ticket once the bundle exists.
- CI gate: whether to adopt an OKF validator (Rust toolkit Action, Node conformance suite, or the official sample tooling) and at which tier (validate vs strict lint vs specific deny-rules such as broken-link).
- Harvest workflow evolution: the harvest skill's drop paths and the `NEW-*.md` prefix semantics under OKF (new concepts as `type: harvest`? separate directory? hook pathspec coupling), including bilingual-README coupling semantics in the new world.
- `assets/` internal structure: how `stack/`, `scripts/`, `sdk/`, `.githooks/` are arranged or flattened under the new top-level domain, and what the two READMEs index.

## Out of scope

- **Behavior changes to `stack/ssap` or scripts**: the restructure moves and re-links; it does not change what the code does.
- **Rewriting knowledge content**: migration is lossless relocation plus metadata; re-authoring or condensing the 78 documents is not this effort.
- **PCB / hardware-design material**: remains excluded from all work in this repo (standing user rule).
