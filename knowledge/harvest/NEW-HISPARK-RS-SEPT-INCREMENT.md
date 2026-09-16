---
type: harvest
title: "hispark-rs September Increment (alpha.26 FRW ABI, NET0 RX lifecycle, fwpkg 0.3.3)"
language: en
created: 2026-09-12
tags: [hispark-rs, rust, ws63, abi, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: B
stale_after: 2027-03-12
---

# hispark-rs September Increment (2026-09-01 → 2026-09-10)

> Incremental digestion of the hispark-rs ecosystem (base report: `../intel/` carries the August snapshot as `RUST-WS73`-family notes; this file covers only the September delta). Local subrepos pulled with `--ff-only`; all claims cite `subrepo:commit` or `subrepo:path`.

## 0. Pull delta summary

| Subrepo | local was → now | commits | Theme |
|---|---|---|---|
| `ws63-radio-sys` | 8-31 → 9-09 | 4 | **verified FRW host-delivery ABI bound**; alpha.26 release; hostap-security CI gate |
| `hisi-rf-ws63` | 9-01 → 9-10 | 15+ | **NET0 native RX stop/cleanup lifecycle** (ownership, deadlines, descriptor rebuild) |
| `hisi-rs-template` | 9-01 → 9-09 | 5 | consume RF alpha.114→116 identity contracts; radio starters with public allocator capabilities; pin load-address image tooling |
| `ws63-examples` | 9-01 → 9-09 | 5 | pin Wi-Fi examples to RF alpha.115/116; sequenced two-board HIL startup test |
| `hisi-fwpkg` | 7-13 → 9-07 | 1 | **0.3.3 release: load-address image semantics** |
| `ws63-pac` | 8-07 → 9-03 | 2 | **complete KM flush-status registers**; SVD→PAC regen CI |
| `ws63-svd` | 8-07 → 9-03 | 1 | model all KM flush-status registers |

Ecosystem phase shift: from "alpha RF facade" to **verified host-delivery ABI + native RX/TX lifecycle discipline + published image semantics** — the device-side Rust base for rust-ws73 is now materially deeper than the August snapshot.

## 1. FRW host-delivery ABI (ws63-radio-sys alpha.26)

New module `crates/ws63-radio-sys/src/frw.rs` (82 lines) + `docs/abi/frw-host-delivery.md` (commit `2f4b180`, 9-09):

- **ROM callback slot 261** = `FRW_ROM_CB_RX_NETBUF` per the pinned `frw_rom_cb_rom.h`; the two-word argument ABI is documented by `frw_dmac_rom.h`. The delivered profile registers `frw_rx_netbuf` at this slot; ROM `hcc_slave_tx` invokes it **before** the host copy and optional MSG595 enqueue.
- **API surface**: `frw_rom_cb_register(function_id, callback)` (replace one ROM slot, returns no status — verify expected old owner before, read back after, serialize writers), `frw_get_rom_cb(function_id)` (read slot without ownership transfer), and the receiver `frw_rx_netbuf(netbuf: *mut c_void, payload_len: u32) -> u32` — `osal_u32` both, **not** `u16`/C-boolean.
- **The loaded historical trap**: do not use the one-argument declaration in `port_frw.h` (`frw.rs:6-11`). Two ABI warnings with direct WS73 relevance:
  - **A callback return is not a receipt**: the delivered receiver also returns zero after allocation/copy failures — zero does not mean not-delivered; downstream processing or native RX/TX quiescence must not be inferred from it.
  - Ownership contract: the callee owns one vendor DMAC netbuf including error/free paths; forwarders must call it exactly once and must not reinterpret or retain the payload.
- Relevance to us: the WS73 Linux SDK's WiFi firmware is ROM-patched the same way (`memory_config.h` ROM-code regions, `frw_rom_cb` family appears in our SDK's device-side headers). If rust-ws73 ever goes on-die, this slot ABI is the integration point; on the Linux-host side it explains why host-delivered WiFi frames can be silently dropped without a status path — our `PERF-WIFI-THROUGHPUT` hot-path assumptions should not equate RX call returns with delivery receipts.

## 2. NET0 native RX lifecycle (hisi-rf-ws63, 15+ commits 9-02→9-10)

`docs/net0-rx-stop.md` + CI checkers (`check-net0-rx-mode.py` 138 lines, `check-net0-consumer.py`):

- **One-shot native RX stop is a maintainer-only terminal operation** after *checked* station disconnect. Composition rejection is compile-time (BLE/SLE/SoftAP rejected because the operation stops the shared Wi-Fi MAC).
- **Ownership contract**: the incremental controller's explicit Disconnect operation owns the stop. After the native ioctl receipt + DISCONNECTED event, a **separate worker turn** rechecks ioctl completion, drains observed host TX, checks user cleanup, then submits the terminal stop — with its own deadline; error/cancellation/deadline-expiry cannot report success.
- **Why ownership matters** (the historical bug): the first prototype put the experiment in the shared deauth helper; hostap deauthentication is a *protocol request*, not the terminal stop, and a long test matrix reached stop before initial authorization. Moving ownership to explicit operation completion removed the destructive coupling; protocol recovery restores checked queue-4 handshake admission separately (`net0-host-tx.md`).
- Supporting fixes: direct native RX ownership for NET0 (`9afac33`), reject native RX receipts completed after deadline (`8a6908c`), drain observed native host TX before NET0 disconnect (`60faf85`), scope terminal RX stop to explicit disconnect (`9ee47bf`), GC-elided RX hook rejected at final ELF verification (`70efda3` — ELF-level liveness check, noteworthy: link-graph proof that a test hook was not elided).
- Relevance to us: **this is the stop-sequence discipline our WS73 WiFi deadlock work lacked**. Our `RESEARCH-DIRECTIONS` direction 1 (WiFi lazy-init PM deadlock) is the init-side twin of this teardown contract: exclusive PM, single-owner transitions, deadline-checked terminal ops, drain-before-disconnect. The NET0 experiment's ownership model is directly quotable when we design the wifi_soc stop/init FSM.

## 3. Image and registers

- **hisi-fwpkg 0.3.3** (9-07): publishes **load-address image semantics** — the fwpkg/image packaging now has a versioned load-address contract (`366c2cf`); template and examples pin it (`hisi-rs-template 6e4cd23`).
- **ws63-pac 0.4.x / ws63-svd**: **complete KM flush-status registers** exposed/modeled (`7ed7665` 223-line `src/lib.rs` change; `3f10373` in SVD) — Key-Manager flush status is now hardware-queryable from Rust; SVD→PAC regeneration is CI-verified.
- **hisi-rs-template**: radio starters now generate with **public allocator capabilities** (`a877d88`) — allocation ownership is part of the starter contract, aligning with the `hisi-alloc` story.
- **ws63-examples**: sequenced two-board HIL Wi-Fi startup test (`527bed0`) — a pattern for our dual-dongle bring-up (RESEARCH-DIRECTIONS direction 2): startup sequence as testable fixture, not ad-hoc shell.

## 4. What this updates for rust-ws73 (device-side base)

1. Pin **hisi-fwpkg 0.3.3** and RF alpha.116 contracts in our workspace when the device-side phase starts (today the device FW is an opaque blob; the Rust base is for the on-die future).
2. Note the **FRW slot-261 ABI** and its two warnings (two-arg signature, zero-return-not-a-receipt) as standing device-side facts; they apply to any WS63/WS73 firmware-adjacent work, including reading our SDK's `frw_rom_cb` references correctly.
3. Adopt the **NET0 stop ownership model** (explicit-op ownership, worker-turn recheck, deadline-checked terminal ops, drain-before-disconnect, compile-time composition rejection) in our wifi_soc init/stop FSM design — the init-side twin of the WiFi deadlock fix (RESEARCH-DIRECTIONS direction 1).

## 5. Boundaries

- All device-side RISC-V firmware engineering; no host-side Linux driver changes in the delta.
- `net0-rx-stop` is explicitly maintainer-only experimental — not a production profile, not a DMA fence, and "does not prove the preceding association failure is fixed" (`net0-rx-stop.md:19-24`).
- RF alpha.114→116 is a fast-moving target; pin, don't track, per the template's own release pins.

---

*Written 2026-09-12. Local subrepos refreshed (7 × `git pull --ff-only`). English-only per `AGENTS.md`.*
