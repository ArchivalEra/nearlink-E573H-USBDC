# SDK Intel: ws73_sdk_linux_WS73_1.10.110

> Version: WS73_1.10.110 (artifacts dated 2024-10-28, extracted 2025-03-12) · HiSilicon WS73 ("fregata" family) **host-side** Linux driver SDK
> Role: the host CPU drives the WS73 NearLink chip over USB/SDIO/UART via the **HCC** bus

## Top-level layout

```
ws73_sdk_linux_WS73_1.10.110/
├── Makefile                     # top-level build (Kconfig-style, uses build/scripts/hconfig.py)
├── application/
│   ├── bin/<platform>/          # prebuilt user-space daemons (ARM/uClibc or aarch64/glibc)
│   │   └── 3518_usb/{ble,sle}/ sparklinkd, sparklinkchba, sparklinkctrl, sle_chba.ko
│   ├── lib/<platform>/{sle,ble}/    # static libs libsle_host.a / libble_host.a (ARM prebuilt only)
│   ├── dft/                     # DFT channel: bp_test.ko + bp_channel
│   ├── sample/{ble,sle}/        # samples (sle_uuid_server/client etc.)
│   └── sle_android/             # Android 9/11/12 patches + NearlinkDemo app
├── build/
│   ├── config/                  # config templates: ws73_default.config(SDIO), ws73_light.config,
│   │                            #   ws73_usb_light.config, ws73_usb_light_v2.config, ws73_cfg_default.ini
│   └── scripts/                 # hconfig.py, build_host_hso.py etc.
├── driver/
│   ├── bsle/{ble_driver,sle_driver}/  # ble_soc.ko / sle_soc.ko sources
│   │   └── sle_driver/
│   │       ├── sle_dev/         # misc device /dev/hwsle
│   │       ├── sle_hcc/         # HCC service glue
│   │       ├── sle_chba/        # CHBA: sle_hci_chba_proc.c, sle_tm_chba_proc.c
│   ├── platform/
│   │   ├── hcc/{comm,cfg,host,slave,inc,octty,build}
│   │   │   └── host/hcc_usb_host.c   # ★ KEY: the ffff:3733 usb_driver "wireless_usb"
│   │   ├── firmware_download/   # plat_firmware.c firmware download framework
│   │   ├── main/ pm/ drv/ osal/ diag/ cfg/ exce/ libc_sec/
│   └── wifi/                    # full 802.11 host driver source → wifi_soc.ko
├── firmware/{e,us}/             # ws73.bin(143KB), wifi_cali.bin, btc_cali.bin, wow.bin
├── include/bsle/{ble,common,sle}/  # public SLE/BLE user-space API headers
├── open_source/                 # wpa_supplicant / kernel patches
└── output/                      # build output (empty)
```

## Build flow (top-level Makefile)

- `make` → `wifi_soc.ko` + `plat_soc.ko` (+ optional `ble_soc.ko`/`sle_soc.ko`/`bp_test.ko`)
- Config injection: `build/scripts/hconfig.py <Kconfig> <config> -a output/bin/autoconfig.h` (Kconfig/.config absent from this extraction; falls back to `ws73_default.config`)
- Cross-build: `make -C $(WSCFG_KERNEL_DIR) ARCH=$(WSCFG_ARCH_NAME) CROSS_COMPILE=$(WSCFG_CROSS_COMPILE) M=... modules`
- **USB reference config** `build/config/ws73_usb_light.config` essentials:

```
WSCFG_CROSS_COMPILE="arm-himix100-linux-"
WSCFG_KERNEL_DIR=.../Hi3518_SDK_0110_USB/osdrv/opensource/kernel/linux-4.9.y   # Hi3518EV300
WSCFG_BUS_USB=y
CONFIG_FIRMWARE_BIN_PATH="/etc/ws73/ws73.bin"
CONFIG_FIRMWARE_WIFICALI_PATH="/etc/ws73/wifi_cali.bin"
CONFIG_FIRMWARE_BSLECALI_PATH="/etc/ws73/btc_cali.bin"
CONFIG_INI_FILE_PATH="/etc/ws73_cfg.ini"
```

## Daemons (ARM prebuilt only, no source)

- **sparklinkd** — SLE protocol-stack daemon (GAP/GATT-like APIs, low latency, HID reports)
- **sparklinkchba** — CHBA (Channel Bridge Adapter) daemon: opens `/dev/hwslechba` + `/dev/hwsle`, bridges HCI data
- **sparklinkctrl** — control/test CLI
- **bluetoothd/bluetoothctrl** — BLE equivalents

## Directly reusable pieces (by value)

| Piece | Path | Purpose |
|---|---|---|
| **USB host driver** | `driver/platform/hcc/host/hcc_usb_host.c`(1793 lines) + `hcc_usb_host_ops.c`(1581 lines) | complete usb_driver binding `ffff:3733`, boot/kernel state machine, firmware download |
| **Firmware download framework** | `driver/platform/firmware_download/plat_firmware.c` | chunked transfer, SHA256 verify, `WRITEM/WMEM/RMEM/FILES/QUIT` command protocol |
| **SLE kernel module** | `driver/bsle/sle_driver/` | `/dev/hwsle` misc device, HCC service, CHBA hooks |
| **Protocol headers** | `driver/platform/hcc/comm/hcc_bus_usb_comm.h`, `hcc_usb_host.h` | VID/PID, 92-byte packet header, EP layout, buffer sizes |
| **Firmware blobs** | `firmware/us/*.bin` | images that must be pushed to the boot-stage device |
| **User-space API headers** | `include/bsle/sle/` | SLE application-layer interface reference |

## Platforms & USB

- `3518_usb` = Hi3518EV300 (ARM) + USB transport; `7205_usb`/`7205_sdio` are variants for another host chip
- WS73 is the USB **device**; the host runs the `wireless_usb` usb_driver
- Two-stage enumeration: boot(2 EP) → firmware download → re-enumerate kernel(5 EP)
- CHBA is a shared concept across WiFi/SLE (mac_chba_common.h)

## Limitations

1. Prebuilt daemons have no source (ARM/uClibc); on x86 the SLE HCI user-space must be implemented independently
2. `libsle_host.a` is ARM-only, cannot link x86
3. Reference kernel is Hi3518EV300's 4.9.y; but the USB/HCC/SLE code uses standard Linux USB APIs and is **arch-portable** (adjust `WSCFG_ARCH_NAME`/`WSCFG_CROSS_COMPILE`/`WSCFG_BUS_USB`)
4. Almost no docs (a few sample ReadMes only)
