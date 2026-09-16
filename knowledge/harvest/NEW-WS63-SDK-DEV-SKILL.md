---
type: harvest
title: "SlumberMin/ws63-sdk-dev-skill — an independent agent skill for WS63 SDK development; distills build, serial-log, GPIO-mux and sample-hygiene knowledge"
language: en
created: 2026-09-13
tags: [ws63, agent-skill, build, menuconfig, fwpkg, serial-log, gpio-mux, methodology, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/ws63-sdk-dev-skill"
trust: verified
stale_after: 2026-12-13
---

# SlumberMin/ws63-sdk-dev-skill — an independent agent skill for WS63 SDK development; distills build, serial-log, GPIO-mux and sample-hygiene knowledge

- Inspection date: 2026-09-13 (staleness check: pushed 2026-07-05, not archived — ALIVE)
- Source root: `/mnt/hdd/nearlink-stuff/ws63-sdk-dev-skill`
- Mode: read-only inspection; the skill targets the same SDK family our harvest does, making it an external calibration of what an agent workflow needs

## Executive findings

1. The skill is a 785-line SKILL.md + 235-line reference for "develop, debug, build, verify firmware in a WS63 SDK workspace" — covering build.py targets, menuconfig/Kconfig, SLE/BLE/WiFi sample integration, build-failure triage, artifact lookup, fwpkg generation, and serial boot-log diagnosis. Its trigger list is effectively an independent inventory of the WS63 developer pain surface. [`SKILL.md:1-8`]
2. Workspace detection is path-portable: SDK root identified by `build.py` + `config.in` + `build/config/target_config/ws63/ws63.json`, with `application/samples/` and `output/ws63/` as tie-breakers — no hardcoded machine paths. [`SKILL.md:15-30`]
3. Methodology codifies five rules worth mirroring: evidence before conclusions; prefer sibling-sample reuse over memory; one root cause per round; **stop blind retries after three failures**; no "done" without build/artifact/log/hardware evidence — plus a Scout/Builder/Verifier role split with a single-writer rule. [`SKILL.md:36-58`]
4. The reference file distills hard-won WS63 facts: command baseline and success markers, main artifacts, config chain, common failure patterns, serial boot-log reading, a **GPIO mux quick reference with a power-on prohibition list** (pins that must not be driven at boot), and a **sample hygiene checklist** (dead source files, copy-paste artifacts, shared protocol files, tool artifacts in source tree, placeholder sensor data). [`references/ws63-reference.md` headings]
5. It also documents a **multi-device connection state-machine pattern** with rules and pseudo-code — the same topology problem our 1v8 vehicle digest solved differently, here generalized as a reusable pattern. [`references/ws63-reference.md` final section]

## Boundaries and gaps

- The skill ships a packaged zip (v1.0.0) of itself; no firmware code.
- GPIO power-on prohibitions are board-conditional; the list must be validated per board revision.
- Serial-log diagnosis is described procedurally, without a captured log corpus.

## Reusable for our stack

- Compare methodology against our own standing rules (wait-for-idle, -j1, no sub-agents): the "three-failure stop rule" and "single-writer" discipline are cheap additions worth adopting for long unattended sessions.
- The sample-hygiene checklist is a ready review gate for any WS63 sample code we import into our tree.
- Independent confirmation that build-failure triage, fwpkg artifacts, and serial boot logs are the dominant WS63 friction — our knowledge plane should keep those strands dense.

## Comparison anchors (vs existing reports)

- `NEW-BEARPI-H3863-DOCS.md`: vendor tutorials vs this agent-curated reference — complementary views of the same SDK surface.
- `NEW-WS63-MIDDLEWARE-OPEN-CLOSED.md`: the skill's build/config knowledge operates entirely in the open half of that map.
- `WS63-BUILD-FLASH.md`: our build/flash intel can absorb the skill's command baseline and success markers as cross-checks.
