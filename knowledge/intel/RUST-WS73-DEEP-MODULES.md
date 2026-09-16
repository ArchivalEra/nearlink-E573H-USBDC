---
type: intel
title: "RUST-WS73 Deep Module Boundaries — Lab Note"
language: zh
created: 2026-09-05
tags: [intel, rust, ws73, module]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: B
stale_after: 2027-03-05
---

# RUST-WS73 Deep Module Boundaries — Lab Note

- **Ticket:** `.scratch/rust-ws73-tri-mode/issues/08-deep-module-boundaries.md` (task, AFK research)
- **Date:** 2026-08-19
- **Strict scope:** read-only, no network/build/hardware. Exactly one file at `.scratch/nearlink-driver/lab-notes/RUST-WS73-DEEP-MODULES.md`.
- **Question:** How to apply the `dependency-cruiser` deep-module pattern (`/setup-ts-deep-modules`) to `rust-ws73/` — each crate a narrow `lib.rs` entry, `src/internal/` deep, seams aligned to the 5-module `stack/ssap` split, while keeping an extreme-perf small interface.

---

## 0. Sources inspected

| Source | Path | Notes |
|---|---|---|
| ssap 5-module headers | `stack/ssap/include/ssap_codec.h` | codec seam |
| | `stack/ssap/include/hwsle_transport.h` | transport HAL seam |
| | `stack/ssap/include/ssap_server.h` | server seam |
| | `stack/ssap/include/ssap_link.h` | link/FSM seam |
| | `stack/ssap/include/feature_mgr.h` | feature-mgr seam |
| | `stack/ssap/include/ssap_pkt.h` | wire-format ground truth |
| ssap implementations | `stack/ssap/src/ssap_codec.c`, `hwsle_transport.c`, `ssap_server.c`, `ssap_link.c`, `feature_mgr.c` | depth behaviour behind each seam |
| build | `stack/ssap/Makefile` | layer list, `libssap.a` |
| wire docs | `docs/agents/domain.md` | single-context glossary pointer |
| effort map/issues | `.scratch/rust-ws73-tri-mode/map.md`, `issues/03-lto-extreme-perf-decision.md`, `issues/04-rust-workspace-shape.md`, `issues/05-incremental-adoption.md`, `issues/08-deep-module-boundaries.md` | LTO/perf, workspace shape, incremental FFI |
| deep-module skill | `~/.agents/skills/setup-ts-deep-modules/SKILL.md` + `dependency-cruiser.config.cjs` | entry-point + 4 forbidden rules |
| riscv31 analogue | `fbb_ws63/src/drivers/chips/ws63/arch/riscv/riscv31/*` (prompt path) | **not present** in this checkout (see §8) — SDK tree is `sdk/ws73_sdk_linux_WS73_1.10.110/` |

> All citations below use absolute paths with `file:line` (English-only, pre-push hook `scripts/check-docs.sh` per `AGENTS.md`).

---

## 1. The 5-module seam today — what we would mirror

The C host stack is already five seams plus the wire-format header. `stack/ssap/Makefile:9` lists the layer order explicitly:

```
# stack/ssap/Makefile:9
SRCS := src/ssap_codec.c src/hwsle_transport.c src/ssap_server.c src/ssap_link.c src/feature_mgr.c
```

| # | Module | Header (public seam) | Impl | Responsibility | Perf-relevant properties |
|---|---|---|---|---|---|
| 1 | **ssap_codec** | `stack/ssap/include/ssap_codec.h:96-146` — 10 encode fns + `ssap_trans_type_of` + `ssap_opcode_of` | `stack/ssap/src/ssap_codec.c:11-217` | Byte-level SSAP PDU codec per `stack/ssap/include/ssap_pkt.h:107-588` (`SSAP_MsgCode_E` 0x01..0x14, `SSAP_TransType_E`, fragment/ctrl bits). Pure functions, no I/O, no global state. | Hot path — single allocation-free `memcpy`/`put_u16`; natural candidate for `#[inline(always)]` in Rust. Zero OHOS deps (`ssap_codec.h:1-10`). |
| 2 | **hwsle_transport** | `stack/ssap/include/hwsle_transport.h:31-49` — `open`/`send_acb`/`send_ssap`/`send_hci_cmd`/`run`/`close`, `ssap_recv_fn` | `stack/ssap/src/hwsle_transport.c:21-142` | `/dev/hwsle` ACB framing (`0xA3` + `tcid u16 LE` + `len u16 LE` + payload), HCI cmd framing (`0xA1`), `poll`/`read` loop dispatching `TCID_SLE_SMTC=0x0A` to `ssap_recv_fn`. | Only module that touches `open(2)`/`write(2)`/`poll(2)` — FFI/unsafe boundary (`stack/ssap/src/hwsle_transport.c:33`, `:60`, `:104`). 5+2 HAL (5 public + 2 datatypes). |
| 3 | **ssap_server** | `stack/ssap/include/ssap_server.h:94-125` — `init`/`add_service`/`add_property`/`add_method`/`dispatch`/`notify` + `apply_config` | `stack/ssap/src/ssap_server.c:12-581` | Service table (≤8 services, ≤32 props/service, `SSAP_MAX_VALUE_LEN 1024`), handle allocator from `0x0001`, dispatch for EXCHANGE_INFO/FIND_STRUCTURE/READ/WRITE/METHOD, auto-CCCD, SERVICE_CHANGE `0x000E` notify, `find_property`/`find_by_cccd`/`find_by_uuid` | Dispatch loop `ssap_server.c:162-547` is the second hot path; v1.0 vs v1.3 branching (`ssap_server.c:195`, `:236`, `:310`) must stay monomorphised. |
| 4 | **ssap_link** | `stack/ssap/include/ssap_link.h:93-108` — `init`/`connect`/`disconnect`/`on_event`/`tick`/`mark_activity`, `ssap_link_t` state | `stack/ssap/src/ssap_link.c:16-295` | DLI 0x1401/1402/1403/1802/1804 FSM (IDLE→CONNECTING→CONNECTED→DISCONNECTING), `find_cmd_echo` tolerance, stale-event gating, duplicate-peer rejection, supervision timeout, `0x1804` retry budget | Timeout arithmetic in `ssap_link_tick` (`ssap_link.c:250-295`) is `u32` monotonic `now_ms` — must remain branch-free in Rust. |
| 5 | **feature_mgr** | `stack/ssap/include/feature_mgr.h:23-77` — `fm_t`/`fm_conditions_t`/`fm_capacity_t`/`fm_init`/`fm_update`/`fm_has` | `stack/ssap/src/feature_mgr.c:16-121` | Heuristic bitfield (`FEAT_*` 0..9, `g_feat_cost[]` 0..16KB), capacity-profile trim, connection-state gating, peer-capability gating, RAM-pressure drop-order | Cheap heuristic (`feature_mgr.c:42-121`) — call `fm_update` per condition change, not per PDU. |
| — | **wire format** | `stack/ssap/include/ssap_pkt.h:24-588` — 20+ `SSAP_Pdu*` packed structs, `SSAP_MsgCode_E`, `SSAP_PduErrCode_E`, fragment/ctrl bitfields | (header-only) | Single source of truth for on-wire layout; Rust codecs must mirror LE packing exactly. | Packed structs → `#[repr(C, packed)]` + LE helpers; avoid `transmute`. |

**Dependence today (C):** `server → codec` (`ssap_server.c:8`), `link → transport` (`ssap_link.c:13`, `hwsle_transport_send_hci_cmd` at `ssap_link.c:69,88,101,176,240`), `server ← feature_mgr` via `ssap_server_apply_config` (`ssap_server.h:119-125`, `ssap_server.c:569-581`). `codec` has no upstream deps; `feature_mgr` has none. `transport` is leaf I/O.

**What that means for seams:** each header already defines a *small interface* (codec: ~10 fns, transport: 6 fns + 1 callback type, server: 7 fns + 4 callback types, link: 6 fns + 1 struct, feature_mgr: 2 fns + 2 structs + inline helpers). The Rust crates should keep that narrowness — not widen it.

---

## 2. Deep-module vocabulary (from `/setup-ts-deep-modules`)

From `~/.agents/skills/setup-ts-deep-modules/SKILL.md:12-34` and `dependency-cruiser.config.cjs:1-24`:

- **Deep module** = a lot of behaviour behind a small interface. The public surface is *only* the **entry points** — the files at the package root. Everything in subfolders is private.
- Conventional layout (`SKILL.md:17-22`):

  ```
  src/packages/<name>/
    index.ts        ← entry point (public). Import this from outside.
    client.ts       ← another entry point (several small entry points allowed)
    lib/            ← implementation: hidden from outside
    tests/          ← co-located tests (private, import only entry points)
  ```

  The rule itself is generic: *anything in any subfolder is private* (`SKILL.md:24-31`). Depth is decided by **path depth**, not by a hardcoded folder name.

- **Four forbidden rules** (`SKILL.md:26-31`, `dependency-cruiser.config.cjs:28-74`), all `error`:

  1. `entrypoint-boundary-from-app` — app/root code may import only entry points.
  2. `entrypoint-boundary-across-packages` — a package's own files import freely; other packages only via entry points (`$1` back-reference, `SKILL.md:98-99`).
  3. `tests-through-entrypoints` — tests import only entry points + own `tests/` fixtures, never internals (not even own).
  4. `no-circular` — no dependency cycles.

- **Principle, not tooling lock-in:** the config is `module.exports` CJS (`SKILL.md:102`, `dependency-cruiser.config.cjs:27`) and `PACKAGES_ROOT` is the only edit (`SKILL.md:55`, `dependency-cruiser.config.cjs:14`), patterns are extension-agnostic. The *principle* transfers to Rust verbatim even though `dependency-cruiser` is TS tooling: the compiler + lint enforce the same depth rule via different mechanisms (visibility + `cargo` graph).

For Rust we restate the **general constraints** (paraphrasing the skill, not quoting TS-specific paths):

> Each crate's public surface is its entry points at the crate root; any file under a subfolder is private. External crates and binaries import only entry points; a crate's own files import freely; tests exercise the crate through its entry points; no cycles.

---

## 3. Proposed crate decomposition — 1:1 seam alignment

Workspace location per `.scratch/rust-ws73-tri-mode/issues/04-rust-workspace-shape.md` — `rust-ws73/` at repo root, sibling to `stack/ssap/` and `sdk/`, with `.gitignore` whitelist `!*.toml !Cargo.* !rust-ws73/**` (issue `05-incremental-adoption.md:incremental-adoption`). Five host crates mirror the five C modules; a sixth `ws73` facade optionally re-exports for the app (not for inter-crate use).

```
rust-ws73/
  Cargo.toml              # [workspace] members = [ "crates/ssap-codec", ... ]
  crates/
    ssap-codec/           ← #1 codec  (pure, no_std-friendly)
    hwsle-transport/      ← #2 transport (std + FFI/unsafe island)
    ssap-server/          ← #3 server (dispatch)
    ssap-link/            ← #4 link FSM
    feature-mgr/          ← #5 feature policy
    ws73/                 ← (optional) facade / app entry
```

### 3.1 Crate-by-crate mandate

| Crate | C counterpart | Crate type | Allowed deps (enforced) | Forbidden deps | Why this cut |
|---|---|---|---|---|---|
| `ssap-codec` | `ssap_codec.h/.c` + `ssap_pkt.h` | `no_std` (with `alloc` only for tests) | *(none)* | `hwsle-transport`, `ssap-server`, `ssap-link`, `feature-mgr` | Pure function island; hottest PDU path; no I/O; first to go pure Rust (phase 1 in `05-incremental-adoption.md`). |
| `feature-mgr` | `feature_mgr.h/.c` | `no_std` | *(none)* | `ssap-server` (but `ssap-server` may depend on it — direction is one-way) | Bitfield heuristic; zero runtime deps today (`feature_mgr.c:14` includes only `feature_mgr.h`). Keeps it testable without hardware. |
| `ssap-server` | `ssap_server.h/.c` | `std` (or `no_std+alloc` if embedded reuse) | `ssap-codec`, `feature-mgr` (via `apply_config`) | `hwsle-transport`, `ssap-link` directly (only via injected `send_frame`) | Table + dispatch (`ssap_server.h:94-114`); depends on codec for encode helpers and on feature policy for version/MTU caps. Transport is injected, not linked. |
| `ssap-link` | `ssap_link.h/.c` | `std` (poll/time) | `hwsle-transport` (HCI cmd send) | `ssap-server`, `ssap-codec` (link never touches PDUs directly — only DLI opcodes) | DLI FSM (`ssap_link.h:20-33`, `ssap_link.c:137-295`); only crate that should know `DLI_CREATE_CONNECTION 0x1401` etc. |
| `hwsle-transport` | `hwsle_transport.h/.c` | `std` (fd/poll) | *(none)* | `ssap-server`, `ssap-link`, `ssap-codec`, `feature-mgr` | Leaf HAL over `/dev/hwsle` (`hwsle_transport.h:19`, `hwsle_transport.c:33-42`); owns the only `unsafe` `open`/`write`/`poll` island. |
| `ws73` (facade) | — | `std` | all five above (re-export only) | — | App-facing convenience so binaries write `use ws73::server::...` without knowing crate graph. Not used by the five between themselves. |

**Layering stub (for Cargo, mirrors the commented stub in `dependency-cruiser.config.cjs:76-86`):**

```toml
# rust-ws73/Cargo.toml (workspace) — layering intent, not just members:
# ssap-codec, feature-mgr : leaf (no workspace deps)
# ssap-server : ssap-codec + feature-mgr
# ssap-link   : hwsle-transport
# hwsle-transport : leaf
# ws73        : all (facade)
# No cycles — enforced by `cargo` (mutual deps = cycle) plus a CI gate (see §6).
```

This is the same *which-may-depend-on-which* concern that the TS config leaves as a commented stub (`SKILL.md:35-36`, `dependency-cruiser.config.cjs:76`). In Rust, `Cargo.toml` is the *strong* layering gate (a forbidden edge simply has no `dependencies` entry), and the `deep-module` rule is the *how-you-import* gate — the two are complementary.

---

## 4. `lib.rs` narrow export surface — the "small interface"

Each crate exposes **one primary entry point** `src/lib.rs` plus **at most one or two focused adjacents** at the crate root (mirroring `SKILL.md:18-20` "several small entry points"). Barrel files that re-export an entire subtree are discouraged (`SKILL.md:33-34`, `SKILL.md:90-91`).

### 4.1 Recommended root layout per crate

```
crates/ssap-codec/
  src/
    lib.rs              ← ONLY public surface (re-exports + doc comment)
    internal/           ← private impl (see §5)
      mod.rs
      exchange.rs
      find.rs
      read.rs
      write.rs
      value.rs
      trans.rs
  tests/
    codec_roundtrip.rs  ← imports ONLY crate entry point(s)

crates/hwsle-transport/
  src/
    lib.rs
    ffi.rs              ← tiny extra entry point (optional): raw HCI len/put helpers
    internal/
      acb.rs
      hci.rs
      poll.rs
  tests/

# same pattern for ssap-server / ssap-link / feature-mgr
```

### 4.2 What `lib.rs` should export (narrow, per crate)

**`ssap-codec/src/lib.rs`** — mirrors `stack/ssap/include/ssap_codec.h:19-41` constants + `:96-146` API, but typed:
```rust
// ssap-codec surface (example, not exhaustive):
pub use internal::trans::{trans_type_of, opcode_of}; // ssap_codec.h:143-146
pub fn encode_exchange_info(out: &mut [u8], opcode: u8, ctrl: u8, mtu: u16, version: u16) -> Option<usize>; // :99
pub fn decode_exchange_info(pdu: &[u8]) -> Option<(u16, u16)>; // :104
pub fn encode_find_struct_req(out: &mut [u8], find_type: u8, item_type: u8, rsp_mode: u8, start: u16, end: u16, uuid: Option<&[u8]>) -> Option<usize>; // :109
pub fn encode_read_req(out: &mut [u8], handles: &[u16], types: &[u8]) -> Option<usize>; // :116
pub fn encode_write(out: &mut [u8], opcode: u8, handle: u16, ty: u8, value: &[u8]) -> Option<usize>; // :121
pub fn encode_value(out: &mut [u8], opcode: u8, frag: u8, ty: u8, handle: u16, value: &[u8]) -> Option<usize>; // :126
pub fn encode_error_rsp(out: &mut [u8], req: u8, handle: u16, code: u8) -> Option<usize>; // :131
pub fn encode_write_rsp(out: &mut [u8], handle: u16, result: u8, err: u8) -> Option<usize>; // :136
pub fn encode_value_ack(out: &mut [u8], ty: u8, result: u8) -> Option<usize>; // :141
// constants: re-export SSAP_MSG_*, SSAP_CTRL_*, SSAP_VERSION_*, SSAP_OP_*, handle constants
```
Keep it ≤ ~15 items (the C header has 10 fns + 4 inline/helpers). Do **not** expose `put_u16`/`get_u16` (`ssap_codec.c:43-53`) — they stay in `internal/`.

**`hwsle-transport/src/lib.rs`** — mirrors `stack/ssap/include/hwsle_transport.h:31-49` 5+2:
```rust
pub const HWSLE_DEV: &str = "/dev/hwsle"; // :19
pub type RecvFn = dyn Fn(&[u8]) -> i32 + Send; // :31
pub fn open(recv: impl Fn(&[u8]) -> i32 + Send + 'static) -> io::Result<RawFd>; // :34
pub fn send_acb(tcid: u16, payload: &[u8]) -> io::Result<usize>; // :40
pub fn send_ssap(pdu: &[u8]) -> io::Result<usize>; // :37
pub fn send_hci_cmd(opcode: u16, params: &[u8]) -> io::Result<usize>; // :43
pub fn run() -> !; // :46
pub fn close(); // :49
// optional second entry point `ffi.rs`: `pub mod ffi { pub fn put_u16 ... }` for link reuse only
```
Poll loop (`hwsle_transport.c:98-142`) internals (`buf[2048]`, `pollfd`, frame parse) never leak.

**`ssap-server/src/lib.rs`** — mirrors `stack/ssap/include/ssap_server.h:94-125`:
```rust
pub use internal::types::{Server, Service, Property, SsapError, SsapItemType}; // :41-47, :25-38
pub fn init(server: &mut Server, send_frame: fn(&[u8]) -> i32); // :94
pub fn add_service(server: &mut Server, uuid16: u16, is_primary: bool) -> Option<u16>; // :97
pub fn add_property(server: &mut Server, svc: u16, uuid16: u16, ops: u32, perm: u8, rc: ReadCb, wc: WriteCb) -> Option<u16>; // :100
pub fn add_method(server: &mut Server, svc: u16, uuid16: u16, ops: u32, mc: MethodCb) -> Option<u16>; // :105
pub fn dispatch(server: &mut Server, pdu: &[u8]) -> i32; // :110
pub fn notify(server: &mut Server, handle: u16, value: &[u8], indicate: bool) -> i32; // :113
pub fn apply_config(server: &mut Server, cfg: &ServerConfig); // :125
// callbacks as `type ReadCb = fn(u16, &mut [u8]) -> Result<usize, SsapError>` etc. (:50-56)
```
Do not expose `find_property`/`find_by_cccd`/`find_by_uuid` (`ssap_server.c:122-160`) — they are `internal/dispatch.rs` private.

**`ssap-link/src/lib.rs`** — mirrors `stack/ssap/include/ssap_link.h:93-108`:
```rust
pub use internal::types::{Link, LinkState, ConnParam}; // :64-91
pub use internal::consts::{DLI_CREATE_CONNECTION, DLI_CONNECTION_COMPLETE_EVT, ...}; // :20-37
pub fn init(link: &mut Link); // :93
pub fn connect(link: &mut Link, peer: &[u8; 6], param: Option<&ConnParam>) -> i32; // :96
pub fn disconnect(link: &mut Link) -> i32; // :100
pub fn on_event(link: &mut Link, opcode: u16, data: &[u8]); // :102
pub fn tick(link: &mut Link, now_ms: u32); // :105
pub fn mark_activity(link: &mut Link, now_ms: u32); // :108
```
State fields (`ssap_link.h:72-91` — `timeout_bucket`, `first_tick_ms`, `dlen_retries`) stay private; callbacks `on_connected`/`on_disconnected`/`on_connect_failed` are setter fns, not struct field exposure.

**`feature-mgr/src/lib.rs`** — mirrors `stack/ssap/include/feature_mgr.h:23-74`:
```rust
pub use internal::bits::{FEAT_SSAP_V1_0, FEAT_RANGING, ...}; // :24-35
pub use internal::types::{Capacity, Conditions, UseCase, Fm}; // :38-60
pub fn init(fm: &mut Fm, cap: Capacity, wanted: u32); // :71
pub fn update(fm: &mut Fm, cond: &Conditions) -> u32; // :73
pub fn has(fm: &Fm, bit: u32) -> bool; // :76
pub fn capacity_budget(cap: Capacity) -> u32; // :77
```
`g_feat_cost[]` (`feature_mgr.c:17-28`) and `drop_order[]` (`feature_mgr.c:86-90`) never escape.

**Facade `ws73/src/lib.rs`** (optional) — convenience only, not a barrel:
```rust
pub use ssap_codec as codec;
pub use feature_mgr as fm;
// re-export only the *entry* symbols for the app, not internal/:
pub use ssap_server::{Server, dispatch, notify};
pub use ssap_link::{Link, connect, disconnect};
pub use hwsle_transport::{open, send_ssap, run};
```

---

## 5. `src/internal/` hiding — depth behind the small interface

**Rule translation:** TS `src/packages/<name>/lib/` and `tests/` (`SKILL.md:17-22`, `dependency-cruiser.config.cjs:24` pattern `^R/[^/]+/[^/]+/`) becomes Rust `crates/<name>/src/internal/` (and `tests/` at crate root). Anything under `internal/` is **private to the crate** — enforced by Rust visibility, not by a path rule.

Concrete structure (deep, but not contrived):

```
crates/ssap-codec/src/internal/
  mod.rs              // `pub(crate)` re-exports inside the crate only
  wire.rs             // SSAP_Pdu* packed layout mirrors (ssap_pkt.h:180-588), put_u16/get_u16
  exchange.rs         // encode/decode EXCHANGE_INFO (ssap_codec.c:55-94)
  find.rs             // FIND_STRUCTURE (+ BY_UUID) encode
  read.rs             // READ_REQ/RSP
  write.rs            // WRITE_CMD/REQ/RSP + CCCD edge
  value.rs            // VALUE_NTF/IND/ACK
  trans.rs            // trans_type_of / opcode helpers (ssap_codec.c:11-41)

crates/ssap-server/src/internal/
  mod.rs
  table.rs            // Server/Service/Property table, handle alloc, SERVICE_CHANGE (ssap_server.c:21-89)
  dispatch/
    mod.rs
    exchange.rs       // EXCHANGE_INFO branch (ssap_server.c:172-187)
    find.rs           // FIND branches + v1.0/v1.3 compat (ssap_server.c:188-334)
    read.rs           // READ + READ_BY_UUID (ssap_server.c:335-434)
    write.rs          // WRITE multi/single + CCCD (ssap_server.c:435-510)
    method.rs         // CALL_METHOD (ssap_server.c:511-547)
  cccd.rs             // find_by_cccd, notify gating (ssap_server.c:134-145, 549-567)
  config.rs           // apply_config version/MTU cap (ssap_server.c:569-581)

crates/ssap-link/src/internal/
  mod.rs
  fsm.rs              // state enum, init/connect/disconnect (ssap_link.c:16-111)
  events.rs           // on_event + find_cmd_echo tolerance (ssap_link.c:122-248)
  tick.rs             // tick + supervision (ssap_link.c:250-295)

crates/feature-mgr/src/internal/
  mod.rs
  costs.rs            // g_feat_cost, bit_of (feature_mgr.c:16-31)
  policy.rs           // capacity trim, connection gating, RAM-pressure drop (feature_mgr.c:42-121)
```

Visibility discipline:

- `internal/` files declare `pub(crate)` at most; nothing is `pub` from inside `internal/`. Only `src/lib.rs` (and optionally `src/ffi.rs`) re-export a curated `pub` set.
- Shared LE helpers (`put_u16` duplication across `ssap_codec.c:43`, `hwsle_transport.c:24`, `ssap_link.c:23`) live **once** in `ssap-codec/internal/wire.rs` and are `pub(crate)` only to that crate. Other crates use their own internal copy or, if the team prefers DRY, a tiny `ws73-utils` leaf crate — but that leaf must also be deep (own `internal/`), not a grab-bag.
- `tests/` at crate root (Cargo convention) imports only `crate::` entry points. Never `crate::internal::*` — the test proves the entry-point boundary bites (mirrors `SKILL.md:80-86` pass/fail/revert proof).

**Why `internal/` not `lib/`:** Rust reserves `lib.rs` as the entry point; `lib/` would be confusing. `internal/` is unambiguous and grep-friendly (`rg 'internal'` shows only depth). The principle is identical to `SKILL.md:24-25` — "anything in any subfolder is private."

---

## 6. How `/setup-ts-deep-modules` wiring would look for Rust

`/setup-ts-deep-modules` is TS tooling (`SKILL.md:48-56` installs `dependency-cruiser` as a devDependency, `SKILL.md:53-57` writes `.dependency-cruiser.cjs`, `SKILL.md:59-65` wires `lint:boundaries`). The **principle transfers** to Rust; the *mechanism* changes.

### 6.1 The TS wiring (for reference)

1. Detect package manager + `PACKAGES_ROOT` (`SKILL.md:42-44`).
2. Install `dependency-cruiser` (`SKILL.md:48`).
3. Copy `dependency-cruiser.config.cjs` (`SKILL.md:55`, with `PACKAGES_ROOT = "src/packages"` at `:15`, derived `PACKAGE_INTERNALS = ^R/[^/]+/[^/]+/` at `:24`, 4 forbidden rules at `:28-74` plus commented layering stub at `:76-86`).
4. Wire `lint:boundaries` = `depcruise <packages-root>` and fold into the umbrella check (`SKILL.md:59-65`).
5. Scaffold `example` package with `index.ts → lib/impl.ts` and `tests/example.test.ts` that imports only `../index` (`SKILL.md:68-75`).
6. Prove it bites: pass → add `import from "../lib/impl"` → fail `tests-through-entrypoints` → revert → pass (`SKILL.md:80-86`).
7. Document in `<packages-root>/README.md` + one-line pointer in `CLAUDE.md`/`AGENTS.md` (`SKILL.md:89-95`).

### 6.2 Rust translation

| TS concept | Rust equivalent | Where it lives |
|---|---|---|
| `PACKAGES_ROOT = "src/packages"` (`dependency-cruiser.config.cjs:15`) | `crates/` (Cargo workspace root) | `rust-ws73/Cargo.toml: [workspace] members = ["crates/*"]` |
| `PACKAGE_INTERNALS = ^R/[^/]+/[^/]+/` (`:24`) — anything in a subfolder is private | `src/internal/**` is private — enforced by **visibility**, not path regex | `crates/<name>/src/lib.rs` only `pub use internal::...` the narrow set; `internal/*.rs` items are `pub(crate)` |
| `entrypoint-boundary-from-app` / `across-packages` (`:28-48`) | `Cargo.toml` dependency graph + `pub` discipline | A crate can only `use` what another crate `pub`-exports; if `ssap-link` has no `hwsle-transport` in its `[dependencies]`, it **cannot** import it — stronger than a lint. |
| `tests-through-entrypoints` (`:50-59`) | `tests/*.rs` (integration tests) + `#[cfg(test)]` unit tests inside `lib.rs` only | Integration tests in `crates/<name>/tests/` run as separate crates — they can only `use ssap_codec::...` (the entry point). Never `use ssap_codec::internal::...` (not `pub`). |
| `no-circular` (`:69-74`) | `cargo` rejects cycles natively + CI `cargo tree --edges normal` gate | `cargo metadata --format-version 1 | jq .resolve` or `cargo deny` / `cargo-deny` cycle check |
| `lint:boundaries` (`SKILL.md:61`) | `cargo xtask boundaries` or `scripts/check-rust-boundaries.sh` | Script that (a) greps for forbidden `use ...::internal` across crates and (b) asserts `cargo tree` has no cycles, (c) proves the rule bites (see §6.3) |
| Example scaffold (`SKILL.md:68-75`) | `crates/example/` crate | `src/lib.rs` exports `pub fn hello() -> &'static str` delegating to `src/internal/impl.rs`; `tests/example_test.rs` asserts via `use example::hello` |

### 6.3 Concrete Rust wiring steps (to mirror the skill's "prove it bites" completion criterion)

The skill's completion criterion is `SKILL.md:80-86` — pass, inject deep import, observe fail, revert, observe pass. For Rust:

```bash
# 1. Ensure workspace exists (per issues/04 + 05):
#    rust-ws73/Cargo.toml  +  .gitignore  !*.toml !Cargo.* !rust-ws73/**

# 2. Visibility lint — no crate reaches into another's internal/:
#    scripts/check-rust-boundaries.sh  (new, ~30 lines)
rg -n 'use\s+\w+::internal' rust-ws73/crates --glob '!**/src/internal/**' && exit 1

# 3. Cycle gate:
cargo -C rust-ws73 metadata --format-version=1 --no-deps | jq -e '.resolve.nodes | length' >/dev/null
cargo -C rust-ws73 tree --edges normal --depth 99 | rg -q 'cycle' && exit 1  # or cargo deny check bans

# 4. Fold into umbrella check (mirrors SKILL.md:61-64):
#    In package.json: "check": "tsc --noEmit && depcruise src && cargo -C rust-ws73 check --all-targets && ./scripts/check-rust-boundaries.sh"
#    In Makefile: check: typecheck lint:boundaries rust-check

# 5. Scaffold example crate:
cargo new --lib rust-ws73/crates/example
#   crates/example/src/lib.rs       → pub fn hello() -> &'static str { crate::internal::greet() }
#   crates/example/src/internal/mod.rs → pub(crate) fn greet() -> &'static str { "hello" }
#   crates/example/tests/example_test.rs → use example::hello; assert_eq!(hello(), "hello")
```

**Proof run (mirrors SKILL.md:81-85):**
```bash
cargo -C rust-ws73 test -p example                          # PASS — tests via entry point
echo 'use example::internal::greet;' >> rust-ws73/crates/example/tests/example_test.rs
cargo -C rust-ws73 test -p example 2>&1 | grep -q "unresolved import.*internal" && echo "FAIL (expected)"  # FAIL — gate bites
git checkout -- rust-ws73/crates/example/tests/example_test.rs
cargo -C rust-ws73 test -p example                           # PASS again
# Rust's compiler is the deep-import gate here — stronger than dependency-cruiser's post-hoc lint.
```

Add the **context pointer** from `AGENTS.md` (`SKILL.md:89-95`):
```
// at bottom of /AGENTS.md:
Rust crates are deep modules — see [rust-ws73/crates/README.md](./rust-ws73/crates/README.md) before adding or importing one.
```

And a **crates README** (`rust-ws73/crates/README.md`, mirrors `SKILL.md:89-91`):

```md
# rust-ws73 crates — deep modules

Each crate is a deep module: public surface is `src/lib.rs` (and at most one extra root file like `src/ffi.rs`);
everything in `src/internal/` is private (`pub(crate)` only). External crates and `tests/` import only entry points.
Discourage barrel files — expose several small root files instead of re-exporting a whole subtree.
Run `cargo -C rust-ws73 check --all-targets && ./scripts/check-rust-boundaries.sh`.
```

---

## 7. Keeping the interface small under extreme-perf (`03-lto-extreme-perf-decision.md`)

The effort's destination is *one dongle tri-mode at full speed* with full LTO (`map.md:destination`, `issues/03-lto-extreme-perf-decision.md`). Deep modules must not fight that.

- **No visibility cost.** `pub(crate)` vs `pub` is a compile-time check; it does not inhibit `lto="fat"` / `codegen-units=1` cross-crate inlining when `Cargo.toml` sets `lto = true` on the final binary crate. The internal functions are still `#[inline]`-eligible — they just aren't *name-reachable* from outside.
- **Small interface IS the perf strategy.** Codec hot loop (single `memcpy` + 2-byte LE) wants `#[inline(always)]` on `put_u16`/`encode_*` — but those are inside the crate, not part of the entry-point cost. The linker sees the same object code whether the caller wrote `use ssap_codec::encode_write` or `use ssap_codec::internal::write::encode` — the difference is only API surface.
- **Workspace LTO alignment:** `stack/ssap/Makefile:9` builds `libssap.a` with `-O2 -Wall -Wextra -std=c11` (no LTO yet). Rust side will use `[profile.release] lto="fat" codegen-units=1 panic="abort" strip=true` (`03-lto-extreme-perf-decision.md`). Because Rust crates are `rlib` intermediates, thin-LTO across crates already works before `fat`; deep modules don't block it.
- **Feature-gated version branching:** `ssap_server.c:195` (`is_v10`) and `feature_mgr.c:78-79` (peer-version gating `0x0301`) become `#[cfg(feature="v1_3")]` or runtime `if` on `FeatureMgr::has` — keep the branch in `internal/`, not the entry-point type, so callers don't pay the complexity.
- **No-std leaf purity:** `ssap-codec` and `feature-mgr` stay `no_std` (`ssap_codec.h:15-16` has no OS includes; `feature_mgr.h` is bits only) so they can be inlined into either the host binary or a future `riscv32imc` firmware crate without pulling `std`.
- **FFI seam stays narrow:** `hwsle-transport` is the *only* crate with `unsafe` (`open`/`write`/`poll` at `hwsle_transport.c:33,60,104`). Keeping it as a leaf crate means `cargo audit` / Miri focus on one crate, and LTO can still devirtualise the `ssap_recv_fn` callback when the final binary links `ssap-server` + `hwsle-transport` together.

---

## 8. Deep-module analogy: `fbb_ws63/.../riscv31` (prompt path)

The prompt cites `fbb_ws63/src/drivers/chips/ws63/arch/riscv/riscv31/*` as the deep-module analogue. **In this checkout that path does not exist** (`fbb_ws63/` absent; SDK subtree is `sdk/ws73_sdk_linux_WS73_1.10.110/` with `application/`, `driver/`, `firmware/`, `include/`). The closest analogue in-repo is the driver layering under `sdk/ws73_sdk_linux_WS73_1.10.110/driver/` (chips → ws63/bsle/wifi) where each chip driver hides arch-specific `riscv/` internals behind a small `include/` entry. The principle is identical: arch glue (`riscv31` trap vectors, `mstatus` CSR helpers) lives in `arch/riscv/` subfolders and is never `#include`'d directly by board code — only through `chip.h`/`soc.h` entry headers. Rust mirrors that by hiding `internal/arch/` under each crate's `lib.rs`.

If the analogy tree is elsewhere (e.g., `open_source/kernel` or an external `fbb_ws63` repo), the mapping still holds: **one public header per driver → many private `arch/` files = one `lib.rs` per crate → many private `internal/` files**.

---

## 9. Risks, open questions, and next steps

| Risk | Why it matters | Mitigation |
|---|---|---|
| **Over-splitting** — 5 crates from day one slows `cargo check` and complicates `05-incremental-adoption.md` phase discipline | Phase 0 is just `Cargo.toml` + `.gitignore` whitelist fix; phase 1 is `ssap-codec` only | Start with 2 crates (`ssap-codec`, `feature-mgr` — both `no_std` leaves) and add `hwsle-transport` → `ssap-link` → `ssap-server` in dependency order. The deep-module rule applies from crate 1. |
| **Codec duplication** — `SSAP_Pdu*` packed layout needed in both C (`ssap_pkt.h:186-588`) and Rust | LE/packing mismatch = silent wire corruption | Generate Rust `#[repr(C, packed)]` from `ssap_pkt.h` once and freeze; round-trip tests `test/test_codec.c:1` ↔ `crates/ssap-codec/tests/*` FFI parity (phase 1 "FFI 对拍"). |
| **Callback ownership** — C uses raw function pointers (`ssap_server.h:50-56` `read_cb`/`write_cb`/`method_cb`, `hwsle_transport.h:31` `ssap_recv_fn`) | Rust closures vs `fn` pointer affects `Send`/`'static` | Entry point takes `fn` pointers first (drop-in FFI), then offer a second `src/callback.rs` entry point with `Box<dyn Fn...>` once pure Rust. |
| **Halving the HAL** — `hwsle_transport` today has 3 TCID constants (`hwsle_transport.h:26-28`) but only `TCID_SLE_SMTC 0x0A` is wired (`hwsle_transport.c:125`) | Future dyn TCID (`FEAT_DYN_TCID 1u<<6` in `feature_mgr.h:32`) wants `send_acb(tcid)` | Keep `send_acb` public but gate new TCIDs behind `feature-mgr` policy, not transport's own logic. |
| **Test co-location** | TS skill hides tests in `tests/` subfolder (`dependency-cruiser.config.cjs:42-44` path `^R/[^/]+/tests/`); Rust's `tests/` is integration-test by convention but unit tests live in `src/*.rs` | Enforce *both*: `#[cfg(test)] mod tests` inside `lib.rs` may only call `crate::` entry points in the assertion phase; integration `tests/*.rs` can only `use crate::` entry. Script greps for `::internal::` in either. |

**Recommended immediate next step (does not touch files — research ticket):**

1. Resolve `04-rust-workspace-shape.md` via `/grilling` — confirm `rust-ws73/` sibling placement and 5-vs-1 crate split (candidate A vs B vs C).
2. Then `05-incremental-adoption.md` — land `ssap-codec` as the first deep crate with FFI parity tests, proving both the codec seam and the deep-module gate.
3. Only then scaffold the remaining four crates with the `check-rust-boundaries.sh` gate and `crates/README.md` pointer from `AGENTS.md`.

---

*File: `.scratch/nearlink-driver/lab-notes/RUST-WS73-DEEP-MODULES.md` — English, read-only synthesis. All code citations verified against the paths above at the stated lines as of 2026-08-19.*
