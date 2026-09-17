---
type: harvest
title: "RSSI mixed-radio preprocessing: coordinate joins and positional fusion schema"
language: en
created: 2026-09-17
tags: [harvest, rssi, preprocessing, multilateration, schema, alignment, evaluation]
sources:
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/pingjie.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/ronghe.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/helipad/pingjie.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/helipad/ronghe.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/classroom_1_5/pingjie.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/classroom_1_5/ronghe.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/classroom_2/pingjie.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/classroom_2/ronghe.py"
trust: A
stale_after: 2027-03-17
---

# RSSI mixed-radio preprocessing: coordinate joins and positional fusion schema

## Scope and provenance

This candidate covers `pingjie.py` and its mixed-radio consumer `ronghe.py` under `Algorithms/1_Multilateration/`, not a new estimator survey. Local Git origin identifies `CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset`; HEAD is `da1dda7c77aeb0abf841ed86f9ac60eaebc4863b` (2026-08-14, `Update README.md`). The inspected branch files match HEAD. Links are pinned to that locally verified revision; remote freshness is not established. Trust A denotes direct source evidence, not validated dataset quality or reproduced accuracy.

The complete distinct source variants were inspected. All four `ronghe.py` copies are byte-identical. Parking-lot and helipad `pingjie.py` copies are byte-identical and rename eight RSSI columns per radio. The classroom versions rename four; their only difference is the `1_5m` versus `2m` input filename suffix. Directory placement therefore changes preprocessing width without changing the fusion consumer.

## Executive findings

1. **Alignment is an exact coordinate-key inner join, not a synchronized observation pairing.** Every combination uses `pd.merge(..., on=['x', 'y'])`, including a nested three-radio merge. No timestamp, session, sample identifier, join-cardinality validation or unmatched-row audit participates. With ordinary non-null keys, duplicate coordinates generate all matching combinations; coordinates absent from either input disappear. This establishes a conditional alignment hazard, not observed duplicate or missing data. [parkinglot/pingjie.py:35-51](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/pingjie.py#L35-L51).

2. **Renaming is not a validated feature schema.** Outdoor variants rename `rssi1` through `rssi8`; classroom variants rename only `rssi1` through `rssi4`. Neither selects an exact input-column set, checks required RSSI names, nor reorders columns. The consumer keeps columns whose names start with any radio prefix, in their existing CSV order, then interprets fixed four-element positional blocks as radios. Prefix filtering does not bind a value to the radio implied by its name. [parkinglot/pingjie.py:15-32](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/pingjie.py#L15-L32), [classroom_1_5/pingjie.py:15-26](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/classroom_1_5/pingjie.py#L15-L26), [parkinglot/ronghe.py:75-91](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/ronghe.py#L75-L91).

3. **Eight-feature output can satisfy indexing while assigning values to the wrong radio.** Given ordered eight-feature radio blocks, the two-radio consumer takes both four-element slices from the first radio, treating its anchors 5-8 as the second radio's anchors 1-4. For the triple combination it takes Wi-Fi 1-4 as Wi-Fi, Wi-Fi 5-8 as SLE, and SLE 1-4 as BLE; remaining features are unused. The functions apply the selected radio's calibration and the same four anchor coordinates to these slices. This is a source-derived conditional mapping, not a statement about actual CSV headers or published results. [parkinglot/ronghe.py:17-30](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/ronghe.py#L17-L30), [parkinglot/ronghe.py:42-55](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/ronghe.py#L42-L55), [parkinglot/ronghe.py:70-91](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/ronghe.py#L70-L91).

4. **Truth coordinates condition dataset construction but are not explicit estimator features.** The producer joins on `x,y`; the consumer passes only prefixed RSSI values into multilateration and later extracts `x,y` for error calculation. This is label-assisted offline fusion, not evidence of direct truth-coordinate leakage into the solver. There is no train/test split construction or overlap check in this pair, so neither leakage absence nor leakage occurrence is established. [classroom_2/pingjie.py:29-45](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/classroom_2/pingjie.py#L29-L45), [parkinglot/ronghe.py:89-114](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/parkinglot/ronghe.py#L89-L114).

## Alignment and mismatch boundaries

For a coordinate present in two radios with multiplicities `a` and `b`, an unconstrained merge contributes `a*b` rows; the triple join contributes `a*b*c`. These are relational consequences, not measured counts. Independently collected samples at one site need not be simultaneous. A dataset of one aggregate fingerprint per site could intentionally use this join; its aggregation and uniqueness would need independent evidence. The scripts implement neither aggregation nor temporal matching.

Different modality combinations can cover different intersections of coordinate keys and weight repeated sites differently. Consequently comparing their row-averaged errors need not compare the same evaluation population. If fused combinations were later randomly split, reuse of constituent observations could cross the split boundary; these scripts do not perform such a split and do not prove that any downstream experiment did so.

The width mismatch is not automatically an exception. Enough prefixed columns permit positional indexing even when radio identity is wrong. Conversely, renamed-but-missing inputs can leave too few features. `rename` leaves unrecognized columns untouched; `rssi_1` is not `rssi1`. Reordering otherwise valid columns can also alter radio/anchor assignment. No explicit count, suffix order, finite-value, coordinate-unit or missing-value validation protects this interface.

Even with correctly ordered four-feature blocks, the common consumer hardcodes four anchor sites at `(0,5), (5,5), (0,0), (5,0)` and dimensions `5.1` by `5.1`. It assumes those same sites for each technology. The outdoor folder names do not configure outdoor geometry, and increasing feature-index lists alone would not generalize the anchor repetition logic. Calibration and geometry must both match the intended collection, which source alone cannot establish.

## Concrete reuse

- **Make pairing explicit.** Choose a documented site-aggregate join or an observation/session pairing policy. Validate key uniqueness for the former; retain session and observation identifiers for the latter. Record unmatched counts and before/after cardinalities for every modality combination.
- **Select named features in an explicit order.** Define each radio's exact ordered column list and corresponding anchor coordinates. Reject missing, extra or ambiguous RSSI columns rather than slicing a broad prefix match. Keep calibration with the radio/anchor schema.
- **Test contracts with synthetic fixtures before numerical benchmarking.** A site with two Wi-Fi and three BLE rows should expose six join combinations; an absent site should expose the inner-join exclusion. Four- and eight-feature inputs and reordered headers should either preserve named mappings or fail validation. These are proposed tests, not executed results.
- **Preserve evaluation provenance.** Export constituent sample identifiers and schema/calibration/geometry versions with fused rows. Compare modalities on a declared common population or report their differing coverage and site weights. If training is introduced, split by the intended independence unit before generating combinations that share observations.

## Deduplication and evidence limits

[RSSI MLT evaluation contract](NEW-RSSI-MLT-EVALUATION-CONTRACT.md) already covers the baseline solver, acceptance checks, AOI source gap and metric/timing boundary. The new contribution here is the producer-consumer contract: coordinate-only cardinality, four-versus-eight-feature variants, radio misassignment under positional slicing, and truth-conditioned fusion. The shared optimizer and metric formulas are not a separate harvest finding.

[RSSI KNN fingerprint contract](NEW-RSSI-KNN-FINGERPRINT-CONTRACT.md) already identifies a fixed 24-feature positional interface and an unverified split boundary. This report does not establish that these MLT outputs feed KNN. It adds a distinct mechanism by which preprocessing can combine or reweight observations, and shows why a compatible column count alone would not prove radio identity or sample independence.

CSV contents, result artifacts and collection logs are outside this source evidence. Actual duplicate counts, missing keys, header order, units, synchronization, split overlap and the provenance of existing mixed datasets remain unverified. No numerical accuracy or runtime is reproduced. These scripts demonstrate an offline RSSI evaluation interface, not a WS73 USB command, SLE transport implementation or live localization pipeline.
