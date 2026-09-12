---
type: harvest
title: hs-fbb src map — interim_binary holds the closed SLE stack, wearable apps are open, and lwip ships an IP-over-SLE virtual netif (lwip_sle_adapter.c)
language: en
created: 2026-09-13
tags: [hiditing, lwip, ip-over-sle, netif, open-closed-map, wearable, interim-binary, harvest]
sources:
  - url: https://github.com/elfbobo/hs-fbb
    note: metadata tree analysis 2026-09-13 (10,020 .c files inventoried); lwip_sle_adapter.c fetched via raw (434 lines)
trust: verified
stale_after: 2026-12-13
---

# hs-fbb src map — interim_binary holds the closed SLE stack, wearable apps are open, and lwip ships an IP-over-SLE virtual netif (lwip_sle_adapter.c)

- Inspection date: 2026-09-13 (same upstream as `NEW-HIDITING-SLE2-EVIDENCE.md`, pushed 2026-09-07)
- Method: `git ls-tree` inventory of the 764MB tree (no checkout), one targeted raw fetch
- Scope: where the open/closed boundary runs inside HiDiTing's src/, and the first open IP-over-SLE integration

## Executive findings

1. The closed boundary is explicit: `src/interim_binary/3322/` (1135 files) is the prebuilt-binary mount point for the HiDiTing (3322) SoC — the SLE/BT host binaries live there, mirroring the closed `libbth_gle.a` pattern of the ws63 tree. [`git ls-tree src/interim_binary`]
2. The open bulk is substantial: `src/open_source/freebsd` (31,325 files), `middleware/services` (1,762), `kernel/liteos` (1,728), **`application/wearable` (1,579 — the watch application layer is source-open)**, `ohos/foundation` (1,438), `ohos/third_party` (1,069), `drivers` (1,260 combined), `middleware/utils` (804). [src inventory]
3. **`lwip_sle_adapter.c` (434 lines) implements IP over SLE as a lwIP virtual Ethernet netif**: it registers `g_sle_chba_localdev` with `output = etharp_output` and `linkoutput = sle_chba_send_pkt`, sets `flags = NETIF_FLAG_BROADCAST | ETHARP | IGMP`, `mtu = IP_FRAG_MAX_MTU`, and joins via `netifapi_netif_add(..., tcpip_input)` — full ARP/IGMP semantics over the SLE bearer. [`src/ohos/third_party/lwip/lwip_adapter/liteos/src/private/lwip_sle_adapter.c:203-233`]
4. The adapter's lifecycle API is complete and callback-driven: `sle_chba_netdev_create/destroy`, `_add_link/_del_link`, `_register_callbacks`, `_set_link_down_cb`, `_driver_send`, `_input_cb` — SLE link state maps 1:1 onto netif link state. [adapter function set]
5. Together with `NEW-HIDITING-SLE2-EVIDENCE.md` (16 Mbps, `bs_sle_*` API, Port service) and the middleware open/closed map, HiDiTing presents as the most complete public SLE 2.0 system: closed radio stack, open IP integration, open applications.

## Boundaries and gaps

- The netif MTU borrows `IP_FRAG_MAX_MTU` (IP fragmentation compensates the SLE payload budget); the actual SLE MTU value behind it is set elsewhere (closed side).
- Only one file fetched; the wearable application tree and foundation services were inventoried, not audited.
- chba naming (CH BA) is internal nomenclature; its expansion is not documented in-tree.

## Reusable for our stack

- The lwIP-over-SLE netif pattern (etharp_output + custom linkoutput + link-state callbacks) is a ready blueprint for exposing our WS73 SLE stack as a real network interface — IP stacks, ping, and TCP tooling would then work over the dongle unchanged.
- `interim_binary` naming gives the canonical place to look for closed firmware in future HiSilicon OHOS trees.
- IP_FRAG_MAX_MTU + fragmentation is the practical answer to small SLE payloads: let IP reassembly absorb the MTU gap instead of application framing.

## Comparison anchors (vs existing reports)

- `USB-PROTOCOL.md` / `02-dli-hcc-dialect.md`: our host stack currently speaks HCC over USB; the lwip adapter shows the device-ecosystem's parallel answer at the network layer.
- `NEW-OHOS-TETHERING-SERVICE.md`: tethering bridges sockets to a Port Profile; HiDiTing bridges at the lwIP netif — two different IP-over-SLE integration heights now archived.
- `NEW-HIDITING-SLE2-EVIDENCE.md`: same repo, complementary layers (API docs vs src tree).
