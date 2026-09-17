---
type: harvest
title: "RSSI HADNN adaptation: row-wise normalization, split ownership and coordinate evaluation"
language: en
created: 2026-09-17
tags: [harvest, rssi, fingerprinting, hadnn, tensorflow, preprocessing, evaluation]
sources:
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/call_data.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/models_repo.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/networks.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py"
  - "https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/classroom_1_5/call_data.py"
trust: A
stale_after: 2027-03-17
---

# RSSI HADNN adaptation: row-wise normalization, split ownership and coordinate evaluation

## Scope and provenance

This report covers one offline learning pipeline: `Algorithms/5_HADNN/code/runs_UJI_repo.py` and its loader, model, schedule and training/evaluation modules. The neighboring classroom loader is a configuration contrast, not a second evaluated pipeline. The pinned revision is `da1dda7c77aeb0abf841ed86f9ac60eaebc4863b`; the inspected source files have no differences against that revision. Trust A applies to direct source observations, not dataset validity, successful execution or numerical accuracy. CSV contents, saved models, training logs and exported results are not evidence for this report.

## Executive findings

1. **The active loader selects 24 mixed-radio features, not a generic scenario or four-input SLE model.** It names helipad-suffixed train/test CSVs, then selects eight Wi-Fi, eight SLE and eight BLE columns in that order. The `n_rss = 24` assignment is consistent with the selection; its adjacent four-per-radio comment is not. Earlier loader definitions in this file are commented out. [call_data.py:512-568][loader]
2. **RSSI scaling is within each observation; coordinate normalization is fitted on the training file.** `scale(..., axis=1)` separately standardizes every train/test feature row across its radio features. Training X/Y means and standard deviations normalize both sets of labels. The shown path neither pools test rows into a fitted feature scaler nor uses test-label statistics for coordinate normalization. [call_data.py:525-532][stats], [call_data.py:542-568][features]
3. **There is no validation partition or visible test-driven checkpoint selection.** The entry point constructs only shuffled training batches and one full test batch, with test evaluation after every epoch. Gradients use training batches only; final reporting consumes the last epoch, rather than the best test score. Repeated exposure of test metrics creates an opportunity for external tuning, but does not establish that such tuning occurred. [runs_UJI_repo.py:56-70][datasets], [train.py:19-25][gradient], [train.py:126-199][epochs], [runs_UJI_repo.py:109-130][reporting]
4. **This adaptation is a two-coordinate branched regressor, not an active hierarchical auxiliary classifier.** `n_hierarchy = 0`, the model output has width two, and the loss is coordinate MSE. The graph has a shared 128-unit block, dropout, two 64-unit branches, a further 64-unit block on one branch, concatenation and a linear output. There are no building/floor heads in this graph. [runs_UJI_repo.py:56-78][setup], [models_repo.py:17-53][model], [train.py:17-25][loss]
5. **Metric names conceal different quantities, and aggregation depends on batching.** The printed `mse` is mean Euclidean distance, `mse2` is mean squared Euclidean distance, and `mae` is mean Manhattan distance. Training averages batch statistics, including batch RMSEs; testing assumes exactly one batch. These are reporting contracts, not verified localization performance. [train.py:27-76][metrics], [train.py:134-187][aggregation]

## Pipeline and preprocessing contract

The entry point imports `CustomIndoor`, `HADNN1`, `cos_lr`, `runs` and `runs_pretrained`. It defaults to 100 epochs, batch size 128, warm-up length five epochs and the string `False` for pretrained mode; NumPy and TensorFlow seeds are set to 119. Loading and execution are top-level rather than isolated behind a main guard. [runs_UJI_repo.py:18-57][entry]

The active `code/call_data.py` builds paths below `datasets/CustomIndoor` and reads `train_wifi_sle_ble_dataset_heli.csv` and `test_wifi_sle_ble_dataset_heli.csv`. Its explicit feature names are `wifi_rssi1` through `wifi_rssi8`, followed by `sle_rssi1` through `sle_rssi8`, then `ble_rssi1` through `ble_rssi8`; truth is selected separately by `x` and `y`. This avoids interpreting arbitrary trailing columns as features, but does not verify radio identity, units, row provenance or missing-value conventions. There is no missing-RSSI mask, imputation policy or explicit finite-value/schema acceptance check. [call_data.py:517-568][input]

For a nonconstant 24-feature row, the intended RSSI transform subtracts that row's feature mean and divides by its feature standard deviation. It therefore removes an observation-wide RSSI offset and normalizes contrast across all three radio blocks together. This is not per-transmitter calibration, not per-radio normalization and not a scaler fitted across training observations. A missing-value sentinel would participate as an ordinary number unless the library or input representation treats it specially; no explicit sentinel rule appears here. This contrasts with the KNN baseline's pairwise feature masking. [call_data.py:544-568][features], [existing KNN contract](NEW-RSSI-KNN-FINGERPRINT-CONTRACT.md)

Coordinates use training-file means and pandas standard deviations independently for X and Y. Both label arrays are normalized in place. An important conditional defect precedes TensorFlow conversion: `np.array` has no explicit floating dtype. If both coordinate columns produce an integer array, assigning normalized floating values back into its slices can truncate them. Casting to float32 later cannot recover those lost fractions. Zero or nonfinite coordinate standard deviations are also unchecked. These are source-visible conditions, not findings about the unopened CSVs. [call_data.py:525-529][stats], [call_data.py:549-563][coordinates], [runs_UJI_repo.py:64-70][datasets]

The separate `classroom_1_5/call_data.py` instead names Wi-Fi classroom CSVs and selects four unprefixed `rssi1`-`rssi4` features, with the same normalization pattern. The entry point contains a bare `from call_data import CustomIndoor`, not a scenario dispatcher. Its colocated `code/call_data.py` is the selected dependency for the ordinary script path; the neighboring file's existence does not prove it participates in that execution. [classroom loader:16-68][classroom], [runs_UJI_repo.py:22-25][imports]

## Train, test and validation ownership

| Stage | Visible owner | Boundary |
|---|---|---|
| File membership | Two pre-existing CSV filenames in the loader | No split generation, session grouping or overlap audit is implemented. |
| Coordinate normalization | Training-file X/Y statistics | Reused for test labels and inverse transforms; test truth does not fit these statistics. |
| RSSI normalization | Each row independently | No cross-row train/test scaling fit; features from different radios are normalized together within a row. |
| Parameter updates | `train_ds` through `GradientTape` and SGD | Test evaluation computes loss and metrics without applying gradients. |
| Validation/model selection | No validation dataset or best-score selector in the shown path | Test metrics are exposed each epoch; final outputs refer to the final epoch. |
| Pretrained evaluation | Loaded model plus the current loader's statistics | Training-set-labelled evaluation also calls `test_step`; original model training provenance is not checked. |

Sources: [call_data.py:517-568][input], [runs_UJI_repo.py:64-100][dispatch], [train.py:19-76][steps], [train.py:189-224][finalepoch], [train.py:252-317][pretrained].

The defensible distinction is **visible data flow versus actual leakage**. There is no direct use of test labels in the gradient update or fitted coordinate statistics. Nevertheless, source separation into two filenames does not establish disjoint observations, acquisition sessions, trajectories or coordinate groups. Duplicates, correlated samples, upstream mixed-radio joins and human decisions based on repeatedly printed test scores remain unverified. The report cannot certify an independent holdout, and cannot claim leakage occurred. The pretrained branch additionally cannot establish what data produced the loaded artifact. [runs_UJI_repo.py:64-100][dispatch], [train.py:170-218][testloop]

## Model and optimization flow

`dense_bat_relu` composes Dense, BatchNormalization with epsilon `1.001e-5`, and ReLU. The graph is `24 -> 128 -> dropout(0.3) -> two 64-unit branches`; one branch passes through another 64-unit block, then both are concatenated before the two-coordinate linear projection. The model returns a functional Keras graph from its custom `build` method. No spatial bounds or post-prediction clipping appear in the evaluation path. [models_repo.py:17-53][model], [train.py:50-76][teststep]

Optimization uses Keras `MeanSquaredError` on normalized labels, not mean Euclidean distance in physical coordinates. Consequently, absent dtype corruption, the coordinate standard deviations implicitly weight raw X/Y squared residuals differently. SGD is configured with momentum 0.9, Nesterov acceleration and decay `1e-4`; a warm-up/cosine list supplies the learning rate per training batch. [train.py:17-25][loss], [runs_UJI_repo.py:80-85][optimizer], [networks.py:13-23][schedule], [train.py:134-144][batchloop]

Two qualifications matter before treating this as a reproduction-ready model:

- Both training and test functions call `model(X)` without explicitly setting `training=True` or `training=False`. The custom training loop therefore does not explicitly activate dropout or training-mode batch normalization. A gradient tape is not itself a training-mode argument. The report cannot assume the declared dropout probability and normalization moving-statistic updates are exercised during training; runtime/framework behavior was not established. [train.py:19-25][gradient], [train.py:50-53][testcall], [models_repo.py:17-49][blocks]
- `n_train_iter` is computed as `ceil(N / batch_size + 1e-10)`. At an exactly integral positive quotient, that epsilon makes the declared count one larger than the actual number of batches. This can depress divided training summaries and misalign epoch-based schedule lengths. Even with the correct count, equal weighting of a short final batch is not sample-weighted aggregation, and averaging batch RMSEs is not global RMSE. Actual triggering dataset sizes are unknown. [runs_UJI_repo.py:81-85][iteration], [train.py:134-153][trainaggregate]

The unused hierarchy helper and hierarchy-oriented parameter names should not be mistaken for extra prediction heads: the active entry point sets zero hierarchy levels, and the model/training step provide one output and one loss. [runs_UJI_repo.py:61-62][hierarchy], [train.py:17-48][trainstep], [models_repo.py:30-53][modeltail]

## Evaluation, export and reuse boundary

Predictions and targets are inverse-transformed before geometric metrics are calculated. For raw-coordinate residual vector `e`, the source reports:

- `mse = mean(||e||_2)`, a mean distance rather than MSE;
- `mse2 = mean(e_x^2 + e_y^2)`;
- `rmse = sqrt(mse2)` within a batch;
- `mae = mean(|e_x| + |e_y|)`, not mean absolute error per coordinate;
- population variance and standard deviation of scalar Euclidean distances after collection.

The normalized optimization loss is separate from all of these. Geometric metric units inherit the CSV coordinate units; no conversion establishes meters. Training distances come from successive model states during batch updates, whereas an epoch's test distances come from its end-of-epoch model. Training and test curves therefore differ in more than sample membership. [train.py:19-76][steps], [train.py:126-187][epochmetrics]

The entry point batches all test samples together, matching the evaluator's hardcoded division by one. Reusing `runs` with multiple test batches would sum batch summaries without the appropriate normalization, although the collected distance vector would still feed variance calculations. [runs_UJI_repo.py:68-70][testbatch], [train.py:170-187][testaggregate]

At the final epoch, the evaluator exports an ECDF plot and a CSV of individual errors. The plot sorts errors and uses ranks `1/N` through `1`; the CSV contains the original error sequence, not paired sorted errors and ECDF ranks. Pretrained mode produces a separate similarly structured export and evaluates rather than retrains the training set. Fixed output names can overwrite earlier outputs. [train.py:78-106][ecdf], [train.py:189-224][finalepoch], [train.py:252-317][pretrained]

The entry point saves the model unconditionally, including after pretrained evaluation, to a hardcoded machine-specific destination. Its pretrained load path is separately fixed, and the results pickle uses another fixed relative location. No explicit directory preparation or scaler/schema manifest accompanies these saves. A saved model therefore does not by itself preserve the coordinate normalization, ordered radio schema or train/test provenance needed to interpret later predictions. The API usage and filesystem assumptions are compatibility questions, not a demonstrated runnable environment. [runs_UJI_repo.py:74-85][load], [runs_UJI_repo.py:107-130][save]

Reusable elements are the explicit named feature order, training-owned coordinate statistics and separate gradient/evaluation functions. Before reuse, preserve those contracts while adding floating-coordinate conversion, finite/variance checks, acquisition-aware split evidence, a separate validation set, explicit Keras training modes, sample-weighted metrics and a model-plus-preprocessing manifest. These are derived acceptance requirements, not implemented capabilities or measured improvements.

## Relationship to existing reports

- [RSSI KNN fingerprint contract](NEW-RSSI-KNN-FINGERPRINT-CONTRACT.md) covers masked neighbor ranking and coordinate averaging. This report adds learned normalized-coordinate regression, branched model construction, train/test ownership and batch aggregation semantics; it does not establish superiority to KNN.
- [RSSI multilateration evaluation contract](NEW-RSSI-MLT-EVALUATION-CONTRACT.md) covers fixed path-loss calibration and solver acceptance. HADNN instead learns a fingerprint-to-coordinate mapping and has no active range solver in this path. Both reports require metric units and output semantics to be established separately from result labels.
- [RSSI mixed-radio preprocessing contract](NEW-RSSI-MIXED-PREPROCESSING-CONTRACT.md) covers coordinate joins and positional fusion. The HADNN loader consumes already-created mixed CSVs by explicit names; it neither performs those joins nor proves that these particular CSVs were produced by the previously described join scripts.

The contribution is an offline learning and evaluation contract, not a NearLink transport, ranging protocol or USB driver implementation. No accuracy, dataset independence or cross-radio performance conclusion follows from the source alone.

[loader]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/call_data.py#L512-L568
[stats]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/call_data.py#L525-L532
[features]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/call_data.py#L542-L568
[input]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/call_data.py#L517-L568
[coordinates]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/call_data.py#L549-L563
[classroom]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/classroom_1_5/call_data.py#L16-L68
[entry]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L18-L57
[imports]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L22-L25
[datasets]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L56-L70
[setup]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L56-L78
[dispatch]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L64-L100
[optimizer]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L80-L85
[iteration]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L81-L85
[hierarchy]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L61-L62
[testbatch]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L68-L70
[load]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L74-L85
[save]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L107-L130
[reporting]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/runs_UJI_repo.py#L109-L130
[model]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/models_repo.py#L17-L53
[blocks]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/models_repo.py#L17-L49
[modeltail]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/models_repo.py#L30-L53
[schedule]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/networks.py#L13-L23
[gradient]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L19-L25
[loss]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L17-L25
[trainstep]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L17-L48
[testcall]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L50-L53
[teststep]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L50-L76
[steps]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L19-L76
[metrics]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L27-L76
[epochs]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L126-L199
[epochmetrics]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L126-L187
[aggregation]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L134-L187
[batchloop]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L134-L144
[trainaggregate]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L134-L153
[testaggregate]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L170-L187
[testloop]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L170-L218
[finalepoch]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L189-L224
[pretrained]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L252-L317
[ecdf]: https://github.com/CQU-UISC/NearLink-RSSI-Fingerprinting-Dataset/blob/da1dda7c77aeb0abf841ed86f9ac60eaebc4863b/Algorithms/5_HADNN/code/train.py#L78-L106
