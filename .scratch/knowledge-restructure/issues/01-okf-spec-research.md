# 01 — OKF v0.2 specification research

Type: research
Status: open
Blocked by:

## Question

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
