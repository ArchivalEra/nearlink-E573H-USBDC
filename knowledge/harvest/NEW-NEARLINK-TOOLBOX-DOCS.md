---
type: harvest
title: "NearLink Toolbox docs: Astro content pipeline and software tutorial evidence boundary"
language: en
created: 2026-09-17
tags: [harvest, toolbox, astro, documentation, serial, at]
sources:
  - "https://github.com/Sgguo-Development-Team/nearlink-toolbox-docs/tree/d3ba95d72fea8d1a19d73a844de841ebc46c511c"
trust: B
stale_after: 2027-03-17
---

# NearLink Toolbox docs: Astro content pipeline and software tutorial evidence boundary

## Executive findings

The separate documentation repository at `d3ba95d72fea8d1a19d73a844de841ebc46c511c` implements an Astro/Starlight content site. Its software tutorials are useful reference material, but are not evidence of a checked-in serial terminal, flash backend, or SLE stack. This is a source-only inspection: no package install, site build, media retrieval, firmware execution, or hardware operation was performed.

- **The executable package surface is a documentation build and deployment pipeline.** `package.json:5-22` defines Astro dev/build/preview plus Wrangler Pages commands. Dependencies include Astro `^5.1.7`, Starlight `^0.31.1`, Markdoc, Sharp, and image zoom; they do not establish a Tauri or serial backend. These are declared version ranges, not a claim about resolved installed versions. [Package source](https://github.com/Sgguo-Development-Team/nearlink-toolbox-docs/blob/d3ba95d72fea8d1a19d73a844de841ebc46c511c/package.json#L5-L22).
- **Content validation is delegated to Starlight schemas.** `src/content.config.ts:1-8` declares `docs` and `i18n` collections using the respective loaders and schemas. This validates documentation structure, not the correctness of embedded AT commands or device compatibility. [Collection source](https://github.com/Sgguo-Development-Team/nearlink-toolbox-docs/blob/d3ba95d72fea8d1a19d73a844de841ebc46c511c/src/content.config.ts#L1-L8).
- **Localization and navigation are site configuration, not protocol capability.** `astro.config.mjs:39-129` uses Chinese at the root and English under `en`, with auto-generated directories for user guides, AT guides, API documentation, plugins, and other categories. A configured category does not prove a substantive implementation or even populated content. [Configuration source](https://github.com/Sgguo-Development-Team/nearlink-toolbox-docs/blob/d3ba95d72fea8d1a19d73a844de841ebc46c511c/astro.config.mjs#L39-L129).
- **The video component embeds an external player rather than handling a device.** `src/components/Video.astro:2-19` accepts a `src` string and optional heading, then emits an iframe. Its 16:9 presentation uses CSS padding and absolute positioning at `:27-43`. It has no serial I/O or protocol lifecycle. [Component source](https://github.com/Sgguo-Development-Team/nearlink-toolbox-docs/blob/d3ba95d72fea8d1a19d73a844de841ebc46c511c/src/components/Video.astro#L2-L43).
- **The setup guide explicitly delegates flashing to external executables.** `src/content/docs/getting-started/first-config.mdx:58-70` describes selecting a Burntool or ws63flash executable path. The automatic-power-on capability is attributed to Burntool in the tutorial, not demonstrated by application source here. The serial settings section documents 115200 baud, eight data bits, no parity, and a merge-delay value of 50 (`:78-99`); the unit of that delay is not specified in the inspected default list. [Software configuration guide](https://github.com/Sgguo-Development-Team/nearlink-toolbox-docs/blob/d3ba95d72fea8d1a19d73a844de841ebc46c511c/src/content/docs/getting-started/first-config.mdx#L58-L99).

## AT tutorial evidence is not an executable test

The software-only section of `src/content/docs/at-guides/hihope-hhd03.mdx:266-502` gives SLE server, client, write, and read examples. The server tutorial orders enable/address, SSAP callback registration, server/service/property/descriptor creation, service start, and advertising configuration. The client orders enable/address, callback registration, scan setup/start/stop, connection, and structure discovery. [Pinned tutorial section](https://github.com/Sgguo-Development-Team/nearlink-toolbox-docs/blob/d3ba95d72fea8d1a19d73a844de841ebc46c511c/src/content/docs/at-guides/hihope-hhd03.mdx#L266-L502).

Three boundaries matter when converting this material into future software tests:

1. Numeric server, connection, and handle values are embedded examples. A robust runner must obtain its own identifiers from runtime responses; this document supplies no parser or state machine to do so (`:297-332,421-440`).
2. The server-to-client tab shows a notify command, a success return, and then an explicit read request with a read confirmation (`:459-485`). That transcript does not independently establish receipt of an asynchronous notification. A notification test must assert a notification callback rather than substitute a successful read.
3. BLE and SLE are separate tutorial tabs. The BLE client instructions explicitly include pairing (`:171-183`); the inspected SLE client sequence does not include a separate pair command (`:363-429`). This difference does not establish that SLE pairing is unnecessary, implicit, or unsupported by the underlying firmware.

The tutorial relies on preloaded AT firmware and an imported command collection (`:266-267`). Its transcripts and externally hosted media are upstream documentation claims, not execution evidence collected during this harvest. Hardware-description sections and media were excluded from analysis.

## Reusable and non-reusable boundaries

- Reuse the two-locale content collection and directory-driven navigation pattern for a software operator manual. Add content checks that distinguish populated reference pages from navigation promises.
- Keep written command examples separate from executable fixtures. A future fixture needs an identified firmware revision, response parser, identifier capture, timeouts, and distinct write/read/notification assertions.
- Treat the iframe's optional heading as a page heading, not as an iframe accessibility label: the component does not pass it as the iframe `title` attribute (`Video.astro:11-18`). A reused component should provide that label and make external-media loading explicit.
- Do not infer an implemented flash or serial backend from dependency declarations, menu names, tutorial screenshots, or references to executable paths. Do not apply the tutorial's numeric identifiers to WS73 USB commands.
- No upstream code was imported. The scope was documentation-site source and software tutorial text only; functionality and build success remain untested.

## Comparison and deduplication

[NEW-NEARLINK-TOOLBOX-WEBSITE](NEW-NEARLINK-TOOLBOX-WEBSITE.md) covers the separate Next.js marketing website and its remote Windows download flow. This report instead establishes the Astro collection/schema/navigation implementation, the external-player component, and the limits of the AT tutorial transcripts.

[NEW-NEARLINK-ASSEMBLY-OPTIMIZATION](NEW-NEARLINK-ASSEMBLY-OPTIMIZATION.md) previously listed this repository as a search hit and a marketing-to-tooling boundary reference. That listing is not a prior source-level inspection of this documentation pipeline. This report closes that candidate without counting the already-covered website again.
