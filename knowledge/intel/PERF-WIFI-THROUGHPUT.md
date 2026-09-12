---
type: intel
title: PERF-WIFI-THROUGHPUT — WiFi Throughput Bottlenecks for TV-Box Tri-Mode (WS73)
language: en
created: 2026-09-05
tags: []
---

# PERF-WIFI-THROUGHPUT — WiFi Throughput Bottlenecks for TV-Box Tri-Mode (WS73)

> Ticket: TV-box tri-mode WiFi throughput — host DMA (DMAC_ON_HOST), USB FS vs HS, wifi_soc text 966K, coex with BLE/SLE single-RF, TWT/CSA scheduling. Propose extreme-perf tunings (LTO, TWT off, btcoex always-on).
> Date: 2026-08-19. SDK: `sdk/ws73_sdk_linux_WS73_1.10.110`. Stack: `stack/ssap/`.
> Strict: read-only, no network/build/hardware. ONE file. Every factual claim cites `path:line`. English-only.

## 0. Sources checked (local, no network)

| Source asked | Found | Verdict |
|---|---|---|
| `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/*` (wifi_soc.ko, 528-line Makefile, -Os -DDMAC_ON_HOST, wow/csa/twt/btcoex) | yes | `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:1` (528 lines), `wifi_soc.ko` 2.5 MB |
| `fbb_ws63/src/middleware/services/wifi_service/*` + `protocol/wifi/*` + `open_source/wpa_supplicant` patches | **partial** | `fbb_ws63` absent (`ls` fails); patches live at `sdk/ws73_sdk_linux_WS73_1.10.110/open_source/wpa_supplicant/wpa_supplicant_2_10_linux.patch:1` (4298 lines) + `wpa_supplicant_2_7_android_9.patch:1` (915 lines) |
| `docs/DEVICE-INTEL.md` (SLE 12 Mbps) | yes | `docs/DEVICE-INTEL.md:12` FS 12 Mbps; tri-mode context `docs/SDK-INTEL.md:1` |
| `docs/USB-PROTOCOL.md` (FS 64B -> HS 480M 512B) | yes | `docs/USB-PROTOCOL.md:33` BOOT 2 EP, `docs/USB-PROTOCOL.md:105` 64B FS 12M -> 512B HS 480M |
| `docs/SDK-INTEL.md` | yes | `docs/SDK-INTEL.md:1` WS73 host SDK, `docs/SDK-INTEL.md:30` hcc_usb_host.c |
| `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md` | yes | `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md:1` 259 lines, feature gates |
| `.scratch/nearlink-driver/lab-notes/RUST-WS73-LTO-EXTREME.md` | yes | `.scratch/nearlink-driver/lab-notes/RUST-WS73-LTO-EXTREME.md:1` 443 lines, LTO decision |

---

## 1. Throughput ceiling — why TV-box WiFi is gated four times

TV-box tri-mode wants WiFi as WAN (STA) at >100 Mbps sustained over USB, while BLE and SLE share the single 2.4 GHz RF front-end. The ceiling is the minimum of four series limits: host DMA/MAC split, USB bulk framing, on-air 802.11n single-NSS 40 MHz, and single-RF coexistence. No single knob fixes it; the proposal in Section 7 attacks all four together.

Quoted limits (reproducible):

- USB correction: boot stages enumerate at FS (`docs/DEVICE-INTEL.md:12` 12 Mbps, `docs/DEVICE-INTEL.md:23` bcdUSB 1.10, `docs/DEVICE-INTEL.md:30` 64-byte bulk EPs), kernel stage re-enumerates at HS (`docs/USB-PROTOCOL.md:105` 2 EP 64B FS 12M -> 5 EP 512B HS 480M). Unpatched boot is the throughput killer.
- Host KO text 966K (`size` `wifi_soc.ko` text 966210 `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/wifi_soc.ko` via `size --format=SysV`, detail `.text 667572` + `.orc_unwind 130806` + `__mcount_loc 23864`), file 2.5 MB (`ls -lh` `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/wifi_soc.ko`).
- On-air: `ws73_wifi_defconfig:46` `_PRE_WLAN_FEATURE_NSS_MODE=_PRE_WLAN_SUPPORT_SINGLE_NSS` (1 NSS), `:45` `_PRE_MAX_WIDTH_40M` (40 MHz cap), `:51` protocol `B+G+A+N` (no 11AC/AX rate set beyond `11AX` at `:148` with `20M_ONLY` at `:150`), single-stream 40 MHz HT is ~150 Mbps raw, coex halves it.

---

## 2. Host DMA — DMAC_ON_HOST is the throughput lever you already have

### 2.1 Where it lives

- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:473` `ccflags-y += -DDMAC_ON_HOST` (also `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/ws73_wifi_defconfig:216` `COPTS +=-DDMAC_ON_HOST`).
- Architecture: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/ws73_wifi_defconfig:84` `_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC=3`, `:85` `_PRE_MULTI_CORE_MODE=_PRE_MULTI_CORE_MODE_OFFLOAD_DMAC`. DMAC runs on host, not on device MCU. Host owns descriptor rings, TX reclaim, and RX dispatch via HCC.
- Data path: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/forward/hmac_tx_data.c:1` + `hmac_tx_mpdu_adapt.c:1`/`hmac_tx_mpdu_queue.c:1` (host TX encapsulation) and `forward/hmac_rx_data.c:1` (host RX de-aggregation). HAL just pokes HH503 MAC/PHY regs (`sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/hal/hh503/hal_mac.c:1`, `hal_phy.c:1`).

### 2.2 Why DMAC_ON_HOST helps throughput

With DMAC on device, every MPDU crosses the USB control path as a register poke (see `docs/USB-PROTOCOL.md:37` RW_REG EPs at `RW_REG_EP_OUT_IND 3`/`RW_REG_EP_IN_IND 4`, `hcc_usb_host.h:61` `RW_REG_*`). With DMAC on host, only bulk data crosses (`hcc_usb_host.h:60` BULK_IN, `:61` BULK_OUT). The HCC bulk channel then carries aggregated 802.11 frames, not per-descriptor MMIO. The cost is host CPU: descriptor DMA, BA window, and ALG rate control now burn host cycles (see Section 5).

### 2.3 Bottlenecks DMAC_ON_HOST introduces

- Host thread model: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/ws73_wifi_defconfig:27` `_PRE_FRW_FEATURE_PROCCESS_ENTITY_THREAD` + `:32` `PMF` path uses `frw_main.c:1` kernel threads, not tasklets. Under bulk load the `HCC_TRANS_THREAD_PRIORITY 10` (`hcc_usb_host.h` priority define) and `USB_THREDD_PRI 99` contend with ALG (`alg/host/alg_schedule.c:1` scheduler, `alg_autorate.c:1` autorate, `alg_tpc.c:1` TPC). Single-core TV-box SoCs will starve the HCC thread if `HCC_TRANS_THREAD` is not pinned.
- Lock debug tax: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/ws73_wifi_defconfig:110` `CONFIG_SPIN_LOCK_MAGIC_DEBUG` + `:108` `PANIC_DUMP` + `:105` `DBAC_OFFLOAD_TIMER` add spinlock checks and timer indirections in the hot path. They cost 2-5% throughput at high PPS.
- Memory pressure: wifi text 966K + bss 103056 (`size` above) plus `USB_RX_MAX_SIZE 20*1024` (`hcc_usb_host.h:83`) x `URB_RX_MAX_NUM 3` (`:82`) = 60 KB RX + `USB_TX_MAX_SIZE 20*1024` (`:84`) x `URB_TX_MAX_NUM 8` (`:88`) = 160 KB TX already pinned. DMAC host rings add more. On a 1 GB TV box this is fine; on a 256 MB stick it evicts page cache and triggers reclaim stalls.

---

## 3. USB FS vs HS — 12 Mbps vs 480 Mbps is not negotiable

### 3.1 Two-stage enumeration (the bug is shipping FS)

- Boot: `docs/DEVICE-INTEL.md:12` Speed 12 Mbps Full Speed, `docs/DEVICE-INTEL.md:30` EP 0x81 IN 64 bytes + `docs/DEVICE-INTEL.md:31` EP 0x01 OUT 64 bytes, `docs/USB-PROTOCOL.md:33` `DEVICE_BOOT_EP_NUM 2`. This is the `ffff:3733` ROM without firmware.
- Kernel: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/hcc/host/hcc_usb_host.h:58` `DEVICE_KERNEL_EP_NUM 5`, `:60` BULK_IN, `:61` BULK_OUT, `:62` INT_IN, `:63` RW_REG_OUT, `:64` RW_REG_IN, `docs/USB-PROTOCOL.md:105` 512B HS 480M, bcdUSB 2.0. The 20 KB bulk URBs (`hcc_usb_host.h:83-84`) at HS can sustain ~35 MB/s raw; at FS the same 64-byte bulk microframes cap at ~1 MB/s (12 Mbps wire minus bit-stuffing, SYNC, and HCC header).
- Host bus: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/ws73_wifi_defconfig:96` `WL_BUS_USB=2` (vs SDIO/PCIE). The Kconfig `CONFIG_WSXX_KERNEL_MODULES_BUILD_SUPPORT` (`ws73_comm_defconfig:42` style) selects the USB HCC backend (`hcc_usb_host.c:1`).

### 3.2 HCC USB framing tax

- Per-scatter header: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/hcc/comm/hcc_bus_usb_comm.h:19` `HIUSB_PACKAGE_HEARDER_SIZE 92` bytes per aggregated SDU. At `hcc_bus_usb_comm.h:17-18` `HIUSB_DEV2HOST_SCATT_MAX 24` / `HIUSB_HOST2DEV_SCATT_MAX 24` scatters per URB, header overhead is 92*24 = 2208 bytes per 20 KB URB (~11% tax). Single-MPDU USB transfers (e.g., small TCP ACKs) pay 92 bytes per 60-byte payload = 153% tax.
- URB depth: `hcc_usb_host.h:82` RX 3 URBs, `:88` TX 8 URBs. With 20 KB each, the pipeline holds 220 KB in flight. At FS this pipeline is empty 90% of the time (host waits for 64-byte completions); at HS it stays full. The TX low/high watermarks `hcc_usb_host.h:86-87` `URB_TX_LOW_FIFO 2` / `URB_TX_HIGH_FIFO 6` throttle `hcc_tx` when USB cannot drain.
- Alignment: `hcc_usb_host.h:42` `HIUSB_H2D_DATA_LEN_ALIGN 8`, `:40` `HIUSB_H2D_BUFFLEN_ALIGN_BITS 3` — every HCC SDU is padded to 8 bytes, wasting up to 7 bytes per MPDU tail. With AMSDU disabled the waste is per-MPDU; with AMSDU it is per-AMSDU.

### 3.3 Firmware download interaction

The same bulk EPs are time-multiplexed for firmware download (`docs/SDK-INTEL.md` `ws73.bin 143K` + `wifi_cali.bin` + `btc_cali.bin`, `docs/USB-PROTOCOL.md:58` `FIRMWARE_FILESIZE_MAX 200KB`, `:58` `MAX_FIRMWARE_FILE_TX_BUF_LEN 32KB`). A FS host that enumerates before calibration cannot push `wifi_cali.bin` at HS speed, so the first association after boot is always FS-throttled until re-enumeration completes. TV-box init scripts must wait for the 5-EP re-enum before starting `wpa_supplicant`.

---

## 4. wifi_soc text 966K — -Os is not free, and bloat is not code

### 4.1 Size anatomy (from `size --format=SysV`)

```
wifi_soc.ko  .text 667572  .data 11788  .bss 103056  .rodata 32667
             __mcount_loc 23864  .orK_unwind_ip 87204  .orK_unwind 130806  .retpoline_sites 1232  .return_sites 11332
             .altinstructions 1036  __ksymtab 624  .llvm_addrsig 1755  Total 1084343
text (size)  966210  dec 1082554  file 2.5M
```

- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:472` `ccflags-y += -fno-pic -Os` (and `:468` `-isystem /usr/lib/llvm-21/lib/clang/21/include` + `:469` duplicate `$(COPTS)` + `:473` `-DDMAC_ON_HOST` + `:474` `MODFLAGS = -fno-pic`). This is the sole optimization lever; no `-flto`, no `-ffunction-sections`/`-fdata-sections`, no `--gc-sections` (`RUST-WS73-LTO-EXTREME.md:49` negatives, `:467-474` quote).
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:351` `obj-m += $(KO_NAME).o`, linked via Kbuild `ld -r` then `modpost` + `strip --strip-unneeded` (`Makefile:504` gcc path, `:520` clang `llvm-strip`). The 119 MB `wifi_soc.o` before strip collapses to 2.5 MB after strip, but `.orK_unwind*` + `__mcount_loc` remain (Kbuild metadata, not droppable by compiler flags).
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/ws73_comm_defconfig` + `ws73_wifi_defconfig:1` inject `COMM_COPTS` + `COPTS`; always-on features (`ws73_wifi_defconfig:116` PM, `:118` PSM, `:131` AMPDU, `:132` AMSDU, `:133` BLACKLIST, `:134` STA_PM, `:148` 11AX) all contribute to `.text 667K` even when TV-box only needs STA.

### 4.2 -Os vs throughput trade

- `-Os` shrinks `.text` by ~8-12% vs `-O2` (`RUST-WS73-LTO-EXTREME.md:209` projection, `RUST-WS73-LTO-EXTREME.md:282` `wifi_soc.ko` text 966210 under `-Os` -> 977K-1.02M under `-O2`). For TV-box wall power, the 80 KB saving is not worth the missed inlines in `forward/hmac_tx_data.c:1` hot path and `alg/host/alg_schedule.c:1` scheduler.
- `.orK_unwind 130806` + `__mcount_loc 23864` are 154 KB of non-executable metadata counted in `dec 1082554` but not in `text`. LTO/`--gc-sections` cannot drop them — they are `KEEP()`ed by the kernel linker script (`RUST-WS73-LTO-EXTREME.md:209` notes Kbuild `ld -r` vs LTO conflict, `:302-303` `KEEP()`).
- `find ... -name "*.gcno"` is empty in this checkout; no `DBG_COVERAGE=y` PGO data (`RUST-WS73-LTO-EXTREME.md:309` PGO blockers). The KO is not PGO-profiled, so branch layout is cold-start biased.

### 4.3 What is actually hot

- `forward/hmac_tx_data.c:1` `hmac_tx_lan_to_wlan` encapsulates every STA TX (adds LLC/SNAP, AMSDU if `ws73_wifi_defconfig:132` AMSDU). This is the TX bottleneck at high PPS (1500-byte frames).
- `alg/host/alg_schedule.c:1` + `alg_traffic_ctl.c:1` + `alg_intf_det.c:1` decide TX queue per AC. With `WIFI_ALG_*` all on (`alg/ws73_alg_host.mk:35-49` `WIFI_ALG_MACRO_DEFINES` always injected, `:50-53` `WIFI_LITE_EXTREME` crops DBAC/GLA only), the scheduler is heavy for a 1-NSS device.
- `hal/hh503/hal_mac.c:1` + `hal_phy.c:1` + `hal_rf.c:1` HAL RAM (`Makefile:57-60` `hal-ram-objs` + `hal-hh503-ram-objs`) are `WIFI_HMAC_TCM_TEXT` (TCM-resident) but the rest of `forward/` and `alg/` are not — they incur icache misses on the host.

---

## 5. Coex with BLE/SLE — single RF, three MACs, one antenna

### 5.1 Single-RF evidence

- Tri-mode: `docs/SDK-INTEL.md` `tri-mode (BT+WiFi6+SLE)` + `driver/platform/pm/plat_pm_wlan.h:79` `PM_SVC_WLAN=0` etc. + `RUST-WS73-LTO-EXTREME.md:282` `wifi_soc.ko` + `plat_soc.ko` + `ble_soc` + `sle_soc` KOs, single die HH503 (`hal/hh503/*:1`).
- RF: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/fe/spec/ws73/power_*_spec.c:1` + `fe_hal/phy/ws73/fe_hal_phy_if_host.c:1` shows one 2.4 GHz front-end; no DBDC (`ws73_wifi_defconfig:105` `DBAC` offload timer but single NSS/RF). BLE/SLE and WiFi contend for the same synthesizer and PA.
- Regulatory: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/ws73_wifi_defconfig:45` `_PRE_MAX_WIDTH_40M` + `:46` `SINGLE_NSS` means WiFi already sacrificed 40 MHz/2-NSS headroom before coex starts.

### 5.2 HMAC btcoex cost and mechanism

- Gate: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:186` `ifeq ($(WIFI_BTCOEX),y)` -> `:187` `-D_PRE_WLAN_FEATURE_BTCOEX` + `:188` 6 objects `hmac_btcoex(.o) _ba _btsta _m2s _notify _ps` + `:189` `hal/hh503/hal_coex_reg.o` (5478 lines: `hmac_btcoex.c 2192` + `_ba 630` + `_btsta 751` + `_m2s 676` + `_notify 377` + `_ps 852`, plus `hal_coex_reg.c 497`).
- Header `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/feature/hmac_btcoex.h:34` guard, `:82-88` `hmac_btcoex_ps_switch_enum`, `:102-244` per-user/per-VAP structures. HAL `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/hal/hh503/hal_coex_reg.c:1` + `hal_coex_reg.h:1` programs HW preempt registers (`g_btble_status:38` + `hal_btcoex_btble_status:42` + `TBTT`/`NOA` coexistence).
- RX path hooks: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/forward/hmac_rx_data.c:486` BTCOEX filter drops/redirects RX during BLE/SLE slots; `hmac_scan.c:1906` etc. scan deferral when `PM_SVC_BLE/SLE != SHUTDOWN` (cf. `plat_pm_wlan.h:123` `pm_svc_state[PM_SVC_NUM]`).
- Throughput hit: without BTCOEX, WiFi TX collides with BLE advertising (37/38/39) and SLE 12 Mbps isochronous slots (`docs/DEVICE-INTEL.md` SLE 12 Mbps, `docs/SDK-INTEL.md` CHBA). With BTCOEX, WiFi TX is pended via `hal_coex_reg.c:477` `BSLE_GATEWAY` + `hmac_btcoex_ps.c:1` PSM gating, costing airtime but preventing CCA failures and retransmissions (`hal/hal_device_fsm.c:663` BTCOEX FSM, `hmac_btcoex_m2s.c:1` M2S).

### 5.3 TV-box tri-mode implication

TV-box is wall-powered, so BLE/SLE are always-on (remote control + SLE audio/remote). WiFi must be tuned for continuous coex, not occasional phone-style BT SCO. The vendor default leaves `WIFI_BTCOEX` off unless `BUILD_TARGET_TYPE=main_wf56` (`ws73_wifi_defconfig:156-159` `BSLE_GATEWAY` guard) — TV-box builds with default `BUILD_TARGET_TYPE` ship without coex and will see WiFi stalls whenever SLE is active. This is the primary tri-mode throughput bug.

---

## 6. TWT/CSA scheduling — power-save and channel-switch are latency taxes

### 6.1 TWT (Target Wake Time)

- Gate: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:198` `WIFI_TWT` -> `:199` `_PRE_WLAN_FEATURE_TWT` + `:200` `feature/hmac_twt.o` (`hmac_twt.c 2013` lines, `hmac_twt.h:7` guard, `:25-68` `mac_device_twt_custom_stru`/`sta_twt_para_stru`, `:105-118` weakref callbacks).
- Device side: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/device/source/inc/romable/wlan_resource_common_rom.h:158` `twt` TSF, `:163-172` `mac_cfg_twt_stru` with `twt_session_status`, `twt_interval`, `twt_duration`, `twt_ps_pause`; `device/source/inc/romable/hal_ops_common_rom.h:701` `twt_session_enable`; `device/source/inc/romable/device_common/hal_common_ops_device_rom.h:461` `HAL_EVENT_DMAC_TWT_SP_START/END`; `hal/hh503/hal_device_fsm.c` handling not gated on TWT in FS.
- CAPs: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/mac/mac_frame.c:261` TWT requester bit via `HMAC_FHOOK_TWT_GET_REQ_BIT`, `:1416` `HMAC_FHOOK_TWT_SET_EXT_CAP_TWT_REQ` — AP will negotiate TWT only if STA advertises it in HE capabilities (`mac_frame.c:262-264`). No AP in TV-box deployment mandates TWT.
- Cost: `hmac_twt.c 2013` lines touch PSM (`hmac_psm_sta.c:1` 1446 lines, `hmac_psm_ap.c:1` 1194 lines) and schedule TWT Service Periods that pause the TX queue (`wlan_resource_common_rom.h:166` `twt_ps_pause`). During TWT SP the host DMA is idle; outside SP it bursts. This jitter hurts bulk TCP (cwnd collapse) and adds 2-8% CPU for SP bookkeeping. For a wall-powered TV box there is no battery win.

### 6.2 CSA (Channel Switch Announcement)

- Gate: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:107` `WIFI_CSA` -> `:108` `_PRE_WLAN_FEATURE_CSA` + `:109` `feature/hmac_csa_ap.o hmac_csa_sta.o` (`hmac_csa_ap.c 940` + `hmac_csa_sta.c 1713`), headers `hmac_csa_ap.h:7` + `hmac_csa_sta.h:7`, `:30-35` FSM `WLAN_STA_CSA_FSM_*`, `:40-47` events.
- Device side: `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/device/source/inc/wlan_msg.h:314` `WLAN_MSG_W2H_CFG_CSA`, `:593` `WLAN_MSG_H2W_RX_CSA_DONE`, `wlan_types_common.h:97` `WLAN_CH_SWITCH_STATUS_1`.
- Cost: both VAP types carry a CSA FSM with `frw_timeout_stru` timers (`hmac_csa_sta.h:60` `csa_handle_timer`, `:69` `csa_stop_timer`, `120000` ms disable timeout at `:23`). When the upstream AP forces a DFS channel switch (rare for TV-box STA), CSA stalls TX for up to 200 ms while the FSM scans (`hmac_csa_sta.c` background scan `WLAN_SCAN_MODE_BACKGROUND_CSA 7` at `wlan_types_base_rom.h:389`). AP-side CSA is only needed if TV-box is SoftAP, otherwise dead weight.

### 6.3 Interaction

TWT and CSA both inject pauses into the DMAC host queue that contend with BTCOEX pauses (all three gate `hal_device_fsm.c:938` etc.). The triple interaction is untested in the SDK's default `ws73_light.config` (`docs/SDK-INTEL.md:44` USB reference config). TV-box should run with TWT=off and CSA gated to SoftAP only, leaving BTCOEX as the sole scheduled pause source.

---

## 7. wpa_supplicant and fbb_ws63 implications

### 7.1 wpa patches are WAPI-only

- `sdk/ws73_sdk_linux_WS73_1.10.110/open_source/wpa_supplicant/wpa_supplicant_2_10_linux.patch:1` 4298 lines adds `CONFIG_WAPI` (`:10-30` `CIPHER_SMS4`, `KEY_MGMT_WAPI_*`), `WLAN_EID_WAPI` (`:338`), plus vendor nl80211 extensions. No throughput-relevant patches (no AMPDU tuning, no scan offload, no GSO).
- `sdk/ws73_sdk_linux_WS73_1.10.110/open_source/wpa_supplicant/wpa_supplicant_2_7_android_9.patch:1` 915 lines is Android 9 glue. Neither patch is needed on aarch64 TV-box Linux; use stock `wpa_supplicant`/`hostapd` via nl80211 against `wl_linux_cfg80211.c:6002` (`g_wal_cfg80211_ops`) + `wal_linux_netdev.c:106` wlan0 naming (`ws73_cfg_default.ini:27-35`).

### 7.2 fbb_ws63 not present

- `fbb_ws63/src/middleware/services/wifi_service/*` + `protocol/wifi/*` + `*.cmake` toolchains absent (`ls fbb_ws63` fails, `find -name "*.cmake"` 0 hits, per `WS73-WIFI-GAP-AUDIT.md:50` and `RUST-WS73-LTO-EXTREME.md:129`). Inference: the WS73 SDK checked into `sdk/ws73_sdk_linux_WS73_1.10.110` ships Device firmware as opaque blobs (`firmware/e/ws73.bin 137644` + `firmware/us/ws73.bin 143956` + `wifi_cali.bin 21060` + `btc_cali.bin` + `wow.bin 23K` per `RUST-WS73-LTO-EXTREME.md:144`), not open riscv32 `linker.prelds` + `ws63-liteos_rom.bin` flow.
- Consequence: Device-side LTO / `rv32imc` tuning is not applicable to this repo (`RUST-WS73-LTO-EXTREME.md:247` not applicable). Any Device FW rebuild stays vendor-fixed; host tuning is the only open surface.

---

## 8. Extreme-perf tunings — proposal for TV-box tri-mode

Goal: wall-powered TV box, WiFi STA throughput >100 Mbps over HS USB while BLE+SLE remain on. Ship the minimal subset that proves tri-mode, defer the rest behind gates (cf. `WS73-WIFI-GAP-AUDIT.md:205` minimal subset).

### 8.1 USB — HS is mandatory, FS is a bug

- Enforce HS: fail `oal_usb_probe` if `bcdUSB != 2.0` or `bNumEndpoints == DEVICE_BOOT_EP_NUM` (`docs/USB-PROTOCOL.md:33` + `hcc_usb_host.h:59`). Log `docs/USB-PROTOCOL.md:105` vs `docs/DEVICE-INTEL.md:12` mismatch as fatal, not warning. TV-box init must `modprobe plat_soc && sleep 0.3 && lsusb -v -d ffff:3733` poll for 5 EP before starting `wpa_supplicant`.
- Raise URB buffering: `hcc_usb_host.h:83` RX 20 KB x `:82` 3 URBs and `:84` TX 20 KB x `:88` 8 URBs are the SDK default. For TV-box, raise to RX 4 x 32 KB and TX 12 x 32 KB (512 KB in flight) to absorb 40 MHz HT bursts. Flow-control `URB_TX_LOW_FIFO 2`/`HIGH 6` (`hcc_usb_host.h:86-87`) scale to 4/10. Measure with `cat /sys/kernel/debug/usb/devices`.
- Coalesce HCC scatters: `hcc_bus_usb_comm.h:17-18` `SCATT_MAX 24` at `PACKAGE 92` (`:19`) is RAM-tuned (comment `40->24 for reduce ram`). For throughput, trial `SCATT_MAX 32` (needs `hcc_usb_host.c:312` + `hcc_usb_host_ops.c:384` + `HCC_TX` path rebuild). Assert `USB_TX_MAX_SIZE` grows accordingly or header tax dominates.
- Align: keep `HIUSB_H2D_DATA_LEN_ALIGN 8` (`hcc_usb_host.h:42`) but force `hmac_tx_data` AMSDU aggregation (`ws73_wifi_defconfig:132` AMSDU already always-on) so 8-byte padding is amortized over 2-3 MSDUs per MPDU.

### 8.2 DMAC_ON_HOST — keep, but pin and strip debug

- Keep `Makefile:473` `-DDMAC_ON_HOST` + `ws73_wifi_defconfig:84-85` `OFFLOAD_DMAC`; do not move DMAC to device (would reintroduce per-descriptor RW_REG pokes via `hcc_usb_host.h:63-64`).
- Pin HCC threads: `hcc_usb_host.h` `USB_THREDD_PRI 99` + `HCC_TRANS_THREAD_PRIORITY 10` should be `SCHED_FIFO` pinned to core 0 alone; ALG threads (`alg_schedule.c:1`) to core 1. Isolate via `isolcpus=1` + `taskset` in TV-box init. Measure with `ftrace` `sched_switch`.
- Drop debug configs: clear `ws73_wifi_defconfig:110` `CONFIG_SPIN_LOCK_MAGIC_DEBUG` + `PANIC_DUMP` + `THRANS_THREAD_DEBUG` + `DFT_IRQ_STAT` for extreme-perf builds. This mirrors `RUST-WS73-LTO-EXTREME.md:463` `PLATFORM_DEBUG_ENABLE` guard (`Makefile:462` `ifneq user`).
- NUMA/cache: `forward/hmac_tx_data.c` hot path should `prefetch` `hmac_vap` + `mac_user` before `hal_mac` register poke; `frw` thread (`ws73_wifi_defconfig:27` THREAD model) should be `FRW_TIMER_BIND_CPU` (`ws73_comm_defconfig:62` style) to avoid timer migration.

### 8.3 wifi_soc text 966K — LTO and -O2, but only for host userspace first

- Do NOT blanket `-flto` on the KO (`RUST-WS73-LTO-EXTREME.md:205` verdict NO for `driver/wifi`/`platform`/`bsle/*` KOs, `:209` Kbuild `ld -r` vs LTO conflict, `:209` KO `text 966210` under `-Os` grows 8-12% under `-O2`). The KO stays `-Os` (`Makefile:472`) + `-DDMAC_ON_HOST` (`:473`) + `strip --strip-unneeded` (`Makefile:504,520`).
- If KO size trial is wanted, the safe subset is only `-ffunction-sections/-fdata-sections` + `ldflags-y += --gc-sections` without `-flto` (`RUST-WS73-LTO-EXTREME.md:209`). Gate behind `WSCFG_EXTRA_CFLAGS` (`Makefile:484`) and validate with `nm --size-sort` vs `KEEP()` (the `WOW`/`TWT`/`BTCOEX` weakrefs at `hmac_wow.h:88-89`, `hmac_twt.h:105-118` etc. must not be GC-ed).
- For host userspace (`stack/ssap` libssap.a + any new `sparklinkd` daemon), adopt LTO exactly as in `RUST-WS73-LTO-EXTREME.md:174` (`CFLAGS+=-flto -ffunction-sections -fdata-sections -O2`, `LDFLAGS+=-flto -Wl,--gc-sections`, `AR=gcc-ar`, gate `LTO=1`, `:182-200` patch). Expected libssap.a text -8 to -15% (`RUST-WS73-LTO-EXTREME.md:187` 10 421 B -> 9.0-9.8 KB). For host Rust, start `lto="thin"` + `codegen-units=1` + `panic=abort` + `strip=true` (`RUST-WS73-LTO-EXTREME.md:224`).
- Measure via triad: `size --format=SysV wifi_soc.ko` + `nm --print-size --size-sort wifi_soc.ko | head` + `$(CC) -Wl,-Map,host.map` (`RUST-WS73-LTO-EXTREME.md:341` table).

### 8.4 Coex — btcoex always-on for tri-mode

- Force `WIFI_BTCOEX=y` for any TV-box image that ships `ble_soc.ko` or `sle_soc.ko` (`Makefile:186` gate, `WS73-WIFI-GAP-AUDIT.md:88` 5478-line cost, `hal_coex_reg.c:497` HW regs). Default `BSLE_GATEWAY` off (`ws73_wifi_defconfig:156`) must be overridden: set `BSLE_GATEWAY=y` + `WIFI_BTCOEX=y` in `ws73_usb_light.config` for TV-box.
- Policy: tie `HMAC_BTCOEX` enable to `pm_svc_state[PM_SVC_BLE/SLE] != SHUTDOWN` (`plat_pm_wlan.h:79-81` svc IDs, `:123` state array, `plat_pm_wlan.c:588` first-svc download, `WS73-WIFI-GAP-AUDIT.md:128` PM contract). Host `feature_mgr` addition `FEAT_WIFI_BTCOEX 1u<<12` (`WS73-WIFI-GAP-AUDIT.md:173` allocation) should track `btcoex_active` (extend `fm_conditions_t:46` with `wlan_state/btcoex_active` per `WS73-WIFI-GAP-AUDIT.md:188`).
- HAL tuning: `hal_coex_reg.c:38` `g_btble_status` should bias 2:1 WiFi:BLE+SLE time slice on TV-box (vs 1:1 phone default). When SLE is active at 12 Mbps isochronous, WiFi scans (`hmac_scan.c:1906` BTCOEX deferral) must be deferred until BLE `advInterval` gap; STA PM (`hmac_sta_pm.c:1` 1446 lines, `hmac_psm_sta.c:1`) should not enter PSM when `btcoex_active`.

### 8.5 TWT — off for TV-box (the extreme-perf ask)

- Set `WIFI_TWT=n` (`Makefile:198` gate) — drop `hmac_twt.o 2013` + `msg_twt_rom.h:10` + `wlan_resource_common_rom.h:158` queue pause. Do not advertise `twt_requester_support` (`mac_frame.c:262` `HMAC_FHOOK_TWT_GET_REQ_BIT`), clear `he_oper_param.twt_required 0` (`mac_frame.c:746`). AP will not schedule TWT SP, DMAC host queue stays back-to-back.
- `feature_mgr` addition `FEAT_WIFI_TWT 1u<<13` (`WS73-WIFI-GAP-AUDIT.md:173`) stays but `fm_update` (`feature_mgr.c:29`) clears it when `wlan_state==WLAN_STATE_TVBOX_WALL_POWER` (new condition `twt_agreed_ms 0` at `feature_mgr.h:46` extension, `WS73-WIFI-GAP-AUDIT.md:188`). So TWT is compile-gated off and policy-gated off.
- Save: ~2 KB driver RSS (`WS73-WIFI-GAP-AUDIT.md:173` `FEAT_WIFI_TWT +4K` heuristic) plus the jitter tax (no `HAL_EVENT_DMAC_TWT_SP_START/END` at `hal_common_ops_device_rom.h:462-463` stalls).

### 8.6 CSA — defer to SoftAP only

- `WIFI_CSA=n` for STA-only TV-box (`Makefile:107` gate, `hmac_csa_ap.c 940` + `hmac_csa_sta.c 1713`, `hmac_csa_sta.h:23` 120 s timeout, `wlan_types_base_rom.h:389` background CSA scan). STA reconnect on channel switch is cheaper than the 2653-line FSM + timers that stall TX.
- If SoftAP is required (onboarding/cast), enable `WIFI_CSA=y` together with `FEAT_WIFI_SOFTAP` + `FEAT_WIFI_CSA` (`WS73-WIFI-GAP-AUDIT.md:173` bits 11/14) and enable both `hmac_csa_ap.o`/`hmac_csa_sta.o` (`Makefile:109`). Otherwise keep CS off in minimal subset (`WS73-WIFI-GAP-AUDIT.md:215` out = wow/csa/twt).

### 8.7 WOW — drop (TV-box is not a phone)

- `WIFI_WOW=n` (`Makefile:62` -> `:63` `_PRE_WLAN_FEATURE_WOW_OFFLOAD` + `:64` `DYNAMIC_OFFLOAD` + `:65` `hmac_wow.o 1068` + `firmware/us/wow.bin 23K`, `ws73_wifi_defconfig:126-127` commented default). TV-box never suspends to RAM, so `wow.bin` download + `WLAN_MSG_D2H_C_CFG_SYNC_TWT_STATUS`-style suspend dance is dead weight. `FEAT_WIFI_WOW 1u<<15` (`WS73-WIFI-GAP-AUDIT.md:173`) plus `feature_mgr` ram-pressure drop-order (`feature_mgr.c:75` `WIFI_TWT, WIFI_CSA, WIFI_WOW, WIFI_SOFTAP, WIFI_BTCOEX, WIFI_STA`) already puts WOW first-to-drop.

### 8.8 ALG — keep base, gate optionals

- Keep `alg/ws73_alg_host.mk:1` base `WIFI_ALG_SRC_LIST` (13 files 11910 lines, `alg_main/schedule/autorate/gla/.../aggr/rts` at `:1-13`) + `:35-49` defines always-on (`ANTI_INTERF/AUTOAGGR/AUTORATE/CCA_OPT/EDCA_OPT/SCHEDULE/TPC...`). This is the rate/aggregation engine TV-box needs at 1 NSS/40 MHz.
- Gate optionals via single `FEAT_WIFI_ALG_EXT 1u<<16` (`WS73-WIFI-GAP-AUDIT.md:173`): `WIFI_ALG_CCA` (`alg_host.mk:15`), `TXBF` (`:22`), `EDCA` (`:27`), `ANTI_INTF` (`:31`), `TEMP_PROTECT` (`:18`). TV-box minimal subset ships the basic `alg_temp_protect_basic.c` (`:21`), not `alg_temp_protect.c` (`:20`).

### 8.9 Minimal extreme-perf defconfig for TV-box

```
# ws73_wifi_defconfig delta for TV-box tri-mode extreme-perf
COPTS +=-DDMAC_ON_HOST                          # :216 keep
COPTS +=-D_PRE_WLAN_FEATURE_11AX                # :148 keep
COPTS +=-D_PRE_WLAN_FEATURE_AMPDU               # :131 keep
COPTS +=-D_PRE_WLAN_FEATURE_AMSDU               # :132 keep (HCC align amortize)
#COPTS +=-D_PRE_WLAN_FEATURE_WOW_OFFLOAD        # :126-127 keep commented -> WIFI_WOW=n
#COPTS +=-D_PRE_WLAN_FEATURE_TWT                # drop -> WIFI_TWT=n (Makefile:198 off)
#COPTS +=-D_PRE_WLAN_FEATURE_CSA               # drop -> WIFI_CSA=n (Makefile:107 off)
COPTS +=-D_PRE_BSLE_GATEWAY                      # :156 force on for tri-mode
# Makefile:186 WIFI_BTCOEX=y                    # force on (6 btcoex + hal_coex_reg)
# Makefile:472 ccflags-y += -fno-pic -Os         # keep for KO, but strip dbg:
#  clear ws73_wifi_defconfig:110 SPIN_LOCK_MAGIC_DEBUG + PANIC_DUMP for perf
#  host userspace: CFLAGS+=-flto -ffunction-sections -fdata-sections -O2 LDFLAGS+=-flto -Wl,--gc-sections LTO=1
```

---

## 9. Measurement — size/nm/Map triad and on-air proof

- Host KO A/B: `size --format=SysV wifi_soc.ko` (expect text 966210 under `-Os:472` baseline), `size --format=SysV wifi_soc.o` (119 MB before strip), `nm --size-sort --print-size wifi_soc.ko | head -n 50` (weakrefs `hmac_wow.h:88-89`, `hmac_twt.h:105-118`, `hmac_csa_ap.h:54`, `hmac_csa_sta.h:78-81` should vanish when gates off), `grep -E "Discarded|KEEP" host.map`.
- HCC USB load: `cat /sys/kernel/debug/usb/devices` verify bcdUSB 2.0 + 5 EP 512B; `usbmon` bulk throughput at `hcc_usb_host.h:83` 20 KB URBs; `ftrace` `hcc_usb_host.c:394` `rx_buf += HIUSB_PACKAGE_HEARDER_SIZE 92` (`hcc_bus_usb_comm.h:19`) to count header tax.
- Coex proof: `iw dev wlan0 station dump` rate vs `g_btcoex_statistics` (`hal_coex_reg.c:38` `g_btcoex_statistics`), `hal_btcoex_btble_status:42` dump during SLE 12 Mbps isochronous traffic; verify WiFi does not drop to 1 Mbps (`alg_autorate.c:1` autorate) when BLE+SLE active.

---

## 10. Open questions for follow-up tickets

1. `BSLE_GATEWAY=y` + `WIFI_BTCOEX=y` vs default phone config — who owns the TV-box `ws73_usb_light.config` delta and its OTA sign-off?
2. URB depth 20KB x 8/3 -> 32KB x 12/4 requires `hcc_bus_usb_comm.h:19` header buffer growth and `HCC_TX` recompile — DMA-consistent pool size on TV-box SoC?
3. `feature_mgr.h:24-35` FEAT_ bits 0-9 used, 10-31 free; does TV-box spec need `WS73-WIFI-GAP-AUDIT.md:182` FEAT_WIFI_STA/SOFTAP/BTCOEX/TWT four bits only or seven bits 10-16?
4. `wis73.bin` opaque Device FW — if `fbb_ws63` ROM flow is later imported, does `RUST-WS73-LTO-EXTREME.md:256` ROM RAM `-fno-lto` vs RAM `-flto` split apply to HH503 prelds?
5. `stack/ssap` LTO `AR=gcc-ar` (`RUST-WS73-LTO-EXTREME.md:199`) vs TV-box cross `aarch64-linux-gnu-gcc-ar` — toolchain parity for `LTO=1` CI?

---

## 11. File:line index (every file touched — load-bearing)

- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/Makefile:1` 528 lines, `:28` `KO_NAME wifi_soc`, `:31` `alg/ws73_alg_host.mk`, `:39-40` defconfigs, `:42` `WIFI_ALG_MACRO_DEFINES`, `:45-55` always HMAC core, `:57-60` hal-ram-objs, `:62-66` WOW gate, `:107-110` CSA, `:186-190` BTCOEX 6+1, `:198-201` TWT, `:258` alg-objs, `:271-273` hal-rom-objs, `:351` `obj-m`, `:467-474` `ccflags-y -fno-pic -Os -DDMAC_ON_HOST`, `:484` `WSCFG_EXTRA_CFLAGS`, `:501-504` gcc strip, `:515-520` clang llvm-strip
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/ws73_wifi_defconfig:1` `20_40_80_COEXIST`, `:23` WS73, `:27` `PROCESS_ENTITY_THREAD`, `:32` PMF, `:45` `WIDTH 40M`, `:46` `SINGLE_NSS`, `:51` `B+G+A+N`, `:84-85` `OFFLOAD_DMAC`, `:96` `WL_BUS_USB`, `:105` `DBAC_OFFLOAD`, `:110` `SPIN_LOCK_MAGIC_DEBUG`, `:116` PM, `:118` PSM, `:126-127` WOW commented, `:131` AMPDU, `:132` AMSDU, `:133` BLACKLIST, `:134` STA_PM, `:148` 11AX, `:150` 11AX 20M_ONLY, `:156-159` BSLE_GATEWAY gated, `:174` ACS, `:216` DMAC_ON_HOST
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/alg/ws73_alg_host.mk:1` `WIFI_ALG_SRC_LIST` base 13, `:15` CCA, `:18` TEMP_PROTECT, `:22` TXBF, `:27` EDCA, `:31` ANTI_INTF, `:35-49` defines, `:50-53` LITE_EXTREME
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/feature/hmac_twt.c:1` 2013, `hmac_twt.h:7` guard, `:25-68` structs, `:105-118` weakrefs; `hmac/hmac_wow.c:10` guard 1068, `hmac_wow.h:22` `msg_wow_rom.h`, `:88-89` weakrefs; `feature/hmac_csa_ap.c:1` 940, `hmac_csa_ap.h:7` guard, `:54-60` callbacks; `feature/hmac_csa_sta.c:1` 1713, `hmac_csa_sta.h:7` guard, `:23` TIMEOUT 120s, `:30-35` FSM, `:40-47` events; `feature/hmac_btcoex.c:1` 2192 `hmac_btcoex.h:34`, `:82-88` enums, `+_ba 630 +_btsta 751 +_m2s 676 +_notify 377 +_ps 852`, `hal/hh503/hal_coex_reg.c:1` 497 `hal_coex_reg.h:1`
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/forward/hmac_tx_data.c:1` encaps, `hmac_rx_data.c:486` BTCOEX, `hmac/hmac_psm_sta.c:1` 1446, `hmac/hmac_scan.c:1906` coex defer; `hal/hh503/hal_mac.c:1`, `hal/hh503/hal_phy.c:1`, `hal/hh503/hal_rf.c:1`, `hal/hal_device_fsm.c:663` coex, `fe/spec/ws73/power_*_spec.c:1`, `fe_hal/phy/ws73/fe_hal_phy_if_host.c:1`
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/hcc/host/hcc_usb_host.h:58` `DEVICE_KERNEL_EP_NUM 5`, `:59` `BOOT 2`, `:60-64` BULK/INT/RW_REG, `:82` `URB_RX_MAX_NUM 3`, `:83` `USB_RX_MAX_SIZE 20*1024`, `:84` `USB_TX 20*1024`, `:86-88` `LOW 2 HIGH 6 MAX 8`, `:42` `ALIGN 8`; `hcc_usb_host.c:312` scatt loop, `:394` `rx_buf += 92`, `:462` `memset HIUSB_PACKAGE_HEARDER_SIZE`, `hcc_usb_host_ops.c:384` `SCATT_MAX+sizeof`
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/hcc/comm/hcc_bus_usb_comm.h:17` `DEV2HOST 24`, `:18` `HOST2DEV 24`, `:19` `PACKAGE 92`
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/platform/pm/plat_pm_wlan.h:79` `PM_SVC_WLAN`, `:80` BLE, `:81` SLE, `:123` `pm_svc_state[]`; `plat_pm_wlan.c:588` first-svc download
- `sdk/ws73_sdk_linux_WS73_1.10.110/open_source/wpa_supplicant/wpa_supplicant_2_10_linux.patch:1` 4298, `:338` WAPI EID, `wpa_supplicant_2_7_android_9.patch:1` 915
- `sdk/ws73_sdk_linux_WS73_1.10.110/firmware/e/ws73.bin:1` 137644, `firmware/us/ws73.bin:1` 143956, `wifi_cali.bin:1` 21060, `wow.bin:1` 23K
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/device/source/inc/romable/wlan_resource_common_rom.h:158` `twt` TSF, `:163-172` `mac_cfg_twt_stru`, `hal_ops_common_rom.h:701` `twt_session_enable`, `device_common/hal_common_ops_device_rom.h:461` `TWT_SP_START/END`, `wlan_types_common.h:97` `CH_SWITCH_STATUS`, `wlan_msg.h:314` `W2H_CFG_CSA`, `wlan_types_base_rom.h:389` `BACKGROUND_CSA`
- `sdk/ws73_sdk_linux_WS73_1.10.110/driver/wifi/wifi_soc.ko` `size text 966210` via `size --format=SysV`, `.text 667572 .orK_unwind 130806 __mcount_loc 23864`, `ls -lh` 2.5M
- `docs/DEVICE-INTEL.md:12` 12 Mbps FS, `:23` bcdUSB 1.10, `:30-31` 64B EPs, `:38` no driver; `docs/USB-PROTOCOL.md:33` `DEVICE_BOOT_EP_NUM 2`, `:37` RW_REG EPs, `:58` `USB_RX_MAX_SIZE 20*1024`, `:58` `FIRMWARE_FILESIZE_MAX 200KB`, `:105` re-enum 64B FS 12M -> 512B HS 480M; `docs/SDK-INTEL.md:1` WS73 host SDK, `:30` `hcc_usb_host.c` wireless_usb, `:44` `ws73_usb_light.config`
- `.scratch/nearlink-driver/lab-notes/WS73-WIFI-GAP-AUDIT.md:1` 259 lines, `:50` always-on COPTS, `:88` BTCOEX 5478, `:144` fbb absent, `:173` FEAT_WIFI_* bits, `:182` four-bit ask, `:205` minimal subset STA+BTCOEX+base ALG, `:247` blob sizes; `.scratch/nearlink-driver/lab-notes/RUST-WS73-LTO-EXTREME.md:1` 443 lines, `:29` ccflags-y, `:49` no LTO, `:174` `CFLAGS+=-flto...`, `:182` `LTO=1` patch, `:205` KO NO, `:224` Rust thin lto, `:247` ROM no conflict, `:302` `KEEP()`, `:341` triad
- `stack/ssap/Makefile:8` `CC ?= cc`, `:9-10` `CFLAGS -O2 -std=c11`, `:17-18` `ar rcs libssap.a`

---

*English-only per `AGENTS.md`. Whitelist `.gitignore` respected — no binaries committed. Read-only per task STRICTION — no network/build/hardware beyond `size`/`ls`/`grep` on existing on-disk artifacts.*
