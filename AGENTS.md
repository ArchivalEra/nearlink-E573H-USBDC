# AGENTS.md

This file orients any agent working in this repository. See `docs/agents/*.md` for the skill configuration.

## Repository

`ArchivalEra/nearlink-E573H-USBDC` — Linux driver for a HiSilicon WS73 NearLink (SparkLink/SLE) USB dongle (`ffff:3733`, "00000000"). See `README.md` for orientation and `docs/` for intel.

## Agent skills

### Issue tracker

Issues and specs live as markdown files under `.scratch/<feature>/` (local markdown tracker — no external service). See `docs/agents/issue-tracker.md`.

### Triage labels

Default vocabulary: `needs-triage`, `needs-info`, `ready-for-agent`, `ready-for-human`, `wontfix`. See `docs/agents/triage-labels.md`.

### Domain docs

Single-context: root `CONTEXT.md` + `docs/adr/`. See `docs/agents/domain.md`.

## Project rules

- **Docs are English-only** (`docs/`); `README.md` (zh) ↔ `README.en.md` (en) cross-link. A pre-push hook (`scripts/check-docs.sh`, installed via `scripts/install-hooks.sh`) enforces this — see README's document index.
- **Whitelist gitignore**: nothing is tracked unless allowlisted. SDK binaries/firmware/archives stay out of git; sources/docs are tracked.
- The SDK tree lives at `sdk/ws73_sdk_linux_WS73_1.10.110/` (HiSilicon source, reference only — do not commit firmware blobs or prebuilt daemons).
