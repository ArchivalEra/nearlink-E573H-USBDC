---
type: intel
title: WS63 AT Framework — fbb_ws63 Dialect vs OpenHarmony Dialect (HHD-01)
language: zh
created: 2026-08-17
tags: []
---

# WS63 AT Framework — fbb_ws63 Dialect vs OpenHarmony Dialect (HHD-01)

**Date:** 2026-08-17
**Status:** Read-only research. No build, no network, no hardware. All claims cited as `file:line`. Items marked [VERIFY] must be confirmed against the physical board / official PDFs.

**Bottom line up front:**
1. The official prebuilt firmware `ws63-liteos-app_all.fwpkg` (HopeRun `firmware/WS63 WS63E/`) **is an AT-basic firmware carrying the OpenHarmony-dialect SLE/SSAP/GATT command set** (all 18 key SLE commands confirmed present in the binary strings, plus the full OH BLE GATT set, WiFi, Radar, productline `SLEFACCALLBACK`, and `SYSINFO`).
2. The FBB-dialect SLE command set (`SLEADDSERVER`, `SLESSAPSERREGISTER`, `SLESSAPCENWRITE`, `SLECONNADDR`, …) is **NOT present in the prebuilt fwpkg**, and its handler implementations are **not present in the fbb_ws63 source tree** (only the command-table header `at_bt_cmd_table.h` ships; the prebuilt `libbt_at.a` only registers the *productline* RF-test table).
3. Therefore **tomorrow we drive HHD-01 over UART with the OH-dialect names** from the AT case PDF (`AT+SSAPSADDSRV`, `AT+SLECONN`, `AT+SSAPCWRITECMD`, …). The FBB dialect is a red herring for the out-of-box path.

---

## 1. Sources

Local files read (all under `/mnt/hdd/nearlink-stuff/` and the workspace scratch):

- `fbb_ws63/src/middleware/utils/at/at/src/` — core AT engine: `at_base.c/h`, `at_cmd.c/h`, `at_channel.c/h`, `at_msg.c/h`, `at_notify.c/h`, `at_parse.c/h`, `at_process.c/h`
- `fbb_ws63/src/middleware/utils/at/at/include/at_product.h`
- `fbb_ws63/src/middleware/chips/ws63/dfx/include/at_config.h`
- `fbb_ws63/src/include/middleware/utils/at.h`
- `fbb_ws63/src/middleware/utils/at/at_bt_cmd/at/at_bt_cmd_table.h` (main SLE/BLE table)
- `fbb_ws63/src/middleware/utils/at/at_bt_cmd/at/at_bt_productline_cmd_table.h` (productline/factory table)
- `fbb_ws63/src/middleware/utils/at/at_bt_cmd/at/at_bt.h`, `at_bt_productline.h`, `src/at_bt_cmd_register.h`
- `fbb_ws63/src/middleware/utils/at/at_bt_cmd/CMakeLists.txt`, `Kconfig`, prebuilt `ws63-liteos-app/libbt_at.a` (+ `ws63-liteos-xts/`)
- `fbb_ws63/src/middleware/utils/at/{at_wifi_cmd,at_radar_cmd}/at/at_*_cmd_table.h` (brief scan)
- `fbb_ws63/src/application/ws63/ws63_liteos_application/main.c`
- `fbb_ws63/src/application/samples/bt/sle/sle_uuid_server/src/*.c` (native-API flow, sanity check)
- `fbb_ws63/src/build/config/target_config/ws63/config.py`
- `fbb_bs2x/.../at_btc_cmd_table/at_bt_cmd_table.c` + `at_btc_product.c` (sibling chip BS21 — shows how the same header's table is *registered* in a tree that ships the registration source)
- `HopeRun-NearLink/firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg` (1,391,436 B) — `strings` analysis for embedded AT command names
- Workspace: `.scratch/nearlink-driver/assets/HHD01-WS63V100-AT-commands.txt` (extracted OH AT case PDF), `lab-notes/HHD01-BOARD.md` (board note, read for context)

---

## 2. AT 框架架构 (fbb_ws63 AT engine)

The AT engine is a generic, table-driven framework under `src/middleware/utils/at/at/` + one product `at_config.h`. It is agnostic of the command domain (WiFi/BLE/SLE/Radar/PLT are separate command tables).

### 2.1 Command registry
- **Entry struct** `at_cmd_entry_t` (`at.h:346-358`):
  ```
  const char *name; const uint16_t cmd_id; const uint16_t attribute;
  const at_para_parse_syntax_t *syntax;
  at_cmd_func_t cmd;  at_set_func_t set;  at_read_func_t read;  at_test_func_t test;  [query]
  ```
- **Handler signatures** (`at.h:122-151`):
  - `at_cmd_func_t` = `at_ret_t (*)(void)` — execute form `AT+NAME` (no args)
  - `at_set_func_t` = `at_ret_t (*)(const void *arg)` — set form `AT+NAME=...`; `arg` points at a per-command args struct
  - `at_read_func_t` / `at_test_func_t` = `at_ret_t (*)(void)` — `AT+NAME?` / `AT+NAME=?`
  - `at_ret_t` enum error codes in `at.h:46-71` (OK=0, syntax/parse/para/proc errors…).
- **Registration** `uapi_at_cmd_table_register(table, len, struct_max_size)` (`at.h:377`, impl `at_cmd.c:283-301`). Each call appends a node to a **linked list** `g_at_cmd_list` (`at_cmd.c:16,59-81`); `struct_max_size` feeds the single largest SET-args allocation (`at_cmd.c:17,153-154`). There is **no registration macro** — tables are plain `const at_cmd_entry_t at_<domain>_cmd_parse_table[]` arrays passed to the register API.
- **Table check** (optional, `CONFIG_AT_SUPPORT_CMD_TABLE_CHECK`, `at_cmd.c:222-275`): name ≤32 chars (`AT_CMD_NAME_MAX_LENGTH`, `at_config.h:42`), uppercase-only names, unique across registered tables, `set` requires `syntax`, at least one handler non-NULL.

### 2.2 Parameter parsing
- Per-parameter descriptor `at_para_parse_syntax_t` (`at.h:323-337`): bitfield `type:4` (`AT_SYNTAX_TYPE_INT/STRING/BIT_STRING/OCTET_STRING/NUM`, `at.h:262-268`), `last:1`, `attribute:12` (optional/min/max/list/length attrs, `at.h:277-314`), `offset:15` (byte offset into the args struct). Strings are malloc'd + **upper-cased** unless `AT_SYNTAX_ATTR_FIX_CASE` (`at_parse.c:440-458`); INTs are parsed with `strtoull(..., 0)` so **decimal, octal and `0x` hex all work** (`at_parse.c:370-393`); OCTET_STRING hex-pairs → byte array (`at_parse.c:521-546`).
- Arg count = number of syntax entries until `.last==true` (max), minus `OPTIONAL` flags (min) (`at_parse.c:230-301`). Params are comma-split (`at_parse.c:249-285`).
- The `para_map` first field in every args struct is set as a bitmask of which params were supplied (`at_parse.c:625-626`) — handlers can detect missing optional args.
- `at_bt_cmd_table.h` defines its args structs + syntax arrays inline (e.g. `gle_connect_args_t`/`gle_connect_syntax` 2 args, `gle_add_property_args_t` 6 args, `gle_add_descriptor_args_t` 8 args, `gle_write_value_args_t` 6 args — see §3).

### 2.3 Command flow (single UART channel)
`main.c` (`fbb_ws63/src/application/ws63/ws63_liteos_application/main.c`):
- AT task `{"at", AT_STACK_SIZE, ..., uapi_at_msg_main}` (`main.c:205`).
- `at_uart_init()` — UART0, `CONFIG_AT_UART_BAUDRATE` (board 115200,8N1), RX callback → `uapi_at_channel_data_recv` (`main.c:468-489,458`).
- `at_base_api_register()` + `uapi_at_channel_write_register(AT_UART_PORT, at_write_func)` (`main.c:632-633`).
- `do_at_cmd_register()` (`main.c:568-589`) calls: `at_custom_cmd_register()` (weak), `at_plt_cmd_register()`, then **weakrefs** `at_sys_cmd_register` (WiFi, only if `WIFI_TASK_EXIST`), `at_bt_cmd_register` (BT, only if `BTH_TASK_EXIST`), `at_radar_cmd_register` (only if `CONFIG_RADAR_SERVICE`). For ws63-liteos-app both `AT_COMMAND` and `BTH_TASK_EXIST` are set in `config.py:24,30` (and again at `:174,180`).
- Dispatch chain: UART RX → channel state machine strips leading junk, waits for `\r`/`\n` (`at_channel.c:110-165`) → msg queue (`at_msg.c:19-44`) → `at_proc_cmd_handle` (`at_process.c:351-379`) → split on `;` (`at_parse.c:137-201`) → parse format (must be `+` or `^` prefix, `at_parse.c:61-71`) → `at_cmd_find_entry` (case-insensitive name match, `at_cmd.c:98-120`) → parse type (`=`,`=?`,`?`,`\0` → SET/TEST/READ/CMD, `at_parse.c:203-228`) → for SET: malloc `struct_max_size`, `at_parse_para_arguments` fills args (`at_process.c:139-167`) → `at_proc_perform_current_cmd` dispatches `cmd/set/read/test` (`at_process.c:99-126`) → response `OK`/`ERROR` via `at_proc_need_exit` (`at_process.c:314-333`). URC (async notify) via `uapi_at_urc_to_channel` + `at_notify.c` mutex-protected queue.
- Config constants (`at_config.h`): `AT_CMD_MAX_LENGTH` 4160, `AT_MSG_MAX_NUM` 10, `AT_MAX_TIME_OUT` 150, `AT_UART_PORT`=0.

### 2.4 Table ownership — critical subtlety
- `at_bt_cmd_table.h` contains **only** the `at_cmd_entry_t at_bt_cmd_parse_table[]` **plus declarations** of ~40 handler functions (`at_bt_cmd_table.h:945-1020`). The **handler bodies are not in this source tree** (grep for `bt_at_sle_enable`, `bt_at_gle_at_cmd_add_server` matches only the header itself). The only non-header source in `at_bt_cmd/` is the prebuilt `libbt_at.a`, whose members are `at_bt_cmd_register.c.obj` + `at_bt_productline.c.obj` (`ar t`), i.e. **the productline/factory table only**.
- Disassembly of `at_bt_cmd_register.c.obj` (`llvm-objdump`): `at_bt_cmd_register` is a trampoline to `los_at_bt_productline_cmd_register`; the only `.rodata` table is `at_bt_productline_cmd_parse_table` (names `SLEENABLE`, `SLEFACCALLBACK`, `SLETX/SLERX/SLETRXEND/SLERST`, `BTTXLO`, `BLEPWR`…; `strings` of the .a confirms). `uapi_at_bt_register_cmd` = thin wrapper over `uapi_at_cmd_table_register(tbl, num, 0x80)`.
- So in fbb_ws63 **the big SLE/BLE command table `at_bt_cmd_parse_table` is compiled from source that the SDK snapshot does not contain** (handler .c missing; the `CMakeLists.txt` only compiles `src/at_bt_cmd_register.c` + `at/at_bt_productline.c`, `at_bt_cmd/CMakeLists.txt:9-12`). The sibling BS21 SDK (`fbb_bs2x`) ships the registration source `middleware/chips/bs2x/at/at_btc_cmd_table/at_bt_cmd_table.c` that instantiates the very same header's table and exposes `uapi_get_bt_at_table()`/`uapi_get_bt_table_size()` (`at_bt_cmd_table.c:311-318`) — evidence that the full SLE table *is* normally built, but not in this WS63 tree snapshot.

---

## 3. SLE AT 指令全清单 (FBB dialect) + 对照表

### 3.1 Full FBB-dialect SLE/SSAP inventory from `at_bt_cmd_table.h:1317-1657`

Format: `NAME (cmd_id, #params)` → handler (declaration at `at_bt_cmd_table.h:945-1020`).

SLE management:
| Command | ID | #par | Handler |
|---|---|---|---|
| `SLEENABLE` | 1 | 0 | `bt_at_sle_enable` |
| `SLESETADV` | 12 | 0 | `bt_at_gle_at_cmd_set_adv` |
| `SLEENABLEADV` | 13 | 1 | `bt_at_gle_at_cmd_enable_adv` |
| `SLESETADVPAR` | 13 | 8 | `bt_at_sle_set_adv_param` |
| `SLESETADVDATA` | 13 | 5 | `bt_at_sle_set_adv_data` |
| `SLEENADVBYPAR` | 13 | 1 | `bt_at_sle_enable_adv_by_param` |
| `SLEDISADVBYPAR` | 13 | 1 | `bt_at_sle_disable_adv_by_param` |
| `SLECONNADDR` | 11 | 2 | `bt_at_gle_at_cmd_connect` |
| `SLEDISCONN` | 14 | 2 | `bt_at_gle_at_cmd_disconnect` |
| `SLEPAIRREMOTE` | 14 | 2 | `bt_at_gle_pair_remote` |
| `SLEREMOVEPAIR` | 14 | 2 | `bt_at_gle_remove_pair_remote` |
| `SLESETPHY` | 14 | 9 | `bt_at_sle_set_phy` |
| `SLECONNPARUPD` | 14 | 5 | `bt_at_sle_conn_para_update` |
| `SLEGETPAIRDEVICESNUM` | 14 | 0 | `bt_at_gle_sdk_get_paired_devices_num` |
| `SLEGETPAIRDEVICES` | 14 | 0 | `bt_at_gle_sdk_get_paired_devices` |
| `SLEGETPAIRSTATE` | 14 | 0 | `bt_at_gle_sdk_get_pair_state` |

SLE 数传 (server/client SSAP):
| Command | ID | #par | Handler |
|---|---|---|---|
| `SLEADDSERVER` | 16 | 1 | `bt_at_gle_at_cmd_add_server` |
| `SLEADDSERVICE` | 17 | 2 | `bt_at_gle_at_cmd_add_service` |
| `SLEADDPROPERTY` | 18 | 6 | `bt_at_gle_at_cmd_add_property` |
| `SLEADDDESCRIPTOR` | 19 | 8 | `bt_at_gle_at_cmd_add_descriptor` |
| `SLESTARTSERVICE` | 20 | 1 | `bt_at_gle_at_cmd_start_service` |
| `SLESSAPSERREGISTER` | 21 | 0 | `bt_at_gle_at_cmd_ssaps_register_cbks` |
| `SLESSAPCENREGISTER` | 23 | 0 | `bt_at_gle_at_cmd_ssapc_register_cbks` |
| `SLEDDREGISTER` | 22 | 0 | `bt_at_gle_at_cmd_dd_register_cbks` |
| `SLECMREGISTER` | 24 | 0 | `bt_at_gle_at_cmd_cm_register_cbks` |
| `SLEDISCOVERYSERVICES` | 25 | 3 | `bt_at_gle_at_cmd_discovery_services` |
| `SLESSAPCENWRITE` | 26 | 6 | `bt_at_gle_at_cmd_ssapc_write_req` |
| `SLESSAPCENREAD` | 27 | 4 | `bt_at_gle_at_cmd_ssapc_read_req` |

SLE sample helpers:
| Command | ID | #par | Handler |
|---|---|---|---|
| `SLECLIENTINIT` | 28 | 0 | `bt_at_sle_sample_client_init` |
| `SLECLIENTSCAN` | 28 | 0 | `bt_at_sle_sample_client_scan` |
| `SLESERVERINIT` | 28 | 0 | `bt_at_sle_sample_server_init` |
| `SLESSENDBYUUID` | 28 | 2 | `bt_at_sle_sample_server_send_data_by_uuid` |
| `SLESSENDBYHANDLE` | 28 | 2 | `bt_at_sle_sample_server_send_data_by_handle` |
| `CHBASTART` (if `CHBA_SUPPORT`) | 29 | 5 | `bt_at_chba_start` |

Productline table (`at_bt_productline_cmd_table.h:382-575`): `SLEENABLE` (ID1, `bt_at_enable_sle_cmd`), `SLEFACCALLBACK` (ID3, `bt_at_sle_register_callback_cmd`), `SLETX`/`SLERX`/`SLETRXEND`/`SLERST` (RF test, IDs 8-11), `BTTXLO` (18), `BLEPWR` (19), plus BLE RF (`BLETX/BLERX/BLETRXEND/BLERST`) and MFG (`XOTRIM/READTEMP/XOEFUSE/TEMPEFUSE/PWRCALI/PWRCALIEFUSE` under `BGLE_FEATURE_MFG_TEST`). Note `SLEENABLE` appears in **both** tables with ID 1 (duplicate name — registration check is per `uapi_at_cmd_table_register` call; acceptable because they register in separate calls).

### 3.2 Cross-dialect mapping table (OH names from `assets/HHD01-WS63V100-AT-commands.txt`)

The 18 requested commands, mapped FBB↔OH:

| # | OH dialect (in fwpkg / AT case PDF) | FBB dialect (`at_bt_cmd_table.h`) | Status |
|---|---|---|---|
| 1 | `SLEENABLE` | `SLEENABLE` | **Same** (in both FBB tables) |
| 2 | `SLESETADDR` | — (none in main table; BLE has `PHYNUM`) | **OH-only** |
| 3 | `SSAPSREGCBK` | `SLESSAPSERREGISTER` | Renamed (SSAPS server cbk reg) |
| 4 | `SSAPSADDSRV` | `SLEADDSERVER` | Renamed (create server, app UUID) |
| 5 | `SSAPSADDSERV` | `SLEADDSERVICE` | Renamed (add service) |
| 6 | `SSAPSADDPROPERTY` | `SLEADDPROPERTY` | Renamed (add property) |
| 7 | `SSAPSADDDESCR` | `SLEADDDESCRIPTOR` | Renamed (add descriptor) |
| 8 | `SSAPSSTARTSERV` | `SLESTARTSERVICE` | Renamed (start service) |
| 9 | `SLESETADVPAR` | `SLESETADVPAR` | **Same** (8 params both) |
| 10 | `SLESETADVDATA` | `SLESETADVDATA` | **Same** (5 params both) |
| 11 | `SLESTARTADV` | `SLEENABLEADV` | Renamed (adv on; FBB also has `SLEENADVBYPAR`/`SLEDISADVBYPAR`) |
| 12 | `SLESETSCANPAR` | — (BLE has `BLESETSCANPARAM`; SLE none) | **OH-only** |
| 13 | `SLESTARTSCAN` | — (BLE has `BLESCANOPEN`; SLE none) | **OH-only** |
| 14 | `SLESTOPSCAN` | — (BLE has `BLESCANCLOSE`; SLE none) | **OH-only** |
| 15 | `SLECONN` | `SLECONNADDR` | Renamed (connect) |
| 16 | `SLEPAIR` | `SLEPAIRREMOTE` | Renamed (pair remote) |
| 17 | `SSAPCFNDSTRU` | `SLEDISCOVERYSERVICES` | Renamed (find structure / discover) |
| 18 | `SSAPCWRITECMD` | `SLESSAPCENWRITE` | Renamed (client write req) |

Other OH commands (from the PDF/fwpkg) and their FBB counterparts:
- `SSAPCREGCBK` → `SLESSAPCENREGISTER`
- `SSAPSSNDNTFY` → `SLESSENDBYHANDLE` / `SLESSENDBYUUID` (sample server notify)
- `SLEDISCONN` — present in **both** dialects (OH `SLEDISCONN`, FBB `SLEDISCONN`); `SLESETPHY` in both; `SLECONNPARUPD` in both (OH list) / FBB has `SLECONNPARUPD`.
- OH pairing/query: `SLEGETPAIRDEV`/`SLEGETPAIREDNUM`/`SLEGETPAIRSTA`/`SLEUNPAIR`/`SLEUNPAIRALL`/`SLERMVADV`/`SLEGETADDR`/`SLEGETNAME`/`SLESETNAME`/`SLESETDEFAULTCONNP`/`SLEREADPEERRSSI`/`SLEREGCONNCBK`/`SLEATCOMMONREGCBK`/`SLEDISCONNALL`/`SLEDISABLE` (fwpkg strings) vs FBB `SLEPAIRREMOTE`/`SLEREMOVEPAIR`/`SLEGETPAIRDEVICES[NUM/STATE]`.
- OH BLE GATT set (`GATTSREGCBK`, `GATTSREGSRV`, `GATTSSYNCADDSERV/ADDCHAR/ADDDESCR`, `GATTSSTARTSERV`, `GATTCREGCBK`, `GATTCREG`, `GATTCFNDSERV/CHAR/DESCR`, `GATTCWRITEREQ/WRITECMD/READBYHDL/READBYUUID`, `GATTSSNDNTFY`, `GATTCEXCHMTU`, `GATTSUNREG`, `GATTSSETMTU`…) vs FBB BLE (`BTENABLE`, `BTDISABLE`, `BLESETADVPAR/ADVDATA`, `BLEENADVBYPAR/BLEDISADVBYPAR`, `BTCONNADDR`, `BLEENTERSCAN`, `BLESETSCANPARAM`, `BLESCANOPEN/CLOSE`, `GAPENABLEBLE`, `HID*`, `GETBTNAME`, `BTNAME`, `PHYNUM`, `BTPAIRINFO?`, `MOUSEINPUT`…) — different vocabulary entirely.

### 3.3 What the fwpkg actually contains (binary strings)
All 18 OH SLE commands above confirmed present in `ws63-liteos-app_all.fwpkg` (each exactly once): `SLEENABLE`, `SLESETADDR`, `SSAPSREGCBK`, `SSAPSADDSRV`, `SSAPSADDSERV`, `SSAPSADDPROPERTY`, `SSAPSADDDESCR`, `SSAPSSTARTSERV`, `SLESETADVPAR`, `SLESETADVDATA`, `SLESTARTADV`, `SLESETSCANPAR`, `SLESTARTSCAN`, `SLESTOPSCAN`, `SLECONN`, `SLEPAIR`, `SSAPCFNDSTRU`, `SSAPCWRITECMD` — plus `SSAPCREGCBK`, `SSAPCEXCHINFO`, `SSAPCWRITEREQ`, `SSAPCREADREQ`, `SSAPCREADBYUUID`, `SSAPSADDPROPERTY/ADDDESCR` (sync), `SSAPSSNDNTFY`, `SSAPSSNDRESP`, `SSAPSNTFYBYUUID`, `SSAPSSYNCADDSERV/ADDPROPERTY/ADDDESCR`, `SSAPSDELALLSRV`, `SLEDISCONN`, `SLESETPHY`, `SLECONNPARUPD`, `SLERST`, `SLEFACCALLBACK`, `SYSINFO`, full BLE GATT set, WiFi `STARTAP`/`STARTSTA`/`SCAN`… and Radar `RADARSETST`… The FBB-dialect SLE names are **absent** (`SLEADDSERVER`, `SLESSAPSERREGISTER`, `SLESSAPCENREGISTER`, `SLESSAPCENWRITE`, `SLECONNADDR`, `SLEPAIRREMOTE`, `SLEDDREGISTER`, `SLECMREGISTER`, `SLESSENDBYUUID` all count 0). Shared names present in both: `SLESETADVPAR`, `SLESETADVDATA`, `SLEDISCONN`, `SLESETPHY`, `SLEENABLE`, `SLECONNPARUPD`.

---

## 4. 服务端/客户端完整 AT 交互序列

### 4.1 OH dialect (verified against fwpkg; identical to AT case PDF `assets/HHD01-WS63V100-AT-commands.txt:112-161`)
**Server** (board A):
```
AT+SLEENABLE                     ; enable SLE
AT+SLESETADDR=0,0x112233445566   ; set local addr (0=index, 6B)
AT+SSAPSREGCBK                   ; register SSAPS server callbacks
AT+SSAPSADDSRV=0x1234            ; create server, app uuid 0x1234
AT+SSAPSADDSERV=0x2222,1         ; add service 0x2222 (matches 23_sle_uart)
AT+SSAPSADDPROPERTY=1,0x2323,5,5,2,0x1234
                                 ; handle?;uuid 0x2323;perm read|write;operate;properties;...
AT+SSAPSADDDESCR=1,2,0x3333,5,5,2,2,0x0200
                                 ; CCCD 0x0200 descriptor
AT+SSAPSSTARTSERV=1              ; start service
AT+SLESETADVPAR=1,3,200,200,0,0x112233445566,0,0x000000000000
AT+SLESETADVDATA=1,10,4,aabbccddeeff11223344,11224455
AT+SLESTARTADV=1                 ; start announce
```
**Client** (board B):
```
AT+SLEENABLE
AT+SLESETADDR=0,0x112233445577
AT+SSAPCREGCBK                   ; register SSAPC client callbacks
AT+SLESETSCANPAR=1,0x48,0x48
AT+SLESTARTSCAN
AT+SLESTOPSCAN
AT+SLECONN=0,112233445566
AT+SLEPAIR=0,112233445566
AT+SSAPCFNDSTRU=1,0,1            ; find structure (start handle, end handle)
```
**Data plane**: client→server `AT+SSAPCWRITECMD=0,0,1,0,5,0x1122334455` (clientIdx, serverIdx, handle, ?, len, payload); server→client `AT+SSAPSSNDNTFY=0,1,0,5,0x66778899AA` (notify).

### 4.2 FBB dialect (derived from table + syntax; requires the FBB firmware build that includes the handler source, which this snapshot lacks)
**Server**: `AT+SLEENABLE` → `AT+SLESSAPSERREGISTER` (reg cbks) → `AT+SLEADDSERVER=0x1234` (app UUID, 16 hex) → `AT+SLEADDSERVICE=0x2222,1` → `AT+SLEADDPROPERTY=1,0x2323,5,5,2,0x1234` → `AT+SLEADDDESCRIPTOR=1,2,0x3333,5,5,2,2,0x0200` → `AT+SLESTARTSERVICE=1` → `AT+SLESETADVPAR=1,3,200,200,0,0x112233445566,0,0x000000000000` → `AT+SLESETADVDATA=1,10,4,aabbccddeeff11223344,11224455` → `AT+SLEENABLEADV=1`.
**Client**: `AT+SLEENABLE` → `AT+SLESSAPCENREGISTER` → `AT+SLECLIENTINIT` → `AT+SLECLIENTSCAN` (scan; no explicit scan-param command in the main table) → `AT+SLECONNADDR=0,112233445566` → `AT+SLEPAIRREMOTE=0,112233445566` → `AT+SLEDISCOVERYSERVICES=1,0,1` → `AT+SLESSAPCENWRITE=0,0,1,0,5,0x1122334455`.
(Argument shapes mirror §3.1 syntax arrays: `gle_connect_syntax` = 2 strings (3/16 chars); `gle_add_service_syntax` = UUID(16) + type(4); `gle_add_property_syntax` = 6 args; `gle_add_descriptor_syntax` = 8 args with 512-char value; `gle_write_value_syntax` = 6 args; `gle_discovery_services_syntax` = 3 args.)

The native-API call order that these handlers wrap is confirmed by the FBB sample `sle_uuid_server/src/sle_uuid_server.c:83-186` (`ssaps_register_server` → `ssaps_add_service_sync` → `ssaps_add_property_sync` → `ssaps_start_service`) and `sle_server_adv.c:199` (`sle_start_announce`) — i.e. both dialects map 1:1 onto the same SSAP stack.

---

## 5. 固件内置情况 (is the AT framework in the prebuilt firmware?)

- **Yes for the OH dialect**: `HopeRun-NearLink/firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg` (1,391,436 B) embeds the complete OH-dialect AT command set — SLE (`SLEENABLE`…`SSAPCWRITECMD`), BLE GATT, WiFi (`STARTAP`/`STARTSTA`/`SCAN`/`CONN`…), Radar (`RADARSETST`…), productline/factory (`SLEFACCALLBACK`, `SLETX/SLERX/SLETRXEND/SLERST`, `BLEFACCALLBACK`, `BLETX/…`), and `SYSINFO`. It is the natural "基础固件 / AT-basic" candidate the AT case PDF instructs users to flash (`assets/...txt:69,100,168`). [VERIFY on hardware that it boots to an AT prompt; string presence ≠ guaranteed reachable command, but the whole OH SLE sequence set being present together strongly indicates the AT-basic image.]
- **No for the FBB SLE dialect in this SDK snapshot**: `at_bt_cmd/ws63-liteos-app/libbt_at.a` contains only `at_bt_cmd_register.c.obj` (registers productline table) and `at_bt_productline.c.obj` (RF test handlers). The big `at_bt_cmd_parse_table` handler bodies are **not shipped as source** in this tree, and none of the WS63 libs define `at_bt_cmd_parse_table` / `bt_at_sle_set_adv_param` (searched all `*.a`). Building a FBB-dialect AT firmware would require recovering the missing handler source (or using the BS21 tree's `at_btc_cmd_table.c` registration pattern — but that file lives in `fbb_bs2x` and targets BS21).
- The **core AT engine** (`at/` sources, `at_cmd.c`, `at_parse.c`, `at_process.c`, …) is fully in-source and compiles whenever `AT_COMMAND` is set (`config.py:24`), independent of the BT command domain.

---

## 6. 明天上手建议 (practical path)

1. **Do NOT chase the FBB dialect for the out-of-box path.** The board ships with (or we flash) the HopeRun OH-dialect AT firmware; FBB's SLE handlers aren't even in the SDK snapshot.
2. **Flash/keep `firmware/WS63 WS63E/ws63-liteos-app_all.fwpkg`** as the AT-basic firmware on HHD-01 (HH-D01 = WS63 per `HopeRun-NearLink/firmware/README.md:9-13`). [VERIFY the fwpkg is flashable as-is with HiBurn and that the board boots to an AT prompt.]
3. **Probe first**: 115200,8N1, no flow control; send `AT` (expect `OK`) and `AT+SYSINFO` (string present in fwpkg) to confirm the AT-basic image and reveal which vocabulary is live.
4. **PC scripts must send the OH names**: `AT+SLEENABLE`, `AT+SLESETADDR=0,0x112233445566`, `AT+SSAPSREGCBK`, `AT+SSAPSADDSRV=0x1234`, `AT+SSAPSADDSERV=0x2222,1`, `AT+SSAPSADDPROPERTY=1,0x2323,5,5,2,0x1234`, `AT+SSAPSADDDESCR=1,2,0x3333,5,5,2,2,0x0200`, `AT+SSAPSSTARTSERV=1`, `AT+SLESETADVPAR=…`, `AT+SLESETADVDATA=…`, `AT+SLESTARTADV=1`, and on the client `AT+SLESETSCANPAR=…`, `AT+SLESTARTSCAN`, `AT+SLESTOPSCAN`, `AT+SLECONN=0,112233445566`, `AT+SLEPAIR=0,112233445566`, `AT+SSAPCFNDSTRU=1,0,1`, `AT+SSAPCWRITECMD=0,0,1,0,5,0x1122334455`, `AT+SSAPSSNDNTFY=0,1,0,5,0x66778899AA`. Server UUIDs 0x2222/0x2323/0x1234 and the adv params are exactly as in the AT case PDF and match the `23_sle_uart` demo — good for cross-checking against our WS73 PC stack.
5. **UART is the single channel**: the framework's `at_channel_id_t` has only `AT_UART_PORT` (0) plus optional `AT_ZDIAG_PORT` (1) under `CONFIG_AT_SUPPORT_ZDIAG` (`at_config.h:84-90`); the AT case PDF's setup (115200,8N1) matches `main.c` `at_uart_init`.
6. **If we later want the FBB-dialect AT firmware** (e.g. to test the `SLEADDSERVER`… naming or the sample helpers), we must obtain the missing handler source — not available in this snapshot. Out of scope for tomorrow.

---

## 7. Open questions

1. [VERIFY] Does `ws63-liteos-app_all.fwpkg` boot to a working AT prompt on HHD-01, and does it answer `AT+SYSINFO`? (string present, behavior unverified).
2. [VERIFY] Is the fwpkg's AT vocabulary exactly the OH dialect (all 18 SLE + BLE GATT + WiFi + Radar confirmed in strings — expect yes), or does it also enable some FBB commands (only `SLESETADVPAR/SLESETADVDATA/SLEDISCONN/SLESETPHY/SLEENABLE/SLECONNPARUPD` overlap)?
3. Where is the fbb_ws63 `at_bt_cmd_parse_table` handler source? (missing from this snapshot; BS21 tree shows the intended registration pattern via `uapi_get_bt_at_table`.)
4. Does the fwpkg AT firmware implement the **scan** path the same as the OH PDF (scan params `SLESETSCANPAR=1,0x48,0x48` then `SLESTARTSCAN`), and does the seek/announce timing differ from the `23_sle_uart` demo? Only the sequence in the PDF is authoritative [VERIFY].
5. The OH PDF does not cover Radar AT; the companion `WS63V100 AT命令 使用指南_03.pdf` does (per `HH-D01/board/README.md:12`). Not needed for the SLE bridge.

---

## 8. 结论 (AT 桥接路径)

Feasible **with the OH-dialect firmware, zero compilation**:
- PC serial (115200,8N1) ↔ HHD-01 running the official prebuilt `ws63-liteos-app_all.fwpkg`.
- Send the OH-dialect sequence from §4.1 to turn HHD-01 into a SSAP server advertising service 0x2222 / property 0x2323 (CCCD 0x0200), same UUIDs as the `23_sle_uart` demo — then our WS73 PC stack can scan/connect/find/read/write that service, using the AT board as an independent reference implementation of SSAP (cf. `HHD01-BOARD.md:6.2-6.4`).
- The FBB dialect (`SLEADDSERVER`/`SLESSAPCENWRITE`/…) is a parallel naming scheme in the same SDK lineage, but its executable form is not available in this snapshot; do not build tomorrow's scripts on it.

**PC script command list (final):** §3.2 table row "OH dialect" column — 18 core SLE commands + `SSAPCREGCBK` + `SSAPSSNDNTFY` + BLE equivalents if needed.
