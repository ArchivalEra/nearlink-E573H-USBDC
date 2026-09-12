# 06 — gates regression and push

Type: task
Status: open
Blocked by: 05

## Question

Final regression over the restructured repo, then push:

1. Controlled pre-push gate scenarios against the new pathspec: no new harvest note passes; new knowledge concept + only one README change fails; new concept + both README changes passes.
2. `check-docs.sh` full pass (cross-links, doc-index completeness against knowledge-plane paths, English-only rule, whitelist sanity).
3. Bilingual README sync verified in the same push as the migration commits.
4. Optionally: adopted OKF validator (ticket 01 recommendation) green on the new bundle.
5. Push each batch and the final state; report every verification result.
