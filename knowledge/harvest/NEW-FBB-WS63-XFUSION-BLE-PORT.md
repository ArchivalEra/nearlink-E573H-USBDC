---
type: harvest
title: "port_xf_ble — the XFusion BLE port inside x-eks-fusion/fbb_ws63: GAP/GATT client/server contracts, a dead scan-type map, and a ten-item defect census (dangling adv slots, empty descriptor discovery, double malloc, self-assigned end_hdl)"
language: en
created: 2026-09-17
tags: [fbb_ws63, xfusion, port-layer, ble, gap, gatt, gatt-client, gatt-server, defect-census, ws63, harvest]
sources:
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/port_xf_ble_gap.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/port_xf_ble_gatt_client.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/port_xf_ble_gatt_server.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/CMakeLists.txt"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/application/ws63_porting_xf_0p2/port_xf/CMakeLists.txt"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/application/ws63_porting_xf_0p2/port_xf/Kconfig"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/include/middleware/services/bts/ble/bts_le_gap.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/include/middleware/services/bts/ble/bts_gatt_client.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/include/middleware/services/bts/ble/bts_gatt_server.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/include/middleware/services/bts/common/bts_def.h"
trust: A
stale_after: 2027-03-17
---

# port_xf_ble — the XFusion BLE port inside x-eks-fusion/fbb_ws63: GAP/GATT client/server contracts, a dead scan-type map, and a ten-item defect census

- Inspection date: 2026-09-17; read-only inspection of a local mirror of
  `x-eks-fusion/fbb_ws63` (shallow clone, depth 1, partial clone
  `[blob:none]`, working tree clean for the inspected paths).
- Pinned revision: commit `f0fbaef71197650e7a25695bb814e8fadca511ba`
  (2025-03-13T06:41:24Z, author `dotchan`, master) — the same pin as
  NEW-FBB-WS63-XFUSION-PORT.md, so both port slices are byte-comparable.
  `origin = https://github.com/x-eks-fusion/fbb_ws63.git` verified from git
  config. Every inspected worktree file was re-hashed with
  `git hash-object` and matches its commit blob exactly (blob SHAs in the
  source table), i.e. all citations below are byte-verified against the
  pinned commit; the GitHub URLs are constructed from the verified remote
  plus commit/blob SHAs and were not fetched (no network this round) — they
  are self-validating once opened. File history is one commit deep locally;
  the BLE port's last change is squashed into the head commit itself
  (`!16 refactor(port-ble;sle): sync the xf ble/sle integration`).
- Scope: the BLE port slice `src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/`
  — 3 code files + CMakeLists, measured 1,797 code lines (gap 775, gatt
  client 620, gatt server 402) + 54 CMake lines. Read fully this round: all
  four files, end to end. This is exactly the slice NEW-FBB-WS63-XFUSION-PORT.md
  inventoried-only and named as its follow-up ("port_xf_ble/ (3 files, ~1.8k
  lines — gap/gatt client/server entirely unanalyzed)"). Selection glue
  (`port_xf/CMakeLists.txt`, `port_xf/Kconfig`) and the vendor BLE headers
  (`bts_le_gap.h`, `bts_gatt_client.h`, `bts_gatt_server.h`,
  `bts_def.h`) read at signature level to anchor the API family.

## Executive findings

1. **The BLE port compiles unconditionally; its feature gate lives outside
   the vendor tree.** `port_xf/CMakeLists.txt:14` adds
   `add_subdirectory_if_exist(port_xf_ble)` with no condition, and the
   component is the standard vendor template: `file(GLOB *.c)`
   (`port_xf_ble/CMakeLists.txt:10`), `"-Wno-error"` (`:41`),
   `WHOLE_LINK true` (`:44-46`), `MAIN_COMPONENT false` (`:48-50`),
   `install_sdk "*" ` (`:52`). All three .c files are wrapped in
   `#if (CONFIG_XF_BLE_ENABLE)` (gap `:5`, client `:5`, server `:5`), but
   `port_xf/Kconfig` or-sources Kconfigs for uart/wifi/gpio/i2c/pwm/spi/
   utils and does **not** contain any entry for `port_xf_ble` (nor for
   `port_xf_sle`) — so `CONFIG_XF_BLE_ENABLE` cannot come from the vendor
   Kconfig menu; it must be defined in the injected XFusion `xfconfig.h`.
   With the gate off the files still build — as empty translation units.
   This resolves the open question NEW-FBB-WS63-XFUSION-PORT.md recorded
   ("the BLE/WiFi Kconfig gating could not be confirmed from the
   vendor-side Kconfig alone"): there is no vendor-side BLE gating at all.
2. **Every vendor API the port calls is open and in-tree — the BLE host is
   fully open, unlike the closed SLE GLE host.** All `gatts_*` calls
   verified in `src/include/middleware/services/bts/ble/bts_gatt_server.h`
   (`gatts_register_server` `:493`, `gatts_add_service_sync` `:610`,
   `gatts_add_characteristic_sync` `:637`, `gatts_add_descriptor_sync`
   `:665`, `gatts_start_service` `:687`, `gatts_stop_service` `:708`,
   `gatts_delete_all_services` `:748`, `gatts_send_response` `:775`,
   `gatts_notify_indicate` `:801`, `gatts_register_callbacks` `:878`), all
   `gattc_*` calls in `bts_gatt_client.h` (`gattc_register_client` `:540`,
   `gattc_discovery_service` `:586` — takes only `(client_id, conn_id,
   uuid)`, `gattc_discovery_descriptor` `:637` — takes only a
   characteristic-declaration handle, `gattc_write_req` `:708`, `gattc_write_cmd` `:733`,
   `gattc_exchange_mtu_req` `:756`), and the GAP calls in `bts_le_gap.h`
   (`enable_ble` `:989`, `gap_ble_set_adv_data` `:1136`,
   `gap_ble_get_paired_devices` `:1377`, `gap_ble_set_sec_param` `:1539`,
   `gap_ble_register_callbacks` `:1579`). `bt_uuid_t` is
   `{uint8_t uuid_len; uint8_t uuid[BT_UUID_MAX_LEN]}`
   (`bts_def.h:155-159`).
3. **Ten byte-verified implementation defects** (all reachable, all muted by
   `-Wno-error`):
   a. **Freed advertising slots are never cleared — the 8-instance table
      permanently exhausts.** Both free helpers execute
      `s_list_adv[num_adv] == NULL;` — a no-op comparison instead of an
      assignment (`port_xf_ble_gap.c:695` and `:712`). The freed pointer
      stays in the table, so `_port_ble_adv_alloc` (`:669-684`) finds no
      free slot after 8 create/delete cycles and returns NULL →
      `XF_ERR_RESOURCE` (`:163`), and `_port_ble_adv_get_by_id`
      (`:719-732`) can hand back a dangling pointer (use-after-free).
   b. **The adv-slot free helpers are declared to return pointers.**
      `static xf_err_t *_port_ble_adv_free(...)` / `..._free_by_id(...)`
      (prototypes `:53-54`, definitions `:686`, `:702`); `return XF_OK`
      (`:696`) and `return XF_ERR_NOT_FOUND` (`:699`, `:716`) are
      integers-as-pointers, and `xf_ble_gap_delete_adv` returns one
      directly (`:208`) — implicit pointer-to-int conversions suppressed
      by the component's `-Wno-error`.
   c. **The scan-result advertising-type map is dead code.** A local
      `uint8_t xf_scanned_adv_type = 0;` (`:641`) is never assigned; the
      sentinel `if (xf_scanned_adv_type == 0)` (`:652`) is therefore
      always true and `param.scan_result.type =
      scan_result_data->event_type;` (`:654`) unconditionally overwrites
      whatever the 10-entry port→XF type table (`:623-650`) had just
      written. The raw vendor event type always reaches the app; the TODO
      at `:640` ("optimize into a table") asks for the table that already
      exists one line above.
   d. **Descriptor discovery can never return anything.** The vendor
      `discovery_desc_cb` is wired to `port_ble_gattc_desc_found_cb`
      (`port_xf_ble_gatt_client.c:475`), whose body only marks its
      parameters unused (`:527-534`) — nothing is ever queued, so
      `s_queue_desc_found.cnt` stays 0 and
      `xf_ble_gattc_discovery_desc_all` (`:318-395`) always returns
      cnt=0 with an empty set.
   e. **Characteristic discovery double-allocates.**
      `chara_set_info->set = xf_malloc(chara_set_size)` (`:274`) and then,
      seven lines later, `chara_set_info->set = xf_malloc(size_chara_set)`
      (`:281`) — the first block leaks unconditionally on every call.
   f. **Every discovered service's end handle is zeroed by
      self-assignment.** The service fill reads
      `.end_hdl = service_set[cnt_service].end_hdl` (`:198`) — the
      destination, not `service_found->service.end_hdl` — so after the
      preceding zeroing memset (`:182`) all returned end_hdl values are 0.
   g. **Client app-register bypasses its own prepared UUID.**
      `xf_ble_gattc_app_register` builds `app_uuid_param` (`:116-118`) and
      then never uses it, passing the raw cast
      `gattc_register_client((bt_uuid_t *)app_uuid, app_id)` (`:119-120`)
      — correctness depends on `xf_ble_uuid_info_t` having exactly the
      `{type, uuid128}` layout of vendor `bt_uuid_t`, which is
      unverifiable in-tree (the XF header is build-time injected). The
      server-side twin does the explicit field-by-field copy
      (`port_xf_ble_gatt_server.c:62-72`).
   h. **Pair/bond list buffers leak on vendor-error paths.**
      `xf_ble_gap_get_pair_list` (`:381-400`) and `..._get_bond_list`
      (`:403-422`) heap-allocate a `bd_addr_t` array and `XF_CHECK`-return
      on `gap_ble_get_paired_devices`/`..._get_bonded_devices` failure
      (`:388-390`, `:410-412`) before the `xf_free` (`:398`, `:420`) —
      the same leak shape NEW-FBB-WS63-XFUSION-PORT.md documented in the
      SLE connection manager.
   i. **Passkey pairing is silently faked.** `xf_ble_gap_respond_pair`
      (`:467-471`) and `xf_ble_gap_pair_exchange_passkey` (`:473-478`)
      return `XF_OK` with FIXME comments noting the platform has no
      corresponding operation — an app doing authenticated pairing gets
      fake success.
   j. **Status/error codes are dropped across the whole event surface.**
      Pair completion discards the pairing status (`unused(status)`,
      `:554`); GATT client read/write confirmations discard
      `gatt_status_t` (client `:541`, `:559`) and notification/indication
      discard the errcode (client `:576`, `:596`) — the XF event structs
      carry no status field, so a failed read or a failed pairing is
      indistinguishable from a successful one at the XF level.
4. **Discovery results can be silently partial: the client's quiescence
   budget is 10 ms.** All three discovery calls poll a shared queue until
   its count stops growing for `TIMEOUT_CNT_CHECK_DISCOVERY_RESULT` ×
   `INTERVAL_MS_CHECK_DISCOVERY_RESULT` = 5 × 2 ms = **10 ms**
   (`port_xf_ble_gatt_client.c:27-28`, wait loops `:161-173`, `:256-269`,
   `:348-359`). If the peer streams discovery responses with more than
   10 ms between items, the call returns a prefix of the truth — with no
   error. Compare: the SLE SSAP client in the same tree uses 20 ms × 500 =
   10 s (NEW-FBB-WS63-XFUSION-PORT.md), and the GATT server's
   `INTERVAL_MS_CHECK_ATTR_ADD` 5 ms × 20 (`:25-26`) is dead — the
   constants survive from an async design that was replaced by `_sync`
   vendor calls (see finding 6).
5. **Discovery result sets misalign after any error entry, and the queues
   are process-global.** On a failed entry the code decrements
   `cnt` but still advances the fill index (client `:191-193` + `:208` for
   services; `:294-296` + `:312` for characteristics), so later successes
   land at array positions beyond the returned count. The three queues
   `s_queue_{service,chara,desc}_found` are single statics shared across
   apps and links (`:94-107`), and the malloc-failure early returns
   (`:179-180`, `:275-276`, `:365-367`) leave the queue undrained, so a
   failed call poisons the next one — the same cross-contamination shape
   as the SLE discover queue (prior report, finding 7a).
6. **The GATT server tree is built synchronously with write-back into the
   caller's structs.** `xf_ble_gatts_add_service` (`:99-198`) chains
   `gatts_add_service_sync` (`:117-119`, writing the service handle back
   into `service->handle`), a sentinel-terminated characteristic loop
   (`while (...uuid != XF_BLE_ATTR_SET_END_FLAG)`, `:134` — correctly
   incremented at `:169`/`:195`, unlike the SLE port's descriptor bug) over
   `gatts_add_characteristic_sync` (`:150-151`, writing back
   `handle`/`value_handle` at `:155-156`), and a per-characteristic
   descriptor loop (`:173-194`) over `gatts_add_descriptor_sync`
   (`:184-185`, writing back `desc_set[cnt_desc].handle`). Any
   `XF_CHECK` failure (`:120-121`, `:152-153`, `:186-187`) aborts mid-tree
   with no rollback — a partially added service persists.
7. **UUID byte order is converted only on the server path.**
   `xf_ble_uuid_endian_convert` — a non-static, non-prototyped global
   helper over the vendor type performing a pairwise byte swap
   (`port_xf_ble_gatt_server.c:86-97`) — is applied to the service,
   characteristic and descriptor UUIDs (`:115`, `:148`, `:183`). The GAP
   and GATT-client files never convert (e.g. client `:116-118`,
   `:147-148`, gap `:94-95`). So the XF-side UUID endianness expectation
   differs by role, and any app reusing UUID constants across
   client/server roles gets silently different UUIDs on air.
8. **The event surface is single-slot everywhere and half of it goes
   nowhere.** All three planes register with the event-mask parameter
   ignored (`unused(events)`: gap `:485`, client `:468`, server `:306`)
   into one static callback slot overwritten per registration (gap `:61`,
   client `:92`, server `:53`) — the same last-registrant-wins pattern as
   the SLE dual-role clash. Forwarded: connect/disconnect/pair-end/
   param-update/scan-result (gap `:519-658`), GATT client read/write
   confirm, notify/indicate (client `:536-610`), GATT server
   read/write/MTU requests (server `:339-400`). Never forwarded to the
   app: BLE enable (`:507-510`), adv start/stop/terminate outcomes
   (`:512-517`, `:591-600` — log-only), and `port_ble_gatts_service_del_cb`
   is declared and defined but never registered (server `:38`, `:310-314`,
   `:332-336`). Dead residue also includes the unused
   `port_service_added_node_t` type (server `:30-34`), the unused `xf_ret`
   (server `:103`), and the dead attr-add timing constants (server `:25-26`).
9. **Security-parameter handling is one accumulating global, and enable is
   a hard-coded 1-second sleep.** `xf_ble_gap_set_pair_feature`
   (`:424-465`) keeps a single static `gap_ble_sec_params_t` (`:430`),
   latches `bondable`/`sc_enable` only to true (no unset path,
   `:434-445`), ignores `conn_id` entirely (security params are global,
   not per-connection), and re-sends the whole accumulated struct to
   `gap_ble_set_sec_param` on every feature set (`:461`).
   `xf_ble_enable` unconditionally `osal_msleep(1000)` before
   `enable_ble()` (`:70-77`) — the same boot-order workaround family as
   the SLE port's 800 ms sleep. Appearance is cache-only: the getter
   returns the static `s_appearance` (default `0XFFFF`, `:61-62`) without
   touching the stack (`:119-134`).
10. **The data plane is fire-and-forget — no flow control anywhere.** In
    contrast to the SLE port's watchdog-kicking busy-wait before every
    SSAP operation (prior report, finding 9), the BLE port has no
    backpressure: writes go straight to `gattc_write_cmd`/`gattc_write_req`
    (client `:441-449`), notifications/indications are the same vendor
    call with identical mapping — `gatts_notify_indicate` for both, no
    confirm distinction (server `:236-245` vs `:253-262`) — and read/write
    responses are byte-identical bodies with `.request_id = param->trans_id`
    carrying `// ?` doubt comments (server `:265-299`, `:274`/`:292`).
    Combined with finding 3j (dropped statuses), a stalled or failing
    peer is invisible until reads return stale/empty data.

## Substantive contracts

### Build/selection contract

| Concern | Contract | Evidence |
|---|---|---|
| Component inclusion | `add_subdirectory_if_exist(port_xf_ble)`, unconditional | `port_xf/CMakeLists.txt:14` |
| Compile gate | `#if (CONFIG_XF_BLE_ENABLE)` from injected `xfconfig.h` (no vendor Kconfig entry) | gap/client/server `:5`; `port_xf/Kconfig` (absent) |
| Sources | `file(GLOB *.c)` — silent on file adds | `port_xf_ble/CMakeLists.txt:10` |
| Warning policy | `-Wno-error` (hides all finding-3 defects) | `port_xf_ble/CMakeLists.txt:41` |
| Link posture | `WHOLE_LINK true`, `MAIN_COMPONENT false`, `install_sdk "*"` | `:44-52` |

### GAP contracts (775 lines)

- **Advertising object model**: up to `DEFAULT_BLE_ADV_MAX_CNT = 8` adv
  instances (`:24`, table `:63`); slot index doubles as `adv_id` because
  the vendor API takes a user-chosen id (comment `:678`); `create_adv`
  packs adv data first (`:168`), caches `gap_ble_adv_params_t` with
  `duration = 0` (`:173-186`), start later injects the duration and calls
  `gap_ble_set_adv_param` + `gap_ble_start_adv` (`:212-235`; set failure
  is remapped to `XF_ERR_INVALID_STATE`, discarding the vendor code,
  `:227-228`).
- **Advertising payload packing** (`_port_ble_gap_set_adv_data`,
  `:734-774`): XF TLV struct sets → `xf_ble_gap_adv_data_packed_size_get`
  / `..._packed_by_adv_struct_set` into stack buffers
  `uint8_t data_packed_adv[XF_BLE_GAP_ADV_STRUCT_DATA_MAX_SIZE]` (`:746`,
  `:759`); adv and scan-response lengths are set unconditionally
  (`:754`, `:767`); one vendor call `gap_ble_set_adv_data(adv_id,
  &adv_data_info)` (`:769`). Unlike the SLE port, no workaround for
  zero-length payloads is needed here (size 0 is passed through).
- **Pair/bond enumeration**: heap `bd_addr_t[*max_num]`, vendor fill,
  reverse-order copy-out, free (`:381-422`) — leak on error paths
  (finding 3h).
- **Security**: global accumulating `gap_ble_sec_params_t` (finding 9);
  passkey interaction stubbed (finding 3i).
- **Event funnel**: connect → `XF_BLE_GAP_EVT_CONNECT` (conn_id + addr);
  anything else → `XF_BLE_GAP_EVT_DISCONNECT` (conn_id + addr + reason)
  (`:529-548`); param update forwards interval/latency/timeout
  (`:571-589`); scan result forwards rssi/adv_data/addr (`:604-615`) but
  the type field is always the raw vendor event type (finding 3c).

### GATT client contracts (620 lines)

- **Registration**: layout-dependent raw cast into
  `gattc_register_client` (finding 3g); unregister is a thin wrapper
  (`:126-132`).
- **Discovery trio**: service (`:134-211`), characteristic (`:213-315`),
  descriptor (`:318-395`). Service/chara discovery support an all-zero
  UUID meaning "search all" (chara: per-byte validity scan collapsing to
  `uuid_len = 0`, `:227-241`); the vendor service discovery has no handle
  range, so the XF-level `start_handle`/`end_handle` arguments are
  silently ignored (`:134-141` vs `bts_gatt_client.h:586`), and descriptor
  discovery ignores `end_handle` (`:324`) because
  `gattc_discovery_descriptor` takes only a
  characteristic-declaration handle (`bts_gatt_client.h:637`).
- **Result ownership**: the port mallocs the result array
  (`service_set_info->set`, `:176-182`; chara double-malloc, finding 3e;
  descriptor out-param always reallocated with a leak warning if the
  caller passed non-NULL, `:330-337`) and frees queue nodes as it drains
  (`:205-208`, `:309-312`, `:389-392`).
- **Properties passthrough**: `.props = chara_found->chara.properties`
  with an inline comment conceding that XF's property enumeration extends
  beyond ws63's (`:302`) — no value mapping is attempted.
- **Data ops**: read by handle / by UUID (`:397-426`); write NO_RSP →
  `gattc_write_cmd`, else `gattc_write_req` (`:441-449`); MTU exchange →
  `gattc_exchange_mtu_req(app_id, conn_id, mtu_size)` (`:454-462`).

### GATT server contracts (402 lines)

- **App registration**: explicit `bt_uuid_t` copy (`:62-65`) + log, then
  `gatts_register_server` (`:71-72`).
- **Service tree**: synchronous `_sync` chain with handle write-back
  (finding 6); primary flag from `service->type == XF_BLE_GATT_SERVICE_TYPE_PRIMARY`
  (`:117-119`); descriptor sets optional (`desc_set == NULL` skips,
  `:167-171`).
- **Lifecycle**: `start_service` / `stop_service` / `del_services_all` are
  thin wrappers (`:201-229`) — notably the XF BLE stop-service IS
  implemented, where the SLE port's `xf_sle_ssaps_stop_service` is a
  NOT_SUPPORTED stub (prior report, finding 10).
- **Responses**: read/write response builders are identical
  (`gatts_send_response`, `.status = param->err`, `.request_id =
  param->trans_id` with doubt comments, `:265-299`).

## Runtime limits

- **8 advertising instances maximum** (`DEFAULT_BLE_ADV_MAX_CNT`), and the
  table never recycles slots (finding 3a) — effectively 8 lifetime
  create/delete cycles per boot unless a fix lands.
- **10 ms discovery quiescence window** on the client (finding 4) —
  partial results are returned as normal successes.
- **1 s hard-coded delay** on every `xf_ble_enable` (finding 9).
- **No flow control and no status reporting** on the data plane
  (findings 10, 3j) — backpressure and error visibility are both absent.
- **Synchronous GATT server tree build** on the calling task; failures
  leave partial trees (finding 6).
- One global security-parameters struct; one callback slot per plane;
  three global discovery queues shared across apps/links (findings 9, 8, 5).

## Dedup / novelty

- **No harvest report covers this slice.** `knowledge/harvest/index.md`
  has no BLE-port entry; a corpus-wide search for
  `port_xf_ble` / `port_xf_gap` / `port_xf_gatt` across
  `knowledge/harvest/` and `knowledge/intel/` matches exactly one file —
  NEW-FBB-WS63-XFUSION-PORT.md — which mentions `port_xf_ble` only to
  record it as inventoried-only and to name it the natural follow-up
  (lines 29, 271, 327, 402). The intel documents that cite the port family
  as witnesses (`knowledge/intel/WS63-SSAP-API.md`,
  `knowledge/intel/WS63-CONN-DISCOVERY.md`,
  `knowledge/intel/WS63-VS-WS73.md`) reference the SLE port files, never
  the BLE ones. This report is the first line-level read of the BLE slice.
- **Complements, does not reslice, the sibling report**:
  NEW-FBB-WS63-XFUSION-PORT.md read `port_xf_sle/` fully at the same
  commit; this report reads the other half of the same component
  (`port_xf_ble/`), completes its recorded follow-up, and resolves its
  open BLE/WiFi Kconfig question (finding 1). No overlap in file scope.
- **Closed topics untouched**: this tree contains no hisi-nvs/alloc/
  crypto/fwpkg, no hisiflash, no hisi-registers/rom-sys/rtos/rf, no
  hispark-rs, no DSoftBus, no Ai-BS21 OSAL/NV, no RSSI, no toolbox docs,
  no NLChat web content. The BLE host here is the open vendor
  `bts_*` surface — not the closed SLE GLE host mapped by
  NEW-GLE-HOST-SYMBOL-SURFACE.md.

## Reusable for our stack

- A complete open BLE-host consumer reference for WS63 — GAP + GATT
  client + GATT server in 1.8k lines — useful as a cross-check for our
  own BLE adaptation: callback fan-in shapes, MTU ordering, notify paths.
- The defect census is an addition to the port-quality checklist started
  by the SLE report: comparison-instead-of-assignment in cleanup loops,
  pointer-typed error returns, dead lookup tables behind always-true
  sentinels, empty vendor callbacks behind live-looking APIs, double
  malloc, self-assignment instead of field copy, and status-dropping
  event funnels — all invisible under `-Wno-error`.
- The selection-contract finding (feature gates living in the injected
  xfconfig, not vendor Kconfig) matters for anyone building this fork:
  toggling BLE/SLE ports from the vendor menuconfig is impossible.
- The 10 ms discovery race and the global queues are concrete evidence
  for "sync-over-async discovery must be per-connection and
  completion-signaled, not count-quiesced" — the design lesson the SLE
  report's 10 s variant already hinted at.

## Comparison anchors (existing reports/intel)

- `NEW-FBB-WS63-XFUSION-PORT.md` — the sibling SLE slice at the same
  commit; source of the 800 ms vs 1000 ms enable-delay contrast, the 10 s
  vs 10 ms discovery-budget contrast, the SLE sentinel-loop defect this
  report's server loop does NOT have, and the flow-control contrast.
- `NEW-XFUSION.md` — the XFusion API reconstruction (xf_ble surface was
  inferred from examples there; this report pins the in-tree glue).
- `NEW-WILDLINK-XFUSION.md` — an XFusion consumer project (tri-link SLE +
  BLE + LoRa) that would ride this port.
- `NEW-GLE-HOST-SYMBOL-SURFACE.md` — the closed SLE host archive; the BLE
  host is its open counterpart (all signatures in-tree).
- `NEW-SSAP-SERVM-MODULE-MAP.md` / `NEW-SSAPC-CLIENT-OBJECT-MODEL.md` —
  the OHOS SSAP engine; the vendor BLE GATT layer here is the
  embedded-vendor counterpart for the GATT role.
- `knowledge/intel/WS63-SSAP-API.md` — header-dialect cross-validation
  note; contains no BLE-port references (grep-verified).

## Boundaries and gaps

- Read fully: all four `port_xf_ble/` files (1,797 code lines + CMake) and
  the selection glue. Signature-level only: vendor headers
  `bts_le_gap.h`, `bts_gatt_client.h`, `bts_gatt_server.h`,
  `bts_def.h` (line anchors in finding 2). Not read: `port_xf_wifi/`,
  peripheral ports, `port_utils/`, `platform_def/` (inventoried by the
  sibling report).
- The XFusion-side headers (`xf_ble_gap.h`, `xf_ble_gatt_client.h`,
  `xf_ble_gatt_server.h`, `xfconfig.h`) are build-time injected and not in
  this tree — XF API-side semantics (event enum values, struct layouts,
  `XF_CHECK`/`XF_ASSERT` expansion) are bounded to what the port files
  imply; the `bt_uuid_t` layout-cast correctness (finding 3g) is therefore
  stated as unverifiable in-tree, not as a defect.
- History is one commit deep locally (shallow, partial `[blob:none]`
  clone); the BLE port's evolution before `f0fbaef7` was not traced.
- No build, no hardware, no network; GitHub pin-URLs constructed, not
  fetched.

## Pinned sources (commit `f0fbaef71197650e7a25695bb814e8fadca511ba`)

Base: `https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/`

| Path | Blob SHA (git) |
|---|---|
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/port_xf_ble_gap.c` | `85b1adca26cc38ecdb1da22a0ed04f20e3ac80a1` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/port_xf_ble_gatt_client.c` | `19eeae6f252ead582d99bc476ffeca8f4aae9a04` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/port_xf_ble_gatt_server.c` | `55083e86b055d7b98ac63f03d5f66bb73b398912` |
| `src/application/ws63_porting_xf_0p2/port_xf/port_xf_ble/CMakeLists.txt` | `76e93b961680b9ac5ea4d7c135e529cf793a9fc4` |
| `src/application/ws63_porting_xf_0p2/port_xf/CMakeLists.txt` | `ea0db2a2af2e79a193673b05e05313ff8f782167` |
| `src/application/ws63_porting_xf_0p2/port_xf/Kconfig` | `7bf05940f57f2299de8dca1cb825ec0d91b221b9` |
| `src/include/middleware/services/bts/ble/bts_le_gap.h` | (worktree-verified at pinned commit) |
| `src/include/middleware/services/bts/ble/bts_gatt_client.h` | (worktree-verified at pinned commit) |
| `src/include/middleware/services/bts/ble/bts_gatt_server.h` | (worktree-verified at pinned commit) |
| `src/include/middleware/services/bts/common/bts_def.h` | (worktree-verified at pinned commit) |

## Ten-line summary

1. `port_xf_ble/` in `x-eks-fusion/fbb_ws63` (pinned `f0fbaef71197`, the
   commit also used by the SLE-port report) is a 1,797-line GAP + GATT
   client/server port — the follow-up slice that report named unanalyzed.
2. It compiles unconditionally via `add_subdirectory_if_exist` and gates
   on `CONFIG_XF_BLE_ENABLE` from the injected XFusion xfconfig — no
   vendor Kconfig entry exists for it (nor for the SLE port).
3. Every vendor API it calls is open in-tree (`bts_le_gap.h`,
   `bts_gatt_client.h`, `bts_gatt_server.h`) — the BLE host is fully
   open, unlike the closed SLE GLE archive.
4. Headline defects: freed adv slots never cleared (`== NULL` instead of
   `= NULL`, two sites) so 8 adv instances exhaust permanently; the
   10-entry scan-type map is overwritten by an always-true sentinel
   check; descriptor discovery's vendor callback is empty, so descriptor
   discovery always returns nothing.
5. More: unconditional double malloc in characteristic discovery,
   self-assigned `end_hdl` zeroing every discovered service, a
   layout-dependent raw cast bypassing the client's prepared UUID,
   pair/bond-list leaks on error paths, and passkey pairing stubbed to
   fake success.
6. Statuses are dropped across the event surface (pair result, read/write
   confirms, notify/indicate) — failures are invisible at the XF level.
7. Discovery waits only 10 ms of queue quiescence (vs the SLE port's
   10 s), returns partial sets, misaligns them after error entries, and
   shares three global queues across apps/links.
8. The GATT server tree builds synchronously from `_sync` vendor calls
   with handle write-back and correct loop increments; UUID endian
   conversion is applied server-side only — client/GAP paths never
   convert.
9. No flow control anywhere (contrast: the SLE port's watchdog-kicking
   busy-wait); enable sleeps a hard-coded 1 s; security params are one
   accumulating global with bondable/SC latching only-true.
10. Novelty confirmed: only the sibling report names this slice, as
    inventoried-only follow-up; zero intel coverage; no closed topic
    touches these files.
