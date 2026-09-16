---
type: harvest
title: "hispark-rs/fbb_ws63-qemu — QEMU-oriented fbb_ws63 fork: what boots, what faults, and why (sparse-adopted)"
language: en
created: 2026-09-13
tags: [qemu, ws63, fbb_ws63, emulation, bootloader, liteos, rf-calibration, efuse, harvest]
sources:
  - "ArchivalEra/nearlink-E573H-USBDC workspace"
trust: verified
stale_after: 2026-12-13
---

# hispark-rs/fbb_ws63-qemu — QEMU-oriented fbb_ws63 fork: what boots, what faults, and why (sparse-adopted)

- Inspection date: 2026-09-13 (staleness check: pushed 2026-06-11, not archived — ALIVE)
- Method: metadata clone + sparse checkout of `README-QEMU.md` and `ci/` only (the emulator itself, `hispark-rs/hisi-riscv-qemu` aka ws63-qemu, is already in our local library)
- Scope: the fork's delta vs upstream fbb_ws63, boot status on the emulator, and the fault attribution for cut subsystems

## Executive findings

1. The fork's entire QEMU-specific delta is one config edit: in `src/build/config/target_config/ws63/config.py` the BT (`BGLE_TASK_EXIST`, `BTH_TASK_EXIST`) and WiFi (`WIFI_TASK_EXIST`) task-creation defines are commented out for the `ws63-liteos-app` targets; everything else is upstream fbb_ws63. [`README-QEMU.md:21-25`]
2. Boot status on ws63-qemu: `flashboot`/`loaderboot` reach UART output (clock bring-up), and `ws63-liteos-app` boots LiteOS, creates subsystem tasks, reaches `cpu 0 entering scheduler`, and idles cleanly with the BT/WiFi tasks cut — unmodified vendor-compiled firmware, no hardware. [`README-QEMU.md:9-13`]
3. The fault attribution for the cut subsystems is the reusable knowledge: BT and WiFi deep init "depends on on-chip ROM data / RF calibration / hardware that QEMU cannot model (no ROM/efuse dump)", so those tasks fault when enabled — the missing pieces for emulating radio init are an efuse dump and RF calibration data, not more CPU modeling. [`README-QEMU.md:23-25`]
4. The emulator side (already local as `hispark-rs/hisi-riscv-qemu`) implements the HiSilicon "xlinx" custom RISC-V ISA, models all 35 SVD peripherals, and intercepts mask-ROM calls, which is why unmodified vendor firmware runs at all. [`README-QEMU.md:15-17`, local `hispark-rs/hisi-riscv-qemu/README.md`]

## Boundaries and gaps

- 470MB tree not cloned; verdict is README+ci based. The one-file config delta claim was verified by the README only, not by diffing config.py against upstream.
- No SLE task in the fault list implies SLE init either shares the BT fault path (bts = BT+SLE combined stack) or was not separately exercised — not differentiated in the README.
- BLE/RF task status under newer emulator revisions may have moved; freshness date of this verdict is the fork's last push (2026-06-11).

## Reusable for our stack

- The explicit gap list for emulating WS63-family radio init — efuse dump + RF calibration data + ROM data interception — is a checklist for any future attempt to boot our WS73 firmware (or its SLE stack) under hisi-riscv-qemu-class emulators.
- The build-level workaround pattern (comment out subsystem task-creation defines instead of patching subsystem code) keeps the fork diff to one file — adoptable for our own hardware-dependent bring-ups.
- Confirms the local ws63-qemu emulator's boot coverage claims from the outside (independent fork README cross-references the same boot-to-scheduler status).

## Comparison anchors (vs existing reports)

- `NEW-HISPARK-RS-ECOSYSTEM.md` / `NEW-HISPARK-RS-SEPT-INCREMENT.md`: the emulator itself was covered there; this report adds the SDK-fork side and the radio-init fault attribution that those reports did not carry.
- `HHD01-BOARD.md`: the efuse/RF-calibration dependency explains part of why our board bring-up needed real hardware flashes rather than host simulation.
- `NEW-YL63-FORK-VERDICT.md` / `NEW-OHOS-DEVICE-SOC-WS63.md`: third application of the big-fork sparse playbook, smallest footprint yet (3.5M).
