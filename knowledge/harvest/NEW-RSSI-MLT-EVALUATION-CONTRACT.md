---
type: harvest
title: "RSSI multilateration baseline: calibration, solver acceptance and evaluation boundary"
language: en
created: 2026-09-17
tags: [harvest, rssi, positioning, multilateration, evaluation]
sources:
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/H-IPS-MLT.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/README.md"
trust: A
stale_after: 2027-03-17
---

# RSSI multilateration baseline: calibration, solver acceptance and evaluation boundary

## Executive findings

This source-only inspection covers the complete 171-line `Algorithms/1_Multilateration/H-IPS-MLT.py` at revision `da1dda7c77aeb0abf841ed86f9ac60eaebc4863b`. It is an offline RSSI-to-position evaluation script, not a NearLink transport or Channel Sounding implementation. Source anchors below refer to that pinned file unless otherwise stated.

1. **Distance calibration is fixed, not learned from the loaded dataset.** `find_distance` uses reference RSSI -47.97 and path-loss exponent 1.742, applies `10 ** ((RSSI - reference) / (-10 * exponent))`, and processes exactly four values (`:33-42`). The script does not fit separate antenna/radio/environment parameters. Those defaults cannot establish equivalent accuracy for Wi-Fi, BLE and SLE. [Calibration source](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/H-IPS-MLT.py#L33-L42).
2. **The objective is unweighted range-residual least squares.** `multilateracao` minimizes the sum of squared differences between Euclidean anchor distances and RSSI-derived distances, initialized at `[2.5, 2.5]` for every observation (`:44-68`). It calls L-BFGS-B without a `bounds` argument, despite the method name. There is no warm start, confidence weighting, outlier rejection or geometry diagnostic in this function. [Solver source](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/H-IPS-MLT.py#L44-L68).
3. **Coordinates are accepted without optimizer success checks.** The function consumes `location.x` without testing success, status, residual, finite values or convergence (`:60-68`). Options specify `ftol=1e-5` and `maxiter=1e+7`; this report does not claim that every SciPy version accepts or behaves identically with those options. Returning a position is not an acceptance test.
4. **The active input is a Wi-Fi file with a strict column contract.** The top-level code reads the relative path `wifi_test_dataset_1_5m.csv`, extracts `rssi_1` through `rssi_4`, and later uses `x` and `y` as truth (`:79-83,101-102`). Four anchors are hardcoded at the corners of a five-by-five square (`:70-77`). Selecting a differently named file is insufficient if radio calibration, coordinates or feature order differ. [Input and geometry](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/H-IPS-MLT.py#L70-L102).
5. **Metrics describe post-processed positions, not raw optimizer output.** After solving every row, the script calls imported `limitador` with the scenario dimensions before computing error (`:93-108`). The module README says AOI limits positions inside the environment (`README.md:8`), but the pinned tracked tree has no `AOI.py`; it contains an `__pycache__/AOI.cpython-39.pyc` artifact. The actual limiting algorithm was not established from source and no bytecode was executed. [Evaluation entry](https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/1_Multilateration/H-IPS-MLT.py#L87-L115).

## Reproducibility and reporting contract

The script imports NumPy, pandas, SciPy and `AOI.limitador` (`:13-17`). No `if __name__ == '__main__'` guard separates loading the module from reading data, optimization and writing output. The input path is relative to the process working directory, not to the source file. The missing tracked AOI source prevents a source-complete reproduction based on this directory alone; a cached bytecode artifact is not a demonstrated usable import or a reviewed implementation.

The error vector is Euclidean position distance. MSE is the mean squared Euclidean error; RMSE is its square root. The separately printed variance is population variance of the scalar distance errors, not coordinate covariance or optimizer uncertainty (`:104-123`). The empirical distribution sorts the same errors and assigns ranks `1/N` through `1` (`:144-146`). Pointwise output includes truth, estimate and error; a separate file contains error/ECDF pairs (`:157-171`). Both use fixed output filenames and will overwrite those names if executed in a directory where they already exist.

The timer starts after CSV reading and feature extraction, and stops after the limiting call (`:85-99`). It includes the solve loop and post-processing, but excludes data loading, metric calculation and CSV export. It must not be described as end-to-end ingest latency or per-frame radio latency.

The README describes a hybrid fingerprinting/multilateration system and separate Kalman files (`README.md:1-8`). The inspected script itself runs only the multilateration branch and evaluation. Do not attribute a KNN or Kalman fusion stage to it merely from that README.

## Reuse and comparison

- Preserve separate raw-solver and post-processed outputs when designing a WS73 host benchmark. Report which one enters the error metrics, and retain solver status alongside coordinates.
- Parameterize radio calibration, anchor geometry, feature order and input schema explicitly rather than treating dataset filename changes as sufficient configuration.
- Package all source dependencies and record library versions before claiming a reproducible baseline. Missing source cannot be replaced by assuming the cached bytecode works.
- Reuse the clear pointwise/ECDF output separation as an evaluation design, while adding dataset and parameter provenance and non-overwriting output selection.

[NEW-POSITIONING-OUTPUT-CONTRACT](NEW-POSITIONING-OUTPUT-CONTRACT.md) examines a different Channel Sounding GUI with per-client priors and Kalman state. This report adds a genuinely different RSSI propagation model, iterative optimizer and offline metric contract. The existing [ECOSYSTEM](../intel/ECOSYSTEM.md) row classifies this repository as unrelated to the USB product-ID search; that does not make its positioning software irrelevant to host-side algorithm comparison.

No numerical experiment, dependency installation, build, bytecode execution or hardware operation was performed. No reported accuracy was reproduced. The root license is MIT with CQU-UISC attribution; the script also carries earlier author/reviser comments, which should be retained and investigated before reuse. This harvest imports facts, not code or datasets, and excludes the accompanying PDF and all hardware-design material.
