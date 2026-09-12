# 05 — execute migration in batches

Type: task
Status: open
Blocked by: 04

## Question

Execute the migration per ticket 04's map, batch by batch, with the gates green after every batch:

1. `git mv` the knowledge plane into `knowledge/` (history preserved), then relocate/rename artifact-plane pieces into `assets/` per the map.
2. Rewrite every path-sensitive reference (scripts, hooks, README indexes, AGENTS.md, skill paths) in the same batch that moves its target paths.
3. Add OKF frontmatter per the batch plan; regenerate bundle `index.md` and `log.md`.
4. Each batch ends with: `bash -n` on all shell scripts, `check-docs.sh` 6/6, `check-harvest-archive.sh` passing, `git diff --check` clean.

Record per-batch: what moved, gate results, rollback notes.
