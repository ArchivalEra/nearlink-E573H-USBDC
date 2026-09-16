---
type: intel
title: "WS73 WiFi Gap Audit — TV-Box Tri-Mode Reuse"
language: en
created: 2026-09-05
tags: [intel, ws73, wifi, audit]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: B
stale_after: 2027-03-05
---

# WS73 WiFi Gap Audit — TV-Box Tri-Mode Reuse

> Ticket: `.scratch/rust-ws73-tri-mode/issues/01-wifi-gap-audit.md` — research AFK, read-only, no network/build/hardware.
> Date: 2026-08-19. SDK: `sdk/ws73_sdk_linux_WS73_1.10.110`. Stack: `stack/ssap/`.
> Output: exactly one file per task (this file); file:line citations are load-bearing.

## 0. Sources checked (local, no network)

| Source asked | Found | Note |
|---|---|---|
| `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/*` | yes | `wifi_soc` KO, 528-line `Makefile`, `ws73_wifi_defconfig`/`ws73_comm_defconfig`, `alg/ws73_alg_host.mk` |
| `fbb_ws63/src/middleware/services/wifi_service/*` and `protocol/wifi/*` and wpa_supplicant patches | **no** | `fbb_ws63` absent on this host (`ls` fails); wpa patches live under `sdk/.../open_source/wpa_supplicant/` instead |
| `stack/ssap/*` | yes | `stack/ssap/include/feature_mgr.h:1`, `stack/ssap/src/feature_mgr.c:1` — confirm 0 wifi rows (see Section 5) |
| `sdk/.../open_source/wpa_supplicant*` | yes | `wpa_supplicant_2_10_linux.patch` (4298 lines) + `wpa_supplicant_2_7_android_9.patch` (915 lines) |
| `docs/SDK-INTEL.md` | yes | Host driver / tri-mode PM described; no WiFi gap table |
| `.scratch/nearlink-driver/lab-notes/*` wifi-related | yes | `BLE-WIFI-USERLAND-RESEARCH.md`, `EXPLORE-20260815.md`, `SHIFU-BUILD-LIST.md`, `WS63-VS-WS73.md` |

Rule: every factual claim below cites `path:line`. `wc -l` totals are given without a single source line but are reproducible via `wc -l <file>`.

---

## 1. SDK WiFi driver shape (what exists, how it is wired)

### 1.1 Build entry and size

- Top: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:1` (`CUR_MK_PATH`), `:28` (`KO_NAME := wifi_soc`), `:351` (`obj-m += $(KO_NAME).o`), 528 lines total (`wc -l`).
- Config injection: `Makefile:39-40` includes `ws73_comm_defconfig` + `ws73_wifi_defconfig`; `:31` includes `alg/ws73_alg_host.mk`; `:42` appends `$(WIFI_ALG_MACRO_DEFINES)`.
- Compiler model: `Makefile:467-468` `ccflags-y += -isystem /usr/lib/llvm-21/lib/clang/21/include` plus `-DDMAC_ON_HOST` (`:216`/`Makefile:384`), `-include autoconfig.h` (`:388`).

### 1.2 Layer inventory (always linked)

```
driver/wifi/
  hmac/        host MAC maintenance (mgmt/encap/PSM/scan/BA/DFS …)
  hal/ + hal/hh503/   HAL RAM + HH503 ASIC regs (cca/rts/tpc/txbf/coex …)
  mac/         MAC utils (mac_device/vap/user/frame/ie/resource)
  frw/         framework (frw_main/hcc_adapt/thread/timer)    -> frw/:1
  wal/         cfg80211 adaptation (wal_linux_cfg80211.c:6292 wiphy_new/register)
  oal/         OS abstraction (oal_net/cfg80211)
  fe/          RF front-end cali/power/customize
  alg/host/    rate/aggregation/schedule/TPC/G LA/TPC/anti-interference
  forward/     TX/RX data path (hmac_tx_data/rx_data*)
  common/      customize_wifi, soc_ini
  main/linux/  main.c module init
  inc/         public headers (oal/mac/frw/hal/hmac/wal)
```

Always-linked HMAC core (`Makefile:45-55`): `hmac_fsm`, `hmac_mgmt_ap`, `hmac_mgmt_sta`, `hmac_encap_frame_ap`, `hmac_encap_frame_sta`, `hmac_encap_frame`, `hmac_sme_sta`, `hmac_11i/11w`, `hmac_psm_ap` (`:50`), `hmac_psm_sta` (`:54`), `hmac_blockack/ampdu/dfs/arp_offload/dhcp_offload`, `hmac_txopps/wur_ap/sniffer/wmm/bsle`, etc. — STA and AP data/control foundations are **not** feature-gated.

Always-on COPTS in `ws73_wifi_defconfig` (line numbers from that file): `_PRE_WLAN_FEATURE_P2P` gated only by `WIFI_LITE_EXTREME` (`:137-139`), but otherwise always on: `11AX` (`:148`), `PM` (`:116`), `PSM` (`:118`), `AMPDU` (`:131`), `AMSDU` (`:132`), `BLACKLIST` (`:133`), `STA_PM` (`:134`), `UAPSD` (`:142`), `WAPI` (`:143`), `ACS` (`:174`), `11AX 20M_ONLY/ER_SU_DCM` (`:150-151`), plus `DMAC_ON_HOST` (`:216`).

### 1.3 Feature-gated vs. commented-out

`Makefile:62-218` is the single source of truth for optional WiFi features. Table in Section 2 maps each `WIFI_*` Kconfig-ish variable to its `COPTS` and object list. Commented-out COPTS in `ws73_wifi_defconfig` (`:126-127` WOW/DYNAMIC_OFFLOAD, `:130` ALWAYS_TX, `:141` SNIFFER, `:175` ANT_SWITCH, etc.) mean the default TV-box build ships **without** those features unless the caller sets `WIFI_*=y`.

---

## 2. Capability-by-capability reuse verdict for TV-box tri-mode

Scoring: **reuse** = bring up on the TV box; **defer** = wire the gate but ship off; **drop** = not applicable to wall-powered TV box. Cost notes under 3.2.

### 2.1 STA + SOFTAP — reuse, mandatory core

- Evidence: STA path `Makefile:46` (`hmac_mgmt_sta`, `hmac_encap_frame_sta`, `hmac_sme_sta`) + `hmac/hmac_mgmt_sta.c:1`, `hmac/hmac_sme_sta.c:1`, `hmac/hmac_sta_pm.c` (1446 lines), `hmac/hmac_sta_channel_scoring.c:1`; AP path `Makefile:46` (`hmac_mgmt_ap`, `hmac_encap_frame_ap`) + `hmac/hmac_mgmt_ap.c:1`, `hmac/hmac_psm_ap.c` (1194 lines), `hmac/hmac_beacon.c:1`; VAP plumbing `hmac/hmac_vap.c:1`, `wal/release/linux/wal_linux_cfg80211.c:6002` (`g_wal_cfg80211_ops` scan/connect/start_ap), `:6292` `oal_wiphy_new`, `:6312` wiphy_register, `wal_linux_netdev.c:106` wlan0/1/p2p0 naming cribbed from `ws73_cfg_default.ini:27-35`; data path `forward/hmac_tx_data.c:1`, `forward/hmac_rx_data.c:1`.
- TV-box need: WiFi is the TV box's upstream WAN (STA) and optional SoftAP for onboarding/cast. Stock `wpa_supplicant`/`hostapd` via nl80211 already drives `wifi_soc` — see `.scratch/nearlink-driver/lab-notes/BLE-WIFI-USERLAND-RESEARCH.md:14` (interfaces wlan0/wlan1/p2p0) and `:18` (real cfg80211).
- Verdict: **reuse both**. Propose two feature_mgr bits (`FEAT_WIFI_STA`, `FEAT_WIFI_SOFTAP`) so the PM can policy-gate them. STA is in the minimal subset; SoftAP is minimal-if-onboarding else defer.
- Dependencies: STA_PM (`ws73_wifi_defconfig:134` always-on; `hmac/hmac_sta_pm.c:1`, `hmac/hmac_psm_sta.c:1`), scan/chan-mgmt (`hmac/hmac_scan.c:1`, `hmac/hmac_chan_mgmt.c:1`), regulatory (`mac/mac_regdomain.c:1`, `wal/wal_regdb.c:1`), FE cali via `wifi_cali.bin` (`firmware/us/wifi_cali.bin:1` 21K). No BT/SLE coupling except PM arbitration.

### 2.2 WOW (Wake-on-WLAN) + DYNAMIC_OFFLOAD — drop for TV box

- Evidence: `Makefile:62-66` `ifeq ($(WIFI_WOW),y)` → `COPTS += -D_PRE_WLAN_FEATURE_WOW_OFFLOAD` (`:63`), `+ _DYNAMIC_OFFLOAD` (`:64`), `hmac-objs += hmac_wow.o` (`:65`); default `ws73_wifi_defconfig:126-127` leaves both commented out; implementation `hmac/hmac_wow.c:1` (1068 lines) guarded by `_PRE_WLAN_FEATURE_WOW_OFFLOAD` (`:10`) and nested `_DYNAMIC_OFFLOAD` (`:43`), header `hmac/hmac_wow.h:22` (`msg_wow_rom.h`), weakrefs `:88-89`; firmware `firmware/us/wow.bin:1` (23K).
- TV-box need: wall-powered, always-on; suspend-to-RAM WoW is a phone/laptop primitive. TV-box tri-mode PM keeps WLAN powered; WoW would add `wow.bin` download + ARP/NS offload + power-state dance with no benefit.
- Verdict: **drop** (default off, do not add to minimal subset). Keep the gate for a future low-power stick variant; if later needed, cost is small (~2 KB driver state + `wow.bin` load).

### 2.3 CSA (Channel Switch Announcement) — defer, keep gate

- Evidence: `Makefile:107-110` `WIFI_CSA` → `_PRE_WLAN_FEATURE_CSA` + `feature/hmac_csa_ap.o` + `hmac_csa_sta.o`; split: `feature/hmac_csa_ap.c:1` (940 lines) + `feature/hmac_csa_sta.c:1` (1713 lines); headers `feature/hmac_csa_ap.h:7` (`HMAC_CSA_AP_H`), `:54-60` weakref callbacks, `feature/hmac_csa_sta.h:7`, `:30-35` FSM states `WLAN_STA_CSA_FSM_*`, `:40-47` events, `:52-71` timers.
- TV-box need: STA-side CSA is needed only when the upstream AP forces a channel switch (DFS); AP-side CSA only if the TV box itself is SoftAP. Both are rare.
- Verdict: **defer** (gate stays, default off). If SoftAP is in the minimal subset, enable `WIFI_CSA` together with it; otherwise leave off and rely on reconnect.

### 2.4 TWT (Target Wake Time, 11ax power-save) — defer, gated

- Evidence: `Makefile:198-201` `WIFI_TWT` → `_PRE_WLAN_FEATURE_TWT` + `feature/hmac_twt.o`; `feature/hmac_twt.c:1` (2013 lines); header `feature/hmac_twt.h:7`, `:25-68` `mac_device_twt_custom_stru`/`sta_twt_para_stru`/`hmac_twt_status_enum`, `:105-118` weakref callbacks.
- TV-box need: TWT is a battery-life feature (negotiated wake schedule). A TV box wants throughput/latency, not micro-sleeps; no AP in the target deployment mandates TWT.
- Verdict: **defer**. Keep `FEAT_WIFI_TWT` gate (see Section 6) but ship off in the minimal subset. Interaction: `hmac_twt.c` touches PSM; verify `hmac_psm_sta.c` coexistence.

### 2.5 BTCOEX (WLAN + BT/SLE coexistence) — reuse, mandatory for tri-mode

- Evidence: `Makefile:186-190` `WIFI_BTCOEX` → `_PRE_WLAN_FEATURE_BTCOEX` + 6 objects `hmac_btcoex(.o) _ba _btsta _m2s _notify _ps` + `hal/hh503/hal_coex_reg.o` (`:189`); impl totals 5478 lines (`hmac_btcoex.c` 2192, `_ba` 630, `_btsta` 751, `_m2s` 676, `_notify` 377, `_ps` 852); header `feature/hmac_btcoex.h:7` (`__HMAC_BTCOEX_H__`), `:34` guard, `:82-88` `hmac_btcoex_ps_switch_enum`, `:102-244` per-user/per-VAP structures, `hal/hh503/hal_coex_reg.c:1`/`hal_coex_reg.h:1` HW preempt regs.
- TV-box need: tri-mode (WLAN + BLE + SLE) on one WS73 die with shared 2.4 GHz front-end. Without BTCOEX the single RF chain will lose to itself. PM state confirms tri-mode: `driver/platform/pm/plat_pm_wlan.h:79` (`PM_SVC_WLAN=0`), `:80` `PM_SVC_BLE`, `:81` `PM_SVC_SLE`, `:123` `pm_svc_state[PM_SVC_NUM]`, `plat_pm_wlan.c:140-142` init, `:588-598` first service does firmware download, `:703-711` last-close power-down. Lab note `.scratch/nearlink-driver/lab-notes/BLE-WIFI-USERLAND-RESEARCH.md:22` confirms tri-mode and `_PRE_WLAN_FEATURE_BTCOEX`; `EXPLORE-20260815.md:169` + `SHIFU-BUILD-LIST.md:177` agree.
- Verdict: **reuse, mandatory**. Put in minimal subset; policy ties to `pm_svc_state[PM_SVC_BLE/SLE]` (enable BTCOEX whenever any non-WLAN service is open).

### 2.6 ALG (host rate/schedule/TPC/aggregation/TA subsystems) — reuse as base, partially gate

- Evidence: `Makefile:31` `alg/ws73_alg_host.mk:1`, `:1-13` base `WIFI_ALG_SRC_LIST` (`alg_main/schedule/autorate/gla/probe_common/tpc/dbac_hmac/tx_tb_hmac/traffic_ctl/intf_det/aggr/rts/intrf_mode`: 13 files, 11910 lines total), `:15-33` optional `WIFI_ALG_CCA/TEMP_PROTECT/TXBF/EDCA/ANTI_INTF`, `Makefile:42` `COPTS +=$(WIFI_ALG_MACRO_DEFINES)` always applied, `Makefile:258` `alg-objs`, `_INCLUDES` `:435`; base macros `ws73_alg_host.mk:35-49` (`_ANTI_INTERF _AUTOAGGR _AUTORATE _CCA_OPT _EDCA_OPT _INTF_DET _NEGTIVE_DET _RTS _SCHEDULE _WMM_ENSURE _TEMP_PROTECT _TPC _TRAFFIC_CTL _TXBF`) plus conditional `DBAC/GLA` unless `WIFI_LITE_EXTREME`.
- TV-box need: ALG is not a UI feature; it is the throughput/latency engine (autorate, aggregation, TPC, scheduler). The TV box wants it always.
- Verdict: **reuse base ALG as non-optional** (do not put behind a feature_mgr bit). Gate only the heavy optionals: `WIFI_ALG_CCA`, `WIFI_ALG_TXBF`, `WIFI_ALG_EDCA`, `WIFI_ALG_ANTI_INTF`, `WIFI_ALG_TEMP_PROTECT` via a single `FEAT_WIFI_ALG_EXT` or leave them as build-time Kconfig, not runtime feature_mgr.

### 2.7 PSM / STA_PM / UAPSD / offloads — always-on assists

- `ws73_wifi_defconfig:116` `PM`, `:118` `PSM`, `:134` `STA_PM`, `:142` `UAPSD`, `:133` `BLACKLIST` are always-on; offpacks `hmac/hmac_psm_sta.c:1`, `hmac/hmac_sta_pm.c:1` (1446 lines), `feature/hmac_uapsd.c:1` (1067 lines) / `_sta` (359 lines), `hmac/hmac_arp_offload.c:1`, `hmac_dhcp_offload.c:1`, `hmac_rekey_offload.c:1`, `hmac_tcp_opt.c:1` (`Makefile:50`). No feature_mgr gate needed; they ride with STA.

### 2.8 Other feature/ gates (not asked but affect TV cost)

`Makefile:106-218` full gate list for costing reference:

| Gate | COPTS | Objects |
|---|---|---|
| `WIFI_CSA` | `_CSA` (`:108`) | `hmac_csa_ap.o hmac_csa_sta.o` (`:109`) |
| `WIFI_SLP/SDP/WPS/PROMISC/TX_AMSDU` | various (`:111-171`) | `hmac_slp/sdp/wps/promisc/tx_amsdu` |
| `WIFI_UAPSD` | `_STA_UAPSD` (`:173`) | `hmac_uapsd(_sta).o` (`:174-175`) |
| `WIFI_ANT_SEL/PSD` | `_ANT_SEL/_PSD` (`:177-184`) | `hmac_ant_sel/hal_ant_sel`, `hmac_psd` |
| `WIFI_BTCOEX` | `_BTCOEX` (`:187`) | 6 btcoex + `hal_coex_reg` (`:188-189`) |
| `WIFI_DNB/BSRP_NFRP/TWT/SR_STA` | `_STA_DNB/_BSRP/_TWT/_SR` (`:191-204`) | `hmac_dnb_sta/bsrp_nfrp/twt/sr_sta` |
| `WIFI_ROAM/11KVR/MBO` | `_ROAM/_11K/_MBO` (`:123-132`) | `hmac_roam_*`, `hmac_11k/v/r`, `hmac_mbo` |
| `WIFI_CSI/M2U/BLACKLIST/WAPI` | `_CSI/_M2U/_BLACKLIST/_WAPI` (`:141-155`) | `hmac_csi/m2u/blacklist/wapi*` |

Counts: `feature/*.c` total 40793 lines (`wc -l`). TV-box should leave all of these off except those pulled by the minimal subset.

---

## 3. WiFi vs. NearLink stack boundaries

### 3.1 BT/SLE + WiFi are separate KOs, shared PM

- Host USB entry: `driver/platform/hcc/host/hcc_usb_host.c:1` (the `ffff:3733` `wireless_usb` driver).
- WLAN KO: `driver/wifi/Makefile:28` `wifi_soc`; BLE KO: `driver/bsle/ble_driver/`; SLE KO: `driver/bsle/sle_driver/`.
- PM: `driver/platform/pm/plat_pm_wlan.c:140-142` `pm_svc_state[]` init, `:341`/` :354` `pm_svc_open(PM_SVC_WLAN, ...)`, `:588-598` only the **first** service triggers `pm_init_n_firmware_download`, `:703-711` power-down only when WLAN+BLE+SLE all `SHUTDOWN`. This is the tri-mode coexistence contract; WiFi cannot be evaluated in isolation.

### 3.2 wpa_supplicant / hostapd is the real userland

- `sdk/.../open_source/wpa_supplicant/wpa_supplicant_2_10_linux.patch:1` (4298 lines) is the only WiFi userland in tree; it adds `CONFIG_WAPI` (`:10-30` `CIPHER_SMS4`, `KEY_MGMT_WAPI_*`), `WLAN_EID_WAPI` (`:338`), group/pairwise cipher picks (`:2760-2790`). The note in `BLE-WIFI-USERLAND-RESEARCH.md:2` calls this "real protocol features, not a build shim."
- On the TV box (aarch64 Linux) the right answer is **stock** `wpa_supplicant` + `hostapd` via nl80211 against `wifi_soc`'s `wal_linux_cfg80211.c:6002` ops — no custom daemon to port. The `open_source/open_source.mk:89` build targets are reference only.

---

## 4. `fbb_ws63` — not present, implication

`fbb_ws63/src/middleware/services/wifi_service/*` and `protocol/wifi/*` and wpa_supplicant patches were asked explicitly; `ls fbb_ws63` fails (no such directory). Verdict: do not spec a dependency on `fbb_ws63` for tri-mode WiFi; all evidence must come from `sdk/ws73_sdk_linux_WS73_1.10.110`. The WiFi service layer in WS63 (LiteOS) is not reusable for the Linux TV box without a port — track as a follow-up fetch if WS63 service semantics are needed.

---

## 5. stack/ssap gap — confirmed 0 WiFi rows

### 5.1 Exhaustive grep

`grep -rn -c "wifi\|WIFI\|wlan\|WLAN" stack/ssap/` returns 0 for every file (14 files, 2469 lines total per `wc -l stack/ssap/src/*.c stack/ssap/include/*.h`). Tool output: all `*:0` then `no-match=1`. A broader `grep -rn "wifi\|WIFI" stack/` also returns empty. **The SSAP stack in-repo has no WiFi awareness.**

### 5.2 What feature_mgr does today

- Defined bits `stack/ssap/include/feature_mgr.h:24-35`: `FEAT_SSAP_V1_0` (bit 0) through `FEAT_SM_SECURE` (bit 9) — 10 bits, all SSAP/BLE/SLE, none WiFi.
- Capacity profiles `feature_mgr.h:38-43`: `CAP_TINY` (32K), `CAP_SMALL` (64K), `CAP_MEDIUM` (128K), `CAP_FULL` (256K).
- Conditions `feature_mgr.h:46-52`: `ram_free/connected/peer_version/peer_mtu/use_case`; use-cases `feature_mgr.h:54-60` `IDLE/ADVERTISER/SCANNER/CONNECTED/RANGING`.
- State `feature_mgr.h:62-69` (`fm_t` with `capacity/wanted/enabled/on_change`), helpers `feature_mgr.h:76-85` (`fm_has`, `fm_capacity_budget`).
- Policy `stack/ssap/src/feature_mgr.c:1` (121 lines): `g_feat_cost[]` (`:6-17`) assigns 0/2K/4K/8K/16K to the 10 bits; `fm_init` (`:20-27`), `fm_update` (`:29-89`) with three trims — capacity (`:38-53`), connection-state gating (`:56-68`), peer-version gating (`:71-72`), then RAM-pressure drop-order (`:75-106`). Tests `stack/ssap/test/test_feature.c:1` cover all four trims.

### 5.3 What is missing for WiFi tri-mode

- **No WiFi bit**. Bits 10-31 are free (16 bits remain in the 32-bit `enabled/wanted` mask `feature_mgr.h:64-65`).
- **No WiFi condition**. `fm_conditions_t` has no `wlan_state / btcoex_state / wow_armed / csa_pending / twt_agreed / sta_connected` — the update heuristic cannot make WiFi choices.
- **No WiFi ↔ SSAP/PM coupling**. The only `connected` is the SSAP ACB link (`feature_mgr.h:48`). WLAN PM (`PM_SVC_WLAN` etc. `plat_pm_wlan.h:79-83`) and BTCOEX state are invisible.
- **No WiFi cost model**. `g_feat_cost[]` (`feature_mgr.c:6`) has no WLAN entries; adding WiFi without extending the table will silently under-budget host RSS (wifi.ko is 2.5 MB on disk per `SHIFU-BUILD-LIST.md:131`).
- **No INI/cali gate**. `common/soc_customize_wifi.h:1` + `soc_ini.h:1` + `firmware/us/wifi_cali.bin:1` customisation is outside feature_mgr.

---

## 6. Proposed feature_mgr FEAT_WIFI_* extension (costs + policy)

### 6.1 Bit allocation (preserves existing 0-9)

```c
// stack/ssap/include/feature_mgr.h:24 — append after FEAT_SM_SECURE = 1u<<9
FEAT_WIFI_STA     = 1u << 10,  // STA up (scan/connect/data)      RAM +6K (hmac_mgmt_sta/sme/psm/scan)
FEAT_WIFI_SOFTAP  = 1u << 11,  // SoftAP (beacon/psm_ap/mgmt_ap)  RAM +8K
FEAT_WIFI_BTCOEX  = 1u << 12,  // WLAN+BT/SLE coexistence (6 btcoex + hal_coex_reg)  RAM +8K
FEAT_WIFI_TWT     = 1u << 13,  // 11ax TWT (2013-line hmac_twt)    RAM +4K
FEAT_WIFI_CSA     = 1u << 14,  // CSA AP+STA FSMs (2653 lines)     RAM +2K
FEAT_WIFI_WOW     = 1u << 15,  // WoWLAN + dynamic offload (1068)  RAM +2K
FEAT_WIFI_ALG_EXT = 1u << 16,  // optional ALG (CCA/TXBF/EDCA/ANTI_INTF)  RAM +4K
```

Seven bits consume 10-16; 8 free bits remain (17-31 and bit 0 is base mask bit). If strict 4-bit ask is enforced, keep 10-13 (`STA/SOFTAP/TWT/BTCOEX`) and park CSA/WOW/ALG_EXT as build-time `WIFI_*` only.

RAM numbers are heuristic host-RSS deltas (driver heap + feature state), chosen to harmonize with `feature_mgr.c:6` existing 0-16K scale. They are **not** flash KO size. Calibrate with `slabinfo` / `/proc/meminfo` once wifi.ko is loaded on the TV box.

### 6.2 Conditions extension

```c
// stack/ssap/include/feature_mgr.h:46 — extend fm_conditions_t
uint8_t  wlan_state;     // 0=down, 1=STA idle, 2=STA connected, 3=SoftAP up
uint8_t  btcoex_active;  // pm_svc_state[PM_SVC_BLE/SLE] != SHUTDOWN
uint8_t  wow_armed;      // suspend armed (TV box: always 0)
uint16_t twt_agreed_ms;  // negotiated TWT interval, 0 if none
```

### 6.3 fm_update policy sketch (mirrors existing three trims)

1. **Capacity trim** (`feature_mgr.c:38` pattern): `CAP_TINY` clears all WiFi bits; `CAP_SMALL` keeps `WIFI_STA` only; `CAP_MEDIUM` keeps `STA(+CSA)`; `CAP_FULL` keeps all four (+ optionally deferrable bits).
2. **Service-state gating** (`feature_mgr.c:56` pattern): if `wlan_state==0` clear `TWT/CSA`; if `btcoex_active==0` force-clear `WIFI_BTCOEX` (no one to coexist with); if `wow_armed==0` clear `WOW` (TV box always).
3. **Peer gating** (`feature_mgr.c:71` pattern): TWT only if AP advertises TWT responder; CSA only if STA got a CSA IE or SoftAP is up.
4. **RAM pressure** (`feature_mgr.c:75-106` drop-order): extend `drop_order[]` with `WIFI_TWT, WIFI_CSA, WIFI_WOW, WIFI_SOFTAP, WIFI_BTCOEX, WIFI_STA` (most deferrable first; STA last to drop before BTCOEX because tri-mode without STA is still useful as SoftAP).

---

## 7. Minimal portable subset for TV-box tri-mode (defensible, shippable)

Goal: first boot that proves tri-mode without dragging the 40K-line `feature/` tail.

| In | Out | Rationale |
|---|---|---|
| **STA** (`FEAT_WIFI_STA`) + always-on `STA_PM/PSM/AMPDU/AMSDU/BLACKLIST` | — | WAN for TV box; `hmac_mgmt_sta` etc. already in KO (`Makefile:46`) |
| **BTCOEX** (`FEAT_WIFI_BTCOEX`) | — | single-RF tri-mode is broken without it; HW preempt regs `hal_coex_reg` (`Makefile:189`) |
| **Base ALG** (non-optional) | ALG optionals (`CCA/TXBF/EDCA/ANTI_INTF`) off | throughput engine must be on; optionals are tuning |
| `wpa_supplicant`/`hostapd` stock (no patch fork) | WAPI (`feature/hmac_wapi.c:1` 1175 lines + sms4/wpi) | TV box has no WAPI mandate; patch is WAPI-only |
| FE cali via `wifi_cali.bin` + INI `soc_ini.h` | `wow.bin`, CSA, TWT, M2U/BSRP/CSR, roaming/MBO | deferred; enable with their bits when needed |

Size hint: STA+BTCOEX+base ALG+always-on PSM adds ~6+8+0(i.e. base)+2K RSS in the cost model and pulls in `hmac_btcoex*` (5478 lines) + `hal_coex_reg` but avoids `hmac_twt` (2013) + `hmac_csa_{ap,sta}` (2653) + `hmac_wow` (1068) + the ~30K-line `feature/` tail.

Enable order on boot: `plat_soc` → `ble_soc` → `sle_soc` → `wifi_soc` (`BLE-WIFI-USERLAND-RESEARCH.md:33` load order; first service does `plat_pm_wlan.c:588` firmware download). On aarch64 TV box confirm `ccflags-y` isystem (`Makefile:467-468`) and the `SHIFU-BUILD-LIST.md:131-142` 7.x compat shims (`dev_addr_set`, `cfg80211_roam_info.links[0]`, etc.) — out of scope for this AFK audit but blocking bring-up.

### When to add the deferred bits

- Add **SoftAP + CSA** together when onboarding/cast AP is required (`WIFI_CSA` + `hmac_csa_ap.c`/`hmac_csa_sta.c`).
- Add **TWT** only if a battery-powered peer mandates it and PSM interactions are validated.
- Add **WOW** only for a suspend-capable stick variant (needs `wow.bin` + ARP/NS offload `hmac_arp_offload.c`).
- Add **ALG_EXT** (`WIFI_ALG_*=y`) only after antenna/thermal tuning needs CCA/TXBF.

---

## 8. Open questions for downstream tickets (no code, just edges)

1. `fbb_ws63` fetch: do we need the WS63 `wifi_service` state machine for SoftAP lifecycle, or does `wal_linux_cfg80211.c:6002` + `hostapd` suffice? (Blocked: 01 → 02 WiFi gap audit)
2. PM arbitration policy `pm_svc_state[WLAN/BLE/SLE]` (`plat_pm_wlan.h:123`) + `WIFI_TCM_OPTIMIZE` combo — who owns the decision: kernel PM or feature_mgr? (Map "Not yet specified" MAP note)
3. `feature_mgr` `CAP_*` RAM budgets (`feature_mgr.h:77-85` 32/64/128/256K) are SSAP budgets; wifi.ko RSS (2.5 MB) needs a separate budget or a scaled `g_feat_cost[]` — agree on units.
4. `WIFI_LITE_EXTREME` (`Makefile:73`, `ws73_wifi_defconfig:137`) crops DBAC+GLA (`ws73_alg_host.mk:50-53`); does the TV box use `LITE_EXTREME=y` to save flash, or pay the ~50K-line ALG?
5. Verify `btc_cali.bin` (`firmware/us/btc_cali.bin:1` 7.6K) vs `wifi_cali.bin` (21K) load order with BTCOEX enabled.

---

## 9. File:line index (every SDK file touched)

- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:1` CUR_MK_PATH, `:28` KO_NAME, `:31` alg include, `:42` WIFI_ALG_MACRO_DEFINES, `:45-55` always HMAC core, `:62-66` WOW gate, `:88-91` MFG/ALWAYS_TX, `:97-101` DAQ, `:106-110` CSA, `:111-198` SLP/APF/MBO/11KVR/ROAM/AUTO_ADJUST/SINGLE_PROXYSTA/CSI/M2U/BLACKLIST/WAPI/SDP/WPS/LATENCY/PROMISC/TX_AMSDU, `:172-190` UAPSD/ANT_SEL/PSD/BTCOEX, `:191-204` DNB/BSRP/TWT/SR, `:206-218` DEBUG/WFA/TCP_ACK_FILTER, `:220` feature-objs prefix, `:258` alg-objs, `:351` obj-m, `:467-468` ccflags isystem, `:388` autoconfig.h, `:384` DMAC_ON_HOST
- `sdk/.../driver/wifi/ws73_wifi_defconfig:1` 20_40_80_COEXIST, `:23` WS73, `:32` PMF, `:116` PM, `:118` PSM, `:131` AMPDU, `:132` AMSDU, `:133` BLACKLIST, `:134` STA_PM, `:137-139` P2P/LITE_EXTREME gate, `:126-127` WOW commented, `:130` ALWAYS_TX commented, `:142` UAPSD, `:143` WAPI, `:148` 11AX, `:150-151` 11AX 20M_ONLY/ER_DCM, `:174` ACS, `:216` DMAC_ON_HOST, `:220` DFS_OFFLOAD
- `sdk/.../driver/wifi/ws73_comm_defconfig:1` COPS product IDs, `:42-43` WITP chip, `:99-102` DBAC timer
- `sdk/.../driver/wifi/alg/ws73_alg_host.mk:1` WIFI_ALG_SRC_LIST base, `:15-33` optional ALG, `:35-49` WIFI_ALG_MACRO_DEFINES, `:50-53` LITE_EXTREME crops DBAC/GLA
- `sdk/.../driver/wifi/hmac/hmac_wow.c:10` WOW guard, `:88-89` weakrefs, `:1068` wc -l; `hmac/hmac_wow.h:22` msg_wow_rom.h, `:88-89` weakrefs
- `sdk/.../driver/wifi/feature/hmac_csa_ap.c:1` 940 lines; `feature/hmac_csa_ap.h:7` guard, `:54-60` callbacks; `feature/hmac_csa_sta.c:1` 1713 lines; `feature/hmac_csa_sta.h:7` guard, `:30-47` FSM/events
- `sdk/.../driver/wifi/feature/hmac_twt.c:1` 2013 lines; `feature/hmac_twt.h:7` guard, `:25-68` structs, `:105-118` weakrefs
- `sdk/.../driver/wifi/feature/hmac_btcoex.c:1` 2192 lines; `feature/hmac_btcoex.h:7` guard, `:34` guard, `:82-244` structs/enums; plus `_ba` 630, `_btsta` 751, `_m2s` 676, `_notify` 377, `_ps` 852; `hal/hh503/hal_coex_reg.c:1`/`hal_coex_reg.h:1`
- `sdk/.../driver/wifi/hal/hh503/hal_*.c:1` HAL RAM list `Makefile:57-60` (cca/rts/tpc/txbf/anti_intf/ce/chan_mgmt/edca/ftm/gp_reg/mac/mfg/phy/pm/power/psd/reset/rf/rx_filter/sr/tbtt/tpc/vap)
- `sdk/.../driver/wifi/fe/*:1` `fe/fe_hal/phy/ws73/fe_hal_phy_if_host.c:1`, `fe/power_ctrl/fe_tpc_rate_pow.c:1`, `fe/spec/ws73/power_*_spec.c:1`, `fe/calibrate/online_cali/cali_online.c:1`
- `sdk/.../driver/platform/pm/plat_pm_wlan.h:79` PM_SVC_WLAN, `:80` BLE, `:81` SLE, `:83` NUM, `:123` pm_svc_state[]; `plat_pm_wlan.c:140-142` init, `:341`/` :354` open WLAN, `:580`/` :588-598` first-svc download, `:622` mark OPEN, `:678`/` :685-711` close/power-down; `plat_pm.h:55-57` PM_SVC_STATE_*
- `sdk/.../driver/platform/hcc/host/hcc_usb_host.c:1` `ffff:3733` usb_driver
- `sdk/.../open_source/wpa_supplicant/wpa_supplicant_2_10_linux.patch:1` 4298 lines, `:210` CIPHERS, `:338` WAPI EID, `:2760` cipher picks
- `sdk/.../firmware/us/ws73.bin:1` 141K, `wifi_cali.bin:1` 21K, `btc_cali.bin:1` 7.6K, `wow.bin:1` 23K
- `stack/ssap/include/feature_mgr.h:24-35` FEAT_ bits 0-9, `:38-43` CAP_TINY..FULL, `:46-52` fm_conditions_t, `:54-60` FM_USECASE_*, `:62-69` fm_t, `:76-85` fm_has/capacity_budget; `stack/ssap/src/feature_mgr.c:6-17` g_feat_cost[], `:20-27` fm_init, `:29-89` fm_update (capacity/connected/peer/RAM trims), `:75-106` drop_order; `stack/ssap/test/test_feature.c:1` 7 test cases; 0 wifi rows per `grep -rn -c` over 14 files (2469 lines)
- `docs/SDK-INTEL.md:1` SDK layout, `build/config/ws73_usb_light.config:1` `WSCFG_BUS_USB=y`, `WSCFG_CROSS_COMPILE arm-himix100`; no WiFi gap table
- `.scratch/nearlink-driver/lab-notes/BLE-WIFI-USERLAND-RESEARCH.md:14` wlan0/1/p2p0, `:18` cfg80211 ops `:6002`/`:6292`/`:6312`, `:22` tri-mode pm_svc_state + BTCOEX, `:33` load order; `EXPLORE-20260815.md:5` tri-mode WS73, `:169` speed gap, `:204` 4-module bring-up; `SHIFU-BUILD-LIST.md:131-142` wifi 253-file 7.x compat shims; `WS63-VS-WS73.md:1` comparison
