# 03 — knowledge topology and bilingual policy

Type: grilling
Status: resolved
Blocked by: 01, 02

## Answer

User confirmed all recommendations (2026-09-12, Q6=A topology, Q4=A bilingual).

**Topology — single OKF v0.2 bundle at `knowledge/`**:

```
knowledge/
  index.md                 # bundle-root; frontmatter = okf_version: "0.2" only; sections enumerate all concepts
  log.md                   # bundle log; ISO 8601 date headings, newest first
  harvest/                 # type: harvest — external-repo digestion reports (15 files, NEW-*.md names kept)
    index.md
  intel/                   # type: intel — protocol/chip/SDK deep-dives + prior research docs + docs/*.md intel (file names kept)
    index.md
  decisions/               # type: decision — recorded rulings
    index.md
    knowledge-assets-split.md
```

File names are kept as-is (path is identity; existing cross-references and `git log --follow` survive). The former "NEW-*" prefix semantics becomes `type: harvest` frontmatter while the filename prefix is retained for hook-gate pathspec convenience. No per-feature bundles (rejected Q6=B) and no flat single directory (rejected Q6=C) — three typed subdirectories mirror the existing semantics of NEW-* reports, research docs, and wayfinder decisions.

**Language policy**:
- Existing documents migrate **unchanged** (lossless); every concept gets `language: zh|en` frontmatter (auto-detected at migration time; a custom extension key, spec-legal per §4 extensions).
- **New** knowledge documents must be English (AGENTS.md English-only rule generalized from `docs/` to all newly authored knowledge; legacy zh files exempt with `language: zh`).
- `check-okf.py` enforces: files declaring `language: en` contain no CJK characters.
- The README pair remains the **only** bilingual surface and keeps its hard zh/en sync gate (`check-docs.sh` check 6).

**README relationship**: README indexes the two planes at top level; `check-docs.sh` check 2 auto-extracts `` `*.md` `` references from README tables, so updating the tables moves the completeness check automatically. `knowledge/index.md` is the bundle's progressive-disclosure surface for agents (§8).

Once the OKF spec (ticket 01) and the split boundary (ticket 02) are settled: what is the internal topology of `knowledge/`, and what language policy governs it?

1. **Topology**: single bundle with typed subdirectories (e.g. `concepts/`, `harvest/`, `decisions/`) vs multiple bundles; how the 15 NEW harvest reports, the ~63 prior research docs, `docs/DEVICE-INTEL`-style intel, and ADR-style decisions map to OKF concept types; whether `index.md` is one root or per-directory; `log.md` as the harvest activity journal.
2. **Language policy**: current rules — `docs/` English-only (`AGENTS.md`), lab-notes are zh+en mixed, README pair bilingual with a hard sync gate, bilingual README check lives in `check-docs.sh`. Decide: does the English-only rule extend to all of `knowledge/`? Do existing Chinese-language documents get translated (violates "no content rewrite"?) or carried as-is with a `language` frontmatter field? Does the README pair remain the bilingual surface while knowledge itself is single-language?
3. **README relationship**: what do the two READMEs index in the new world — the bundle root `index.md`? And does the check-docs doc-index completeness rule now validate knowledge-plane paths?

Output: the topology in one diagram + the language policy as explicit rules with veto-able defaults.
