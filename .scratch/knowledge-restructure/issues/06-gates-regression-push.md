# 06 — gates regression and push

Type: task
Status: resolved
Blocked by: 05

## Question

Final regression over the restructured repo, then push:

1. Controlled pre-push gate scenarios against the new pathspec: no new harvest note passes; new knowledge concept + only one README change fails; new concept + both README changes passes.
2. `check-docs.sh` full pass (cross-links, doc-index completeness against knowledge-plane paths, English-only rule, whitelist sanity).
3. Bilingual README sync verified in the same push as the migration commits.
4. Optionally: adopted OKF validator (ticket 01 recommendation) green on the new bundle.
5. Push each batch and the final state; report every verification result.
## Answer

Regression results (2026-09-12):

- check-docs.sh: 6/6 pass (cross-links, doc-index completeness over knowledge-plane paths, English-only docs/agents, whitelist sanity, identity, zh/en sync).
- check-okf.py: 90 concepts, M1-M3 + repo quality rules pass.
- check-harvest-archive.sh: pass (new pathspec knowledge/harvest/*.md).
- Controlled scenarios (throwaway repo): S1 new harvest concept + both READMEs = PASS; S2 new concept + neither README = rejected (coupling); S3 one-sided README = rejected (docs sync).
- git diff --check: clean at every batch.
- Push: see final push record in session log; transient SSH drops retried.
