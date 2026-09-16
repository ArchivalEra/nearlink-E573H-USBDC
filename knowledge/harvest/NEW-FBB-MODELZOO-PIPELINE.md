---
type: harvest
title: "fbb-modelzoo pipeline decoded: 7 scenes, config-driven convert-to-fwpkg chain, MindSpore Lite micro_quant, per-chip accuracy tools"
language: en
created: 2026-09-15
tags: [harvest, modelzoo, tinml, pipeline, ws63, mindspore]
sources:
  - "https://gitcode.com/HiSpark/fbb-modelzoo"
trust: A
stale_after: 2027-03-15
---

# fbb-modelzoo pipeline deep-dive

## Executive findings

**1. The repo is HiSpark.AI's open model zoo with a uniform `run.sh → config.cfg` pipeline** for WS63 MCU / HiDiTing Nano NPU / Hi1156E Tiny NPU targets: **dataset generation → model conversion → SDK project packaging**, producing flashable artifacts. Seven scenes: image_classification, keyword_spotting (cnn-s on GoogleSpeechCommands), audio_event_detection (AnomalyDetection), audio_enhancement (RNNoise), human_activity_recognition, time_series_prediction, visual_wake_words. Each scene dir = `run.sh` (thin wrapper → `tools/application/run_app.sh --scene X --profile default`) + `config.cfg` + `src/`.

**2. config.cfg is the whole pipeline contract.** A keyword_spotting preset (`application/keyword_spotting/config.cfg`) shows the division: "generally don't change" block (PLATFORM=RISCV, CHIP_VERSION=ws63, TOOLCHAIN=mindspore-lite, QUANT_MODE=micro_quant — quantize at conversion, keep float32 I/O), user-must-fill block (SDK_PATH into fbb_ws63/src, ADAPTOR_PATH with adaptor/+include/, TOOLCHAIN_PATH to MindSpore Lite linux-x64), then scene/model/dataset paths (MODEL_FILE=cnn-s.onnx, MODEL_DIR=models/keyword_spotting/cnn-small/models). One config file drives everything; profiles allow presets per model+board.

**3. models/metadata.yaml is the chip→template binding table.** Per chip (WS63): SAMPLE_COMMON lists template files (`ai_main.c/h`, `app_msl_runtime.c/h`, `merge_ai_main.py`) that the packer injects into the generated SDK project — i.e., the zoo doesn't ship firmware, it **stitches model + runtime template + board sample into the fbb_ws63 tree and builds there**. Per-model entries bind model files (e.g. RNNoise-int8.tflite), quantization mode, dataset command, and sample paths.

**4. Accuracy tooling is per-chip comparative.** `tools/`: `compare_accuracy.py`, `nano_accuracy_compare.py` (HiDiTing Nano), `evaluate_custom.py`, plus convert/compile/application subtools — the repo treats "accuracy vs reference" as a first-class deliverable, matching the DS10/KWS competition pattern of quoting precision alongside latency.

**5. skills/ continues the agent-surface pattern.** `fbb-modelzoo-dataset` and `fbb-modelzoo-docs` are agent skills (same family as hs-fbb-cli, skills-nearlink CLAUDE.md) — the HiSpark.AI ecosystem consistently ships AI-agent interfaces alongside pipelines.

## Boundaries

- Pipeline verified from configs/scripts/tools tree; no conversion actually executed (needs SDK_PATH + MindSpore Lite download).
- Template internals (`ai_main.c`) live in tools/application/templates — not read; the MSL runtime contract is follow-up.
- HiDiTing Nano / Hi1156E paths inferred from tools names and README; not separately traced.

## Reusable

- run.sh+config.cfg+profile pipeline shape for any model-deployment toolkit (including our own if a WS73 firmware AI path ever appears).
- `micro_quant` convention (int8 weights, float32 I/O) as the WS63 quantization idiom.
- metadata.yaml chip→template binding — how to formalize "model to firmware" without forking SDKs.
- Scene list as the canonical TinyML task taxonomy for this chip class (KWS/anomaly-denoise/activity/visual-wake/time-series).

## Comparison anchors

- vs. WS63-AI-ECOSYSTEM report (sync 66): replaces the one-line ModelZoo mention with the full pipeline anatomy; confirms ecosystem claims at config level.
- vs. BYLE SDK (sync 93): BYLE closes its pipeline in an IDE with prebuilt libs; ModelZoo is open, scriptable, accuracy-verified — two philosophies of the same stack.
- vs. 17513 radar CNN (batch 9): ModelZoo is the reusable infrastructure version of the one-off competition edge-AI loops.
