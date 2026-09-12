# 01 — OKF v0.2 specification research

Type: research
Status: resolved
Blocked by:

## Answer

**Spec source**: `GoogleCloudPlatform/knowledge-catalog` `okf/SPEC.md` (OKF v0.2, fetched 2026-09-12, 37,684 chars; §1-§13 + Appendix). Digest verified against two independent oracles (Sudhakaran88/okf-conformance, galkleinman/okf-toolkit).

**Bundle structure (§3)**: a directory tree of markdown files; layout is domain-independent. MAY be a subdirectory of a larger repo (our case: `knowledge/` at repo root). Reserved filenames at any level: `index.md` (directory listing, §8), `log.md` (update history, §9) — MUST NOT be used for concepts; all other `.md` are concept documents. "Path is identity": the file path is the concept's identity; subdirectories are free grouping.

**Concept frontmatter (§4)**: `---`-delimited YAML block + markdown body. REQUIRED: `type` only (short string, unregistered vocabulary, consumers tolerate unknown types). Recommended: `title`, `description`, `resource` (canonical URI of underlying asset), `tags` (YAML list). Extensions: producers MAY add any keys; consumers MUST tolerate unknown keys — so our `language: zh|en` extension is spec-legal.

**Provenance/trust/lifecycle (§5, all optional families)**: `sources` (per-source records with credibility signals; per-claim attribution via footnotes keyed to sources — NOT a body citations list), `generated`/`verified` (trust; bare `verified` mapping = one-element list), trust tiers derived only from specified fields, `status` (lifecycle), `stale_after` (freshness). §10: Attested Computations (deferred runtime details).

**Index (§8)**: no frontmatter except bundle-root `index.md` MAY carry `okf_version` (the only frontmatter permitted in an index). Body: sections grouping entries `[Title](relative-url) - description`, entries SHOULD carry the concept's `description`; MAY be generated automatically.

**Log (§9)**: flat date-grouped list, newest first, `## YYYY-MM-DD` ISO 8601 headings (MUST); bold verb convention (`**Update**`/`**Creation**`/`**Deprecation**`) is convention only.

**Conformance (§11) — the three MUST rules**: (1) every non-reserved `.md` has parseable YAML frontmatter; (2) every frontmatter has non-empty `type`; (3) reserved files follow §8/§9. Consumers MUST NOT reject for: missing optional fields, unknown types, unknown keys, broken cross-links, missing `index.md`. §6: a link to a not-yet-existing concept "may simply represent not-yet-written knowledge".

**Versioning (§12)**: minor = backward-compatible additions; major = breaking. Bundle declares `okf_version: "0.2"` in root index.md.

**Adoption decision for this repo**:
- **v0.2** (needed: `sources`/`verified`/`stale_after` provenance for harvest knowledge that cites network sources + file:line evidence).
- **Hard gate = the three §11 MUST rules**, implemented self-hosted (no external binary; whitelist-discipline + zero runtime deps): `scripts/check-okf.py` in the pre-push hook.
- **Quality layer, repo-specific**: internal-link resolution for knowledge-plane paths, `language: en` files must contain no CJK, harvest directory non-empty. Broken links to future concepts remain tolerated per §6/§11.
- Frontmatter batch plan: `type` (harvest|intel|decision), `title` (from H1), `language` (auto-detect), `created` (first-commit date), `tags` (empty list). `sources` populated opportunistically for harvest reports (their network origins are in-body URLs); not required for conformance.

Type vocabulary (producer-defined, self-explanatory per §4): `harvest` = external-repo knowledge digestion reports; `intel` = protocol/chip/SDK deep-dives and research docs; `decision` = recorded rulings (wayfinder resolutions, split boundaries).

What exactly is Open Knowledge Format v0.2 — precisely enough to design a real bundle with it?

Resolve from the primary source (GoogleCloudPlatform `knowledge-catalog` repo, `okf/SPEC.md`) plus the two independent oracles (`Sudhakaran88/okf-conformance` CONFORMANCE.md, `galkleinman/okf-toolkit` docs):

1. **Bundle layout**: top-level structure — concepts vs reserved files (`index.md`, `log.md`), subdirectory organization, whether sub-`index.md`s exist, "path is identity" consequences for file naming.
2. **Concept frontmatter**: the full field set — required (`type`) vs optional (`tags`, `timestamp`, `generated`, `verified`, `sources`, `status`, `stale_after`) — exact YAML shape, allowed `type` vocabulary, what v0.2 added over v0.1.
3. **Linking**: §6 semantics (concept-to-concept links, links to not-yet-written knowledge), citations format (`# Citations` / `sources`), and how "path is identity" governs relative links.
4. **Conformance**: M1–M6 and S1–S6 verbatim; §11 tolerated list; what `--strict` adds; which rule tiers a repo CI should gate on.
5. **Provenance/lifecycle**: `sources`, `verified`, `stale_after`, Attested Computations, actor conventions, §13.1 supersessions — what they mean for harvest knowledge that ages.
6. **Reference bundles**: shape of Google's four published bundles (`acme_retail`, `crypto_bitcoin`, `ga4`, `stackoverflow`).

Deliverable: an English spec digest recorded in this ticket's Answer (and optionally as a knowledge-plane doc once ticket 03 fixes the language policy), ending with a recommendation: which OKF version this repo adopts and which validation tier (validate / strict lint / specific deny-rules) its CI should enforce.

Note: no subagents (standing preference) — resolve in the main session.
