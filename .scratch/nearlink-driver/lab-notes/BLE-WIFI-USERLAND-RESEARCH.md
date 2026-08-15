# BLE + WiFi Userland Stacks — Research (ticket 10 input)

Source: subagent B survey, 2026-08-15. SDK: `sdk/ws73_sdk_linux_WS73_1.10.110`.

## 1. ble_soc.ko = REAL BlueZ HCI device (not a char device!)

- Registers with kernel Bluetooth subsystem: `hci_alloc_dev()` (ble_host_hcc.c:532),
  `hci_register_dev()` (:553); `hdev->bus = HCI_SDIO` or `HCI_USB` under WSCFG_BUS_USB
  (:540-544); `hdev->open/close/flush/send/setup` (:547-551)
- → appears as `hci0`, usable by **stock BlueZ** (bluetoothd/bluetoothctl/hcitool) — no
  /dev/hwble needed (that's Android-only: driver/bsle/ble_driver/android/bt_dev/bt_dev.c:323)
- H2D_MSG_BT_OPEN=25 / BT_CLOSE=26 (hcc_cfg_comm.h:120-125)
- Open trigger: `hci_bt_open()` calls `pm_ble_enable()` when HCI device brought up
  (`hciconfig hci0 up` / BlueZ) — NOT on a /dev open (differs from SLE)
- Module init (:618-666): `pm_ble_open()` → firmware download (first svc) →
  `hcc_service_init(HCC_ACTION_TYPE_BT)` → register HCI
- Build: driver/bsle/ble_driver/linux/Makefile — obj-m := ble_soc.o, ble_soc-objs :=
  ble_host_hcc.o (:10-11); uses EXTRA_CFLAGS (needs ccflags-y/isystem adaptation)

## 2. wifi_soc.ko = full cfg80211 driver

- Interfaces: wlan_ifname0/1/2 = wlan0/wlan1/p2p0 (ws73_cfg_default.ini:31/33/35,
  wal_linux_netdev.c:106-113); VAP count 1/2/3 (ini:27-29)
- Real cfg80211: `oal_wiphy_new(&g_wal_cfg80211_ops)` (wal_linux_cfg80211.c:6292),
  `oal_wiphy_register()` (:6312); full `struct cfg80211_ops` (.scan/.connect/.disconnect/
  .start_ap/... :6002) — **standard wpa_supplicant/hostapd (nl80211) can drive it**
- Build: driver/wifi/Makefile KO_NAME := wifi_soc, obj-m (351); ccflags-y + isystem
  adapted already (467-468); `-DDMAC_ON_HOST`, `-include autoconfig.h`
- wpa patch (open_source/wpa_supplicant/wpa_supplicant_2_10_linux.patch, 4298 lines) =
  **real protocol features** (WAPI stack, CONFIG_WAPI + roam/p2p/log patches), NOT a
  build shim — applies to stock 2.10, any arch

## 3. Tri-mode coexistence — CONFIRMED supported

- `pm_svc_state[]` array (plat_pm_wlan.c:140-142), `PM_SVC_WLAN=0, BLE, SLE`
  (plat_pm_wlan.h:79-83, 123)
- `pm_svc_open()` (:546-627): **only the FIRST service runs firmware download**
  (:588-598: `if (!(WLAN||BLE||SLE)) pm_init_n_firmware_download`); WLAN opens directly,
  BLE/SLE call `pm_bsle_open()` only when neither BLE/SLE already open, then
  `bsle_open_close_cmd(service, TRUE)`
- **No hard mutual exclusion** — all three can be open; pm_svc_close powers down only
  when ALL three close (:703-711)
- BT/WLAN coexistence compiled in: `_PRE_WLAN_FEATURE_BTCOEX` (wifi Makefile:187)

## 4. Userland stacks on x86 — mostly FREE via system stacks!

| Mode | Kernel module | Userspace | Source |
|---|---|---|---|
| BT | ble_soc.ko (hci0) | **stock BlueZ** (system bluetoothd) | ✅ free |
| WiFi | wifi_soc.ko (cfg80211) | **stock wpa_supplicant/hostapd** (nl80211) | ✅ free |
| SLE | sle_soc.ko (/dev/hwsle) | sparklinkd 系 = ARM only; OHOS stack or self-written | ❌ gap |

- All shipped userland binaries/libs are ARM (rk3568=aarch64/glibc-2.33, 3516V610=32-bit
  musl, MIPS t23/t41) — no x86 ELF anywhere
- Samples (application/sample/{ble,sle}) are x86-compilable in principle (CROSS= override)
  but link `${LIBDIR}/*.a` where lib/ has only placeholder ReadMe — HiSilicon ships
  libble_host.a/libsle_host.a only for ARM/MIPS → **SLE host stack closed on x86**
- WiFi userland: open_source.mk defines wpa_supplicant/hostapd targets (:89-119) but
  only wpa patch shipped (libnl/openssl/hostapd sources absent) — system packages suffice

## Bottom line (tri-mode full-speed path)

Load order: `plat_soc` → `ble_soc` → `sle_soc` → `wifi_soc` (first service does firmware
download). On x86: adapt ccflags-y/isystem (wifi+sle done; ble pending), confirm
hci_register_dev/cfg80211 symbols. BLE = stock BlueZ, WiFi = stock wpa_supplicant/hostapd,
SLE = /dev/hwsle + self-written HCI client or OHOS stack port (ticket 03 GREEN).
