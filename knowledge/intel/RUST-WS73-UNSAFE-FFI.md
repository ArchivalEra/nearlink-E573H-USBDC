---
type: intel
title: RUST-WS73 Unsafe / FFI Seam Audit — Lab Note
language: en
created: 2026-09-05
tags: []
---

# RUST-WS73 Unsafe / FFI Seam Audit — Lab Note

- **Ticket:** `.scratch/rust-ws73-tri-mode/issues/09-unsafe-ffi-audit.md` (research, AFK, Blocked by 04,08 — both taken as satisfied for research purposes per prompt)
- **Date:** 2026-08-19
- **Strict scope:** read-only, no network/build/hardware. Exactly one file at `.scratch/nearlink-driver/lab-notes/RUST-WS73-UNSAFE-FFI.md` (English, detailed, `file:line` cited).
- **Question:** Audit the unsafe boundary & FFI seam where `stack/ssap` would touch `libble_host.a` + the `hwsle_transport` HAL — which Rust crates need `unsafe` (transport only?), byte-parsing ownership, shared-memory `&mut` escape, and the `cargo clippy` + `miri` gate. Propose the policy: `unsafe` isolated to `hwsle-transport`, everything else safe.

---

## 0. Sources inspected

| Source | Path | What it tells us |
|---|---|---|
| ssap codec header | `stack/ssap/include/ssap_codec.h:1-149` | 10 encode/decode fns, 0 FFI, 0 globals |
| ssap codec impl | `stack/ssap/src/ssap_codec.c:1-218` | `put_u16`/`get_u16` LE helpers, bounds-checked `memcpy` |
| wire format | `stack/ssap/include/ssap_pkt.h:1-593` | 20+ `__attribute__((packed))` PDU structs, authoritative LE layout |
| transport header | `stack/ssap/include/hwsle_transport.h:1-51` | 5+2 HAL seam (`open`/`send_acb`/`send_ssap`/`send_hci_cmd`/`run`/`close` + `ssap_recv_fn` + `HWSLE_DEV`) |
| transport impl | `stack/ssap/src/hwsle_transport.c:1-143` | `g_fd`/`g_recv_cb` globals, `open`/`write`/`poll`/`read`, ACB/HCI framing, `buf[2048]` |
| link header | `stack/ssap/include/ssap_link.h:1-110` | `ssap_link_t` with `timeout_bucket`/`first_tick_ms`/`last_activity_ms`/`dlen_retries`/callbacks |
| link impl | `stack/ssap/src/ssap_link.c:1-296` | FSM `IDLE→CONNECTING→CONNECTED→DISCONNECTING`, `find_cmd_echo:124-135`, `on_event:137-248`, `tick:250-295` |
| server header | `stack/ssap/include/ssap_server.h:1-126` | service table `≤8×32`, `send_frame` fn ptr, `apply_config` |
| server impl | `stack/ssap/src/ssap_server.c:1-582` | dispatch `EXCHANGE_INFO`/`FIND`/`READ`/`WRITE`/CCCD/`CALL_METHOD`, `notify:549-567`, `rsp[512]` stack buf |
| feature-mgr header | `stack/ssap/include/feature_mgr.h:1-88` | 10 `FEAT_*` bits, `Capacity`/`Conditions`/`Fm` |
| feature-mgr impl | `stack/ssap/src/feature_mgr.c:1-121` | heuristic only, no syscalls, `g_feat_cost[]:17-28` |
| build | `stack/ssap/Makefile:1-44` | 5-file `SRCS`, `libssap.a`, `CFLAGS -O2 -Wall -Wextra -std=c11` |
| tests | `stack/ssap/test/test_codec.c:1-118`, `test/test_link.c:1-181`, `test/test_server.c:1-290`, `test/test_feature.c:1-98` | injection tests without hardware |
| SDK BLE host | `sdk/ws73_sdk_linux_WS73_1.10.110/application/lib/7205_usb/libble_host.a` via `ar t` + `nm` (see §3) | ~120 Thumb objects (`hci_*`, `gap_*`, `smp_*`, `att_gatt`, `l2cap_*`, `osal_*`) — ARM-only, stripped |
| fbb_ws63 reference | `fbb_ws63/src/drivers/chips/ws63/arch/riscv/riscv31/*` (prompt path) + `sdk/ws73_sdk_linux_WS73_1.10.110/driver/` | **not present** in this checkout — see §3 |
| deep-module note | `.scratch/nearlink-driver/lab-notes/RUST-WS73-DEEP-MODULES.md:1-421` | 5-crate mirror, narrow `lib.rs` + `src/internal/` deep, `Cargo.toml` is layering gate |
| BT GAP audit | `.scratch/nearlink-driver/lab-notes/WS73-BT-GAP-AUDIT.md:1-244` | `0xA3/0xA4` framing, `0x1F CUTC`/`0xA4 ICB` gaps, `libble_host.a` is ARM/MIPS only |
| workspace shape | `.scratch/rust-ws73-tri-mode/issues/04-rust-workspace-shape.md:1-8` | taken as satisfied: `rust-ws73/` at repo root, 5 crates mirroring `stack/ssap` + HAL 5+2 seam |
| deep boundaries | `.scratch/rust-ws73-tri-mode/issues/08-deep-module-boundaries.md:1-8` | taken as satisfied: `dependency-cruiser` narrow entry + `RUST-WS73-DEEP-MODULES.md` exists |

> All paths below are absolute under `/home/archivalera/plum/zcode-projects/nearlink` and cited `file:line`.

---

## 1. Where unsafe *actually* lives in the C stack

### 1.1 `ssap_codec` — byte parsing, no unsafe surface

- **Header:** `stack/ssap/include/ssap_codec.h:19-149` exposes only pure functions with explicit bounds (`out_sz`, `handles`/`types` + `count`, `value_len`). No globals, no `void *`, no lifetime escape.
- **Impl:** `stack/ssap/src/ssap_codec.c:11-217` is 207 lines of branch-on-opcode + `put_u16:43-48` / `get_u16:50-53` (hand-rolled LE store/load, 2 bytes each) + `memcpy` for the payload tail. Every encoder checks `out_sz` before writing:
  - `ssap_encode_exchange_info:61-73` — `out_sz < 6` guard, then `ctrl & 0x01/0x02` branches each check `out_sz - len < 2`.
  - `ssap_encode_find_struct_req:102-115` — `out_sz < 7 || uuid_len > 16` guard, `out_sz - len < uuid_len` before `memcpy`.
  - `ssap_encode_read_req:122-133` — `need = 2 + count*3` vs `out_sz`, `count==0` rejects.
  - `ssap_encode_write:139-151` / `ssap_encode_value:153-169` — `need` pre-check, `memcpy` only if `value_len && value`.
  - `ssap_decode_exchange_info:76-94` — `len < 2` early return, each `len - off < 2` guard before `get_u16`.
- **Struct risk:** the authoritative wire types are in `stack/ssap/include/ssap_pkt.h:180-588` as `__attribute__((packed))` (e.g., `SSAP_PduErrRsp_S:180-186`, `SSAP_PduExchangePkt_S:223-236`, `SSAP_PduValueItem_S:520-524`). The codec **never casts a byte buffer to these structs** — it does manual `p[0]/p[1]` LE assembly, so there is no unaligned or strict-aliasing hazard. A Rust port that `transmute`s or `&*ptr.cast::<Pdu>()` would *introduce* UB where none exists today.
- **Ownership in C today:** caller owns `out[]`; callee borrows `out` + `value` as `(ptr,len)` and returns `size_t` written or `0`/`-1` on error. No allocation, no `&mut` escape, no shared memory. This is the model Rust should keep as `&[u8]` / `&mut [u8]` (see §5).

### 1.2 `hwsle_transport` — the only unsafe / shared-memory seam

- **Public seam:** `stack/ssap/include/hwsle_transport.h:19-49` is deliberately tiny — 5 functions + `close`/`run` and one callback type `ssap_recv_fn:31` (`int (*)(const uint8_t*,size_t)`). Constants `HCI_DATATYPE_CMD 0xA1:21`, `HCI_DATATYPE_EVENT 0xA2:22`, `HCI_DATATYPE_ACB 0xA3:23`, `HCI_DATATYPE_ICB 0xA4:24`, `TCID_SLE_SMTC 0x0A:27` etc. are inert macros.
- **Globals (the escape):** `stack/ssap/src/hwsle_transport.c:21-22`:
  ```c
  static int g_fd = -1;
  static ssap_recv_fn g_recv_cb = NULL;
  ```
  Two process-wide mutable statics — every call to `open:31-40`, `close:42-49`, `send_acb:51-71`, `send_hci_cmd:78-96`, and the dispatch in `run:125-126` (`g_recv_cb(buf+off+5,len)`) touches them. No lock, no `thread_local`, single-FD design.
- **Syscalls (the unsafe operations):** `open(HWSLE_DEV, O_RDWR|O_NONBLOCK):33`, `write(g_fd,…):60,65,87,91`, `poll(&pfd,1,500):105`, `read(g_fd,buf,2048):113`. Each is an `unsafe` boundary in Rust terms (`*mut c_void`, raw `fd`, blocking semantics). Error paths log to `stderr:35,62,67` and return `-1`.
- **Shared buffer:** `hwsle_transport_run:102` allocates `uint8_t buf[2048]` on the stack each call to `run`. The parse loop `while(off < r):118-140` slices this buffer (`buf+off+5,len`) and **hands the slice directly to `g_recv_cb:126`** — which today is `ssap_server_dispatch` via the app. The callback receives a pointer into a stack buffer that will be overwritten on the next `read`. If the callback retained the pointer, it would dangle. In C this is a documented caller contract ("copy if you need it"); in Rust this is an `&[u8]` lifetime escape that must be explicit.
- **Truncation hazard:** `stack/ssap/src/hwsle_transport.c:59,86` both do `(uint16_t)len` / `(uint16_t)plen`. A Rust port must not silently truncate `usize→u16` (see §6).
- **Frame parse truncation:** `run:121-133` assumes `off+5 <= r` and `off+5+len <= r`, otherwise `break`. The `HCI_DATATYPE_EVENT:128-137` branch advances by `5+elen` using `buf[off+3..4]` as LE `elen`. No bounds check on `elen` beyond `break`. No `0xA4 ICB` branch — `0xA4` frames fall to `off++:138`.

### 1.3 `ssap_link` — timeout & `&mut` state, no syscalls

- **State:** `stack/ssap/include/ssap_link.h:72-91` packs all FSM + watchdog bookkeeping in one struct (`state`, `conn_handle`, `target_addr[6]`, `supervision_timeout_ms:80`, `last_activity_ms:82`, `first_tick_ms:83`, `timeout_bucket:84`, `dlen_retries:85`, plus 3 fn-ptr callbacks + `ctx`). All mutation is through `ssap_link_t *` — single-owner `&mut` in Rust terms.
- **Mutations:** `ssap_link_init:16-21` zeroes then sets `SSAP_LINK_IDLE` + default `5000ms:20`. `ssap_link_connect:36-80` mutates `target_addr`, `supervision_timeout_ms`, `dlen_retries`, `state=CONNECTING`, `timeout_bucket=1` via `start_timeout:30-34` (which itself mutates `timeout_bucket`+`first_tick_ms`). `ssap_link_disconnect:82-111` mutates `state` to `DISCONNECTING` and `timeout_bucket=2`. `ssap_link_on_event:137-248` mutates `state`/`conn_handle`/`timeout_bucket`/`dlen_retries` per `opcode` (`0x0015:144-182`, `0x0005:183-201`, `0x0001/0x0002:202-244`). `ssap_link_tick:250-295` mutates based on `now_ms` monotonic `u32` subtraction (`now_ms - first_tick_ms > 5000:259,271`, `now_ms - last_activity_ms > supervision_timeout_ms:282`) and may call back `on_connect_failed(0xB0):267` or `on_disconnected(0x08):289`.
- **`&mut` escape risk:** `on_event` and `tick` are only safe if no alias exists to the same `ssap_link_t` during the call. C has no alias analysis — the app must ensure no concurrent `tick` + `on_event` on the same `link` (e.g., `poll` thread vs HCI event thread). Rust will enforce `&mut link` exclusive borrow, which is the desired fix — but it means `link` cannot be shared (`Arc<Mutex<_>>`) without refactoring the callback model. Also `ssap_link_connect:53,69` copies `peer_addr` into `target_addr:71` via `memcpy` and then calls `hwsle_transport_send_hci_cmd:69` — the only place `ssap_link` reaches into the unsafe HAL.
- **Bonus `&mut` via `u32` wrap:** `tick:250-295` relies on unsigned `u32` wrap semantics (`now_ms - first_tick_ms`) — Rust `u32::wrapping_sub` equivalent is needed if the port ever uses `wrapping_sub` vs `checked_sub`.

### 1.4 `ssap_server` — dispatch, `&mut` table + bounded stack buffer

- **Table mutation:** `stack/ssap/include/ssap_server.h:81-92` holds `services[8]` + `property_count` + `next_handle` + `mtu/version/connected` + `send_frame` fn ptr. Every `add_service:21-48`, `add_property:91-120`, `add_method:50-89`, `dispatch:162-547`, `notify:549-567`, `apply_config:569-581` takes `ssap_server_t *` — exclusive `&mut`. No shared memory, but `services[i].properties[j]` is indexed by `handle` searches (`find_property:122-132`, `find_by_cccd:135-145`, `find_by_uuid:148-160`).
- **Byte parsing inside dispatch:** `ssap_server_dispatch:162-547` re-does LE decoding inline (`pdu[2]|pdu[3]<<8:193-194`, `pdu[2]|pdu[3]<<8:345`, etc.) and builds responses in `uint8_t rsp[512]:168`. Bounds are checked per branch (e.g., `len < 6:191`, `len < 5:337`, `len < 7:286`, `n+ vlen <= sizeof(rsp):359`) but the dispatch is ~380 lines of `switch(opcode):171` with 8 `case` groups — easy to miss a `len` guard when porting. The server never touches `g_fd`/`g_recv_cb` directly — it only calls `srv->send_frame:186,220,250,…`.
- **`CCCD` side-effect:** `ssap_server.c:449-454` mutates `cccd_prop->cccd_value` on a write to `prop_handle+1`; `notify:554-561` reads it to gate `indicate`/`notify`. This is another `&mut` path that must stay behind a single borrow.

### 1.5 `feature_mgr` — pure policy, no `&mut` escape

- `stack/ssap/include/feature_mgr.h:23-88` / `stack/ssap/src/feature_mgr.c:1-121` is a closed arithmetic function over `fm_t {capacity, wanted, enabled}` + `fm_conditions_t {ram_free, connected, peer_version, peer_mtu, use_case}`. `fm_init:33-40` and `fm_update:42-121` only read `cond` and write `fm->enabled`; `fm_has:76` is an inline bit test. `g_feat_cost[]:17-28` and `drop_order[]:86-90` are file-scope `static const`. **Zero syscalls, zero shared memory, zero raw pointers.** Port should be a plain `fn update(&mut self, cond: &Conditions) -> u32` with no `unsafe`.

### 1.6 `ssap_pkt.h` packed structs — the hidden FFI hazard

- `stack/ssap/include/ssap_pkt.h:180-588` defines `__attribute__((packed))` structs with bitfields and zero-length `uuid[0]` tails (`SSAP_PduFindStructReq_S:248-259`, `SSAP_PduValue_S:507-518`, `SSAP_PduWriteMultiItem_S:456-460`, etc.). In C, mis-aligned access to packed structs is UB on some arches (ARM). Rust `#[repr(C, packed)]` has the same hazard — need `#[repr(C, packed)]` + `unsafe { ptr::addr_of!((*ptr).field).read_unaligned() }` if mirroring, or better: keep the manual `p[0]|p[1]<<8` codec and never materialize the packed structs.

---

## 2. Crate-boundary proposal — only `hwsle-transport` gets `unsafe`

Using the workspace shape already decided in `04-rust-workspace-shape.md` (taken as satisfied: `rust-ws73/` at repo root, sibling to `stack/ssap/` and `sdk/`, 5 crates mirroring `stack/ssap` + HAL 5+2 seam) and the deep-module entry rule in `RUST-WS73-DEEP-MODULES.md:1-421` (narrow `src/lib.rs` entry, `src/internal/` private, `pub(crate)` only):

| Crate | C counterpart | Needs `unsafe`? | Why |
|---|---|---|---|
| `ssap-codec` | `ssap_codec.h:19-149` + `ssap_codec.c:11-217` | **No** — `#![forbid(unsafe_code)]` | Pure `&[u8]↔&mut [u8]` transforms. All LE helpers are safe shifts (`p[0]|p[1]<<8`). Bounds are `usize` checks. No fd, no globals, no packed-struct casts. The 10 fns map 1:1 to `Option<usize>`/`Result` returns. |
| `feature-mgr` | `feature_mgr.h:23-88` + `feature_mgr.c:16-121` | **No** — `#![forbid(unsafe_code)]` | Arithmetic on `u32` bitflags + `u32` costs. No I/O. Two structs + 3 fns. |
| `ssap-server` | `ssap_server.h:16-126` + `ssap_server.c:12-582` | **No** — `#![forbid(unsafe_code)]` | Table + dispatch over `&mut Server` + `&[u8]` PDU. Uses `ssap-codec` and `feature-mgr` as safe deps. `send_frame` is an injected `fn(&[u8])->Result` (not a raw `write(2)` — transport owns that). `CCCD` gating and `find_*` helpers are `internal/` private. |
| `ssap-link` | `ssap_link.h:20-110` + `ssap_link.c:16-296` | **No** — `#![forbid(unsafe_code)]` | FSM over `&mut Link` + `u32 now_ms`. `on_event`/`tick`/`mark_activity` are `&mut self` methods. Transport send is via an injected trait `HciTx` / `fn(u16,&[u8])`, not direct `write`. `timeout_bucket`/`first_tick_ms`/`dlen_retries` stay private. |
| `hwsle-transport` | `hwsle_transport.h:19-51` + `hwsle_transport.c:21-143` | **Yes — and only here** | Owns `/dev/hwsle` fd (`OwnedFd`), `poll`/`read`/`write`/`open` FFI, `pollfd` dance, callback lifetime, buffer ownership. All `unsafe` is fenced inside `src/internal/sys.rs` and justified with `// SAFETY:` comments. |
| `ws73` (facade) | — | **No** — `#![forbid(unsafe_code)]` | Re-exports only entry symbols (`.scratch/.../RUST-WS73-DEEP-MODULES.md:236-243`); never `pub use internal::…`. |

**Dependence enforced by `Cargo.toml`** (`.scratch/.../RUST-WS73-DEEP-MODULES.md:114-124`): `ssap-codec` and `feature-mgr` have **no** workspace deps; `ssap-server` depends on `ssap-codec + feature-mgr`; `ssap-link` depends on `hwsle-transport` only via a bounded `HciTx` trait (not on `ssap-codec`/`ssap-server`); `hwsle-transport` is a leaf; `ws73` facade depends on all five only as re-export. A forbidden edge simply has no `dependencies` entry — this is stronger than any lint.

---

## 3. What `libble_host.a` + `fbb_ws63` do (and do not) contribute to the FFI seam

### 3.1 `fbb_ws63/.../riscv31/*`

The prompt path `fbb_ws63/src/drivers/chips/ws63/arch/riscv/riscv31/*` **does not exist** in this checkout (confirmed in `RUST-WS73-DEEP-MODULES.md:25,395-397` and `WS73-BT-GAP-AUDIT.md:53`). The SDK root is `sdk/ws73_sdk_linux_WS73_1.10.110/` with `driver/chips/ws63/` (no `fbb_ws63/`). The analogous deep-hiding precedent is `sdk/ws73_sdk_linux_WS73_1.10.110/driver/` (`ws63/bsle/wifi`) where `arch/riscv/` internals are hidden behind `chip.h`/`soc.h` entries — the same `internal/` principle Rust should follow (`RUST-WS73-DEEP-MODULES.md:396-399`). There is no FFI surface from `riscv31/*` to audit here.

### 3.2 `sdk/.../application/lib/*/libble_host.a`

- **Reality:** 15 arch variants under `sdk/ws73_sdk_linux_WS73_1.10.110/application/lib/` (`1155/1156/3516V610/7205_usb/t23/A40I/rk3568/…`, each `libble_host.a`; `WS73-BT-GAP-AUDIT.md:50`). Probed with `ar t …/7205_usb/libble_host.a` + `nm` — ~120 Thumb objects (`memcpy_s.c.o`, `osal_*`, `hci_core.c.o`/`hci_secu.c.o`/`hci_le.c.o`, `att_gatt.c.o`/`att_pdu.c.o`/`att_table.c.o`, `gap_ui.c.o`/`gap_ui_le.c.o`/`gap_period_adv.c.o`, `l2cap_core.c.o`/`l2cap_gle.c.o`, `smp_fsm.c.o`/`smp_aes.c.o`/`smp_ecc.c.o`, `sdk_gatt.c.o`, `sapi_ble_*`, `btsrv_*`, `bt_init.c.o`, `btstack.c.o`, etc.). `nm` output is Thumb-stripped with `U __gnu_thumb1_case_*`/`U malloc`/`U pthread_create` and `T osal_kmalloc/memcpy_s/smp_*` — ARM-only, no x86 objects, `no symbols` for many `.o` on this host. No `0xA1/0xA2/0xA3/0xA4` constants, no `open("/dev/hwsle")` string, no `TCID_*` symbols — `libble_host.a` is the **Android/OHOS BLE host** (BlueZ `hci_dev` path via `sdk/.../driver/bsle/ble_driver/linux/ble_host_hcc.c:1,532,547-551` → `hci_register_dev`), not the WS73 SLE transport.
- **Audit verdict:** `rust-ws73`'s HAL seam is `stack/ssap/include/hwsle_transport.h:19-49` / `stack/ssap/src/hwsle_transport.c:1-143` — **not** `libble_host.a`. Do not link `libble_host.a` from `hwsle-transport` on x86_64. The BLE host FFI, if ever needed, belongs in a separate `ble-host-sys` crate gated by `CARGO_CFG_TARGET_ARCH` (`WS73-BT-GAP-AUDIT.md:176-181` arch-gated `ble-host-ffi` feature) and is out of scope for this unsafe-boundary audit. The transport crate must not expose `libble_host.a` types (like `bd_addr_t`, `bts_def.h`, `Nlstk_Sm*`) — those would widen the unsafe surface for no benefit.

---

## 4. Byte-parsing ownership — `ssap-codec` stays safe

- **Signatures (Rust shape):**
  ```rust
  // stack/ssap/include/ssap_codec.h:99,104,109,116,121,126,131,136,141 vs Rust:
  pub fn encode_exchange_info(out: &mut [u8], opcode: u8, ctrl: u8, mtu: u16, version: u16) -> Option<usize>; // :99
  pub fn decode_exchange_info(pdu: &[u8]) -> Option<(u16,u16)>; // :104 (was int+out-params)
  pub fn encode_find_struct_req(out: &mut [u8], find_type: u8, item_type: u8, rsp_mode: u8, start: u16, end: u16, uuid: Option<&[u8]>) -> Option<usize>; // :109
  pub fn encode_value(out: &mut [u8], opcode: u8, frag: u8, ty: u8, handle: u16, value: &[u8]) -> Option<usize>; // :126
  ```
  Borrowed `&[u8]` input + `&mut [u8]` output, no raw `*const u8`/`*mut u8`, no `unsafe`, no hidden allocation. `Option<usize>` (or `Result<usize, CodecError>`) replaces C's `0/-1` sentinel (`ssap_codec.c:62,94,103,124,141,159,176,193,212`). `value_len` is `value.len()` — not a separate `uint16_t` that can disagree with the slice.
- **LE helpers keep provenance:** keep `stack/ssap/src/ssap_codec.c:43-53` as safe Rust (`out[0]=v as u8; out[1]=(v>>8) as u8;` and `u16::from_le_bytes([p[0],p[1]])`), with `debug_assert!(out.len()>=2)` before indexing. Do not use `ptr::copy_nonoverlapping` or `transmute` — the `memcpy(out+len,uuid,uuid_len):112,148,166` tail is a safe `out[off..off+n].copy_from_slice(uuid)`.
- **Truncation fenced at the boundary:** the two `(uint16_t)len` truncation points (`hwsle_transport.c:59,86`) are avoided in `ssap-codec` because it never converts `usize→u16` — that check belongs in `hwsle-transport` where the on-wire `len u16 LE` is validated against `out.len() <= 65535` and `SSAP_MAX_VALUE_LEN 1024:21` / `SSAP_MTU_MAX 1024:61`.
- **No `repr(packed)` in the codec:** `ssap_pkt.h:180-588` stays header-only ground truth for layout tests; Rust codec never takes `&SSAP_Pdu*` — it round-trips through the byte helpers and is verified by `stack/ssap/test/test_codec.c:23-104` vectors (e.g., `0x02 0x03 0xFB00 0x0103:28-29` for `mtu 251`, `0x01..0x06` for `FIND`, `0x00` for `READ`). Future `ssap-codec` integration tests should replay `test_codec.c` + `test_server.c:1-290` vectors via the safe entry point only (`.scratch/.../RUST-WS73-DEEP-MODULES.md:289-294`).
- **Miri-relevant:** `ssap-codec` is the ideal `miri` pilot — no syscalls, no `std::fs`, deterministic byte transforms. Run `cargo +nightly miri test -p ssap-codec` on every PR; any `unsafe` sneaking into `internal/wire.rs` will be caught.

---

## 5. Shared-memory & `&mut` escape — where Rust must fence

### 5.1 `hwsle_transport` shared memory → isolated to `hwsle-transport`

| C escape | Location | Rust fence |
|---|---|---|
| `static int g_fd = -1;` single global fd | `stack/ssap/src/hwsle_transport.c:21` | `struct Transport { fd: OwnedFd }` — owned value, no global. `open()` consumes/returns the fd; `Drop` calls `close`. No `static mut`. |
| `static ssap_recv_fn g_recv_cb = NULL;` global callback | `stack/ssap/src/hwsle_transport.c:22` | `Transport { on_acb: Box<dyn Fn(&[u8])+Send+'static> }` or `tokio::mpsc::Sender<Vec<u8>>`; no raw fn ptr crossing the FFI line without `Send`. |
| `uint8_t buf[2048];` stack buffer handed to callback | `stack/ssap/src/hwsle_transport.c:102,126` | Hand `&[u8]` that **borrows `buf` for the duration of the callback only** (stack-borrow), or `Bytes` clone if callback needs to retain. Document: "callback must not stash the slice." Miri's stacked borrows will flag a retained `&[u8]` used after `buf` reuse. |
| `(uint16_t)len` truncation | `stack/ssap/src/hwsle_transport.c:59,86` | `u16::try_from(payload.len()).map_err(\|_\|TooLarge)?` — explicit error, not silent wrap. |
| Split `write(hdr,5)+write(payload,len)` | `stack/ssap/src/hwsle_transport.c:60-69,87-94` | Single `write_vectored` or `write_all` loop; handle `EINTR` (`hwsle_transport.c:107` already handles `EINTR` on `poll` but not on `write`). |
| `poll(&pfd,1,500)` + single-FD | `stack/ssap/src/hwsle_transport.c:104-105` | Wrap `poll`/`pollfd` via `rustix::event::poll` so the poll loop is reviewed once. No `&mut` escape — `poll` takes `&mut [PollFd]`. |

**Transport's `unsafe` budget (and only):**
- `unsafe { libc::open(c"/dev/hwsle".as_ptr(), O_RDWR|O_NONBLOCK) }` at `hwsle_transport.c:33`.
- `unsafe { libc::write(fd, hdr.as_ptr().cast(), hdr.len()) }` at `hwsle_transport.c:60,65,87,91`.
- `unsafe { libc::poll(pfd.as_mut_ptr(),1,500) }` at `hwsle_transport.c:105`.
- `unsafe { libc::read(fd, buf.as_mut_ptr().cast(), buf.len()) }` at `hwsle_transport.c:113`.
- `unsafe { libc::close(fd) }` at `hwsle_transport.c:45`.
Each `unsafe` block carries a `// SAFETY:` line citing the invariant: fd is `OwnedFd`, buffer is `&mut [u8;2048]` with valid provenance, `pollfd` is `repr(C)`-compatible. All 5 live in one file `crates/hwsle-transport/src/internal/sys.rs`, behind a `#![deny(unsafe_op_in_unsafe_fn)]` gate.

### 5.2 `ssap_link` `&mut` escape → safe `&mut self`

- `stack/ssap/src/ssap_link.c:16-295` mutates every field of `ssap_link_t` but never aliases — the app is expected to hold one `ssap_link_t` and drive it from one thread (`on_event:137-248` from HCI event path, `tick:250-295` from timer thread). Rust encodes this as:
  ```rust
  impl Link {
    pub fn on_event(&mut self, opcode: u16, data: &[u8]) { /* ... */ } // ssap_link.h:102
    pub fn tick(&mut self, now_ms: u32) { /* ... */ }                  // :105
    pub fn mark_activity(&mut self, now_ms: u32) { /* ... */ }        // :108
  }
  ```
  Exclusive borrow statically prevents `tick(&mut link)` and `on_event(&mut link)` from overlapping — the C double-mut hazard is a compile error in Rust. If the product later needs `tick` from a timer thread while `on_event` runs on the transport thread, the fix is explicit `Arc<Mutex<Link>>` with `try_lock` and documented loss-vs-staleness trade-off — not a silent `&Cell` escape.
- `find_cmd_echo:124-135` does `data[off]|data[off+1]<<8` on a caller-supplied `*data,len` — Rust receives `&[u8]` with implicit `len`, so the `len<3:204` and `len<3:150` guards become `if data.len()<3 { return; }` at the entry of `on_event:137-248`.

### 5.3 `ssap_server` `&mut` + stack buffer

- `ssap_server_t:81-92` holds a fixed `services[8]` array — Rust `heapless`-free `ArrayVec<Service,8>` or `[Option<Service>;8]` with `service_count:84`, mutated only via `&mut self` (`ssap_server.c:21-120`). `rsp[512]:168` is a stack scratch buffer — Rust `let mut rsp = [0u8; 512];` with `&mut rsp` slices; no `Box`, no shared heap.
- `send_frame:91` is the only cross-crate edge (`ssap_server.h:94` `send_frame` param). Rust takes it as `F: Fn(&[u8])->io::Result<()>` or a trait object `dyn HciTx`, injected at `init:12-19`. No raw fn ptr stored as `*const ()`, no `transmute` of the closure.

---

## 6. Clippy + Miri gate

### 6.1 `cargo clippy`

Workplace lint config (put in `rust-ws73/Cargo.toml` `[workspace.lints.clippy]` or `clippy.toml`, enforced by CI):

```toml
[workspace.lints.clippy]
pedantic = { level = "warn", priority = 1 }
nursery  = { level = "warn", priority = 2 }
correctness = { level = "deny", priority = 3 }
suspicious  = { level = "deny", priority = 3 }
# Fence: only transport may contain unsafe
# (checked by the grep gate below, not by clippy alone)
```

CI steps:

```bash
cargo -C rust-ws73 clippy --all-targets --all-features -- -D warnings
# fence gate — anything outside transport mentioning unsafe must fail:
! rg -n '\bunsafe\b|#\[allow.*unsafe' rust-ws73/crates --glob '!crates/hwsle-transport/**' --glob '!**/target/**'
# also gate raw FFI primitives outside transport:
! rg -n 'extern\s+"C"|libc::|nix::|rustix::|OwnedFd|RawFd' rust-ws73/crates --glob '!crates/hwsle-transport/**'
# transport itself must annotate every unsafe:
rg -n 'unsafe' rust-ws73/crates/hwsle-transport --glob '!**/target/**' | rg -q 'SAFETY:'
```

Clippy lints that specifically guard this audit's findings:

- `cast_possible_truncation` / `cast_possible_wrap` — catches the `(uint16_t)len` sites at `hwsle_transport.c:59,86` if Rust does `as u16`.
- `needless_pass_by_value` / `ptr_as_ptr` / `transmute_ptr_to_ptr` — catches packed-struct `transmute` temptation from `ssap_pkt.h:180-588`.
- `mut_from_ref` / `unnecessary_mut_passed` — catches `&mut` escape attempts in `ssap_link` / `ssap_server` APIs.

### 6.2 `cargo miri`

- **Runs miri on the four safe crates only.** Transport's `open/read/write/poll` cannot run under miri (needs host fd) — gate excludes it:
  ```bash
  cargo +nightly -C rust-ws73 miri test -p ssap-codec -p feature-mgr -p ssap-link -p ssap-server
  ```
  This covers: `ssap_codec.c:43-217` byte helpers (provenance + bounds), `ssap_link.c:124-295` u32 wrap arithmetic + callback ordering, `ssap_server.c:122-567` handle table + `rsp[512]` indexing, `feature_mgr.c:42-121` bit arithmetic. Any `unsafe` introduced into these crates (e.g., `ptr::read_unaligned` on a packed PDU) will produce a miri stacked-borrows / alignment error on a test vector from `test_codec.c:23-104` or `test_link.c:52-180`.

- **Transport under miri:** mock the syscall layer. Provide `#[cfg(miri)] mod sys_mock { pub fn open()->OwnedFd {…} pub fn poll()->… }` so `hwsle-transport`'s logic (frame parse `hwsle_transport.c:118-140`, `buf[2048]` reuse, `tcid==0x0A` demux `hwsle_transport.c:125`) still exercises under miri without touching `/dev/hwsle`. Alternatively keep transport out of miri and rely on `kunit`/inject tests (style of `test_link.c:52-180` injecting `on_event` blobs).

### 6.3 Umbrella check (mirrors `RUST-WS73-DEEP-MODULES.md:332-343`)

```bash
cargo -C rust-ws73 check --all-targets \
  && cargo -C rust-ws73 clippy --all-targets -- -D warnings \
  && ./scripts/check-rust-boundaries.sh \
  && ./scripts/check-unsafe-fence.sh   # the two rg gates above
# + nightly miri on 4 safe crates in a separate job
```

Fold into whatever `check`/`lint:boundaries` target `RUST-WS73-DEEP-MODULES.md:341-343` wires — `clippy` + `miri` + `fence.sh` are peers of `check-rust-boundaries.sh`.

---

## 7. Policy — `unsafe` isolated to transport, rest safe

1. **`#![forbid(unsafe_code)]` in every crate except `hwsle-transport`.** `ssap-codec`, `feature-mgr`, `ssap-server`, `ssap-link`, `ws73` each have `#![forbid(unsafe_code)]` at the top of `src/lib.rs`. Any new dependency that introduces `unsafe` (e.g., `bytemuck`, `zerocopy`) must be rejected in those four.
2. **`hwsle-transport` concentrates every `unsafe` in `src/internal/sys.rs`.** Public `src/lib.rs` never contains `unsafe`. Every `unsafe` block in `internal/sys.rs` must have `// SAFETY: <provenance + lifetime invariant>` and the file starts with `#![deny(unsafe_op_in_unsafe_fn)]`.
3. **No `static mut`, no global `g_fd`/`g_recv_cb`.** Transport holds `OwnedFd` + `Box<dyn Fn(&[u8])+Send>` inside an owned `Transport` struct. The C pattern `static g_fd/g_recv_cb:21-22` is gone — no `unsafe` global to audit in the rest of the tree.
4. **Byte parsing is borrow-checked slices, never raw pointers or packed transmutations.** `ssap-codec` works on `&[u8]`/`&mut [u8]`; `ssap_pkt.h:180-588` packed structs are reference only. No `#[repr(C,packed)]` in codec/server/link. Helpers mirror `ssap_codec.c:43-53` manual LE assembly.
5. **`&mut` for state machines, not shared mutability.** `ssap_link_t:72-91` → `Link::on_event(&mut self,…)` / `tick(&mut self, u32)` / `mark_activity(&mut self, u32)` (`ssap_link.h:102-108`). `ssap_server_t:81-92` → `Server::{add_*,dispatch,notify}` on `&mut Self` (`ssap_server.h:94-125`). No `RefCell`/`Cell` aliasing inside the link/server crates; concurrency is an explicit `Mutex` at the app layer if needed.
6. **`libble_host.a` is not part of the transport FFI.** No `extern "C"` binding to `libble_host.a` symbols (`osal_*`, `hci_*`, `gap_*`, `smp_*`, `att_gatt`, etc. per §3.2) from `hwsle-transport`. BLE host bindings, if ever added, live in a separate `ble-host-sys` crate gated by `target_arch` (`.scratch/.../WS73-BT-GAP-AUDIT.md:170-182`) and do not widen this audit's seam.
7. **`feature-mgr` never calls transport.** Mirrors `feature_mgr.c:1-121` purity — `fm_update` returns a `u32` mask; the caller decides whether to `send_hci_cmd`. No hidden `hwsle_transport_send_hci_cmd` from policy code.
8. **CI enforces the fence.** `clippy -- -D warnings` + two `rg` fences (no `unsafe` / no `extern "C"` / `libc` outside `hwsle-transport`) + `miri` on the four safe crates are required checks. `check-unsafe-fence.sh` (see §6) fails the build if any other crate mentions `unsafe`, `extern "C"`, `OwnedFd`, or `RawFd`. `check-rust-boundaries.sh` (`RUST-WS73-DEEP-MODULES.md:332-343`) still enforces the deep `internal/` rule.

---

## 8. Risks & open questions

| Risk / question | Why it matters | Mitigation |
|---|---|---|
| `hwsle_transport.c:60-69,87-94` does two `write()` calls — header then payload — with no retry on partial short-write | Rust `write_all` must be the fence, not `write` | Transport `internal/sys.rs` uses `rustix::fs::write_all` / `writev` loop and returns `io::ErrorKind::WriteZero` if short |
| `buf[2048]:102` stack buffer is reused after `g_recv_cb:126` returns — callback retaining `&[u8]` would alias | Miri can catch this, but only if the test retains | Add a `#[cfg(miri)]` regression that stashes the slice and asserts a stacked-borrows violation; document callback contract as `// slice valid only for this call` |
| `ssap_link_tick:250-295` u32 wrap — `now_ms - first_tick_ms` is `wrapping_sub` semantics | Using `checked_sub` or `Duration` would change timeout by 1 on wrap | Keep `u32::wrapping_sub` and test wrap case (`now_ms = 0xFFFF_FFF0 → 0x0000_0010`) under miri |
| `ssap_server dispatch` has 8 opcode branches (`ssap_server.c:171-544`) with per-branch `len` guards — a ported branch may omit a guard | Off-by-one on `off+3 <= len` vs `<` would create an out-of-bounds read under `miri` | Each branch gets a `debug_assert!(pdu.len()>=min)` + the same `len < N` error return as C; fuzz with `cargo fuzz` on `dispatch(&[u8])` |
| Deep-module `internal/` rule vs `cargo clippy` visibility | `pub(crate)` in `internal/` is the Rust analogue of `dependency-cruiser` forbidding `src/packages/*/lib/*` imports (`RUST-WS73-DEEP-MODULES.md:55-86`) | `check-rust-boundaries.sh` greps `use …::internal` outside `src/internal/` — extend it to also grep `use hwsle_transport::internal` from any crate |
| Need for `PGO/LTO` (`issues/03-lto-extreme-perf-decision.md`) vs `miri` (cannot run under LTO/fat) | `miri` job must be separate; `lto="fat"` breaks miri interpretation | `cargo miri test` runs with `profile.dev.lto=off`; clippy + LTO release build runs separately with `lto="fat" codegen-units=1` — the fence is orthogonal to the perf profile |

---

## 9. File inventory for reviewers

All absolute under `/home/archivalera/plum/zcode-projects/nearlink`:

- `stack/ssap/include/ssap_codec.h:1`, `src/ssap_codec.c:1` — codec seam, LE helpers `:43-53`, all 10 encode/decode paths
- `stack/ssap/include/hwsle_transport.h:1`, `src/hwsle_transport.c:1` — HAL 5+2, `g_fd:21`/`g_recv_cb:22`, `open:33`/`write:60,65,87,91`/`poll:105`/`read:113`/`buf[2048]:102`/`tcid==0x0A:125`
- `stack/ssap/include/ssap_link.h:1`, `src/ssap_link.c:1` — `ssap_link_t:72-91`, `start_timeout:30-34`, `find_cmd_echo:124-135`, `on_event:137-248`, `tick:250-295`, `DLI_* 0x14xx/0x18xx:21-26`
- `stack/ssap/include/ssap_server.h:1`, `src/ssap_server.c:1` — `ssap_server_t:81-92`, `dispatch:162-547`, `rsp[512]:168`, `notify:549-567`, `apply_config:569-581`, `find_*:122-160`
- `stack/ssap/include/ssap_pkt.h:1`, `:180-588` — packed PDU ground truth, `SSAP_PduErrCode_E:188-211`, `SSAP_MsgCode_E:121-143`
- `stack/ssap/include/feature_mgr.h:1`, `src/feature_mgr.c:1` — `FEAT_*:24-35`, `g_feat_cost[]:17-28`, `fm_update:42-121`
- `stack/ssap/Makefile:1` — `SRCS:9`, `CFLAGS:4-6`
- `stack/ssap/test/test_codec.c:1`, `test_link.c:1`, `test_server.c:1`, `test_feature.c:1`
- `sdk/ws73_sdk_linux_WS73_1.10.110/application/lib/7205_usb/libble_host.a` — `ar t` + `nm`, ~120 Thumb objects (`hci_*`/`gap_*`/`l2cap_*`/`smp_*`/`att_gatt`/`osal_*`)
- `.scratch/nearlink-driver/lab-notes/RUST-WS73-DEEP-MODULES.md:1` — 5-crate mirror, `lib.rs` narrow entry, `src/internal/` deep (`:136-294`), Cargo layering gate (`:114-124`)
- `.scratch/nearlink-driver/lab-notes/WS73-BT-GAP-AUDIT.md:1` — `0xA3/0xA4` (`hwsle_transport.h:23-24`), `0x1F CUTC` gap, libble_host ARM-only finding
- Ticket: `.scratch/rust-ws73-tri-mode/issues/09-unsafe-ffi-audit.md:1-8`

---

*Audited 2026-08-19, read-only. Whitelist `.gitignore` respected — no binaries committed. English-only per `AGENTS.md` / `docs/agents/domain.md`.*
