# Rust Extreme-Perf Patterns for rust-ws73 — Lab Note

- **Ticket:** gap-fill for `.scratch/rust-ws73-tri-mode/issues/03-lto-extreme-perf-decision.md:1` + `08-deep-module-boundaries.md:1` + `09-unsafe-ffi-audit.md:1` + `06-perf-baseline-extreme.md:1` (research, AFK, read-only)
- **Date:** 2026-08-19
- **Strict scope:** read-only, no network/build/hardware. Exactly one file at `.scratch/nearlink-driver/lab-notes/PERF-RUST-PATTERNS.md` (English, 200+ lines, `file:line` cited). No other files touched.
- **Question:** Rust extreme-perf patterns for `rust-ws73` — zero-copy codec (`from_le_bytes`, `&[u8]` borrows, no packed transmute), `#[inline(always)]` hot paths, `forbid(unsafe_code)` vs transport unsafe isolation, Thin→Fat LTO + `codegen-units=1` + `panic=abort` + `strip`, `no_std` alloc, miri coverage. `200+` lines.

> All paths below are absolute under `/home/archivalera/plum/zcode-projects/nearlink` and cited `file:line`.

---

## 0. Sources inspected

| Source | Path | What it tells us |
|---|---|---|
| codec impl | `stack/ssap/src/ssap_codec.c:1-218` | `put_u16:43-48` / `get_u16:50-53` LE helpers, `memcpy:112,148,166,360,384`, bounds-checked encode `61-217` |
| transport impl | `stack/ssap/src/hwsle_transport.c:1-143` | `g_fd:21`/`g_recv_cb:22`, `open:33`/`write:60,65,87,91`/`poll:104-105`/`read:113`, `buf[2048]:102`, split `write` header+payload |
| server impl | `stack/ssap/src/ssap_server.c:1-582` | `dispatch:162-547` hot switch, `rsp[512]:168`, `find_property:122-132`, `notify:549-567`, `apply_config:569-581` |
| link impl | `stack/ssap/src/ssap_link.c:1-296` | `put_u16:23-28`, `on_event:137-248`, `tick:250-295`, `wrapping_sub` timeout `259,271,282` |
| feature-mgr impl | `stack/ssap/src/feature_mgr.c:1-121` | `g_feat_cost[]:17-28`, `drop_order[]:86-90`, pure `fm_update:42-121` |
| codec header | `stack/ssap/include/ssap_codec.h:1-149` | 10 encode/decode fns `:99-142`, `SSAP_MTU_MAX 1024:61`, no globals |
| transport header | `stack/ssap/include/hwsle_transport.h:1-51` | 5+2 HAL seam (`open/send_acb/send_ssap/send_hci_cmd/run/close` + `ssap_recv_fn:31`) |
| server header | `stack/ssap/include/ssap_server.h:1-126` | `services[8]:83`, `property_count:77`, `send_frame:91`, `SSAP_MAX_VALUE_LEN 1024:21` |
| link header | `stack/ssap/include/ssap_link.h:1-110` | `ssap_link_t:72-91`, `timeout_bucket:84`, `DLI_* 0x14xx/0x18xx:21-26`, `SSAP_LINK_CMD_TIMEOUT_MS 5000:35` |
| feature-mgr header | `stack/ssap/include/feature_mgr.h:1-88` | `FEAT_* bits:24-35`, `Capacity:38-43`, `Conditions:46-52` |
| wire format | `stack/ssap/include/ssap_pkt.h:1-593` | 20+ `__attribute__((packed))` PDU structs `:180-588`, `SSAP_PduErrRsp_S:180-186`, `SSAP_PduValueItem_S:520-524` |
| build | `stack/ssap/Makefile:1-44` | 5-file `SRCS:12`, `CFLAGS -O2 -Wall -Wextra -std=c11:9-10`, `ar rcs:17-18`, no LTO |
| unsafe audit | `.scratch/nearlink-driver/lab-notes/RUST-WS73-UNSAFE-FFI.md:1-282` | Only `hwsle-transport` gets `unsafe`, `forbid(unsafe_code)` elsewhere, `rg` fence `:199-203`, miri pilot `:133` |
| deep modules | `.scratch/nearlink-driver/lab-notes/RUST-WS73-DEEP-MODULES.md:1-421` | 5-crate mirror, `src/internal/` private `:248-294`, `Cargo.toml` layering gate `:114-124`, `check-rust-boundaries.sh:335` |
| LTO extreme | `.scratch/nearlink-driver/lab-notes/RUST-WS73-LTO-EXTREME.md:1-443` | Host C `-flto --gc-sections -O2:195-200`, Rust `[profile.release] lto thin->fat codegen-units=1 panic=abort strip:224-231`, PGO rejection `:307-333` |
| perf baseline | `.scratch/nearlink-driver/lab-notes/RUST-WS73-PERF-BASELINE.md:1-558` | `libssap.a` text `10421` SysV / `12170` via `size:130-136`, `ssap_codec_test` text `4765:137`, `wifi_soc.ko` text `966210:64`, `memory_config.h` budget `:193-209` |
| repo conventions | `.scratch/nearlink-driver/lab-notes/RUST-WS73-REPO-CONVENTIONS.md:1-476` | `.gitignore:56 !*.rs` vs missing `!*.toml/!Cargo.*:101-117`, `check-docs.sh` 6/6 gate `:150-162` |
| workspace shape | `.scratch/rust-ws73-tri-mode/issues/04-rust-workspace-shape.md:1-8` | `rust-ws73/` at repo root, sibling to `stack/ssap/` and `sdk/` |
| tri-mode map | `.scratch/rust-ws73-tri-mode/map.md:1` | Destination: one dongle tri-mode at full speed |

---

## 1. Zero-copy codec — `from_le_bytes`, `&[u8]` borrows, no packed transmute

### 1.1 What C does today (safe manual LE, no transmute)

`stack/ssap/src/ssap_codec.c:43-53` defines the two LE primitives that every PDU touches:

```c
static size_t put_u16(uint8_t *p, uint16_t v) { p[0]=(uint8_t)(v & 0xFF); p[1]=(uint8_t)(v >> 8); return 2; } // ssap_codec.c:43-48
static uint16_t get_u16(const uint8_t *p) { return (uint16_t)(p[0] | ((uint16_t)p[1] << 8)); }               // ssap_codec.c:50-53
```

These are duplicated in `stack/ssap/src/hwsle_transport.c:24-29` and `stack/ssap/src/ssap_link.c:23-28` — today three copies, all `p[0]/p[1]` shifts, no `memcpy` through a packed struct. The payload tail is `memcpy(out+len,uuid,uuid_len)` at `stack/ssap/src/ssap_codec.c:112` (find), `:148` (write), `:166` (value) and `memcpy(rsp+n,value,vlen)` at `stack/ssap/src/ssap_server.c:360,384,430-431,506` — always behind an `out_sz` / `sizeof(rsp)` guard (`ssap_codec.c:61-73` `out_sz<6`, `:102-103` `out_sz<7`, `:122-124` `need=2+count*3`, `:139-141` `need=2+2+1+value_len`, `:157-159` `need=2+2+2+value_len`; `ssap_server.c:168` `rsp[512]` with `n+vlen<=sizeof(rsp):359`).

`stack/ssap/include/ssap_pkt.h:180-588` is authoritative wire truth (e.g., `SSAP_PduErrRsp_S:180-186`, `SSAP_PduExchangePkt_S:223-236`, `SSAP_PduFindStructReq_S:248-259`, `SSAP_PduValue_S:507-518` with `uuid[0]` tails). The codec **never** casts `uint8_t *` to `SSAP_Pdu*` — it stays byte-wise. A `transmute` or `&*ptr.cast::<Pdu>()` would introduce unaligned UB where none exists today (`RUST-WS73-UNSAFE-FFI.md:49,85-86`).

### 1.2 Rust zero-copy shape

```rust
// stack/ssap/include/ssap_codec.h:99,104,109,116,121,126,131,136,141 -> Rust:
pub fn encode_exchange_info(out: &mut [u8], opcode: u8, ctrl: u8, mtu: u16, version: u16) -> Option<usize>; // ssap_codec.h:99
pub fn decode_exchange_info(pdu: &[u8]) -> Option<(u16,u16)>;                                               // ssap_codec.h:104
pub fn encode_find_struct_req(out: &mut [u8], find_type: u8, item_type: u8, rsp_mode: u8, start: u16, end: u16, uuid: Option<&[u8]>) -> Option<usize>; // ssap_codec.h:109
pub fn encode_read_req(out: &mut [u8], handles: &[u16], types: &[u8]) -> Option<usize>;                    // ssap_codec.h:116
pub fn encode_write(out: &mut [u8], opcode: u8, handle: u16, ty: u8, value: &[u8]) -> Option<usize>;        // ssap_codec.h:121
pub fn encode_value(out: &mut [u8], opcode: u8, frag: u8, ty: u8, handle: u16, value: &[u8]) -> Option<usize>; // ssap_codec.h:126
```

Rules:

- **Borrow, don't copy.** Input is `&[u8]`, output is `&mut [u8]`; `value_len` is `value.len()` — no separate `uint16_t value_len` that can disagree with the slice (`RUST-WS73-UNSAFE-FFI.md:129-130`). `Option<usize>` replaces C's `0/-1` sentinel (`ssap_codec.c:62,94,103,124,141,159,176,193,212`).
- **`from_le_bytes` is the LE primitive.** Keep `ssap_codec.c:50-53` as safe:
  ```rust
  #[inline(always)] fn get_u16(p: &[u8]) -> u16 { u16::from_le_bytes([p[0], p[1]]) } // mirrors ssap_codec.c:50-53
  #[inline(always)] fn put_u16(out: &mut [u8], v: u16) { out[0..2].copy_from_slice(&v.to_le_bytes()); } // mirrors ssap_codec.c:43-48
  ```
  with `debug_assert!(p.len()>=2)` / `out.len()>=2` before indexing. Do not use `ptr::copy_nonoverlapping` or `transmute` (`RUST-WS73-UNSAFE-FFI.md:130-131`).
- **`copy_from_slice` for the tail.** `memcpy(out+len,uuid,uuid_len)` at `ssap_codec.c:112,148,166` becomes `out[off..off+n].copy_from_slice(uuid)` — safe, bounds-checked, zero-copy in the sense of no intermediate allocation; the caller owns `out` (`RUST-WS73-UNSAFE-FFI.md:50,130`).
- **No `repr(packed)` in the codec crate.** `ssap_pkt.h:180-588` stays header-only ground truth for layout tests; Rust codec never takes `&SSAP_Pdu*`. If the server ever needs struct mirrors, use `#[repr(C, packed)]` + `unsafe { ptr::addr_of!((*ptr).field).read_unaligned() }` behind miri, but the preferred path is keep the manual `p[0]|p[1]<<8` codec (`RUST-WS73-UNSAFE-FFI.md:85-86,243`).
- **Truncation fenced in transport, not codec.** The two `(uint16_t)len` truncation points at `stack/ssap/src/hwsle_transport.c:59,86` are avoided in `ssap-codec` because it never converts `usize->u16`. That check belongs in `hwsle-transport` as `u16::try_from(payload.len()).map_err(|_|TooLarge)?` against `SSAP_MAX_VALUE_LEN 1024` at `stack/ssap/include/ssap_server.h:21` / `stack/ssap/include/ssap_codec.h:61` (`RUST-WS73-UNSAFE-FFI.md:131,146`).

Test vectors are `stack/ssap/test/test_codec.c:23-104` (e.g., `0x02 0x03 0xFB00 0x0103` for mtu 251) and `stack/ssap/test/test_server.c:1-290` — future `ssap-codec` integration tests replay them via the safe entry point only (`RUST-WS73-DEEP-MODULES.md:289-294`).

---

## 2. `#[inline(always)]` hot paths — where to pay and where not to

### 2.1 Measured hot set (from `RUST-WS73-PERF-BASELINE.md` + `RUST-WS73-LTO-EXTREME.md`)

| Function | C location | Size hint | Perf role |
|---|---|---|---|
| `put_u16` / `get_u16` | `stack/ssap/src/ssap_codec.c:43-53` (duplicated at `hwsle_transport.c:24-29`, `ssap_link.c:23-28`) | 2-byte LE store/load, 2 insns | Per-PDU, per-field — inlined everywhere |
| `ssap_trans_type_of` | `stack/ssap/src/ssap_codec.c:11-41` | branch on `opcode & 0x7F` | Hot dispatch classifier |
| `ssap_encode_value` | `stack/ssap/src/ssap_codec.c:153-169` | `need` pre-check + `put_u16` x2 + `memcpy` | `ssap_server_notify:549-567` critical path |
| `ssap_encode_write` | `stack/ssap/src/ssap_codec.c:135-151` | `need` pre-check + `put_u16` + `memcpy` | WRITE CMD/REQ per `ssap_server.c:455-462` |
| `ssap_server dispatch` | `stack/ssap/src/ssap_server.c:162-547` (`.text 6145` at `ssap_server.o` per `RUST-WS73-PERF-BASELINE.md:133`) | `switch(opcode)` with 8 cases | Second hot loop, must stay monomorphised |
| `hwsle_transport_send_acb` header build | `stack/ssap/src/hwsle_transport.c:51-71` | `[0xA3][tcid][len][payload]` + 2x `write` | 12 Mbps line-rate bottleneck `:51-76` |
| `ssap_link_tick` / `on_event` | `stack/ssap/src/ssap_link.c:137-295` | `u32` wrap arithmetic `259,271,282` | Timer hot, branch-free desired |

`RUST-WS73-DEEP-MODULES.md:40` already flags the codec `memcpy`/`put_u16` as the natural `#[inline(always)]` candidate; `RUST-WS73-PERF-BASELINE.md:296-298` calls `put_u16:24-29` + `memcpy:56-60` the `<2%` branchy region where LTO win concentrates.

### 2.2 Policy

- **`put_u16` / `get_u16` / `opcode_of` / `trans_type_of` — `#[inline(always)]`.** They are `2..10` insns, called per field, and are pure (`stack/ssap/include/ssap_codec.h:143-146` `ssap_opcode_of` is already `static inline`). Mark them `#[inline(always)]` in `crates/ssap-codec/src/internal/wire.rs:1` (`RUST-WS73-DEEP-MODULES.md:254-262` layout). This is the one place where `always` is justified — the linker cannot out-guess the call-site count.
- **`encode_*` helpers — `#[inline]` (not `always`).** `ssap_codec.c:55-94` `encode_exchange_info`, `:96-217` `find/read/write/value` are `~20..60` insns plus a `memcpy` tail. Use `#[inline]` and let `lto="thin"` / `codegen-units=1` decide cross-crate inlining. Over-annotating `always` on 10 `encode_*` fns bloats the `ssap_server.o` `.text 6145` path (`RUST-WS73-PERF-BASELINE.md:133`) and defeats ThinLTO's size heuristic (`RUST-WS73-LTO-EXTREME.md:235` thin gives ~80% of fat win at lower link cost).
- **Dispatch branches — no forced inline.** `ssap_server.c:172-543` has 8 `case` groups each with `len < N` guards (`:191,286,337` etc.). Keep each branch in `crates/ssap-server/src/internal/dispatch/*.rs:1` (`RUST-WS73-DEEP-MODULES.md:267-274`) and rely on `PGO`'s block layout — except PGO is rejected (`RUST-WS73-LTO-EXTREME.md:307-333` four blockers: no representative trace, KO profiling blocked `:314`, Rust PGO CI-heavy `:316`, ROM PGO halved `:318`). So the layout win must come from `codegen-units=1` + `lto` alone (see §4).
- **Deep modules do not block inlining.** `pub(crate)` vs `pub` is a compile-time visibility check; `RUST-WS73-DEEP-MODULES.md:384-386` states it does not inhibit `lto="fat"` / `codegen-units=1` cross-crate inlining when `Cargo.toml` sets `lto` on the final binary. Internal fns are still `#[inline]`-eligible — they just aren't name-reachable from outside.

Concrete crate placement:

```
crates/ssap-codec/src/internal/wire.rs:1  — #[inline(always)] put_u16/get_u16 (ssap_codec.c:43-53)
crates/ssap-codec/src/internal/*.rs:1     — #[inline] encode_* (ssap_codec.c:55-217)
crates/hwsle-transport/src/internal/acb.rs:1 — #[inline] acb header build (hwsle_transport.c:51-71)
crates/ssap-link/src/internal/tick.rs:1  — branch-free u32::wrapping_sub (ssap_link.c:250-295)
```

---

## 3. `forbid(unsafe_code)` vs transport unsafe isolation

### 3.1 Crate boundary — only `hwsle-transport` gets `unsafe`

Per `RUST-WS73-UNSAFE-FFI.md:91-103` and `RUST-WS73-DEEP-MODULES.md:105-113`:

| Crate | C counterpart | Needs `unsafe`? | Why |
|---|---|---|---|
| `ssap-codec` | `ssap_codec.h:19-149` + `ssap_codec.c:11-218` | **No** — `#![forbid(unsafe_code)]` | Pure `&[u8]<->&mut [u8]`, safe shifts, `usize` bounds |
| `feature-mgr` | `feature_mgr.h:23-88` + `feature_mgr.c:16-121` | **No** — `#![forbid(unsafe_code)]` | Arithmetic on `u32` bitflags, no I/O |
| `ssap-server` | `ssap_server.h:16-126` + `ssap_server.c:12-582` | **No** — `#![forbid(unsafe_code)]` | Table + dispatch over `&mut Server` + `&[u8]`, `send_frame` is injected `fn(&[u8])->Result` |
| `ssap-link` | `ssap_link.h:20-110` + `ssap_link.c:16-296` | **No** — `#![forbid(unsafe_code)]` | FSM over `&mut Link` + `u32 now_ms`, transport via `HciTx` trait |
| `hwsle-transport` | `hwsle_transport.h:19-51` + `hwsle_transport.c:21-143` | **Yes — and only here** | Owns `OwnedFd`, `poll/read/write/open` FFI, `pollfd` dance |
| `ws73` facade | — | **No** — `#![forbid(unsafe_code)]` | Re-exports only entry symbols (`RUST-WS73-DEEP-MODULES.md:236-243`) |

Dependence is enforced by `Cargo.toml` — `ssap-codec` and `feature-mgr` have **no** workspace deps; `ssap-server` depends on `ssap-codec + feature-mgr`; `ssap-link` depends on `hwsle-transport` only via bounded `HciTx`; `ws73` facade depends on all five only as re-export (`RUST-WS73-DEEP-MODULES.md:114-124`). A forbidden edge simply has no `dependencies` entry — stronger than any lint.

### 3.2 Where unsafe lives in the C stack (so we know what to fence)

- `ssap_codec` — **no** unsafe surface (`RUST-WS73-UNSAFE-FFI.md:40-50`): header `:19-149` pure, impl `:11-217` branch-on-opcode + manual LE, `ssap_pkt.h:180-588` packed structs never cast.
- `hwsle_transport` — **only** unsafe seam (`RUST-WS73-UNSAFE-FFI.md:52-65`): globals `g_fd:21` / `g_recv_cb:22`, syscalls `open:33` / `write:60,65,87,91` / `poll:104-105` / `read:113`, stack `buf[2048]:102` handed to `g_recv_cb:126` with documented "copy if you need it" contract, `(uint16_t)len` truncation at `:59,86`, incomplete `0xA4 ICB` branch `:138` falling to `off++`.
- `ssap_link` — `&mut` state only (`RUST-WS73-UNSAFE-FFI.md:66-71`): `ssap_link_t:72-91` with `timeout_bucket/first_tick_ms/dlen_retries`, mutations via `&mut` at `:16-295`, `find_cmd_echo:124-135` doing `data[off]|data[off+1]<<8`.
- `ssap_server` — table `&mut` + bounded stack buf (`RUST-WS73-UNSAFE-FFI.md:73-77`): `services[8]` at `ssap_server.h:81-92`, `rsp[512]:168`, `CCCD` side-effect `:449-454`.

### 3.3 Transport's `unsafe` budget (and only)

All 5 `unsafe` sites fenced inside `crates/hwsle-transport/src/internal/sys.rs:1` behind `#![deny(unsafe_op_in_unsafe_fn)]`, each with `// SAFETY:` (`RUST-WS73-UNSAFE-FFI.md:150-156`):

- `unsafe { libc::open(c"/dev/hwsle".as_ptr(), O_RDWR|O_NONBLOCK) }` at `hwsle_transport.c:33`
- `unsafe { libc::write(fd, hdr.as_ptr().cast(), hdr.len()) }` at `hwsle_transport.c:60,65,87,91`
- `unsafe { libc::poll(pfd.as_mut_ptr(),1,500) }` at `hwsle_transport.c:105`
- `unsafe { libc::read(fd, buf.as_mut_ptr().cast(), buf.len()) }` at `hwsle_transport.c:113`
- `unsafe { libc::close(fd) }` at `hwsle_transport.c:45`

Each block cites: fd is `OwnedFd`, buffer is `&mut [u8;2048]` with valid provenance, `pollfd` is `repr(C)`-compatible (`RUST-WS73-UNSAFE-FFI.md:156`). Global `g_fd/g_recv_cb:21-22` become `struct Transport { fd: OwnedFd, on_acb: Box<dyn Fn(&[u8])+Send> }` — owned value, no `static mut` (`RUST-WS73-UNSAFE-FFI.md:140-148`).

### 3.4 Shared-memory & `&mut` escapes fenced

| C escape | Location | Rust fence |
|---|---|---|
| `static int g_fd = -1;` | `hwsle_transport.c:21` | `Transport { fd: OwnedFd }`, `Drop` closes, no global (`RUST-WS73-UNSAFE-FFI.md:140`) |
| `static ssap_recv_fn g_recv_cb` | `hwsle_transport.c:22` | `Box<dyn Fn(&[u8])+Send>` or `mpsc::Sender<Vec<u8>>` |
| `buf[2048]` handed to callback | `hwsle_transport.c:102,126` | `&[u8]` borrowing `buf` for callback duration only, or `Bytes` clone if retained — miri stacked-borrows flags retained slice (`RUST-WS73-UNSAFE-FFI.md:145,252`) |
| `(uint16_t)len` truncation | `hwsle_transport.c:59,86` | `u16::try_from(payload.len())?` — explicit error (`RUST-WS73-UNSAFE-FFI.md:146`) |
| split `write(hdr)+write(payload)` | `hwsle_transport.c:60-69,87-94` | Single `write_vectored` / `write_all` loop, handle `EINTR` (`:107` already handles `EINTR` on `poll` not `write`) |
| `poll(&pfd,1,500)` single-FD | `hwsle_transport.c:104-105` | `rustix::event::poll` so poll loop is reviewed once (`RUST-WS73-UNSAFE-FFI.md:148`) |

Crate policy restated (`RUST-WS73-UNSAFE-FFI.md:238-245`):

1. `#![forbid(unsafe_code)]` in every crate except `hwsle-transport`.
2. `hwsle-transport` concentrates every `unsafe` in `src/internal/sys.rs` — public `lib.rs` never contains `unsafe`.
3. No `static mut`.
4. Byte parsing is borrowed slices, never raw pointers or packed transmutations.
5. `&mut` for state machines (`Link::on_event(&mut self)` / `tick(&mut self,u32)` at `ssap_link.h:102-108`).
6. `libble_host.a` is not part of the transport FFI (`RUST-WS73-UNSAFE-FFI.md:113-115` — ~120 Thumb objects, ARM-only, `no symbols`).
7. `feature-mgr` never calls transport (`feature_mgr.c:1-121` purity).
8. CI enforces the fence via `rg` gates (`RUST-WS73-UNSAFE-FFI.md:199-203`).

---

## 4. Thin→Fat LTO + `codegen-units=1` + `panic=abort` + `strip`

### 4.1 Host C today — no LTO (baseline for comparison)

`stack/ssap/Makefile:8-10` verbatim: `CC ?= ccache cc` `:8`, `CFLAGS ?= -O2 -Wall -Wextra -Iinclude` `:9`, `+= -std=c11` `:10`, `SRCS 5 files:12`, `ar rcs:17-18`. No `LDFLAGS`, no `-flto`, no `-ffunction-sections`, no `-Wl,--gc-sections` (`RUST-WS73-LTO-EXTREME.md:97`). Baseline `libssap.a` text `10421` SysV / `12170` via `size` across 5 `.o` (`RUST-WS73-PERF-BASELINE.md:130-136`), `ssap_codec_test` text `4765` (`RUST-WS73-LTO-EXTREME.md:108`). KO side is `-Os` at `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:472` and `driver/platform/Makefile:308` — **no** `-flto` for KOs (`RUST-WS73-LTO-EXTREME.md:205-211`).

### 4.2 Rust `[profile.release]` — start thin, promote fat only for the binary

Recommended `rust-ws73/Cargo.toml` profile (`RUST-WS73-LTO-EXTREME.md:224-242`):

```toml
[profile.release]
lto = "thin"          # start here; promote to "fat" for the final binary only
codegen-units = 1
panic = "abort"
strip = true          # or strip = "debuginfo" if you need split debug
overflow-checks = false
# opt-level = 3 is default for release — do not set "z"/"s" unless bloat proves size-bound
```

Per knob:

- **`lto = "thin"` → `"fat"` (`RUST-WS73-LTO-EXTREME.md:235`). Thin gives ~80% of FatLTO's DCE/inline win while keeping incremental builds usable. Workspace has multiple crates (`hwsle-transport`, `ssap-codec`, `ssap-server`, `ssap-link`, `feature-mgr` per `RUST-WS73-DEEP-MODULES.md:100`); Fat's absolute saving for ~12 KB `libssap.a` analogue is <1 KB, rarely worth 2-4× link time. Promote only the final binary crate (e.g., `sparklinkd`) to `lto="fat"` if `cargo bloat` + `nm --size-sort` proves `>5%` text saving left.
- **`codegen-units = 1` (`RUST-WS73-LTO-EXTREME.md:236`).** Correct for release — allows cross-CGU inline and defeats the parallelism that blocks LTO. Keep `codegen-units=16` for `[profile.dev]` / `[profile.test]` so `cargo test` stays fast; do not set `codegen-units=1` globally or CI crawls.
- **`panic = "abort"` (`RUST-WS73-LTO-EXTREME.md:237`).** Correct for daemon `sparklinkd` with no unwind recovery. Drops `eh_frame`/`eh_frame_hdr` (`ssap_codec_test` has `.eh_frame 460` + `.eh_frame_hdr 124` even for C) and removes landing pads. Guard: audit `grep -r catch_unwind` before flipping — `panic=abort` will abort instead of catching.
- **`strip = true` (`RUST-WS73-LTO-EXTREME.md:238`).** Cargo analog of `$(STRIP) --strip-unneeded` at `driver/wifi/Makefile:504,520` and `sle_driver/Makefile:71,74`. For debuggability keep `strip="debuginfo"` + `split-debuginfo="packed"` and ship `*.dwp`.
- **Gate: library crates must not set `lto="fat"` independently.** Cargo's profile inheritance means library crates' `lto` is ignored unless the binary sets it (`RUST-WS73-LTO-EXTREME.md:242,379-381`). Setting it everywhere is redundant and misleading.

Host C extreme-perf patch is gated `LTO=1` (`RUST-WS73-LTO-EXTREME.md:195-203`):

```make
ifeq ($(LTO),1)
CFLAGS  += -flto -ffunction-sections -fdata-sections -O2
LDFLAGS += -flto -Wl,--gc-sections -Wl,-Map=$(@:.a=.map)
AR      := $(shell $(CC) -print-prog-name=gcc-ar 2>/dev/null || echo ar)
endif
```

Expected `libssap.a` text `10421` → `9.0-9.8K` (`-8 to -15%`, ~1 KB) plus elimination of `feature_mgr.o` when `FEAT_*` bits off (`RUST-WS73-PERF-BASELINE.md:167,281`); `ssap_codec_test` text `4765` → `4.3-4.6K` inlines `ssap_trans_type_of:11`. KO `wifi_soc.ko` text `966210` → `977K-1.02M` if `-O2` replaces `-Os:472` (size **up**, not down) — KOs keep `-Os` (`RUST-WS73-LTO-EXTREME.md:205-211`).

### 4.3 Metrics triad — prove the win with `size`/`nm`/`Map`

Per `RUST-WS73-LTO-EXTREME.md:339-372` and `RUST-WS73-PERF-BASELINE.md:423-431`:

| Tool | Invocation (host Rust) | What to assert |
|---|---|---|
| `size` | `size --format=SysV target/release/libsparklinkd.rlib` / `size --format=SysV target/release/sparklinkd` | `text` down 8-15% for codec-heavy binary after `LTO=1`; `text` flat for `no_std` leaves |
| `nm` | `nm --print-size --size-sort target/release/sparklinkd \| head -n 50` | dead symbols gone (`fm_update:42`, `ssap_link_mark_activity:113` when unused); `ssap_encode_value 0x??` retained |
| `Map` | `cargo rustc --release -- -Wl,-Map,device.map` then `grep -E "^\.text\|\.data\|\.bss\|Discarded input sections" device.map` | `Discarded input sections` lists gc'd `.text.*`/`.data.*`; `Memory Configuration` confirms layout |

Host C parity: `size --format=SysV stack/ssap/libssap.a:1`, `nm --size-sort stack/ssap/libssap.a:1 | head`, `cc -Wl,-Map,host.map` then `grep "^\\.text"` (`RUST-WS73-PERF-BASELINE.md:425-429`). Device side once `output/ws63-liteos-app.elf:1` exists: `riscv32-linux-musl-size --format=SysV output/ws63-liteos-app.elf:1` (`RUST-WS73-LTO-EXTREME.md:291-298`).

PGO is **rejected** for this stack — four blockers outweigh 5-15% branch-prediction win (`RUST-WS73-LTO-EXTREME.md:307-333`, `RUST-WS73-PERF-BASELINE.md:435-447`): no representative USB bulk trace, KOs cannot profile, Rust PGO CI-heavy for `<2%` win on `memcpy`-dominated path, Device fixed ROM cannot be reordered.

---

## 5. `no_std` alloc — leaf purity for codec + feature-mgr

### 5.1 Why `no_std` for two crates

`stack/ssap/include/ssap_codec.h:15-16` includes only `stdint.h`/`stddef.h`; `stack/ssap/include/feature_mgr.h:20-21` likewise — zero OS deps today (`RUST-WS73-DEEP-MODULES.md:108` pure function island). The codec is `no_std`-friendly by construction: it does `put_u16` + `memcpy` into a caller-owned `out[]` and returns `size_t` written (`ssap_codec.c:61-73` etc.), no allocation, no `&mut` escape (`RUST-WS73-UNSAFE-FFI.md:50`).

Rust mapping (`RUST-WS73-DEEP-MODULES.md:107-112`):

| Crate | Crate type | Allowed deps | Rationale |
|---|---|---|---|
| `ssap-codec` | `no_std` (with `alloc` only for tests) | *(none)* | Pure island, hottest PDU path, first to go pure Rust (phase 1) |
| `feature-mgr` | `no_std` | *(none)* | Bitfield heuristic `feature_mgr.c:42-121`, no runtime deps |

`ssap-server` stays `std` (or `no_std+alloc` if embedded reuse) because it holds `services[8]` + `property_count` + `next_handle` at `ssap_server.h:83-85` and dispatches via `send_frame` fn ptr `:91`. `ssap-link` is `std` (poll/time at `ssap_link.c:250-295`). `hwsle-transport` is `std` (fd/poll at `hwsle_transport.c:21-143`).

### 5.2 Pattern

```toml
# crates/ssap-codec/Cargo.toml
[package]
name = "ssap-codec"
edition = "2021"
# no std dependency by default — tests opt in
[features]
default = []
std = ["alloc"]
alloc = []

[dependencies]
# none — leaf crate, no workspace deps (RUST-WS73-DEEP-MODULES.md:114-124 layering)
```

```rust
// crates/ssap-codec/src/lib.rs:1
#![cfg_attr(not(feature = "std"), no_std)]
#![forbid(unsafe_code)]
#![deny(unsafe_op_in_unsafe_fn)]
// no `extern crate alloc` in lib — codec never allocates; tests use `extern crate alloc` for Vec<u8> vectors
```

`feature-mgr` mirrors: `g_feat_cost[]:17-28` and `drop_order[]:86-90` are `static`/`const` — no `alloc` needed even for tests; keep it `no_std` without `alloc`.

For `std` crates, `Cargo.toml` declares `[profile.release] lto` only on the binary — library crates' `lto` is inherited, not set per-crate (`RUST-WS73-LTO-EXTREME.md:242`).

---

## 6. Miri coverage — four safe crates under stacked borrows

### 6.1 What miri sees

`ssap-codec` is the ideal miri pilot — no syscalls, no `std::fs`, deterministic byte transforms (`RUST-WS73-UNSAFE-FFI.md:133`). It covers provenance + bounds for `ssap_codec.c:43-217` LE helpers and the 10 encode/decode paths. `feature-mgr` covers `feature_mgr.c:42-121` bit arithmetic. `ssap-link` covers `ssap_link.c:124-295` u32 wrap + callback ordering. `ssap-server` covers `find_property:122-132` / `notify:549-567` handle table + `rsp[512]:168` indexing.

Transport cannot run under miri (needs `/dev/hwsle` fd at `hwsle_transport.h:19` `HWSLE_DEV "/dev/hwsle"` and host `open/read/write/poll` at `hwsle_transport.c:33,60,105,113`) — gate excludes it (`RUST-WS73-UNSAFE-FFI.md:214-220`).

### 6.2 CI shape

```bash
# four safe crates only — transport excluded (RUST-WS73-UNSAFE-FFI.md:214-217, RUST-WS73-PERF-BASELINE.md:215-242 guard)
cargo +nightly -C rust-ws73 miri test -p ssap-codec -p feature-mgr -p ssap-link -p ssap-server
```

Transport logic (`frame parse hwsle_transport.c:118-140`, `buf[2048]:102` reuse, `tcid==0x0A:125` demux) still exercises under miri via a mock syscall layer (`RUST-WS73-UNSAFE-FFI.md:220`):

```rust
// crates/hwsle-transport/src/internal/sys_mock.rs:1  (#[cfg(miri)])
pub fn open() -> OwnedFd { /* deterministic mock */ }
pub fn poll() -> i32 { /* inject ACB/HCI frames */ }
```

Alternatively keep transport out of miri and rely on inject `kunit`-style tests (style of `stack/ssap/test/test_link.c:52-180` injecting `on_event` blobs).

### 6.3 Specific miri traps that guard this audit's findings

- `buf[2048]:102` reused after `g_recv_cb:126` returns — callback retaining `&[u8]` aliases. Add `#[cfg(miri)]` regression that stashes the slice and asserts stacked-borrows violation; document callback contract as `// slice valid only for this call` (`RUST-WS73-UNSAFE-FFI.md:252`).
- `ssap_link_tick:250-295` wrap — keep `u32::wrapping_sub` and test wrap case `now_ms=0xFFFF_FFF0 -> 0x0000_0010` under miri (`RUST-WS73-UNSAFE-FFI.md:253`).
- `ssap_server dispatch:162-547` per-branch `len` guards (`:191,286,337`) — off-by-one on `off+3<=len` creates out-of-bounds read miri will flag; fuzz `dispatch(&[u8])` with `cargo fuzz` (`RUST-WS73-UNSAFE-FFI.md:254`).
- Need for `PGO/LTO` vs `miri`: `miri` job must be separate; `lto="fat"` breaks miri interpretation — run `cargo miri test` with `profile.dev.lto=off`, clippy + LTO release build separately (`RUST-WS73-UNSAFE-FFI.md:258`).

### 6.4 Umbrella check (mirrors `RUST-WS73-DEEP-MODULES.md:332-343` + `RUST-WS73-UNSAFE-FFI.md:223-231`)

```bash
cargo -C rust-ws73 check --all-targets \
  && cargo -C rust-ws73 clippy --all-targets -- -D warnings \
  && ./scripts/check-rust-boundaries.sh \
  && ./scripts/check-unsafe-fence.sh   # the two rg gates at RUST-WS73-UNSAFE-FFI.md:198-203
# + nightly miri on 4 safe crates in a separate job
```

---

## 7. `Cargo.lock` patterns, clippy/miri gates

### 7.1 `Cargo.lock` — nothing on disk today, policy is prospective

`find ... -name Cargo.toml` = 0 hits, `find ... -name Cargo.lock` = 0 hits (`RUST-WS73-LTO-EXTREME.md:19` `Cargo.toml / Rust workspace under stack/ssap/ or repo root` **no**). So patterns cannot be grep'd — they are projected from the wayfinder issues:

| Policy | Rationale | Gate |
|---|---|---|
| Binary crates commit `Cargo.lock` (`rust-ws73/Cargo.lock:1`) | `sparklinkd` is a deployed daemon — hermetic build + `Map` traceability, `10-hermetic-build-ci-gate.md:5` pins `riscv32 gcc7.3 / rustc 1.96 / kconfiglib / cmake` + `wait-for-idle.sh` | CI asserts `git ls-files | grep Cargo.lock` contains the binary's lock |
| Library crates do not require `Cargo.lock` in-tree but CI still locks | `ssap-codec`, `feature-mgr` are `no_std` leaves — version pin is via `[workspace.dependencies]` in `rust-ws73/Cargo.toml:1`, not a per-crate lock | `cargo --locked --offline build` in CI (`10-hermetic-build-ci-gate.md:5` hermetic script) |
| Workspace `Cargo.toml:1` is the layering gate | `RUST-WS73-DEEP-MODULES.md:114-124` — forbidden deps simply have no `dependencies` entry; `Cargo.lock` is not the layering gate, `Cargo.toml` is | `cargo metadata --format-version=1 --no-deps` + `check-rust-boundaries.sh:335` |

For now `rust-ws73/Cargo.lock:1` will appear only after `.gitignore:56 !*.rs` is patched to also allow `!*.toml` / `!Cargo.*` per `RUST-WS73-REPO-CONVENTIONS.md:100-117` (otherwise `git check-ignore -v` at `:109-117` shows `.gitignore:19:*` for `Cargo.toml` — denied). The hermetic script must pin `rustc 1.96` per `10-hermetic-build-ci-gate.md:5` and pass `WSCFG_EXTRA_CFLAGS` / `Cargo profile` dual `LTO` rollback identically to Host C `LTO=1` gate (`RUST-WS73-LTO-EXTREME.md:195-200`).

### 7.2 `cargo clippy` — pedantic fence + `rg` gates

Workspace lint config in `rust-ws73/Cargo.toml:1` (`RUST-WS73-UNSAFE-FFI.md:182-192`):

```toml
[workspace.lints.clippy]
pedantic = { level = "warn", priority = 1 }
nursery  = { level = "warn", priority = 2 }
correctness = { level = "deny", priority = 3 }
suspicious  = { level = "deny", priority = 3 }
# Fence: only transport may contain unsafe (checked by rg, not clippy alone)
```

CI (`RUST-WS73-UNSAFE-FFI.md:195-210`):

```bash
cargo -C rust-ws73 clippy --all-targets --all-features -- -D warnings
# fence — anything outside transport mentioning unsafe must fail:
! rg -n '\bunsafe\b|#\[allow.*unsafe' rust-ws73/crates --glob '!crates/hwsle-transport/**' --glob '!**/target/**'
# also gate raw FFI primitives outside transport:
! rg -n 'extern\s+"C"|libc::|nix::|rustix::|OwnedFd|RawFd' rust-ws73/crates --glob '!crates/hwsle-transport/**'
# transport itself must annotate every unsafe:
rg -n 'unsafe' rust-ws73/crates/hwsle-transport --glob '!**/target/**' | rg -q 'SAFETY:'
```

Lints that guard this audit's findings:

- `cast_possible_truncation` / `cast_possible_wrap` — catches the `(uint16_t)len` sites at `hwsle_transport.c:59,86` if Rust does `as u16` (`RUST-WS73-UNSAFE-FFI.md:208`).
- `needless_pass_by_value` / `ptr_as_ptr` / `transmute_ptr_to_ptr` — catches packed-struct `transmute` temptation from `ssap_pkt.h:180-588` (`RUST-WS73-UNSAFE-FFI.md:209`).
- `mut_from_ref` / `unnecessary_mut_passed` — catches `&mut` escape attempts in `ssap_link:16-295` / `ssap_server:12-582` (`RUST-WS73-UNSAFE-FFI.md:210`).

### 7.3 Deep-module `internal/` gate (peer of clippy/miri/fence)

Per `RUST-WS73-DEEP-MODULES.md:332-360` and `RUST-WS73-UNSAFE-FFI.md:257-258`:

```bash
# visibility lint — no crate reaches into another's internal/:
rg -n 'use\s+\w+::internal' rust-ws73/crates --glob '!**/src/internal/**' && exit 1  # RUST-WS73-DEEP-MODULES.md:335
# cycle gate:
cargo -C rust-ws73 tree --edges normal --depth 99 | rg -q 'cycle' && exit 1          # RUST-WS73-DEEP-MODULES.md:338-339
```

Proof run mirrors `RUST-WS73-DEEP-MODULES.md:352-360`: `cargo test -p example` pass → inject `use example::internal::greet` → fail `unresolved import` → revert → pass. Rust's compiler is the deep-import gate — stronger than `dependency-cruiser` post-hoc lint (`RUST-WS73-DEEP-MODULES.md:359`).

Add `rust-ws73/crates/README.md:1` entry per `RUST-WS73-DEEP-MODULES.md:368-377` and `AGENTS.md:25` pointer.

---

## 8. Risks & open questions

| Risk / question | Why it matters | Mitigation |
|---|---|---|
| `hwsle_transport.c:60-69,87-94` does two `write()` header+payload with no retry on short-write | Rust `write_all` must fence, not `write` — otherwise 12 Mbps throughput drops on partial syscall (`RUST-WS73-UNSAFE-FFI.md:251`) | `sys.rs` uses `rustix::fs::write_all` / `writev` loop, returns `WriteZero` if short |
| `buf[2048]:102` reuse after `g_recv_cb:126` — retained `&[u8]` aliases | Miri can catch only if test retains (`RUST-WS73-UNSAFE-FFI.md:252`) | `#[cfg(miri)]` stash regression + doc `// slice valid only for this call` |
| `ssap_link_tick:250-295` `now_ms - first_tick_ms` is `wrapping_sub` | `checked_sub` or `Duration` changes timeout by 1 on wrap (`RUST-WS73-UNSAFE-FFI.md:253`) | Keep `u32::wrapping_sub`, test `0xFFFF_FFF0->0x0000_0010` under miri |
| `ssap_server dispatch:171-543` 8 branches each with per-branch `len` guards `:191,286,337` | Ported branch may omit guard → OOB read under miri (`RUST-WS73-UNSAFE-FFI.md:254`) | Each branch `debug_assert!(pdu.len()>=min)` + same `len<N` error return as C; `cargo fuzz` on `dispatch` |
| Deep-module `internal/` vs clippy visibility | `pub(crate)` in `internal/` is analog of `dependency-cruiser` forbidding `src/packages/*/lib/*` (`RUST-WS73-DEEP-MODULES.md:55-86`) | Extend `check-rust-boundaries.sh:335` to also grep `use hwsle_transport::internal` |
| `PGO/LTO` vs `miri` (cannot run under fat LTO) | `miri` job must be separate; `lto="fat"` breaks interpretation (`RUST-WS73-UNSAFE-FFI.md:258`) | `cargo miri test` with `lto=off`; clippy+LTO release separate |
| `ssap_codec.c:112,148,166` `memcpy` tail uses caller-owned `out[]` | Rust must not allocate — keep `&mut [u8]` and `copy_from_slice` | Transform `async`/`await` adds hidden alloc — keep codec sync |
| `Cargo.lock` currently absent (`find -name Cargo.lock:0`) | Hermetic CI needs lock today (`10-hermetic-build-ci-gate.md:5` riscv32 gcc7.3/rustc 1.96/kconfiglib/cmake pins) | Land `rust-ws73/Cargo.toml:1` + `Cargo.lock:1` after `.gitignore:56` fix (`RUST-WS73-REPO-CONVENTIONS.md:100-117`), `cargo --locked` in CI |
| `fbb_ws63/src/build/toolchains/riscv32_musl_100.cmake:1` absent on this host | Device `-flto` vs `linker.prelds` + `rom_ram_check` conflict discussed in `RUST-WS73-LTO-EXTREME.md:132,251-258` is not applicable to WS73 Linux checkout | Keep Device FW as opaque `firmware/e/ws73.bin:1` 137644 B proxy; treat `text/data/bss` as vendor-fixed |
| `#[inline(always)]` overuse on 10 `encode_*` fns | Bloats `ssap_server.o .text 6145` and defeats ThinLTO size heuristic (`RUST-WS73-LTO-EXTREME.md:235`) | `always` only on `put_u16/get_u16/opcode_of` (`ssap_codec.c:43-53`, `ssap_codec.h:143-146`), `#[inline]` on the rest |

---

## 9. File inventory for reviewers

All absolute under `/home/archivalera/plum/zcode-projects/nearlink`:

- `stack/ssap/src/ssap_codec.c:1` — LE helpers `:43-53`, encode `:55-217`, `memcpy:112,148,166`, tests at `stack/ssap/test/test_codec.c:1`
- `stack/ssap/src/hwsle_transport.c:1` — `g_fd:21`/`g_recv_cb:22`, `put_u16:24-29`, `open:33`/`write:60,65,87,91`/`poll:104-105`/`read:113`/`buf[2048]:102`/`tcid==0x0A:125`
- `stack/ssap/src/ssap_server.c:1` — `dispatch:162-547`, `rsp[512]:168`, `find_property:122-132`, `find_by_cccd:134-145`, `notify:549-567`
- `stack/ssap/src/ssap_link.c:1` — `put_u16:23-28`, `start_timeout:30-34`, `find_cmd_echo:124-135`, `on_event:137-248`, `tick:250-295`
- `stack/ssap/src/feature_mgr.c:1` — `g_feat_cost[]:17-28`, `drop_order[]:86-90`, `fm_update:42-121`
- `stack/ssap/include/ssap_codec.h:1` — constants `:19-41`, API `:99-142`, `SSAP_MTU_MAX 1024:61`
- `stack/ssap/include/hwsle_transport.h:1` — HAL 5+2 `:19-49`, `HCI_DATATYPE_ACB 0xA3:23`, `HWSLE_DEV:19`
- `stack/ssap/include/ssap_server.h:1` — `services[8]:83`, `SSAP_MAX_VALUE_LEN 1024:21`, `send_frame:91`
- `stack/ssap/include/ssap_link.h:1` — `ssap_link_t:72-91`, `DLI_* :21-26`, `SSAP_LINK_CMD_TIMEOUT_MS 5000:35`
- `stack/ssap/include/ssap_pkt.h:1` — packed PDU ground truth `:180-588`, `SSAP_PduErrRsp_S:180-186`
- `stack/ssap/include/feature_mgr.h:1` — `FEAT_*:24-35`, `CAP_TINY..FULL:38-43`
- `stack/ssap/Makefile:1` — `SRCS:12`, `CFLAGS:9-10`, `ar rcs:17-18`, `CC:8`
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-UNSAFE-FFI.md:1` — unsafe isolation, `forbid(unsafe_code)` policy `:238-245`, miri pilot `:133`
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-DEEP-MODULES.md:1` — 5-crate mirror, `lib.rs` narrow entry `:136-211`, `src/internal/` deep `:248-294`, `Cargo.toml` gate `:114-124`, boundaries script `:335`
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-LTO-EXTREME.md:1` — Host C `-flto --gc-sections:195-200`, Rust `lto thin->fat:224-231`, PGO rejection `:307-333`, triad `:339-372`, sizes `:101-118`
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-PERF-BASELINE.md:1` — `libssap.a` text `10421/12170:130-136`, `ssap_codec_test` text `4765:137`, `wifi_soc.ko` text `966210:64`, `memory_config.h:10-11` ITCM 432K
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-REPO-CONVENTIONS.md:1` — `.gitignore:56 !*.rs` gap `:100-117`, `check-docs.sh` 6/6 `:150-162`
- `.scratch/rust-ws73-tri-mode/issues/03-lto-extreme-perf-decision.md:1` — LTO question verbatim
- `.scratch/rust-ws73-tri-mode/issues/04-rust-workspace-shape.md:1` — `rust-ws73/` at repo root
- `.scratch/rust-ws73-tri-mode/issues/10-hermetic-build-ci-gate.md:1` — hermetic pins `riscv32 gcc7.3 / rustc 1.96 / kconfiglib / cmake`

---

*Written 2026-08-19, read-only synthesis. English-only per `AGENTS.md` / `docs/agents/domain.md`. Whitelist `.gitignore` respected — no binaries committed.*

