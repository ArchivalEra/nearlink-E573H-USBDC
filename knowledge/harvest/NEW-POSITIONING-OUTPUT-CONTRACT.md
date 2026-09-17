---
type: harvest
title: "Ranging GUI positioning contract: hybrid coordinates, per-client priors, and bounded prediction"
language: en
created: 2026-09-17
tags: [harvest, ranging, positioning, kalman, state]
sources:
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/blob/5d6382ad6bcf112db1aae44f2801c4255c26dd5e/host/algorithm.py"
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/blob/5d6382ad6bcf112db1aae44f2801c4255c26dd5e/host/gui/main_window.py"
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/blob/5d6382ad6bcf112db1aae44f2801c4255c26dd5e/host/gui/services/kalman_filter_service.py"
  - "https://github.com/zhuzhengyan50-spec/nearlink-uwb-like-ranging/blob/5d6382ad6bcf112db1aae44f2801c4255c26dd5e/host/tests/test_core.py"
trust: A
stale_after: 2027-03-17
---

# Ranging GUI positioning contract: hybrid coordinates, per-client priors, and bounded prediction

## Executive findings

This report adds the GUI integration and trajectory-state contract to the previously documented ULS plus one-step Gauss-Newton solver. It is not a new solver discovery. The inspected revision `5d6382ad6bcf112db1aae44f2801c4255c26dd5e` matched a direct GitHub CLI HEAD query on 2026-09-17. All source anchors below refer to that revision.

1. **The displayed three-component result is not necessarily the 3D solver result.** `host/gui/main_window.py:1089-1104` combines the 2D solution's x/y with the 3D solution's z when both exist. With only a 2D solution it appends z=0. The separate `estimated_position` field prefers the 2D vector. Consumers must distinguish raw 2D, raw 3D and composed display coordinates.
2. **Warm starts are separated by client and dimension.** `main_window.py:1042-1087` selects priors from `last_pos_2d_by_client` or `last_pos_3d_by_client`, checks their lengths, and saves solver outputs. Insufficient anchor count clears the relevant prior. A changed active anchor subset can still reuse the previous prior when enough anchors remain; no subset-identity comparison occurs in this method.
3. **Admission and numerical success are separate concerns.** The GUI accepts positive distances that have configured 2D and 3D coordinates (`main_window.py:878-894`) and requires at least three/four for 2D/3D. The solver itself checks array shape and distance count, not geometric rank or positive finite measurements (`host/algorithm.py:12-26`). A non-None position therefore is not evidence of observable geometry or an accepted residual.
4. **Singular normal equations silently retain the starting estimate.** `algorithm.py:68-72` falls back to zeros if linear least squares raises LinAlgError; `:94-106` returns the current estimate if the Gauss-Newton solve raises the same exception. The caller receives only coordinates, not a failure status, condition number or residual. Rank-deficient least squares may return a solution without raising, and the returned rank is discarded.
5. **Prediction lifetime is measured in calls.** `host/gui/services/kalman_filter_service.py:101-119` increments `stale_steps` on each missing-position update and removes the state after it exceeds eight. Time delta is clamped to 0.02-0.25 seconds (`:11-14`). Eight missing updates do not represent a fixed real-world outage duration.

## End-to-end state contract

| Stage | Behavior established by source | Source |
|---|---|---|
| Input subset | Drops missing/nonpositive distances and indices outside either coordinate list; positive infinity is not explicitly excluded. | `main_window.py:878-894` |
| Initialization | Uses linear least squares without a prior; otherwise uses a dimension-compatible prior. | `algorithm.py:28-38` |
| Refinement | Exactly one undamped normal-equation update; no convergence loop or improvement test. | `algorithm.py:74-106` |
| Geometry fallback | Singular GN returns the input estimate rather than a flagged failure or automatic 2D reduction. | `algorithm.py:94-104` |
| Coordinate composition | 2D x/y plus 3D z when available; otherwise 2D x/y plus zero. | `main_window.py:1089-1104` |
| Filter identity | Separate 2D/3D dictionaries keyed by client string; an empty key shares one anonymous state. | `kalman_filter_service.py:28-30,53-66` |
| Filter dimension | Any position list of length at least three enters the 3D filter, including the GUI's 2D result padded with zero. | `main_window.py:1091-1092`; `kalman_filter_service.py:61-66` |
| First observation | Initializes position from observation, velocity to zero, covariance to five times identity. | `kalman_filter_service.py:70-78` |
| Missing observation | Predicts existing states; prefers the 3D prediction if both dimension stores contain the key. | `kalman_filter_service.py:53-59` |
| Display fields | Keeps `position` and optional `kf_position` separately. | `main_window.py:1146-1170` |

The constant-velocity filter has state order position followed by velocity, with transition entries linking each velocity to its corresponding position. Its process covariance uses dt powers four, three and two for position, cross and velocity blocks (`kalman_filter_service.py:80-99,121-138`). Initial measurement variances are 1.8 for x/y and 2.5 for z; process variances are 1.2 and 0.8. These are source defaults, not empirically validated uncertainty estimates.

Measurement covariance matrices are precomputed during construction. Changing the public measurement-variance attributes later does not rebuild those matrices in the inspected code (`:31-47,63-66`). Process covariance instead reads process-variance attributes on each call (`:127-131`). Configuration code must account for that asymmetry.

## Boundaries

- No tests, numerical experiments, build or hardware operations were performed. The upstream test source `host/tests/test_core.py:14-27` covers an exact six-anchor 2D solve and distance-count rejection. It does not establish hybrid-coordinate correctness, rank-deficient behavior, Kalman dropout semantics or real-world accuracy.
- The prior system report already documents the mathematical formula and the one-step limitation. This delta concerns caller behavior and state propagation; it does not claim a new ranging primitive.
- Anchor count alone cannot validate rank or conditioning. The GUI does not perform the competition solver's explicit rank-deficiency fallback in the inspected path.
- Clamped time deltas prevent large prediction intervals but also mean backward or repeated timestamps advance by the minimum model step. No finite-observation check appears in the inspected filter update method.
- GUI composition is not a jointly optimized 3D estimate. A visually smooth Kalman trajectory does not repair the underlying distance model or establish position validity.
- Source paths only were inspected. No PCB, firmware blobs or sensor hardware were accessed. The earlier system report records Apache-2.0 for new upstream code with original notices retained for SDK-derived files; no code was imported here.

## Reusable

1. Preserve distinct fields for raw 2D solution, raw 3D solution, display composition and filtered position in a WS73 host service. Do not silently present them as equivalent coordinates.
2. Carry solver status, active-anchor identity, residual and geometry diagnostics alongside coordinates before reusing an estimate as a warm start.
3. Label predictions as predictions and record time since the last actual measurement separately from model time and update count.
4. Keep per-client filter state, but define dimension transitions and whether artificial z=0 should be treated as a measured height.
5. Make covariance configuration explicit and rebuild precomputed matrices when measurement variance changes.

## Comparison anchors

- [NEW-NEARLINK-UWB-LIKE-RANGING](NEW-NEARLINK-UWB-LIKE-RANGING.md): baseline solver and system architecture. This report adds GUI composition, caller-side priors and filter-state semantics.
- [NEW-CS-POSITIONING-PAIR](NEW-CS-POSITIONING-PAIR.md): competition-side rank handling provides a contrast; its geometry protections must not be attributed to this simpler Python implementation.
- [NEW-IQ-PAIRING-PERSISTENCE](NEW-IQ-PAIRING-PERSISTENCE.md): measurement association and IQ provenance remain separate from position admission and trajectory smoothing.
