---
type: harvest
title: "Freshness Round 0916: teki128 link_ready Removal Plus Seven Repos Verified Current"
language: en
created: 2026-09-16
tags: [harvest, freshness, teki128, sle-mesh, web-flasher, qemu, pet-collar, uwb-ranging]
sources:
  - "https://github.com/teki128/nearlink"
  - "https://github.com/teki128/nearlink (HEAD c5cb7ae, delta 98fa110..c5cb7ae)"
  - "pull-verified current: BH4ME/sle_mesh, hinearlink/nearlink-web-flasher (GitCode), hispark-rs/fbb_ws63-qemu, yeyeye0212/starflash-pet-collar, zhuzhengyan50-spec/nearlink-uwb-like-ranging, iainbrux/keyboard-cli, goodspeed34/ws63flash"
trust: A
stale_after: 2027-03-16
---

# Freshness Round 0916: teki128 link_ready Removal Plus Seven Repos Verified Current

Scheduled freshness sweep per the harvest method (pushed_at screen, then
pull-as-ground-truth). One delta found, seven hot repos confirmed current.

## Sources

| Source | What it tells us |
|---|---|
| `teki128-nearlink/nearlink_client.c` (delta `98fa110..c5cb7ae`, -3 lines) | Write-only `link_ready` flag removed (former lines 19/110/174); zero references remain |
| `teki128-nearlink/nearlink_client.c:15,89-110` | Surviving discipline: `g_conn_id` single token, clear-to-0 plus unpair plus scan restart on disconnect |
| pull-verified HEADs, seven repos, no delta | BH4ME/sle_mesh cc0b0dc; hinearlink web-flasher f576841; hispark-rs qemu f105c89; pet-collar a86ae68; uwb-ranging 5d6382a; keyboard-cli e2c2232; goodspeed34 ws63flash 5bab2e7 |

## Executive findings

- teki128/nearlink delta c5cb7ae (2026-09-15, 3 deletions in one file):
  removes the file-scope `link_ready` flag from the minimal SLE client
  [teki128-nearlink/nearlink_client.c, former lines 19/110/174, commit
  c5cb7ae]. The flag was write-only (set on structure-found, cleared on
  disconnect, never read); current file has zero remaining references
  (verified by grep). Surviving state discipline is now: `g_conn_id` as the
  single connection token [nearlink_client.c:15], cleared to 0 on
  `SLE_ACB_STATE_DISCONNECTED` [nearlink_client.c:89-110] followed by
  unpair plus scan restart [nearlink_client.c:109-110]. Lesson for the
  smoke-benchmark role of this repo pair (see comparison anchors): connection
  state is a single integer, not a parallel boolean, so the host-stack
  acceptance baseline cannot desync between two state holders.
- Seven repos pull-clean (local HEAD equals remote HEAD, no delta to digest):
  BH4ME/sle_mesh (cc0b0dc), hinearlink/nearlink-web-flasher on GitCode
  (f576841), hispark-rs/fbb_ws63-qemu (f105c89), yeyeye0212/starflash-pet-collar
  (a86ae68), zhuzhengyan50-spec/nearlink-uwb-like-ranging (5d6382a),
  iainbrux/keyboard-cli (e2c2232), goodspeed34/ws63flash (5bab2e7).
- Owner corrections for future sweeps: the SLE mesh repo lives under
  BH4ME (not the OpenSparklink org), the web flasher's canonical remote is
  GitCode hinearlink (not a StarFish GitHub repo), and the QEMU fork is under
  hispark-rs. Earlier sweep guesses against the wrong owners returned 404;
  the local clone remotes are the authoritative owner record.
- Method note: `pushed_at` alone misleads. pet-collar shows upstream
  pushed 2026-07-13 against local HEAD 2026-07-09, yet pull reports
  already-up-to-date (the push touched a ref other than the default branch
  HEAD). Screen with pushed_at, decide with pull. Same effect seen on
  keyboard-cli (pushed 09-08 vs commit 09-07, no delta).

## Boundaries

- Read the full teki128 diff (3 lines) and the current client file's
  callback region; did not re-read `nearlink_server.c` (unchanged).
- The seven current repos were verified by pull only; no file reads this round.
- License stands as previously recorded per repo; not re-audited.
- No firmware binaries, PCB data, or hardware design material touched.

## Reusable

- Single-token connection state (`g_conn_id`, 0 == idle) is the pattern to
  copy into the WS73 dongle host stack: never keep a parallel ready boolean
  next to a connection id; the teki128 author just deleted exactly that bug
  class in public.
- Disconnect hygiene sequence worth copying verbatim: clear id, unpair
  remote, restart scan (three calls, fixed order).
- Sweep playbook update: always resolve owner from the local clone remote
  before calling the API; correct the sweep table when a 404 exposes a wrong
  owner guess (three corrections banked this round).
- pushed_at is a trigger, pull is the verdict: encode that order into any
  future freshness automation so ref-level pushes do not generate false
  stale flags.

## Comparison anchors

- NEW-TEKI128-MINIMAL-PAIR.md: this delta shrinks that report's 5-step SSAP
  subject by removing dead state; the smoke-benchmark role is unchanged and
  now cleaner (single state holder). No contradiction.
- NEW-FRESHNESS-ROUND-0913.md and NEW-FRESH-SCAN-0916.md: same round   format
  (delta plus current-list). The current-list this round overlaps the 0913
  freshness set with zero drift; the mesh/web-flasher/qemu/pet-collar lines stay
  valid without re-digestion.
