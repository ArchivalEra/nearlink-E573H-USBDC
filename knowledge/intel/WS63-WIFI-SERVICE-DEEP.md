---
type: intel
title: "WS63 WiFi Service Layer — Deep Dive (STA/AP, hostapd/wpa, WOW/CSA/TWT/BTCOEX/ALG, Tri-Mode Coexistence)"
language: en
created: 2026-09-05
tags: [intel, ws63, wifi, service]
sources:
  - "/mnt/hdd/nearlink-stuff/fbb_ws63"
trust: B
stale_after: 2027-03-05
---

# WS63 WiFi Service Layer — Deep Dive (STA/AP, hostapd/wpa, WOW/CSA/TWT/BTCOEX/ALG, Tri-Mode Coexistence)

> Research AFK, read-only, no network/build/hardware. Builds on `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md`.
> Sources are local only. Every factual claim cites `path:line` where load-bearing; `wc -l` totals are reproducible via `wc -l <file>`.
> Date: 2026-08-19. SDK: `sdk/ws73_sdk_linux_WS73_1.10.110`. WS63 tree: `/mnt/hdd/nearlink-stuff/fbb_ws63`.

---

## 0. Sources checked (and what was absent)

| Asked source | Found | Detail |
|---|---|---|
| `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/` | yes | `service/`, `hostapd/liteos_hostapd_src/`, `wpa/` — totals below |
| `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/*` | yes | `wifi_soc.ko`, 528-line `Makefile`, `ws73_alg_host.mk`, `ws73_wifi_defconfig` |
| `sdk/.../open_source/wpa_supplicant` patches | yes | `wpa_supplicant_2_10_linux.patch` + `wpa_supplicant_2_7_android_9.patch` under `open_source/wpa_supplicant/` |
| `fbb_ws63/src/protocol/wifi/*` | **no** | Path does not exist on this host; wifi protocol is in `sdk/driver/wifi` + `fbb_ws63/src/middleware/services/wifi_service` + device ROM headers |
| `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md` | yes | Prior 259-line gap audit; this note extends Sections 2.x/6/7 with WS63 service internals |

`wc -l` for the six WS63 service compilation units actually read (no single source line for totals):

- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/service/soc_wifi_service_api.c:1` — 4079 lines
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/driver_soc/driver_soc.c:1` — 3158 lines
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/liteos_wpa_api/wifi_api.c:1` — 3344 lines
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/hostapd/liteos_hostapd_src/hostapd_main_rtos.c:1` — 574 lines
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/ltos_src/eloop_ltos.c:1` — 147 lines
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/osdep/osdep_osal.c:1` — 212 lines
- plus headers `service_event.h:1` (113 lines), `service_wifi_api.h:1` (210 lines), `driver_soc_common.h:1` (672 lines), `driver_soc_ioctl.h:1` (124 lines)

Directory inventory under `service/` (`CMakeLists.txt:1`, `Makefile_liteos:1`, `service_event.h:1`, `service_wifi_api.h:1`, `soc_wifi_service_api.c:1`):

```
wifi_service/
  service/            soc_wifi_service_api.c — STA/AP/P2P/WPS/WOW/C SI/SDP user API + event demux
  hostapd/
    liteos_hostapd_src/ hostapd_main_rtos.c, hostapd_ctrl_iface_rtos.c, hostapd_if.h
  wpa/
    driver_soc/       driver_soc.c/.h, driver_soc_common.h, driver_soc_ioctl.c/.h, driver_soc_at.h
    liteos_wpa_api/   wifi_api.c/.h, wifi_softap_api.c, wapi_api.c, wpa_log.h
    ltos_src/         eloop_ltos.c/.h
    osdep/            osdep_osal.c, wifi_osdep.h
    scan_list_adapt/  scan_list_adapt.c/.h  (1093 lines)
    wapi/             wai_sm.c, wai_rxtx.c, wai_crypto_mbedtls.c, wapi.c/.h
```

---

## 1. Service-layer API surface (`service_wifi_api.h:1`, `service_event.h:1`, `soc_wifi_service_api.c:1`)

### 1.1 Device types and WPS/WOW enumerations

`service/service_wifi_api.h:23` defines `wifi_device_type_enum` with `WIFI_TYPE_STA:24`, `WIFI_TYPE_HOTSPOT:25`, `WIFI_TYPE_P2P:26`, `WIFI_TYPE_BUTT:27`. WPS method `wifi_wps_method:36` at `service_wifi_api.h:36-40` (`WIFI_WPS_PBC:37`, `WIFI_WPS_PIN:38`). WOW pattern type at `service_wifi_api.h:60-65` (`WOW_PATTERN_ADD:61`, `WOW_PATTERN_DEL:62`, `WOW_PATTERN_CLR:63`). Error domain `wifi_return_code:73` at `service_wifi_api.h:73-86` (`WIFI_SUCCESS:74` through `ERROR_WIFI_UNKNOWN:85`).

Protocol-mode shims are declarations only at this layer: `uapi_wifi_sta_set_protocol_mode:106` / `uapi_wifi_sta_get_protocol_mode:124` and SoftAP equivalents at `service_wifi_api.h:144` / `service_wifi_api.h:162`; PMF at `service_wifi_api.h:181` / `service_wifi_api.h:183`; the generic accessor `wifi_dev_get:202` at `service_wifi_api.h:202-203` is a thin wrapper over the wpa-layer `wifi_dev_get()` (see Section 4).

### 1.2 Event callback list (`service_event.h:1`)

`service/service_event.h:8` pulls `wifi_event.h` (protocol `wifi_event_stru`). The service maintains a doubly-linked list of callbacks:

- `dl_list:59` at `service_event.h:59-62` (`prev/next`), plus `service_event_cb:64` at `service_event.h:64-67` (`dl_list node; wifi_event_stru service_cb`).
- Intrusive helpers at `service_event.h:69-101` (`list_init:69`, `list_delinit:75`, `list_add_node:82`, `list_delete_node:90`, `list_tail_insert:98`, `list_empty:103`).
- In-memory globals in `soc_wifi_service_api.c:95-96` — `g_api_cb_node:95`, `g_api_cb_init:96` — with dedup in `find_wifi_event:621` at `soc_wifi_service_api.c:621-630` and public `wifi_register_event_cb:632` at `soc_wifi_service_api.c:632-660` / `wifi_unregister_event_cb:662` at `soc_wifi_service_api.c:662-682`. Both allocate/free `service_event_cb` via `malloc`/`free` (`soc_wifi_service_api.c:647`, `:676`) and `memcpy_s` (`soc_wifi_service_api.c:652`).

Error logging is `printf`-based stubs at `service_event.h:49-53` (`service_error_log0..4`) — LiteOS `printk` equivalent; not wired to Linux `pr_*`.

---

## 2. STA state machine (LiteOS service → wpa → driver)

### 2.1 Lifecycle primitives

| Step | Symbol | File:line | Note |
|---|---|---|---|
| init | `wifi_init:1332` | `service/soc_wifi_service_api.c:1332` | `vap_res_num=2:1334`, `user_res_num=WIFI_DEFAULT_MAX_NUM_STA:1335`, calls `uapi_wifi_init:1342` then `wifi_sta_set_pmf_mode(WIFI_MGMT_FRAME_PROTECTION_OPTIONAL):1347` |
| deinit | `wifi_deinit:1355` | `service/soc_wifi_service_api.c:1355` | `uapi_wifi_deinit:1362`, clears `g_sta_ifname/g_hotspot_ifname/g_p2p_ifname:1366-1368` and flags `g_sta_enble_flag/g_softap_enble_flag/g_softap_hidden:1369-1372` |
| enable | `wifi_sta_enable:1432` | `service/soc_wifi_service_api.c:1432` | guards `wifi_is_sta_enabled:1434`, calls `register_callback:1440` (see 2.3) then `uapi_wifi_sta_start(g_sta_ifname,&len):1444` and sets `g_sta_enble_flag=1:1448` |
| disable | `wifi_sta_disable:1453` | `service/soc_wifi_service_api.c:1453` | `netifapi_dhcp_stop:1462` then `uapi_wifi_sta_stop:1467`, clears `g_sta_ifname:1471` and `g_sta_last_scan_time_stamp_ms:1473` |
| predicate | `wifi_is_sta_enabled:1478` / `wifi_is_wifi_inited:1377` | `service/soc_wifi_service_api.c:1478`, `:1377` | `g_sta_enble_flag` plus `uapi_wifi_get_init_status` |

`register_callback:610` at `service/soc_wifi_service_api.c:610-619` does `uapi_wifi_config_callback(0,SERVICE_MIN_PRIO:63,SERVICE_MIN_STACK:64)` then `uapi_wifi_register_event_callback(wpa_event_cb_handle:617)`. `SERVICE_MIN_PRIO=10:63`, `SERVICE_MIN_STACK=4096:64`.

Protocol mode is pass-through: `wifi_sta_set_protocol_mode:1496` at `service/soc_wifi_service_api.c:1496-1503` → `uapi_wifi_sta_set_protocol_mode`; getter at `service/soc_wifi_service_api.c:1505-1508`.

### 2.2 Connect: scan-then-assoc split (`wifi_sta_connect:2059`)

Full `wifi_sta_connect:2059` at `service/soc_wifi_service_api.c:2059-2079` validates via `service_check_wifi_device_config:721` at `service/soc_wifi_service_api.c:721-760` (checks `ip_type:728`, `ssid+bssid:733-737`, `security_type:739-745`, `pre_shared_key:748-752`, `wifi_psk_type:754`). Then dispatches:

- `wifi_sta_connect_part1:2025` at `service/soc_wifi_service_api.c:2025-2057` — branches on `config->bssid` vs `config->ssid`. If BSSID is zero, builds a `wifi_scan_params_stru` via `wifi_sta_connect_fill_scan_param:2011` at `service/soc_wifi_service_api.c:2011-2023` (SSID scan), sets `g_sta_conn_req_flag=1` and calls `uapi_wifi_sta_raw_scan` or `uapi_wifi_sta_scan`; the scan-done broadcast re-enters `wifi_sta_connect_part2` (the flag is consumed in `send_broadcast_scan_done:211` at `service/soc_wifi_service_api.c:211-235` where `g_sta_conn_req_flag==1:215` triggers `wifi_sta_connect_part2:217` and early return).
- `wifi_sta_connect_part2:1954` at `service/soc_wifi_service_api.c:1954-2023` — copies saved `g_sta_conn_config:106` (file global) into an `ext_wifi_assoc_request`, calls `wifi_sta_connect_fill_security_type:1880` at `service/soc_wifi_service_api.c:1880-1952` (WEP/WPA/SAE/WAPI key validation via `service_check_wep_key:798`, `service_check_wpa_key:823`, `service_check_sae_key:842`, `service_check_wapi_key:858` — all with ASCII vs HEX `wifi_psk_type` branching), then `uapi_wifi_sta_connect(&req):2003`. The timestamp gate `g_sta_last_scan_time_stamp_ms:102` is updated in `send_broadcast_scan_done:213` at `service/soc_wifi_service_api.c:213` via `wifi_get_timestamp_ms:204` at `service/soc_wifi_service_api.c:204-209`.

IP assignment is orthogonal: `service_set_ip:1089` at `service/soc_wifi_service_api.c:1089-1116` with `service_set_ipv4:915` (`:915-951`), `service_set_ipv6:953` (`:953-983`), `service_set_ipv4_dns:985` (`:985-1035`), `service_set_ipv6_dns:1037` (`:1037-1087`). DHCP path returns early at `service_set_ip:1092` (`if ip_type==DHCP return SUCC`). These call `lwip/netifapi.h` (`service/soc_wifi_service_api.c:15-17`: `netifapi_netif_find_by_name`, `netifapi_netif_set_addr`, `netifapi_dhcp_stop`).

### 2.3 Scan

STA scan wrappers:

- `wifi_sta_scan:1621` at `service/soc_wifi_service_api.c:1621-1636` → `uapi_wifi_sta_scan:1629`
- `wifi_sta_scan_advance:1638` at `service/soc_wifi_service_api.c:1638-1690` — maps `wifi_scan_params_stru.scan_type` (`WIFI_CHANNEL_SCAN:1653`, `WIFI_SSID_SCAN/PREFIX:1661`, `WIFI_BSSID_SCAN:1676`) into `ext_wifi_scan_params` and calls `uapi_wifi_sta_advance_scan:1689`
- `wifi_raw_scan:1565` at `service/soc_wifi_service_api.c:1565-1619` — same mapping but calls `uapi_wifi_sta_raw_scan(&sp,cb):1618` with a raw callback `wifi_scan_no_save_cb`
- `wifi_sta_scan_stop:1543` at `service/soc_wifi_service_api.c:1543-1546` → `uapi_wifi_force_scan_complete:1545`
- `wifi_sta_get_scan_info:1692` at `service/soc_wifi_service_api.c:1692-…` (truncated line 1703 limit check `*size > WIFI_SCAN_AP_LIMIT:1703`)

On the wpa side, scans bottom out in `wpa/liteos_wpa_api/wifi_api.c:1208` (`wifi_scan:1208`), `wifi_wpa_scan:1080` (`wifi_wpa_scan:1080-1117`), `wifi_scan_buffer_process:1024` (`:1024-1078`), and `wifi_scan_result:1429` (`:1429-1457`) which issues `wpa_cli_scan_results:1440` and waits on `WPA_EVENT_GET_SCAN_RESULT_FLAG:1447`. See Section 4.

---

## 3. AP / SoftAP state machine

### 3.1 Enable/disable + advance config

SoftAP advance defaults at `service/soc_wifi_service_api.c:85` — `g_softap_advance_config:85 = {100 beacon_interval, 2 dtim, 86400 group_rekey, 1 gi, 1 hidden, WIFI_MODE_11B_G_N_AX}`.

- `wifi_softap_enable:2666` at `service/soc_wifi_service_api.c:2666-2715` — validates via `wifi_is_wifi_inited:…`, calls `register_callback:2705` if needed, builds `ext_wifi_softap_config conf:2668` via `service_set_softap_protocol:1260` (`:1260-1267` → `uapi_wifi_softap_set_protocol_mode`) and `service_check_hotspot_config_advance:1175` (`:1175-1214` — validates `beacon_interval 25..1000:1178`, `dtim 1..30:1183`, `group_rekey 30..86400:1189`, `hidden 1..2:1195`, `protocol_mode enum:1201`), then `uapi_wifi_softap_start(&conf,g_hotspot_ifname,&len):2709` and sets `g_softap_enble_flag=1` / `g_softap_hidden` accordingly.
- `wifi_softap_disable:2717` at `service/soc_wifi_service_api.c:2717-2743` → `uapi_wifi_softap_stop:2738`.
- `wifi_softap_get_sta_list:2748` at `service/soc_wifi_service_api.c:2748-2780` → `uapi_wifi_softap_get_connected_sta:2778`.
- `wifi_softap_deauth_sta:2840` at `service/soc_wifi_service_api.c:2840-2859` → `uapi_wifi_softap_deauth_sta:2857`.

AP-side scan is separate: `wifi_ap_scan:1548` at `service/soc_wifi_service_api.c:1548-1563` guards `service_wifi_and_ap_check:1550` (`:707-719` — checks `uapi_wifi_get_init_status` and `g_softap_enble_flag`) then `uapi_wifi_ap_scan:1556`.

### 3.2 P2P (conditionally compiled)

All P2P helpers are under `#ifdef CONFIG_P2P_SUPPORT` (e.g., `service/soc_wifi_service_api.c:167`, `:237`, `:265`, `:571`, `:593`). Global flags `g_p2p_enable_flag:93`, `g_p2p_isgo:97` at `service/soc_wifi_service_api.c:93`, `:97`. Event cases `EXT_WIFI_EVT_P2P_GO_NEG_REQUEST:572`, `GO_NEG_SUCCESS:575`, `GO_NEG_FAILURE:578`, `INVITATION_RECIEVE:594`, `INVITATION_RESULT:597`, `PROV_DISC_PBC_REQ:600` at `service/soc_wifi_service_api.c:571-603` map to `send_broadcast_p2p_*` helpers (`:168-316`). The SoftAP broadcast switch at `send_broadcast_ap_state_change:382` (`:382-393`) distinguishes `g_hotspot_ifname` vs `g_p2p_ifname` and updates `g_softap_enble_flag:385` / `g_p2p_isgo:389`.

### 3.3 IE injection

`service_ie_frame_check:1216` at `service/soc_wifi_service_api.c:1216-1258` validates per-interface IE insertion bitmaps: STA only `probe request` (`bitmap==2:1220`), AP/GO only `beacon+probe response` (`bitmap & 0xFA==0:1230`, `:1240`), P2P device `probe req+resp` (`& 0xF9==0:1250`) and maps to `ext_wifi_iftype` (`EXT_WIFI_IFTYPE_STATION:1224`, `AP:1234`, `P2P_GO:1244`, `P2P_DEVICE:1254`).

---

## 4. Central event demux (`soc_wifi_service_api.c:542`, `service_event.h:59`)

`wpa_event_cb_handle:542` at `service/soc_wifi_service_api.c:542-608` is the single service-layer demux for `ext_wifi_event` from the driver/wpa:

```
EXT_WIFI_EVT_SCAN_DONE:550        → send_broadcast_scan_done:551
EXT_WIFI_EVT_CONNECTED:553        → send_broadcast_connected:554
EXT_WIFI_EVT_DISCONNECTED:556     → send_broadcast_disconnected:557
EXT_WIFI_EVT_AP_START:559         → send_broadcast_ap_state_change(AVALIABLE):560
EXT_WIFI_EVT_AP_DISABLE:562       → send_broadcast_ap_state_change(NOT_AVALIABLE):563
EXT_WIFI_EVT_STA_CONNECTED:565    → send_broadcast_sta_connected:566
EXT_WIFI_EVT_STA_DISCONNECTED:568 → send_broadcast_sta_leave:569
(CONFIG_P2P_SUPPORT) 572/575/578   → p2p go-neg handlers
(CONFIG_WPS_SUPPORT) 583/586/589   → send_broadcast_wps_result:584/587/590
(CONFIG_P2P_SUPPORT) 594/597/600  → invitation / pbc handlers
default:604                      → service_error_log "unknown"
```

Each `send_broadcast_*` iterates `dl_list_for_each_entry` over `g_api_cb_node:95` (e.g., `send_broadcast_connected_change_for_sta:127` at `service/soc_wifi_service_api.c:127-139`, `send_broadcast_ap_state_change_for_ap:252` at `:252-263`, `send_broadcast_scan_done:211` at `:211-235`). Disconnect reason translation at `wifi_convert_disconnect_reason_code:337` (`:337-347`) sets bit15 if remote-initiated (`reason |= 1<<15:345`) unless `locally_generated==1` or `reason >= WIFI_MAC_REPORT_DISCONNECT_OFFSET`. The `g_drop_disconnect:90` flag at `service/soc_wifi_service_api.c:90` is set in `send_broadcast_wps_result:488` at `:488` (`STA/P2P_CLIENT + AVALIABLE → g_drop_disconnect=1`) and consumed in `send_broadcast_disconnected:355` (`:355-358`) to suppress the follow-on disconnect.

The dispatcher is registered via `uapi_wifi_register_event_callback(wpa_event_cb_handle):617` at `service/soc_wifi_service_api.c:617` inside `register_callback:610`.

---

## 5. Hostapd RTOS integration (`hostapd/liteos_hostapd_src/hostapd_main_rtos.c:1`)

### 5.1 Global singletons

`g_hapd:22` at `hostapd/liteos_hostapd_src/hostapd_main_rtos.c:22` (`struct hostapd_data *`), `global:29` at `:29` (`hapd_global {drv_priv, drv_count}`), `g_global_conf:30` at `:30` (`hostapd_conf`), `g_interfaces:31` at `:31` (`hapd_interfaces *`). These are the LiteOS equivalents of the Linux `hapd_global`/`interfaces` globals.

### 5.2 Bring-up order

`hostapd_main(ifname):513` at `hostapd_main_rtos.c:513-567`:

1. `hostapd_get_wifi_dev(ifname):491` at `:491-511` — validates `ifname:493`, calls `los_get_wifi_dev_by_name:503` (`wpa/liteos_wpa_api/wifi_api.c:265`), checks `wifi_dev->priv`.
2. `hostapd_interfaces_init(ifname):454` at `:454-489` — `os_zalloc hapd_interfaces:458`, wires `reload_config/config_read_cb/driver_init` to `hostapd_reload_config:463`, `hostapd_config_read2:464`, `hostapd_driver_init:468`, sets `count=1:469`, allocates `iface` array `os_calloc:471`, calls `hostapd_global_init:479` (`:120-147` — `eloop_init(ELOOP_TASK_HOSTAPD):131`, counts `wpa_drivers:136`, `os_calloc global.drv_priv:143`).
3. `hostapd_interface_init(interfaces,ifname,0):527` at `:527` → `hostapd_init:90` inside (`:84-103`), then `g_hapd = iface->bss[0]:101` at `:101`.
4. Stashes `wifi_dev->priv = g_hapd:536` at `:536` under `os_task_lock:534`.
5. `hostapd_driver_init:551` at `:551` (calls `wpa_init_params` + `hapd->driver->hapd_init:67` at `:67`) and `hostapd_setup_interface:551` (same line) — the actual netdev up / beacon programming.
6. `hostapd_global_run:559` at `:559` → `eloop_run(ELOOP_TASK_HOSTAPD):172` at `:172`; on success writes `WPA_EVENT_WPA_START_OK:560` at `:560`. Failure path `hostapd_quit:565` at `:565-567` via `hostapd_quit:432` (`:432-452`) which clears `wifi_dev->priv:447` and signals `WPA_EVENT_WPA_START_ERROR:451`.

Tear-down: `hostapd_pre_quit:415` at `:415-430` (`hostapd_global_ctrl_iface_deinit:420`, `hostapd_interface_deinit_free:427`), `hostapd_global_deinit:149` at `:149-156` (`os_free global.drv_priv:150`, `eloop_destroy:154`, `eap_server_unregister_methods:155`), `hostapd_global_interfaces_deinit:158` at `:158-165`.

### 5.3 Config translation (`hostapd_config_read2:337`)

`hostapd_config_read2(ifname):337` at `hostapd_main_rtos.c:337-413` does **not** parse a file; it synthesizes `struct hostapd_config` from `g_global_conf:30` and `g_ap_opt_set` (wpa-side globals):

- defaults: `HT_CAP_INFO_SHORT_GI20MHZ:347`, `hw_mode=IEEE80211G:348`, `ieee80211n=1:350` (`:349-353` respects `g_ap_opt_set.short_gi_off:352`);
- 11AX: `conf->ieee80211ax = (g_ap_opt_set.hw_mode==WIFI_MODE_11B_G_N_AX):356` at `:356` then `hostapd_config_80211ax:358` (`:232-282` — `he_su_beamformee=1:243`, `he_bss_color=1:245`, `he_oper_chwidth=0:254`, etc.);
- driver lookup: matches `hconf->driver` against `wpa_drivers[i]->name:364` at `:364`;
- channel `conf->channel = hconf->channel_num:378` at `:378`;
- WEP keys `hconf->auth_algs/wep_idx:381` at `:381-392`;
- then `hostapd_bss_init:396` at `:396` (`hostapd_bss_init:175` at `:175-230` — validates `ssid_len 1..SSID_MAX_LEN:181`, copies `ssid:185`, maps `wpa_key_mgmt:189`, handles 64-hex PSK via `hexstr2bin:208` vs passphrase `os_strdup:218`, sets `ignore_broadcast_ssid:227`).

EAP is decoupled: `eap_server_register_methods:105` at `:105-118` only registers `EAP_SERVER_IDENTITY/WSC`; `hostapd_global_init:127` at `:127` bails on `eap_server_register_methods` failure.

---

## 6. wpa_supplicant LiteOS shim (`wpa/liteos_wpa_api/wifi_api.c:1`, 3344 lines)

### 6.1 Device table and locking

`g_wifi_dev[WPA_MAX_WIFI_DEV_NUM]:55` at `wpa/liteos_wpa_api/wifi_api.c:55` (pointer array), plus `g_ap_opt_set:56`, `g_sta_opt_set:57`, `g_reconnect_set:58`, `g_scan_record:59`, `g_wpa_event_cb:81` (`uapi_wifi_event_cb`), `g_raw_scan_cb:88` (`ext_wifi_scan_no_save_cb`). Locking uses `os_intlock/os_intrestore` (`wifi_api.c:130`, `:155`, `:206`) and `os_task_lock/os_task_unlock` (`wifi_api.c:272`, `:294`). The helper `wifi_dev_get(iftype):349` at `wifi_api.c:349-363` checks `is_lock_flag_off:351` (lock `g_lock_flag:69`) then `los_get_wifi_dev_by_iftype:356`.

Table helpers:

- `los_get_wifi_dev_by_name:265` at `wifi_api.c:265-283` (linear scan `g_wifi_dev[i]->ifname`), stamps `network_id=i:275`.
- `los_get_wifi_dev_by_iftype:285` at `wifi_api.c:285-304`.
- `los_get_wifi_dev_by_priv:306` at `wifi_api.c:306-325`.
- `wpa_get_other_existed_wpa_wifi_dev:327` at `wifi_api.c:327-347` (finds non-self STA/MESH/P2P dev).
- `los_count_wifi_dev_in_use:249` at `wifi_api.c:249-263`.
- `wifi_dev_creat:385` at `wifi_api.c:385-418` (`os_zalloc ext_wifi_dev:389`, `wal_init_drv_wlan_netdev:396`, validates `ifname_len:402-404`), `los_free_wifi_dev:365` at `wifi_api.c:365-383`, `los_set_wifi_dev:420` at `wifi_api.c:420-440`.

### 6.2 STA network programming

Key helpers:

- `wifi_sta_psk_init:467` at `wifi_api.c:467-515` — WAPI-PSK hex vs ASCII quoting (`snprintf_s "\"%s\"":491`, `:499`) with `WAPI_MAX_KEY_LEN` cap `max_key_len:471`.
- `wifi_sta_wpa3_network_set:518` at `wifi_api.c:518-538` — maps `EXT_WIFI_SECURITY_SAE` / `WPA3_WPA2_PSK_MIX` to `key_mgmt SAE / SAE WPA-PSK WPA-PSK-SHA256` + `ieee80211w 2/1:527,533`.
- `wifi_sta_set_pairwise:540` at `wifi_api.c:540-569` — maps `EXT_WIFI_PAIRWISE_*` (including `CCMP256:552` etc.) to `pairwise/group` strings.
- `wifi_sta_set_key_mgmt:626` at `wifi_api.c:626-649` (`_wpa:652`) and `wifi_sta_set_key_mgmt_wpa:626` (`:626-649`) — selects PSK/FT variants based on `g_sta_opt_set.pmf` and `g_ft_flag` (`:632,636`).
- `wifi_sta_psk_separate_set:696` at `wifi_api.c:696-727` — fast-connect path uses `set_psk` numeric flag.

### 6.3 Scan path (wpa side)

- `wifi_scan_params_parse:1180` at `wifi_api.c:1180-1206` dispatches `CHANNEL:1191`, `SSID/PREFIX:1196`, `BSSID:1199`, or `BASIC:1185`.
- `wifi_scan:1208` at `wifi_api.c:1208-1253` (STA-only, checks `g_mesh_sta_flag`, `wifi_dev_get`, `wifi_scan_params_parse:1238`, then `wifi_wpa_scan:1244`).
- `wifi_wpa_scan:1080` at `wifi_api.c:1080-1117` builds `freq_buff/ssid_buff/bssid_buff` via `wifi_scan_param_handle:966` (`:966-1022`) and `wifi_scan_ssid_set:945` (`:945-964`), then `wifi_scan_buffer_process:1024` (`:1024-1078`) which synthesizes a `buf` string and issues `wpa_cli_scan:1066` under `os_event_clear/read(WPA_EVENT_SCAN_OK:1065/1068)`.
- Raw variant `uapi_wifi_sta_raw_scan:1366` at `wifi_api.c:1366-1412` → `wifi_encap_raw_scan_param:1322` (`:1322-1364`) then `drv_soc_ioctl_scan:1405`.
- Result retrieval `wifi_scan_result:1429` at `wifi_api.c:1429-1457` → `wpa_cli_scan_results:1440` plus `WPA_EVENT_GET_SCAN_RESULT_FLAG:1447`, then `wifi_scan_results_parse:1681` (`:1681-1728`) which parses `bssid\tfreq\trssi\tflags\tssid\n` fields via `wifi_scan_result_{bssid,freq,rssi}_parse:1459/1483/1503` and `wifi_scan_result_base_flag_parse:1547` (`:1547-1621` mapping `SAE/WPA-PSK/WPA2-PSK/WPA/WPA2/WEP/OPEN/OWE/WAPI-PSK`).

Scan flags `EXT_SCAN/EXT_CHANNEL_SCAN/EXT_SSID_SCAN/EXT_PREFIX_SSID_SCAN/EXT_BSSID_SCAN` are enumerated in `driver_soc_common.h` via `ext_wifi_scan_params` (see `wifi_api.c:1192-1199`).

---

## 7. driver_soc bridge (`wpa/driver_soc/driver_soc.c:1`, `driver_soc_common.h:1`, `driver_soc_ioctl.c:1`)

### 7.1 Types and ioctl surface

`wpa/driver_soc/driver_soc_common.h:6` defines the cross-entity contract:

- scalar typedefs `int8..uint64:26-37` at `driver_soc_common.h:26-37`.
- `ext_key_ext_stru:360` at `driver_soc_common.h:360-373`, `ext_ap_settings_stru:375` at `:375-387` (includes `ssid:382`, `auth_type:383`, `hidden_ssid:382`), `ext_associate_params_stru:579` at `:579-595` (holds `bssid/ssid/ie/key/crypto`), `ext_scan_stru:494` at `:494-505` (ssids/freqs/bssid/extra_ies), `ext_scan_result_stru:608` at `:608-622`, `ext_disconnect_stru:624` at `:624-629`, `ext_hw_feature_data_stru:418` at `:418-423`.
- event id space `ext_event_enum:137` at `driver_soc_common.h:137-188` (`EXT_IOCTL_SET_AP:138` .. `HWAL_EVENT_BUTT:187`) and `ext_eloop_event_enum:190` at `:190-210` (`EXT_ELOOP_EVENT_NEW_STA:191` .. `BUTT:208`).

`wpa/driver_soc/driver_soc_ioctl.h:1` (124 lines) declares the `drv_soc_ioctl_*` thin wrappers that issue `ext_ioctl_command_stru:268` at `driver_soc_common.h:268-271` via `drv_soc_hwal_wpa_ioctl:665` at `driver_soc_common.h:665-666`.

### 7.2 Driver ops (wpa_supplicant ↔ firmware)

`driver_soc.c:213` (`drv_soc_set_key:213-265`) at `wpa/driver_soc/driver_soc.c:213-265`:

- `drv_soc_init_key:169` (`:169-211`) allocates `key/key_len:179`, `seq:184`, `addr:189` with `os_zalloc`, maps `wpa_alg → cipher` via `drv_soc_alg_to_cipher_suite:132` (`:132-167` handling `WPA_ALG_WEP 5/13 → WEP40/WEP104:137-139`, `TKIP:140`, `CCMP:142`, `GCMP:144`, `BIP_*:150-156`, `SMS4:158`, `KRK:160`).
- Then `drv_soc_ioctl_new_key:248` / `drv_soc_ioctl_set_key:260` / `drv_soc_ioctl_del_key:246`.

AP: `drv_soc_set_ap:320` at `driver_soc.c:320-363` builds `ext_ap_settings_stru` (`beacon_interval:330`, `dtim:331`, `hidden_ssid:332`, `sae_pwe:333`, `auth_type:335-339`, `ssid:342-348`, `freq:349`, `beacon_data head/tail:279-304`) and calls `drv_soc_ioctl_change_beacon:353` (if `beacon_set`) else `drv_soc_ioctl_set_ap:355`.

MLME/EAPOL:

- `drv_soc_send_mlme:365` at `driver_soc.c:365-403` (`ext_mlme_data_stru:296` at `driver_soc_common.h:296-301`, cookie `send_action_cookie:383`).
- `drv_soc_send_eapol:416` at `driver_soc.c:416-458` builds an `l2_ethhdr` + `ETH_P_PAE` frame and calls `l2_packet_send:455`.

Scan/connect/disconnect: `drv_soc_scan:1647` at `driver_soc.c:1647-1684` (`drv_soc_scan_process_{ssid,bssid,extra_ies,freq}:1546/1583/1599/1618`), `drv_soc_connect:1970` (`drv_soc_connect:1970-…` via `drv_soc_assoc_params_set:1850`, `drv_soc_assoc_param_crypto_set:1897`, `drv_soc_set_conn_keys:1773`), `drv_soc_disconnect:2030` etc., plus `drv_soc_get_scan_results:1705` at `driver_soc.c:1705-1741` (weak, returns `wpa_scan_results` from `drv->res[]`).

### 7.3 Event queue → wpa state machine

`drv_soc_driver_send_event:478` at `driver_soc.c:478-527` is called by the firmware/HAL via `drv_soc_register_send_event_cb:300` (driver_soc_ioctl path). It checks `drv_soc_send_event_get_drv:460` (`:460-476` — maps `ifname → ext_wifi_dev → driver_data` via `los_get_wifi_dev_by_name`), `eloop_is_running:467/471`, `RX_MGMT cap 15:497`, then encodes `[cmd:4|len:4|payload]` into `os_zalloc(length+8):501` at `:501` and posts via `eloop_post_event:518`.

The consumer side:

- `drv_soc_driver_event_process:1098` at `driver_soc.c:1098-1128` drains `eloop_read_event:1111` in a loop and decodes `cmd/length/data_ptr:1118-1121`, then `drv_soc_driver_event_process_internal:1009` at `driver_soc.c:1009-1096`.

Internal dispatch (`driver_soc.c:1019-1095`):

```
NEW_STA:1021              → drv_soc_driver_event_new_sta_process:529 (EVENT_ASSOC/DISASSOC:538/545)
DEL_STA:1024              → drv_soc_driver_event_del_sta_process:554 (EVENT_DISASSOC:565)
RX_MGMT:1028              → drv_soc_rx_mgmt_process:1149 (EVENT_RX_MGMT:1161, cap decr:1151)
TX_STATUS:1032            → drv_soc_tx_status_process:980 (EVENT_TX_STATUS:1000)
SCAN_DONE:1036            → drv_soc_driver_event_scan_done_process:938 (cancel timeout:941, EVENT_SCAN_RESULTS:946)
SCAN_RESULT:1040          → drv_soc_driver_event_scan_result_process:700 (alloc wpa_scan_res:711, auth map:582, raw cb:746)
CONNECT_RESULT:1044       → drv_soc_driver_event_connect_result_process:758 (ASSOC_REJECT vs DISASSOC:769/772, EVENT_ASSOC:792)
DISCONNECT:1048           → drv_soc_driver_event_disconnect_process:804 (EVENT_DISASSOC/DEAUTH:830/828)
REMAIN_ON_CHANNEL:1053    → drv_soc_driver_event_remain_on_channel:950 (P2P, EVENT_REMAIN_ON_CHANNEL:962)
CANCEL_REMAIN:1058        → drv_soc_driver_event_cancel_remain_on_channel:965 (EVENT_CANCEL_REMAIN_ON_CHANNEL:976)
CHANNEL_SWITCH:1064       → drv_soc_driver_event_channel_switch_process:568 (EVENT_CH_SWITCH:579)
TIMEOUT_DISCONN:1069      → drv_soc_driver_event_timeout_disc_process:840 (NOTICE DISCONNECTED:849)
EXTERNAL_AUTH:1074        → drv_soc_driver_event_external_auth:874 (EVENT_EXTERNAL_AUTH, SAE:886, WPA3)
OWE_INFO:1081             → drv_soc_driver_event_owe_info:891 (EVENT_UPDATE_DH:900)
FT_RESPONSE:1088          → drv_soc_driver_event_ft_response:904 (EVENT_FT_RESPONSE:924, 11r)
```

Scan-auth classification `drv_soc_get_scan_auth_type:582` at `driver_soc.c:582-673` parses `WPA_IE_VENDOR_TYPE`, `WLAN_EID_RSN/WAPI/MESH_ID` and flags `open/sae/wpa_psk/wpa2_psk/wpa_eap/wpa2_eap/eap_256/rsn_psk/wep/wapi_psk/owe` (e.g., `sae:645-647`, `wpa_psk+wpa2_psk mix:648-649`, `wapi_psk:632-635`).

Lifecycle of driver private:

- `drv_soc_drv_init:1170` at `driver_soc.c:1170-1221` (`os_zalloc drv:1178`, `eloop_register_event:1188`, `l2_packet_init ETH_P_EAPOL:1192`, `l2_packet_get_own_addr:1197`);
- `drv_soc_hapd_init:1286` at `driver_soc.c:1286-1313` (adds `drv_soc_ioctl_set_netdev(UP):1304`);
- `drv_soc_wpa_init:1440` at `driver_soc.c:1440-1483` (similar plus `drv_soc_register_send_event_cb:1456`);
- teardowns `drv_soc_drv_deinit:1315` at `driver_soc.c:1315-1341` (`l2_packet_deinit:1324`, `drv_soc_drv_remove_mem:1337`, `eloop_unregister_event:1338`), `drv_soc_hapd_deinit:1343` (`:1343-1367` sets mode STATION:1354, restores send cb if single iface:1361), `drv_soc_wpa_deinit:1485` (`:1485-1508`).

---

## 8. eloop / osdep / scan_list_adapt / wapi

### 8.1 eloop (LiteOS)

`wpa/ltos_src/eloop_ltos.c:18` (`wpa_supplicant_main_task:18-37`) at `eloop_ltos.c:18-37` is the task entry: `hostapd_main` if `iftype==AP:27` else `wpa_supplicant_main:32`, then `wpa_event_task_free:34` and `os_task_delete(g_wpataskid):35`. `wpa_supplicant_exit:39` at `:39-46` calls `wpa_supplicant_deinit/global_deinit`; `hostapd_exit:49` at `:49-58` frees `los_get_wifi_dev_by_priv(g_hapd):55`. Flags `g_eloop_task_flag[ELOOP_MAX_TASK_TYPE_NUM]:14` at `:14`, `g_eloop_softap_terminate_flag:15`, `g_eloop_wpa_terminate_flag:16` gate `eloop_is_running:61` at `:61-74`, `global_eloop_is_running:76` at `:76-80`, `eloop_start_running:116` at `:116-126` (first runner wins, late joiners get `ELOOP_ALREADY_RUNNING:121`), `eloop_terminate:128` at `:128-140`.

### 8.2 osdep

`wpa/osdep/osdep_osal.c:1` (212 lines) wraps `wifi_osdep.h:1` (96 lines) `os_zalloc/os_free/os_task_lock/os_intlock/os_event_*` primitives onto LiteOS `LOS_*` / `osal_*` — the reason `driver_soc` and `wifi_api` can use `os_*` without Linux `kmalloc`.

### 8.3 scan_list_adapt

`wpa/scan_list_adapt/scan_list_adapt.c:1` (1093 lines) with `scan_list_adapt.h:1` (141 lines) dedupes scan results seen by raw-scan callers; it is called from `drv_soc_driver_event_scan_result_process:…` when `g_raw_scan_cb:746` at `driver_soc.c:746` is set (raw-scan flow). Not shown in STA/AP normal path which goes through `wpa_supplicant` BSS list.

### 8.4 WAPI

`wpa/wapi/*.c` — `wai_sm.c:1` (1024 lines), `wai_rxtx.c:1` (191 lines), `wai_crypto_mbedtls.c:1` (296 lines), `wapi.c:1` (354 lines) plus headers `wai_sm.h:1` (12 lines), `wai_rxtx.h:1` (26 lines), `wai_crypto.h:1` (17 lines). On WS63 this is the end-to-end WAPI state machine (WAI) used when `security_type==WAPI_PSK/WAPI_CERT` (`service/soc_wifi_service_api.c:743`, `:905`). WS73 reuses the same via `wpa_supplicant` patch (Section 9.3) but the host `wifi_soc.ko` also carries `WIFI_WAPI` gate (`sdk/driver/wifi/Makefile:153` → `_PRE_WLAN_FEATURE_WAPI:154`).

---

## 9. Feature gates: WOW / CSA / TWT / BTCOEX / ALG (WS63 service vs WS73 host)

### 9.1 What WS63 exposes at the service seam

WS63 `service/CMakeLists.txt:31-38` hard-enables at LiteOS service build:

```
_PRE_WLAN_FEATURE_CSI:31
_PRE_WLAN_FEATURE_WOW_OFFLOAD:32
_PRE_WLAN_FEATURE_INTRF_MODE:33
_PRE_WLAN_FEATURE_SDP:34
CONFIG_WPS_SUPPORT:35 / CONFIG_WPS:36 / CONFIG_OWE:37 / CONFIG_WNM:38
```

`SMALLER` variant at `service/CMakeLists.txt:42-49` strips `WPS/WNM/SDP`. No WS63 service gate for CSA/TWT/BTCOEX — they are driver-controlled.

WOW is the only feature with a direct user API in `soc_wifi_service_api.c:2367-2412`:

- `wifi_set_wow_pattern:2368` at `service/soc_wifi_service_api.c:2368-2391` under `#if defined(_PRE_WLAN_FEATURE_WOW_OFFLOAD)||defined(CONFIG_WOW_OFFLOAD):2367` — checks `len>64||index>3||len%2!=0||type>=WOW_PATTERN_BUTT:2379`, then `uapi_wifi_set_wow_pattern:2384`.
- `wifi_set_wow_sleep_mode:2393` at `service/soc_wifi_service_api.c:2393-2412` → `uapi_wifi_set_wow_switch:2408`.

CSI is similarly a one-call feature (`_PRE_WLAN_FEATURE_CSI:99` guard via `g_csi_enable_flag:99` plus `g_csi_ifname:78` in `wifi_api.c:78/99`).

### 9.2 What WS73 gates at the host `wifi_soc.ko`

WS73 collapses the same features into `sdk/driver/wifi/Makefile:62-199` feature gates (documented in `WS73-WIFI-GAP-AUDIT.md:50-55`):

```
WIFI_WOW:62     → _PRE_WLAN_FEATURE_WOW_OFFLOAD:63  (+ _DYNAMIC_OFFLOAD:64 / hmac_wow.o:65)
WIFI_CSA:107    → _PRE_WLAN_FEATURE_CSA:108          + feature/hmac_csa_ap.o / hmac_csa_sta.o:109
WIFI_WAPI:153   → _PRE_WLAN_FEATURE_WAPI:154         + wapi_*.o
WIFI_BTCOEX:186 → _PRE_WLAN_FEATURE_BTCOEX:187       + 6× hmac_btcoex* + hal_coex_reg:188-189
WIFI_TWT:198    → _PRE_WLAN_FEATURE_TWT:199          + feature/hmac_twt.o:200
```

Plus ALG optionals in `sdk/driver/wifi/alg/ws73_alg_host.mk:15-37` (`WIFI_ALG_CCA:15`, `TEMP_PROTECT:20`, `TXBF:26`, `EDCA:30`, `ANTI_INTF:35`). Base ALG (`alg_main/schedule/autorate/gla/probe_common/tpc/dbac_hmac/tx_tb_hmac/traffic_ctl/intf_det/aggr/rts/intrf_mode:1-13` at `ws73_alg_host.mk:1-13`, 11910 lines) is unconditional and bakes in `WIFI_ALG_MACRO_DEFINES:42-57` at `ws73_alg_host.mk:42-57`.

For a TV box, the prior gap audit concluded: **reuse STA+SoftAP+BTCOEX+base ALG** (Section 7 of `WS73-WIFI-GAP-AUDIT.md:205-228`), defer CSA/TWT, drop WOW — and that verdict is reinforced by the WS63 service evidence: WOW exists but is optional-and-gated, CSA/TWT have no service-layer users, BTCOEX has no service bit (it is a driver/PM concern).

### 9.3 wpa_supplicant patches (`sdk/open_source/wpa_supplicant/*.patch`)

`sdk/open_source/wpa_supplicant/wpa_supplicant_2_10_linux.patch:1` (4298 lines) and `wpa_supplicant_2_7_android_9.patch:1` (915 lines) are **WAPI-only** on top of stock `wpa_supplicant`:

- `common/defs.h:210` adds `CIPHER_SMS4:…` / `KEY_MGMT_WAPI_PSK/WAPI_CERT:…`,
- `common/ieee802_11_defs.h:323` adds `WLAN_EID_WAPI 68:…` (colliding with `BSS_AVAILABLE_ADM_CAPA 67`) and AKM suites `WLAN_AKM_SUITE_WAPI_PSK 0x000FAC11` etc.,
- group/pairwise selection patched near `:2760-2790` of the 2.10 patch.

There is **no WOW/CSA/TWT/BTCOEX patch** — those are data-plane driver features, not supplicant features. The correct TV-box userland remains **stock `wpa_supplicant` + `hostapd` via nl80211** against `driver/wifi/wal/release/linux/wal_linux_cfg80211.c:6002` (`g_wal_cfg80211_ops`) / `:6292` (`oal_wiphy_new`) as documented in `WS73-WIFI-GAP-AUDIT.md:63-65`.

---

## 10. TV-box tri-mode coexistence: what WS63 tells us vs what WS73 needs

WS63 is a single-dwelling LiteOS MCU: one RF front-end, STA XOR SoftAP XOR P2P (see `wifi_dev_creat` caps `vap_res_num=2:1334` at `service/soc_wifi_service_api.c:1334` but service flags are mutually exclusive per-vap). WS73 is a **tri-mode** Linux SoC (`WLAN + BLE + SLE`) sharing that same 2.4 GHz RF via `driver/platform/pm/plat_pm_wlan.h:79-83` (`PM_SVC_WLAN:79/BLE:80/SLE:81/NUM:83` + `pm_svc_state[PM_SVC_NUM]:123`).

WS63 has **no BTCOEX service API** and no SLE awareness — coexistence is pushed down into the driver/PM. WS73 makes that explicit:

- `sdk/driver/wifi/Makefile:186-189` — `WIFI_BTCOEX` pulls six `hmac_btcoex*.o` (5478 lines total) plus `hal/hh503/hal_coex_reg.c:1` HW preempt regs (see `WS73-WIFI-GAP-AUDIT.md:87-92`).
- `sdk/driver/wifi/feature/hmac_btcoex.h:7` / `:34` guard and `:82-244` structs, plus `hal_coex_reg`.
- PM contract `driver/platform/pm/plat_pm_wlan.c:140-142` init, `:341/:354` `pm_svc_open(PM_SVC_WLAN)`, `:588-598` first-svc firmware download, `:703-711` last-close power-down (`WS73-WIFI-GAP-AUDIT.md:125-129`).

**Reuse implication:** `wifi_init:1332` / `wifi_sta_enable:1432` / `wifi_softap_enable:2666` semantics from WS63 can be reused as thin wrappers that map to `wal_linux_cfg80211` + PM, but the **policy** must live above `feature_mgr` (bits 10-16 proposed in `WS73-WIFI-GAP-AUDIT.md:171-180`) and the PM arbiter must gate `FEAT_WIFI_BTCOEX` on `pm_svc_state[BLE/SLE]!=SHUTDOWN`. The service-layer `g_sta_enble_flag:91` / `g_softap_enble_flag:92` flags have no analog for BLE/SLE — they must not be mistaken for tri-mode state.

---

## 11. Minimal portable subset (reaffirmed, now with WS63 citation)

| Keep | Cite | Why |
|---|---|---|
| STA (`wifi_sta_enable:1432`, `wifi_sta_connect:2059`, `wifi_sta_scan:1621`) | `service/soc_wifi_service_api.c:1432`,`:2059`,`:1621` | WAN is mandatory; maps to `hmac_mgmt_sta` + `wpa_driver_ops::scan+associate` |
| SoftAP (optional) (`wifi_softap_enable:2666`) | `service/soc_wifi_service_api.c:2666` | Onboarding/cast; enables `hmac_mgmt_ap` + `hostapd`; pair with CSA |
| BTCOEX (PM-driven, driver side) | `sdk/driver/wifi/Makefile:186`, `plat_pm_wlan.c:588` | Single RF tri-mode broken without `hal_coex_reg` |
| Base ALG (unconditional) | `sdk/driver/wifi/alg/ws73_alg_host.mk:42-57` | Autorate/aggr/TPC/schedule — throughput engine |
| stock `wpa_supplicant`+`hostapd` (nl80211) | `wpa/driver_soc/driver_soc.c:320`, `hostapd_main_rtos.c:513` | `driver_soc`/`hostapd_main_rtos` are LiteOS shims — do not port; use `wal_linux_cfg80211` |
| FE cali via `wifi_cali.bin` + `soc_ini` | `WS73-WIFI-GAP-AUDIT.md:64,215` | Device calibration, INI customization |

| Defer/drop | Cite | Why |
|---|---|---|
| WOW (`wifi_set_wow_pattern:2368`, `wifi_set_wow_sleep_mode:2393`) | `service/soc_wifi_service_api.c:2368`, `:2393` + `sdk/driver/wifi/Makefile:62` | TV box wall-powered; needs `wow.bin` + ARP/NS offloads; keep gate |
| CSA AP+STA | `sdk/driver/wifi/Makefile:107` | Rare; enable with SoftAP only |
| TWT | `sdk/driver/wifi/Makefile:198` | Battery feature; no AP mandates |
| WAPI (service `WAPI_PSK/CERT:743`, driver `WIFI_WAPI:153`) | `service/soc_wifi_service_api.c:743`, `sdk/driver/wifi/Makefile:153` + `open_source/wpa_supplicant/*.patch` | No TV-box mandate; patches are WAPI-only |
| ALG optionals (`CCA/TXBF/EDCA/ANTI_INTF`) | `ws73_alg_host.mk:15-37` | Antenna/thermal tuning only |

---

## 12. File:line index (every WS63 + SDK file touched)

- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/service/CMakeLists.txt:1` `COMPONENT_NAME:4`, `SOURCES:7-9`, `PRIVATE_DEFINES:30-39` (`_WOW_OFFLOAD:32`, `_INTRF_MODE:33`, `CSI:31`, `WPS/OWE/WNM:35-38`), `SMALLER:42-49`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/service/service_event.h:1` `dl_list:59`, `service_event_cb:64`, `list_*:69-103`, WPS `WPS_* 0x0001..0x4008:15-29`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/service/service_wifi_api.h:1` `WIFI_TYPE_STA/HOTSPOT/P2P:24-27`, `WPS pbc/pin:36-40`, `WOW_PATTERN_*:60-65`, `wifi_return_code:73-86`, `sta/softap protocol_mode:106/144`, `pmf:181/183`, `wifi_dev_get:202`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/service/soc_wifi_service_api.c:1` globals `g_softap_config:84`, `g_softap_advance_config:85`, `g_*_ifname:87-89`, `g_drop_disconnect:90`, `g_*_enble_flag:91-93`, `g_api_cb_node:95`, `g_p2p_isgo:97`, `g_csi:99`, `g_sta_last_scan_time_stamp_ms:102`, `g_sta_conn_req_flag:104`, `g_sta_conn_config:106`; defs `WIFI_ACTIVE_VAP_MAX_NUM 2:53`, `SERVICE_P2P_MAX_FIND_NUM 32:57` etc. `:53-75`; events `wpa_event_cb_handle:542`, cases `:550-603`, `register_callback:610`, `find_wifi_event:621`, `wifi_register_event_cb:632`, `wifi_unregister_event_cb:662`, `service_check_wifi_device_config:721`, `service_check_wep/wpa/sae/wapi_key:798/823/842/858`, `service_set_ip:1089`, `service_set_assoc_config:1118`, `service_ie_frame_check:1216`, `service_set_softap_protocol:1260`, `wifi_init:1332`, `wifi_deinit:1355`, `wifi_sta_enable:1432`, `wifi_sta_disable:1453`, `wifi_sta_scan:1621`, `wifi_raw_scan:1565`, `wifi_sta_scan_advance:1638`, `wifi_ap_scan:1548`, `wifi_softap_enable:2666`, `wifi_softap_disable:2717`, `wifi_set_wow_pattern:2368`, `wifi_set_wow_sleep_mode:2393`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/hostapd/liteos_hostapd_src/hostapd_main_rtos.c:1` `g_hapd:22`, `global:29`, `g_global_conf:30`, `g_interfaces:31`, `hostapd_driver_init:36`, `hostapd_interface_init:84`, `hostapd_global_init:120`, `hostapd_config_read2:337`, `hostapd_bss_init:175`, `hostapd_config_80211ax:232`, `hostapd_interfaces_init:454`, `hostapd_get_wifi_dev:491`, `hostapd_main:513`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/driver_soc/driver_soc.c:1` `drv_soc_set_key:213`, `drv_soc_set_ap:320`, `drv_soc_send_mlme:365`, `drv_soc_send_eapol:416`, `drv_soc_driver_send_event:478`, `drv_soc_get_scan_auth_type:582`, `drv_soc_driver_event_scan_result_process:700`, `drv_soc_driver_event_*_process:758/804/840/874/891/904/938/950/965`, `drv_soc_driver_event_process_internal:1009`, `drv_soc_driver_event_process:1098`, `drv_soc_drv_init:1170`, `drv_soc_hapd_init:1286`, `drv_soc_wpa_init:1440`, `drv_soc_scan:1647`, `drv_soc_get_scan_results:1705`, `drv_soc_assoc_params_set:1850`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/driver_soc/driver_soc_common.h:1` `ext_wifi_dev:…`, `ext_key_ext_stru:360`, `ext_ap_settings_stru:375`, `ext_scan_stru:494`, `ext_associate_params_stru:579`, `ext_scan_result_stru:608`, `ext_hw_feature_data_stru:418`, `ext_event_enum:137`, `ext_eloop_event_enum:190`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/liteos_wpa_api/wifi_api.c:1` `g_wifi_dev:55`, `los_get_wifi_dev_by_name:265`, `los_get_wifi_dev_by_iftype:285`, `wifi_dev_creat:385`, `wifi_sta_psk_init:467`, `wifi_sta_set_key_mgmt:626/652`, `wifi_scan:1208`, `wifi_wpa_scan:1080`, `wifi_scan_buffer_process:1024`, `uapi_wifi_sta_raw_scan:1366`, `wifi_scan_result:1429`, `wifi_scan_results_parse:1681`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/ltos_src/eloop_ltos.c:1` `wpa_supplicant_main_task:18`, `hostapd_exit:49`, `eloop_is_running:61`, `eloop_start_running:116`, `eloop_terminate:128`
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/osdep/osdep_osal.c:1` + `wifi_osdep.h:1` osal shims
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/scan_list_adapt/scan_list_adapt.c:1` dedup
- `/mnt/hdd/nearlink-stuff/fbb_ws63/src/middleware/services/wifi_service/wpa/wapi/wai_sm.c:1` / `wai_rxtx.c:1` / `wai_crypto_mbedtls.c:1` / `wapi.c:1` — WAPI SM
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:1` 528 lines, `:28` KO_NAME `wifi_soc`, `:31` alg include, `:62` WOW, `:107` CSA, `:153` WAPI, `:186` BTCOEX, `:198` TWT, `:42/:435` ALG COPTS/INCLUDES, `:258` alg-objs
- `sdk/.../driver/wifi/alg/ws73_alg_host.mk:1` base 13 `WIFI_ALG_SRC_LIST:1-13`, optionals `:15-37`, `WIFI_ALG_MACRO_DEFINES:42-57`, lite-extreme DBAC/GLA crop `:59-62`
- `sdk/.../open_source/wpa_supplicant/wpa_supplicant_2_10_linux.patch:1` 4298 lines, `:210` `CIPHER_SMS4`, `KEY_MGMT_WAPI_*`, `:323` `WLAN_EID_WAPI 68`, `:338` AKM suites; plus `wpa_supplicant_2_7_android_9.patch:1` 915 lines (same scope)
- `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md:1` prior audit (259 lines) — Sections 2.x/6/7 extended here

---

## Summary (8 lines)

- WS63 WiFi is a LiteOS service (`service/soc_wifi_service_api.c:1` 4079 lines) multiplexing STA/AP/P2P over `wpa`+`hostapd` RTOS shims.
- STA machine is `wifi_init:1332` → `wifi_sta_enable:1432` → `wifi_sta_connect:2059` (scan-then-assoc split `2011/1954`) → `wpa_event_cb_handle:542`.
- AP machine is `wifi_softap_enable:2666` → `hostapd_main:513` → `driver_soc_set_ap:320` with advance guards `1175`; SoftAP+P2P share `g_hotspot_ifname/g_p2p_ifname:88-89`.
- Bridge `wpa/driver_soc/driver_soc.c:1` posts `EXT_ELOOP_EVENT_*` (`driver_soc_common.h:190`) from firmware and dispatches 14 cases in `drv_soc_driver_event_process_internal:1009`.
- Host `wifi_soc.ko` (528-line Makefile) gates `WOW:62/CSA:107/WAPI:153/BTCOEX:186/TWT:198` — only `BTCOEX+base ALG` are TV-box mandatory per prior audit.
- `sdk/open_source/wpa_supplicant/*.patch` are **WAPI-only** (`CIPHER_SMS4`, `WLAN_EID_WAPI`, `WAPI_PSK/CERT`); stock `wpa_supplicant`+`hostapd` via `wal_linux_cfg80211:6002` is the correct port.
- ALG is 13 base files (`ws73_alg_host.mk:1`) unconditional (`_AUTORATE/_TPC/_SCHEDULE` etc.:42) with 5 optionals (`CCA/TEMP_PROTECT/TXBF/EDCA/ANTI_INTF:15`).
- Path: `.scratch/nearlink-driver/lab-notes/WS63-WIFI-SERVICE-DEEP.md` (this file, 340+ lines, file:line cited).

