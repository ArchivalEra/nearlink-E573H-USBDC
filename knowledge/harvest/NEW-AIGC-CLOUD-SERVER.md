---
type: harvest
title: "WS63E AIGC cloud server — DashScope text2image pipeline with LLM prompt optimization and Amap weather; exposed credential noted (not reproduced)"
language: en
created: 2026-09-13
tags: [aigc, dashscope, text2image, llm, weather-api, credential-leak, flask, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: A
stale_after: 2026-12-13
---

# WS63E AIGC cloud server — DashScope text2image pipeline with LLM prompt optimization and Amap weather; exposed credential noted (not reproduced)

- Inspection date: 2026-09-13 (same current clone as `NEW-WS63E-MESH-AIGC-FRAME.md`)
- Mode: read-only local program inspection; no network, build, hardware, or PCB access
- Scope: the cloud half of the AIGC frame system — server, generation pipeline, and a credential-hygiene finding

## Executive findings

1. The cloud half is a 3,594-line Python service: `server.py` (2,830) is a photo-gallery review application — paginated rows, EXIF extraction and date parsing, memory-sorted listings, static file serving with a path-traversal guard (`_safe_join`), and an HTML review UI builder; `ai_image_gen.py` (764) is the generation pipeline. [`Cloud_AIGC_Server/server.py:55-407`]
2. The generation pipeline chains three external APIs: **DashScope text2image** (Tongyi Wanxiang, asynchronous submit-then-poll via task id), a **DashScope LLM endpoint** for sentiment analysis, story generation and prompt optimization, and the **Amap weather API** for environment-adaptive wallpapers — the "environment-driven AI painting" loop from the README resolves to these three calls. [`Cloud_AIGC_Server/ai_image_gen.py:36-66`]
3. The mesh distribution side consumes the generated images through the RLE/JPEG path already documented; an additional `encoder_epd_wifi/` C module implements a companion bare-metal e-paper/OLED encoder node with an explicit GPIO pin map (OLED and EPD sharing MOSI/SDA on GPIO_09, per-purpose chip selects) — a wired twin of the wireless display nodes. [`Cloud_AIGC_Server/encoder_epd_wifi/inc/encoder_epd.h:8-22`]
4. **Credential-hygiene finding**: `ai_image_gen.py` commits a hardcoded Aliyun DashScope API key (with a `getattr` fallback so any configured key overrides it), and `config.py` names a local Qwen2.5-VL-3B-Instruct deployment. The key is exposed in a public repository — flagged here by reference only, deliberately not reproduced; the upstream author should rotate it. [`ai_image_gen.py:36-44`]

## Boundaries and gaps

- The review UI's template rendering and the EXIF pipeline were read structurally, not executed.
- No authentication on the server's review/upload endpoints beyond the webui flag; it is a LAN tool.
- The AIGC task polling interval and error-path handling were not line-audited.

## Reusable for our stack

- The three-API chain (weather context → LLM prompt shaping → async text2image with task polling) is a compact reference for any context-adaptive content generation feeding our display work.
- The fallback-then-override credential pattern (`getattr(cfg, KEY, None) or "hardcoded"`) is exactly the anti-pattern to avoid in our own tooling: config override exists but the committed default leaks. Our repos keep credentials out of tree entirely.
- `_safe_join`-guarded static serving is a minimal correct pattern worth keeping for any local web surfaces we ship.

## Comparison anchors (vs existing reports)

- `NEW-WS63E-MESH-AIGC-FRAME.md`: the firmware half; this closes the cloud half of that system.
- `NEW-SMART-CABINET-FULLCHAIN.md`: both pair embedded nodes with a Python cloud/web layer; the AIGC server adds the credential-leak cautionary tale.
- `NEW-MESHGATEWAY-APP-PROTOCOL.md`: the image bits flow cloud → RLE/JPEG → mesh → e-paper; all three stages now documented.
