---
type: harvest
title: elfbobo/hs-fbb (HiDiTing, romanized Di-ting) — SLE 2.0 public evidence: new bs_sle_* API generation, first-class Port service, auto-connection management (sparse-adopted)
language: en
created: 2026-09-13
tags: [hiditing, sle2, bs-sle, port-service, auto-conn, data-length-extension, ohos-watch, harvest]
sources:
  - url: https://github.com/elfbobo/hs-fbb
    note: metadata clone (blob:none, no-checkout) 2026-09-13; pushed 2026-09-07 (fresh, not archived); 764MB standalone HiDiTing SDK; SLE API docs fetched via raw URLs (4 files, ~3.7K lines)
trust: verified
stale_after: 2026-12-13
---

# elfbobo/hs-fbb (HiDiTing, romanized Di-ting) — SLE 2.0 public evidence: new bs_sle_* API generation, first-class Port service, auto-connection management (sparse-adopted)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-09-07, not archived — ALIVE)
- Method: metadata clone + raw-file fetch of four SLE API reference docs (the 764MB tree was not checked out — figure-heavy docs made sparse checkout too slow; targeted raw fetches replaced it)
- Scope: HiSilicon HiDiTing light-smart-terminal solution — the public record for the next SLE generation

## Executive findings

1. HiDiTing is HiSilicon's watch-class light-smart-terminal platform and the first public record we hold of **SLE 2.0 with 16 Mbps bidirectional transfer** (vs 12 Mbps on our 1.10.110 SDK generation), plus a 50-Gops edge NPU, pseudo-3D GPU, H.264, and OpenHarmony 5.1 with a watch app store. [`repo README.md:3`]
2. The SLE API surface is a **new API generation**: `bs_sle_*` functions replacing the `NLSTK_*` family we know — `bs_sle_register_device_callbacks`, `bs_sle_start_discovery`, `bs_sle_start_pair`, `bs_sle_set_connect_param`, `bs_sle_read_remote_device_rssi`, `bs_sle_set_phy_param`. Same domain decomposition (discovery/pair/connection), new naming and signatures. [`docs/zh-CN/HiDiTingV100/api-reference/bt_protocol/sle_service/connection/connection.md`]
3. New capabilities absent from our 1.x headers: `bs_sle_start_auto_conn`/`bs_sle_stop_auto_conn` (stack-managed automatic reconnection), `bs_sle_set_data_length` (data-length extension — the lever behind higher throughput), and `bs_sle_start_directed_reconnect` (fast reconnect to a known peer). [`connection.md` function list]
4. **The Port service is a first-class SLE 2.0 API**: `bs_sle_port_enable`, `bs_sle_port_create_local_port`, `bs_sle_port_create_remote_port`, `bs_sle_port_connect`, `bs_sle_port_write_data_by_uuid`/`by_port`, `bs_sle_port_disconnect/destroy_by_uuid/by_port` — local/remote port objects with UUID- and port-addressed addressing. This independently corroborates the SLE Port Profile pattern we digested from `tethering_nearlink` as an emerging standard transport, not a one-off. [`docs/.../sle_service/port/port.md`]
5. SSAP remains present as client/server sub-services (`sle_service/ssap/{client,server}`) alongside connection/persistence/port — the 2.0 stack keeps the profile layering while adding the port abstraction. [`docs/.../sle_service/` tree]

## Boundaries and gaps

- The 16 Mbps figure is from the solution README (marketing spec); the API docs fetched do not state PHY/MCS parameters — throughput mechanism unverified from these files alone.
- Docs are Chinese; API names/params were extracted from the function index — full parameter semantics not line-audited.
- The firmware sources behind these APIs were not fetched; the verdict is doc-level.
- GitHub mirror (GitCode upstream per README); freshness follows the mirror author's sync (2026-09-07).

## Reusable for our stack

- SLE 2.0 direction is now evidence-backed: our rust-ws73/SSAP abstractions should keep the port-object abstraction in mind (`create_local/remote_port`, write-by-uuid/by-port) since it is becoming a first-class stack service.
- `bs_sle_set_data_length` confirms DLE exists on the SLE roadmap — our throughput plans should account for it.
- Auto-connection management (stack-managed reconnect) is a feature to mirror in our host stack's connection manager.

## Comparison anchors (vs existing reports)

- `NEW-OHOS-TETHERING-SERVICE.md` / `NEW-MESHGATEWAY-APP-PROTOCOL.md`: the port abstraction appears in all three independent sources — strong convergence signal.
- `WS63-VS-WS73.md` / `NEW-OHOS-DEVICE-SOC-WS63.md`: the NLSTK parity we proved applies to the 1.x generation; HiDiTing's bs_sle_* marks where the next generation diverges.
- `NEW-DS10-SLE-DTU.md`: commercial SLE products now span 1.x (DS10) and 2.0 (HiDiTing) generations — our compatibility matrix should track both.
