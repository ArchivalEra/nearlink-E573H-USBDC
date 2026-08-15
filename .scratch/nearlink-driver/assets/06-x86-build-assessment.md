# 06 — Kernel Driver Skeleton: x86 Build Feasibility Assessment

Wayfinder ticket 06, progress asset. 2026-08-15. SDK: `sdk/ws73_sdk_linux_WS73_1.10.110`.

## Goal

Port the SDK's `wireless_usb` host driver (HCC-over-USB for `ffff:3733`) to build
and load on this x86-64 machine (kernel 7.1.5-x64v3-xanmod1, headers present),
so kernel-mode SLE channel bring-up (ticket 07's dead end in userspace) can run
with the full hcc framework context.

## What the driver is

`driver/platform/` builds `plat_soc.ko` containing the HCC bus framework:
- `driver/platform/hcc/comm/` — hcc core: `hcc.c` (hcc_header_init, channel mgmt),
  `hcc_bus.c` (hcc_send_message → control xfer), `hcc_service.c`, `hcc_channel.c`,
  `hcc_flow_ctrl.c`, `hcc_adapt.c`, `hcc_dfx.c`, `hcc_list.c`
- `driver/platform/hcc/host/` — transports: `hcc_usb_host.c` (usb_driver
  "wireless_usb", probe boot/kernel split, URB ring, INT notifications),
  `hcc_usb_host_ops.c` (bulk/control/reg-RW, patch I/O, message xfers),
  `hcc_heartbeat.c`, `hcc_uart_host.c`/`hcc_sdio_host.c` (skip)
- `driver/platform/cfg/` — `hcc_cfg.c` (channel/queue table),
  `customize_bsle.c` (INI push, bsle_msg_tag) ← the missing piece from ticket 07
- `driver/platform/osal/` — 19 .c abstraction layer (spinlock/mutex/wait/atomic/
  kthread/slab wrappers over Linux APIs)
- `driver/platform/firmware_download/` — `plat_firmware.c` (boot handshake,
  already validated byte-exact in ticket 01/05)

## Include surface (thin!)

`hcc_usb_host.c` includes only: `hcc.h`, `hcc_service.h`, `hcc_usb_host.h`,
`plat_pm_wlan.h`. Everything else pulls in through `td_*`/`osal_*`/`soc_osal.h`
headers — all present in `driver/platform/osal/include/` + `driver/platform/drv/`.

## Build approach (3 options)

| option | effort | notes |
|---|---|---|
| A. SDK top-level Makefile with x86 config | low-med | copy `ws73_usb_light.config`, override `WSCFG_KERNEL_DIR`=/lib/modules/$(uname -r)/build, `WSCFG_ARCH_NAME=x86`, empty `WSCFG_CROSS_COMPILE`. Top Makefile needs `prepare` (hconfig.py, python present). Likely ~15-30 min of config fixing. |
| B. Standalone Kbuild in repo | med | new `driver/ws73usb/Makefile` (Kbuild, `obj-m`), copy needed hcc/comm+host+cfg+osal sources + headers. Cleaner ownership, more wiring. |
| C. Hand-rolled minimal driver | high | fresh usb_driver using hcc sources only as reference (ticket 04 said "slim driver using hcc_usb_host.c as protocol reference"). Most work, cleanest code. |

## Risks / known blockers

1. **Kconfig absent**: top Makefile falls back to `ws73_default.config` (SDIO);
   must supply a USB-flavored config (`ws73_usb_light.config` exists in build/config/)
2. **Kernel API drift**: SDK targets 4.9/5.10; this kernel is 7.1.5 (xanmod). Likely
   compile breaks in osal (e.g. `osal_kthread_destroy`, `usb_alloc_urb` params,
   `struct usb_driver` fields). Fix-by-adapt, expect 10-40 errors initially.
3. **hcc depends on pm/wlan glue**: `plat_pm_wlan.h` pulls power-management code;
   may need stubs for PM svc calls not exercised in our bring-up path.
4. **firmware blob loading**: driver needs the 3 blobs at runtime (module param or
   firmware_request). blobs stay out of git; path configurable.

## Recommendation

Try **option A first** (fastest proof the SDK builds on x86 at all), iterate on
compile errors, then if the SDK tree proves too entangled, fall back to **option B**
(standalone Kbuild with copied sources) — still reusing the vendor code as-is
rather than re-writing (option C) until the kernel-mode channel is proven.

## Prototype decision points (for user)

1. A (SDK tree) vs B (standalone copy) vs C (fresh) as first skeleton?
2. Kernel channel bring-up order: replicate ticket 07 sequence (INI push +
   SLE_OPEN) via the real hcc framework on the host, or go straight to exposing
   /dev/ws73hci and letting userspace drive HCI frames?
3. Coexistence with `wifi_soc.ko`-style modules: our driver will claim
   ffff:3733 — fine today (nothing else binds it).
