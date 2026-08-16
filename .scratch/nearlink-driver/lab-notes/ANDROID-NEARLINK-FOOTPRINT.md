# Android NearLink Status, Architecture, and Device-Side Footprint

Compiled 2026-08-16.

## (a) Android NearLink status
- **No upstream AOSP NearLink** — googlesource has zero nearlink repos; Android 15/16/17 release notes never mention 星闪.
- NearLink on Android = vendor patch sets + community apps only.
- The full public Android implementation is HiSilicon's `application/sle_android/` in the WS73 host SDK: AOSP patches for Android 9/11/12.

## Android architecture (from sle_android patches) — mirrors classic BT, NOT a HAL
```
App (NearlinkDemo) → android.nearlink API (~35 classes: Adapter/Seeker/Announcer/SsapClient/Server/HidHost)
→ system_server NearlinkService → packages/apps/Nearlink (6 binder services, uid android.uid.nearlink)
→ JNI libnearlink_jni.so → prebuilt libsle_host.so (699KB closed) → kernel sle_soc.ko /dev/hwsle
→ HCC (USB/SDIO/UART) → WS73 chip
```
No AIDL/HIDL vendor HAL. Native stack entirely in closed libsle_host.so.

## OHOS counterpart — better reference for a lightweight open stack
- ~73 KLOC open C (dli + cp/bsl/sle + dp), proper HAL seam (SleDliLayerAdapter → HDI ISleHciInterface → HDF H4 framing over fd)
- DLI/HCI dialect (0xA1-0xA4, opcode u16 + len u16 preamble) matches our WS73 reverse-engineering

## (c) Device-side footprint
| Item | Value |
|---|---|
| WS63 flash | 8 MB (APP region 2 MB); L2RAM 640K; ROM 268K |
| SLE/BLE controller carve-out | CONFIG_BGLE_RAM_SIZE_16K/32K/64K (smallest 16K!) |
| Chip firmware ws73.bin (WiFi+BT+SLE) | 143 KB total |
| SLE host lib (archive) | libbth_gle.a 781 KB |
| BS2x flash | 1 MB |

## (d) Feature-trimming options (small boards)
- RAM: CONFIG_BGLE_RAM_SIZE_16K; radar RX 8K
- SLE: CONFIG_FEATURE_GLE_LOW_LATENCY/HADM (default OFF)
- AT: CONFIG_AT_GLE/AT_BLE (turn off to save flash)
- Middleware: FTM/NV/UPG/LFS/TIOT/PM/DFX/AT/CODEC each disable-able; FOTA is flash hog
- HILINK: extra SRAM+flash; disable for bare boards
- Target configs: ws63_liteos_btc_only_asic (no WiFi), bs21e_1100e_slp
- BS2x SLP: pick ONE usage (ranging_aox/air_mouse/air_mouse_car); SLE_BLE_CENTRAL vs PERIPHERAL

## (e) Implications for our lightweight host stack
1. Android approach NOT lighter — hides closed lib; take OHOS's seam instead
2. Transport seam is cheap (~1-2 KLOC DLI); SSAP/connection/discovery is where size lives
3. Trim profiles (HID/audio/ranging/JS/SA); bare SSAP client+server+CM is the core
4. Device side is NOT the budget problem (16K RAM, 143KB fw); host stack is
5. For 1MB-flash BS2x-class: budget LiteOS+kernel 100-200K, SLE host core 200-300K, app+NV 100K
