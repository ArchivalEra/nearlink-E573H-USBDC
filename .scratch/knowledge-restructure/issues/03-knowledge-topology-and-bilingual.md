# 03 — knowledge topology and bilingual policy

Type: grilling
Status: open
Blocked by: 01, 02

## Question

Once the OKF spec (ticket 01) and the split boundary (ticket 02) are settled: what is the internal topology of `knowledge/`, and what language policy governs it?

1. **Topology**: single bundle with typed subdirectories (e.g. `concepts/`, `harvest/`, `decisions/`) vs multiple bundles; how the 15 NEW harvest reports, the ~63 prior research docs, `docs/DEVICE-INTEL`-style intel, and ADR-style decisions map to OKF concept types; whether `index.md` is one root or per-directory; `log.md` as the harvest activity journal.
2. **Language policy**: current rules — `docs/` English-only (`AGENTS.md`), lab-notes are zh+en mixed, README pair bilingual with a hard sync gate, bilingual README check lives in `check-docs.sh`. Decide: does the English-only rule extend to all of `knowledge/`? Do existing Chinese-language documents get translated (violates "no content rewrite"?) or carried as-is with a `language` frontmatter field? Does the README pair remain the bilingual surface while knowledge itself is single-language?
3. **README relationship**: what do the two READMEs index in the new world — the bundle root `index.md`? And does the check-docs doc-index completeness rule now validate knowledge-plane paths?

Output: the topology in one diagram + the language policy as explicit rules with veto-able defaults.
