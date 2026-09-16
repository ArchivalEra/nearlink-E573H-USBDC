---
type: harvest
title: "Name-Collision Verdicts 0916: Five SparkLink NearLink Lookalikes Are Web and Mobile Apps"
language: en
created: 2026-09-16
tags: [harvest, triage, name-collision, verdict, web, flutter]
sources:
  - "https://github.com/THRIVE36/NEARLINK"
  - "https://github.com/jaegermichael/sparklink"
  - "https://github.com/Toshakarp/Sparklink"
  - "https://github.com/Ss2809/NearLink-main"
  - "https://github.com/Heebu/NearLinkChat"
trust: B
stale_after: 2027-03-16
---

# Name-Collision Verdicts 0916: Five SparkLink NearLink Lookalikes Are Web and Mobile Apps

Triage-only round: five GitHub search hits from the September freshness window
were screened by language, size, description, and root listing via read-only
API. None was cloned. Verdict on all five: name collision, not NearLink
(SparkLink SLE) wireless technology. This is the fourth collision batch on
record (after nearLinKernel, Terrydev5/NearLink, Qwac and the Android-shell /
C-sharp cases).

## Executive findings

- THRIVE36/NEARLINK (HTML, 88 KB, pushed 2026-09-09): the landing page says
  it all, title "NearLink — Everything You Need, Right Near You", meta
  description "NearLink connects you with trusted local services, shops,
  professionals, rides, delivery and more" [THRIVE36/NEARLINK contents
  index.html, head lines 1-15]. Root holds login/signup/service/auth pages
  [contents app.html, login.html, signup.html, service.html, auth.js].
  Verdict: local-services marketplace web page. No C, no Rust, no radio.
- jaegermichael/sparklink (TypeScript, 9.4 MB, pushed 2026-09-09): root is a
  TypeScript full-stack scaffold [contents client, server, shared, package.json,
  pnpm-lock.yaml, vite-era tsconfig set, .agents/.claude]. Verdict: web app
  sharing the name. Size comes from node_modules-era scaffolding, not firmware.
- Toshakarp/Sparklink (TypeScript, 309 KB, pushed 2026-09-06): root is a
  minimal Vite app [contents index.html, src, public, vite.config.ts,
  tsconfig.app.json]. Verdict: starter-template collision.
- Ss2809/NearLink-main (JavaScript, 14 MB, pushed 2026-09-01): npm package
  name is literally "nearlink-backend", entry `server/server.js`
  [contents package.json: name field, main field], deployed via
  [contents vercel.json] with UI/api/server split. Verdict: Node backend
  plus web UI; the "NearLink" here is a product name for a backend service.
- Heebu/NearLinkChat (Dart, 701 KB, pushed 2026-09-06): root is a standard
  Flutter scaffold [contents pubspec.yaml, lib, android, ios, web, windows].
  Combined with the prior-session finding (pure WiFi calling/messaging app,
  no SLE transport), verdict stands: mobile-app collision, second confirmation.
  Not re-cloned this round.

## Boundaries

- Evidence is root-listing plus manifest level; no source file was opened
  and no symbol search (sle_/ssap_/hcc_) was run inside these repos, because
  the language plus manifest evidence already excludes firmware (a radio stack
  cannot hide inside a Vercel Node backend or a static login page).
- Residual risk accepted: a repo could theoretically vendor a C SDK nobody
  references; judged negligible for HTML/Vite/Flutter scaffolds with matching
  web deployment descriptors.
- Heebu re-verdict reuses the prior session's functional finding; recorded
  here to close the September search-window loop.
- No firmware binaries, PCB data, or hardware design material touched.
  Nothing was cloned.

## Reusable

- Triage ladder that worked again and should stay standard: language plus
  size plus description first, root listing second, manifest content third,
  clone last (never reached this round). Five verdicts cost ~10 API calls
  and zero disk.
- Keyword anchors for instant collision call: `vercel.json`, `vite.config.ts`,
  `pubspec.yaml`, npm `server/server.js` entry, landing-page meta
  description with shops/rides/delivery vocabulary. Any future hit showing
  two or more of these can be verdicted without cloning.
- A hit only graduates to clone when the root shows C/Rust sources, SDK
  manifests (Kconfig, CMakeLists with chip targets), or radio vocabulary in
  file names. None of the five cleared that bar.
- Genuine-article rule restated: a repo is NearLink only with code-level
  evidence (SLE/SSAP include headers or API call sites). All five verdict
  lines above follow that rule in the negative direction.

## Comparison anchors

- NEW-NEARLINKERNEL-CLASSIFICATION.md: same verdict class (numerical-kernel
  Julia code behind a near-linear name). This batch adds the web-app and
  mobile-app collision subclasses.
- NEW-FRESH-SCAN-0916.md: documented Terrydev5/NearLink as another
  Bonjour-plus-WS naming case; this round extends the same collision ledger
  with five web-app and mobile-app verdicts (Heebu counted as re-verdict).
- NEW-GITCODE-SCAN-0915B.md: its upstream-freshness cross-check habit is
  reused here in reverse (pushed_at recency is what surfaced all five;
  recency does not imply relevance).
