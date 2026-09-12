# 04 — migration map and workflow sync list

Type: task
Status: open
Blocked by: 03

## Question

Produce the complete, path-exact migration map and the workflow-sync checklist that ticket 05 will execute — nothing executed yet, everything pinned.

1. **Migration map**: for every existing path that changes (78 research docs, `docs/` intel, tracker dirs, any `assets/` relocations), the table old-path → new-path → method (`git mv` preserving history vs fresh file) — batched into independently revertable groups.
2. **Path-sensitive inventory** (every place that references the old locations):
   - `scripts/check-docs.sh`: README cross-links, doc-index completeness, English-only `docs/` scan (path changes if docs move into knowledge plane).
   - `scripts/check-harvest-archive.sh`: `REPORT_PATHSPEC=".scratch/nearlink-driver/lab-notes/NEW-*.md"` — must track the new harvest location; README-change coupling semantics.
   - `scripts/install-hooks.sh`, `.githooks/pre-push`, `.gitignore` whitelist.
   - `AGENTS.md` and `docs/agents/*.md` path conventions.
   - Harvest skill (`nearlink-harvest`) drop paths and its `lab-notes 总数` counting command.
   - README.md / README.en.md index tables and layout tree.
   - Existing wayfinder trackers' self-referencing paths.
3. **New gate wiring**: which OKF validator tier enters CI (from ticket 01's recommendation) and where the check runs (pre-push? GH Action?).
4. **History-preservation strategy**: `git mv` batching so `git log --follow` survives; commit granularity (one commit per batch, gates green each batch).

Deliverable: the map + checklist as this ticket's Answer, sized so ticket 05 is pure execution with no remaining decisions.
