---
type: harvest
title: 2026 competition — WS63 multi-mode smart speaker: 48kHz/16bit uncompressed stereo over SLE at PHY 4M, plus DLNA/minimp3 network mode
language: en
created: 2026-09-13
tags: [ws63, sle, audio, dlna, minimp3, uncompressed, three-role, competition, harvest]
sources:
  - url: https://gitcode.com/HiSpark/2026_embedded_competition
    note: IOT/18884_WS63_SLE_DLNA_sound; cloned 2026-09-13, pushed 2026-09-07; 43 C/C++ files (cpp17)
trust: verified
stale_after: 2026-12-13
---

# 2026 competition — WS63 multi-mode smart speaker: 48kHz/16bit uncompressed stereo over SLE at PHY 4M, plus DLNA/minimp3 network mode

- Inspection date: 2026-09-13 (from the competition corpus registered in `NEW-2026-COMPETITION-SURVEY.md`; deep-dive queue item: SLE audio)
- Source root: `IOT/18884_WS63_SLE_DLNA_sound/ws63` (three role trees: sending end / receiving end / control end, 43 C/C++ files, cpp17)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access

## Executive findings

1. **Audio-over-SLE headline claim**: the SLE mode streams **48kHz/16bit dual-channel uncompressed PCM** by pushing transmission parameters to maximum (TX power, rate, **PHY 4M**), positioned against Bluetooth's compressed/mono limitations. This is a student-verified demonstration of SLE's audio bandwidth headroom — the concrete datapoint behind the SLE 12Mbps (and SLE 2.0 16Mbps) capacity numbers we hold. [README.md implementation notes]
2. **Three-role deployment on three WS63 boards**: sending end (audio source → SLE transmit), receiving end (SLE receive + playback, and the WiFi network member), control end (basic control + voice module, syncing state and mini-program config to the receiver, driving LED effects from playback status). [`ws63/` three trees]
3. **Dual-mode audio**: DLNA mode receives network audio and decodes with the vendored **minimp3** (`receiving end/includes/miniMP3/minimp3.cpp`), with an I2S/IIS playback path (`iis.cpp`) and audio_analyzer; the sending end reads audio via PCM2706 USB-audio (`pcm2706.cpp`) with a denoise module. [`receiving end/includes/`, `sending end/includes/`]
4. A WeChat mini-program handles provisioning and control (network credentials written into the device, volume/brightness/mode/voice-timbre panels) — app-side config as the provisioning path. [README.md]

## Boundaries and gaps

- The exact SLE audio framing (payload size, buffering, error handling for the uncompressed stream) was not isolated — the audio_read_send pipeline is thin (74 lines) with the transport in sle.cpp (490 lines) whose parameter names differ from our grep patterns.
- No latency/jitter measurements are included in the README; "low-latency stable" is asserted, not measured (contrast with DS10's measured envelope).
- Competition-grade: no security on the SLE audio link.

## Reusable for our stack

- **SLE carries uncompressed 48k/16bit stereo at PHY 4M in practice** — a feasibility proof for any low-latency audio-adjacent use of our WS73 dongle (and a concrete use for the `low_latency` service we saw in Nld's dongle protocol).
- The three-role split (source / player+network / control) is a clean multi-board audio topology over SLE + WiFi.
- DLNA + minimp3 as the network-audio fallback shows a pragmatic dual-mode product shape.

## Comparison anchors (vs existing reports)

- `NEW-HIDITING-SLE2-EVIDENCE.md`: SLE 2.0's 16Mbps makes such uncompressed streams trivial; this project does it on SLE 1.x at PHY 4M.
- `NEW-DS10-SLE-DTU.md`: DS10 measured 32B frames at 14ms — audio needs the opposite regime (large payloads, max PHY); both ends of the envelope now exercised by real projects.
- `NEW-OHOS-SA1190-IPC-SURFACE.md`: the ASC audio interfaces upstream corroborate that audio-over-NearLink is a first-class ecosystem direction.
