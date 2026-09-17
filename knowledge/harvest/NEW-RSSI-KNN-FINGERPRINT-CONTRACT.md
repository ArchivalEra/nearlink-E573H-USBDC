---
type: harvest
title: "RSSI KNN baseline: masked feature distance, inactive history and evaluation contract"
language: en
created: 2026-09-17
tags: [harvest, rssi, positioning, knn, fingerprinting, matlab, evaluation]
sources:
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/Radar_KNN.m"
trust: A
stale_after: 2027-03-17
---

# RSSI KNN baseline: masked feature distance, inactive history and evaluation contract

## Scope and provenance

Both requested MATLAB files were read completely, including inactive code and export/plot sections, under `Algorithms/2_KNN/` of the pinned repository. Local Git metadata identifies origin as `CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset` and HEAD as `da1dda7c77aeb0abf841ed86f9ac60eaebc4863b` (commit date 2026-08-14, subject `Update README.md`). A local comparison against HEAD showed no differences for either inspected file. The HTTPS source links above are derived from that locally verified origin and revision; no remote request or freshness check was made. Trust A applies to direct source observations, not reproduced numerical performance.

This report covers the offline KNN branch only. The MLT branch is already covered by an existing report and was not re-inspected. CSV contents, previously exported results, PDFs, hardware material and other algorithm implementations were not examined. No upstream code, build, dependency installation or numerical experiment was run.

## Executive findings

1. **The active estimator is three-neighbor, equal-weight coordinate averaging, not Gaussian weighting.** `KNN.m` sets `K = 3` and `Num_Mac = 24`; the helper sorts feature-space scores and averages the selected rows' first two columns. Despite the caller's "Using Gaussian Weight" and helper's "weighted average" comments, there is no distance-dependent weight, Gaussian kernel or third coordinate in the active calculation. [KNN.m:23-24](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L23-L24), [KNN.m:98-108](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L98-L108), [Radar_KNN.m:35-47](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/Radar_KNN.m#L35-L47).

2. **Neighbor ranking uses pair-dependent masked mean squared RSSI difference.** A feature contributes only when both the database value and test value are at least -99. The accumulated squared differences are divided by the number of contributing features; no square root is taken. Thus this is not the unqualified full-vector Euclidean distance advertised in the header. A square root would preserve finite score ordering, but the varying feature masks and denominator can change neighbors relative to an unmasked Euclidean metric. [Radar_KNN.m:19-35](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/Radar_KNN.m#L19-L35).

3. **The apparent temporal/history mechanism does not constrain prediction.** The script initializes history from the first test truth coordinate, assigns it to `Core_Weight_Point`, and updates history after every estimate. However, the only spatial-neighbor selection code is commented out: every query receives the entire training database. Neither the history value nor `distance_max` reaches the active helper call. Consequently this code does not implement motion gating, trajectory smoothing or a ground-truth-assisted predictor, despite maintaining truth-derived state. [KNN.m:38-71](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L38-L71), [KNN.m:80-118](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L80-L118).

4. **The data interface is positional, fixed-width and unchecked.** Two hardcoded CSV names are imported from a Windows desktop directory. The caller assumes numeric matrices: columns 1-2 are coordinates and columns 3 onward are RSSI. The helper consumes only the first 24 RSSI features, so extra columns are ignored and fewer than 24 features cannot satisfy its indexing. No feature names, radio identities, schema validation, normalization or training/test split construction are implemented. Filenames mentioning Wi-Fi/SLE/BLE do not establish the actual radio-column mapping. [KNN.m:23-36](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L23-L36), [KNN.m:66-69](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L66-L69), [Radar_KNN.m:24-29](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/Radar_KNN.m#L24-L29).

5. **Reported error is raw two-dimensional coordinate distance; the source does not establish meters.** The evaluation directly takes the square root of squared X/Y residuals. The earlier comment says "1 Unit = 40 inches", but there is no conversion in this path. Export and plot labels use meters regardless. Metric units therefore follow the CSV coordinate units unless separately established; no accuracy-in-meters conclusion follows from these two files alone. [KNN.m:20-24](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L20-L24), [KNN.m:123-139](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L123-L139), [KNN.m:157-171](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L157-L171).

## Data and algorithm boundaries

For a training row i, let V_i contain the feature indices among 1 through 24 for which both RSSI values are at least -99. The helper computes `score_i = sum((query_j - train_ij)^2 for j in V_i) / |V_i|`, sorts these scores, and returns the arithmetic centroid of the first three rows' X/Y coordinates. This describes the active source, not an experimentally validated localization model. [Radar_KNN.m:15-47](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/Radar_KNN.m#L15-L47).

Important consequences and missing acceptance checks are visible in that same helper:

- With no mutually usable features, the initialized zero score is divided by zero. There is no explicit no-overlap rejection, finite-score filter or fallback. No runtime-specific NaN sort behavior was tested.
- A row sharing only one usable feature can receive zero score and outrank a row supported by many features. There is no minimum-overlap requirement or overlap-confidence weighting.
- The -99 threshold is inclusive. There is no upper bound or explicit finite-value check: a zero placeholder would pass, and positive infinity is not excluded by the comparison alone. The actual dataset's missing-value convention remains unverified.
- There is no guard ensuring at least K candidates. K is counted in rows, not distinct coordinate sites; repeated fingerprints at one location can occupy multiple neighbor slots. Duplicate presence and tie behavior were not measured.
- The comments describing `X Y Z MAC1 Mean_RSSI1 ...` do not match direct numeric indexing. MAC identities and Z are not parsed. No averaging of raw RSSI into a "mean database" occurs inside these files.

The caller also uses MATLAB `length` rather than `size(matrix,1)` for both train and test counts. For a numeric matrix with fewer rows than columns, this chooses the larger dimension and can drive indexing beyond the available rows. `importdata` results are used without checking whether header-bearing input produced a structure rather than the expected numeric array. These are input-contract weaknesses, not claims that the unexamined CSVs trigger them. [KNN.m:27-36](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L27-L36).

Nothing here checks separation by collection session, location, time or repeated sample between train and test. Nothing constructs folds, tunes K, balances radios or compares modalities. This branch cannot by itself support a claim that SLE outperforms Wi-Fi/BLE, that train/test leakage is absent, or that an accuracy result generalizes across environments.

## Evaluation and output contract

**Statistics.** The script computes mean Euclidean position error, population variance and standard deviation of scalar distance errors, mean squared distance error and its square root. The MSE is the mean of squared X-plus-Y residuals, not an additional average over the two coordinate dimensions. No invalid-estimate exclusion, uncertainty interval or acceptance count is recorded. [KNN.m:123-139](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L123-L139).

**Timing.** `tic` starts before the test loop and `toc` follows the average-error calculation. This includes full-database matching, coordinate averaging, history bookkeeping, per-point error calculation and unsuppressed `CountPoint` display. It excludes CSV loading, variance/MSE/RMSE calculation, exports and plotting. It is neither isolated helper latency nor end-to-end radio/acquisition latency. [KNN.m:65-128](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L65-L128).

**Artifact naming and provenance.** The script scans `result_*.csv`, extracts numeric indices and chooses max-plus-one, defaulting to 1. It writes one metrics table and reuses that index for the ECDF and error-vector files. Only result filenames reserve an index: existing orphan `ECDF_Data_n.csv` or `Error_Data_n.csv` files are not checked, and the naming sequence is not an atomic concurrent-run reservation. The exported metrics omit K, feature count, threshold, input hashes, coordinate units, environment and software revision. [KNN.m:141-163](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L141-L163), [KNN.m:201-217](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L201-L217).

**Distribution and pointwise data.** The older threshold-loop CDF is commented out. The active path sorts the error vector, pairs entries with ranks 1/N through 1, plots a staircase and exports two headerless columns. A separate headerless file preserves scalar errors in test iteration order, but no active export includes truth coordinates, estimates, neighbor indices or overlap counts. A boxplot is active; trajectory drawing is commented out. The MATLAB runtime/toolbox environment and plot availability were not tested. [KNN.m:173-225](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/2_KNN/KNN.m#L173-L225).

## Deduplication and comparison to existing reports

A local search across the knowledge bundle for `Radar_KNN`, `2_KNN`, `Fingerprinting-Dataset`, `fingerprint` and `KNN` found no dedicated analysis of these two functions. Other fingerprint hits concern unrelated device/security fingerprints or sensors.

- [NEW-RSSI-MLT-EVALUATION-CONTRACT](NEW-RSSI-MLT-EVALUATION-CONTRACT.md), read in full, already covers the sibling multilateration branch. This draft does not duplicate its solver/calibration investigation. The new material is the KNN-specific pairwise masking rule, zero-overlap boundary, equal-weight centroid, inactive history, fixed positional feature contract and indexed exports. The existing report's timer/metric boundary cautions remain relevant, but its MLT-specific solver and post-processing findings must not be attributed to KNN.
- [ECOSYSTEM](../intel/ECOSYSTEM.md) classifies this repository as a positioning dataset unrelated to the USB product-ID search, noting numeric false positives. This harvest preserves that boundary while adding a source-backed offline benchmark contract. It establishes no WS73 USB command, SLE transport, SSAP dialect or Channel Sounding implementation.

## Reusable lessons and evidence limits

For a future host-side benchmark, the useful separable design is a pure fingerprint-to-coordinate helper plus an evaluation driver. Reuse the separation, not the unchecked defaults: make feature order and missing-value semantics explicit; require overlap and finite scores; count matrix rows; validate K and distinct-site policy; parameterize paths and units; and persist per-query estimates, selected neighbors and provenance alongside scalar errors. Keep preprocessing, inference, metric and export timing separate. These are recommendations inferred from source, not changes implemented here.

No reported accuracy or timing was reproduced, and no result CSV was treated as execution evidence. Only this draft was written outside the repository. No network, memory access, subagents, repository edits, Git writes, upstream execution/builds or hardware operations were used for this task.
