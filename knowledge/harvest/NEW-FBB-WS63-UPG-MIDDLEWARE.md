---
type: harvest
title: "fbb_ws63 UPG middleware — the device-side upgrade engine: eight-step package verification, a three-slot flash flag retry protocol, in-place KNVD diff patching with page journaling, and boot-time orchestration"
language: en
created: 2026-09-17
tags: [fbb_ws63, ws63, hisilicon, upg, fota, upgrade, diff-patch, lzma, flash, flashboot, harvest]
sources:
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/common/upg_common.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/common/upg_verify.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/common/upg_alloc.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/local_update/upg_patch.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/local_update/upg_process.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/local_update/upg_upgrade.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/local_update/upg_lzmadec.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/local_update/upg_resource.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/local_update/upg_encry.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/storage/upg_storage.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/adapter/xts/upg_xts_adapt.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_definitions.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_common.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_patch.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_patch_info.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_verify.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_encry.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_lzmadec.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_otp_reg.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_alloc.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/upg_default_config.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/inner_include/rom_public_key.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/common/upg_common_porting.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/ab_upg/upg_ab.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/local_update/upg_encry_porting.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/storage/upg_backup.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/include/upg_ab.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/include/upg_common_porting.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/include/upg_config.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/include/upg_debug.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/include/upg_definitions_porting.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/include/upg_encry_porting.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/chips/ws63/update/include/upg_porting.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/include/middleware/utils/upg.h"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/bootloader/flashboot_ws63/startup/main.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/application/ws63_porting_xf_0p2/tasks_xf_entry/port_xf_ota_client.c"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/build/config/target_config/ws63/build_ws63_update.py"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/CMakeLists.txt"
  - "https://github.com/x-eks-fusion/fbb_ws63/blob/f0fbaef71197650e7a25695bb814e8fadca511ba/src/middleware/utils/update/Kconfig"
trust: A
stale_after: 2027-03-17
---

# fbb_ws63 UPG middleware — the device-side upgrade engine: eight-step package verification, a three-slot flash flag retry protocol, in-place KNVD diff patching with page journaling, and boot-time orchestration

One implementation topic: the device-side upgrade (UPG) middleware shipped inside the fbb_ws63 SDK — how a running WS63 device ingests an upgrade package into a reserved flash partition, verifies it, applies full / LZMA-compressed / differential images, records per-image retry state in a flash flag area, and how flashboot drives and repairs that process at boot. Package *production* (host-side signing tools) is covered only as build-wiring context; bootloader image-verification primitives and host flashers are out of scope. This is not a flashing-protocol investigation, a security assessment, or a hardware procedure.

## Scope and provenance

- Inspection date: 2026-09-17; read-only inspection of a local mirror of `x-eks-fusion/fbb_ws63` (shallow clone, depth 1; working tree clean except one untracked `.obj` build artifact outside the inspected paths).
- Pinned revision: commit `f0fbaef71197650e7a25695bb814e8fadca511ba` (2025-03-13T06:41:24Z, author `dotchan`, branch `master`), tree `455ec1e9c96ea3634cd9bed71561cd243068e28c`, origin `https://github.com/x-eks-fusion/fbb_ws63`. Origin and object identities were verified from local Git metadata only; remote availability and current upstream freshness were not checked.
- Every file in the table below was read fully. Hashing each read working-copy file with Git's blob algorithm matched its entry in the pinned HEAD tree (39/39 OK). Paths and line references are repository-relative to this revision; GitHub URLs are constructed from the verified remote plus the commit and were not fetched (no network this round).
- Vendor-provenance cross-check: the same paths were queried in a second local clone of the official upstream (`gitee.com/HiSpark/fbb_ws63`, HEAD `aa8f856be4a9253a4518b80399b5da16fba76bd1`, 2026-08-14, checkout-less but object-complete). The entire `src/middleware/utils/update/` core and most porting headers are **byte-identical** across both revisions (17 of 25 sampled files). Eight files differ (`upg_common_porting.c`, `upg_ab.c`, `upg_encry_porting.c`, `upg_backup.c`, `upg_ab.h`, `upg_common_porting.h`, flashboot `main.c`, `build_ws63_update.py`) — the fork's snapshot of the chip porting layer predates upstream evolution. Spot checks show both headline quirks below (the `upg_ab_start` guard, the MSID always-success stub) are present unchanged in the 2026-08 official upstream.
- Trust A means source-derived findings from byte-verified vendor sources; no build, no execution, no device, no flash-image or hardware inspection was performed. Line numbers are exact for the pinned blob; adjacent-line drift under other revisions is expected.

| Fully read file (repo-relative) | Git blob SHA-1 |
|---|---|
| `src/middleware/utils/update/common/upg_common.c` | `236ee6f3d495c01e090914049c0c939c551ee154` |
| `src/middleware/utils/update/common/upg_verify.c` | `0776705e87afaad3224e2d2cf3940ffb09e1b8d3` |
| `src/middleware/utils/update/common/upg_alloc.c` | `01ceca66478c728abc7be9a1f1a0e6eaf30ad468` |
| `src/middleware/utils/update/local_update/upg_patch.c` | `0118d624f9f553136199f409d972378c02c7768e` |
| `src/middleware/utils/update/local_update/upg_process.c` | `b47d7c6b3e3a9936c5857a6b215c06b0d9629133` |
| `src/middleware/utils/update/local_update/upg_upgrade.c` | `19fb027b56fd918ba1a4c2d4ace7fc3c4daa7f81` |
| `src/middleware/utils/update/local_update/upg_lzmadec.c` | `0395078fb310aabbfdb212555b86e4f11a83bb89` |
| `src/middleware/utils/update/local_update/upg_resource.c` | `eded6d9338c294411859fb0dd4dc985e282b6761` |
| `src/middleware/utils/update/local_update/upg_encry.c` | `caa69b49600561bf6fa68baeff119a377257218f` |
| `src/middleware/utils/update/storage/upg_storage.c` | `080ef98a0b7199b48a48533489eea1b1e3c3e9ae` |
| `src/middleware/utils/update/adapter/xts/upg_xts_adapt.c` | `98c76d6f6f6603e9d0c7b61b3386888af69373d5` |
| `src/middleware/utils/update/inner_include/upg_definitions.h` | `6bc312a9376bfa962e1729332ed0de4626dd2d0d` |
| `src/middleware/utils/update/inner_include/upg_common.h` | `d56d9efc41e8e9ef1396de1dabc8fa7b40ccb764` |
| `src/middleware/utils/update/inner_include/upg_patch.h` | `7b66143f01c0b889baaeef90b1292a009f68f18a` |
| `src/middleware/utils/update/inner_include/upg_patch_info.h` | `b3fb3d9c2b08460af54d7f35ce35a72bd92ff94e` |
| `src/middleware/utils/update/inner_include/upg_verify.h` | `baeedade30507211cbc9511286617798947c2676` |
| `src/middleware/utils/update/inner_include/upg_encry.h` | `a2a0896e694a7a1621ecdc57fd9d53f8dcfc6c1b` |
| `src/middleware/utils/update/inner_include/upg_lzmadec.h` | `0906bc3c8f26147532f16afd4a5444a43d52fa41` |
| `src/middleware/utils/update/inner_include/upg_otp_reg.h` | `713ba51e35024ae0c24d872deb6edd2ed19ada4a` |
| `src/middleware/utils/update/inner_include/upg_alloc.h` | `b4e1f268071094da372959ad2cdc61c8fa17f073` |
| `src/middleware/utils/update/inner_include/upg_default_config.h` | `16d95065e4d3007f4ed088f9deb0ad24056aa85f` |
| `src/middleware/utils/update/inner_include/rom_public_key.h` | `d1f29141ddf40737b31c9aecacbfb81f23ecf0fa` |
| `src/middleware/chips/ws63/update/common/upg_common_porting.c` | `d8b9e4698aecae8d0e4fca9e5b338facd3a92692` |
| `src/middleware/chips/ws63/update/ab_upg/upg_ab.c` | `e196edf4c882fd79dbcef2c4bf82a86ad5ea7b6c` |
| `src/middleware/chips/ws63/update/local_update/upg_encry_porting.c` | `a70d20e0966c8816bbb45aabb43d7720f381fa3e` |
| `src/middleware/chips/ws63/update/storage/upg_backup.c` | `49525256514f4b92c616265ff889bc8b92293aa1` |
| `src/middleware/chips/ws63/update/include/upg_ab.h` | `aec0d73184309512ceefc54358b53892f173fdf5` |
| `src/middleware/chips/ws63/update/include/upg_common_porting.h` | `ccb96bf5cac5176e934e2aa34cae6bcfd96ce041` |
| `src/middleware/chips/ws63/update/include/upg_config.h` | `4cbe02151a9cfcd5d98e8e1388749d3633c98b8f` |
| `src/middleware/chips/ws63/update/include/upg_debug.h` | `ed55a72a15b001f7837e738115964bafb1f59448` |
| `src/middleware/chips/ws63/update/include/upg_definitions_porting.h` | `219a7514d9a58a3e65a044b9c55e84402ecc5f21` |
| `src/middleware/chips/ws63/update/include/upg_encry_porting.h` | `123abf3344259a093ea543e88e7250ea087a55fb` |
| `src/middleware/chips/ws63/update/include/upg_porting.h` | `c9cf5cc4d6526be60edc8fe5a3fd5b0dc0adb889` |
| `src/include/middleware/utils/upg.h` | `954a43d16cabe1452a6f42892fc5a90753237cb6` |
| `src/bootloader/flashboot_ws63/startup/main.c` | `01138f10dec132d286d29987ed4da5281fccc3f3` |
| `src/application/ws63_porting_xf_0p2/tasks_xf_entry/port_xf_ota_client.c` | `cc946acf48da24fa9502ff0f46ac15d60bd958bc` |
| `src/build/config/target_config/ws63/build_ws63_update.py` | `c529b048ba39a7bce9a1e50f366be324969ae57b` |
| `src/middleware/utils/update/CMakeLists.txt` | `4f2350cf96392a9eeceb23c881472c6d1da8ee73` |
| `src/middleware/utils/update/Kconfig` | `e7002dee05a789374e4c84b945e0eafad78dbe08` |

## Deduplication and exact novelty

The [harvest index](index.md) and the complete knowledge bundle were searched for `upg_`, `upg_verify`, `upg_patch`, `upg_process`, `upg_storage`, `upg_lzmadec`, `middleware/utils/update`, `fota_upgrade_flag`, `KNVD`, and OTA/FOTA/upgrade terminology. The only in-bundle hits are incidental (an "upgrade/sign tooling" pycparser note in `WS63-BUILD-FLASH.md`; an unrelated "strict upgrade" phrase in `03-ohos-stack-port.md`). Relevant prior reports were read, not skimmed by title:

- [The WS63 flasher + Ghidra report](NEW-WS63FLASH-GHIDRA.md) covers the *host* flasher tool, the boot-ROM 0xF0 handshake, YMODEM staging of loaderBoot, and empirical fwpkg parsing. It documents the download path into flash, not what the device does with an upgrade package afterward.
- [The xf_burn_tools report](NEW-XF-BURN.md) covers the Python host flasher and the `fwpkg.py` manifest — again the host/transport plane.
- [The browser flasher reports](NEW-WEB-FLASHER-FWPKG.md) and [the protocol quadruple](NEW-WEB-FLASHER-QUADRUPLE.md) cover browser-side flashing and container format synthesis — no device-side apply engine.
- [The hisi-fwpkg planner report](NEW-HISI-FWPKG-IMAGE-PLANNING.md) covers a *Rust host-side* container-to-image planner in a different repository (hispark-rs); its closed-topic status is respected — this report does not reslice container planning, and its plane (host planning) is disjoint from this one (device application).
- [The ws63v100 middleware map](NEW-WS63-MIDDLEWARE-OPEN-CLOSED.md) surveyed `device_soc_hisilicon` `ws63v100/sdk/middleware` sparse-checkout scope (at, hcc, fs, dfx...); the `utils/update/` tree of the fbb_ws63 SDK was not in its inventory and no `upg_*` contract appears in the bundle.
- [The XFusion port report](NEW-FBB-WS63-XFUSION-PORT.md) read the SLE port and bootstrap of `ws63_porting_xf_0p2` and inventoried the rest; `tasks_xf_entry/port_xf_ota_client.c` is read in full here (the sibling port report had cited only its write-path lines), closing that inventoried gap from the UPG-consumer side.
- The NV/flash and crypto reports ([hisi-nvs](NEW-HISI-NVS-PERSISTENCE.md), [hisi-crypto](NEW-HISI-CRYPTO-ENTROPY-DRBG.md)) cover different modules; the UPG code only *calls* flash and hash primitives and records an NV handoff (see finding 9).

The new coherent contribution is the **device-side apply engine**: the two-area package header and its eight-step verification chain, the three-slot monotonic flash flag protocol with half/complete semantics, the three image pipelines (full / LZMA stream / in-place KNVD delta with page journaling and crash recovery), the porting-layer trust and flash primitives (including erase-with-writeback and a KLAD re-encryption path), A/B region management, and the boot-time orchestration and repair loop in flashboot. No matching implementation treatment exists in the bundle.

## Runtime limits

- The middleware builds as components `update_common`, `update_local`, `update_storage` (`src/middleware/utils/update/*/CMakeLists.txt`) plus WS63 porting components `update_common_ws63`, `update_local_ws63`, `update_storage_ws63`, `update_ab_ws63` (`src/middleware/chips/ws63/update/*/CMakeLists.txt`). The top-level `update/CMakeLists.txt` also references `ssb_fota` and `ota` subdirectories via `add_subdirectory_if_exist`; neither exists in-tree, so those variants are absent, not closed.
- The default application-core build config disables it: `# CONFIG_MIDDLEWARE_SUPPORT_UPDATE is not set` in `src/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_app.config:613`. In the application image it initializes only under `CONFIG_NV_SUPPORT_OTA_UPDATE` (`src/application/ws63/ws63_liteos_application/main.c:407-413`).
- The shipped WS63 product config (`upg_config.h:18-43`) enables verification with **ECC** mode, direct flash access, whole-image erase and progress notification — but compiles **out** differential upgrade (`UPG_CFG_DIFF_UPGRADE_SUPPORT NO`), anti-rollback (`UPG_CFG_ANTI_ROLLBACK_SUPPORT NO`), filesystem paths, NV-upgrade support and re-encryption. A `DECOMPRESS_FLAG_DIFF` image therefore reaches `uapi_upg_diff_image_update`, which returns `ERRCODE_UPG_NOT_SUPPORTED` under this config (`upg_upgrade.c:102-113`).
- `Kconfig` declares `BT_UPG_ENABLE` and `SLE_UPG_ENABLE` (upgrade over BT/SLE links) but no in-tree source consumes either flag — their consumers would live in the closed BT/SLE libraries.
- The XTS HOTP HAL adapter has no in-tree CMakeLists wiring; it is an integration template, not a wired consumer.

## Executive findings

1. **The upgrade package is a two-area header with a hash-table-of-headers trust chain.** `upg_package_header_t` = key area + FOTA info area (`upg.h:236-241`). The key area carries struct version/length, signature length, owner/key/key-algorithm IDs, ECC curve type, MSID fields, maintenance mode, die ID, a 52-byte reserved block, a 64-byte external public key and its 64-byte signature (`upg.h:130-169`). The info area carries the FOTA version, MSID, image-hash-table address/length/hash, image count, hardware ID, a 112-byte user-defined field and its signature (`upg.h:178-209`). Each `upg_image_hash_node_t` (image ID, header address, length, 32-byte hash) hashes an image header (`upg.h:218-227`), and each `upg_image_header_t` carries the hash of its image body plus old-image hash/length for differential upgrades (`upg.h:250-287`). The header magic is `0x464F5451` (`upg_definitions.h:24`), checked in `uapi_upg_verify_file_image` (`upg_verify.c:767-771`).
2. **Verification is an eight-step chain, and a 32-byte "signature" means plain hash-compare.** The in-source comment enumerates the steps (`upg_verify.c:535-543`): root key verifies the key area; the key area's external key verifies the FOTA info; an optional user-registered callback checks the user-defined field (`upg_verify.c:437-441,493-498`); the hash table is verified against the info area's table hash (`upg_verify.c:568-576`); each image header against its table entry; each image body against its header hash (`upg_verify.c:773-784`); for differential images the flash-resident old image against the old hash (`upg_verify.c:786-797,700-752`); and anti-rollback versions against eFuse (`upg_verify.c:519-526`, compiled out in the shipped config). In all three verify sites, `signature_length == SHA_256_LENGTH (32)` switches the check to an unkeyed SHA-256 comparison instead of a signature (`upg_verify.c:346-350,391-395,407-411`) — a downgrade path selected by the package's own declared field. Verification algorithms are compile-time-selected: ECC (default, RFC5639 BrainpoolP256r1 via the unified PKE, or a ROM boot-sample verifier under `CONFIG_MIDDLEWARE_SUPPORT_UPG_SAMPLE_VERIFY`, `upg_verify.c:237-271`), SM2/SM3 with default ID `"1234567812345678"` (`upg_verify.c:43-44,197-235`), or RSA-4096 PKCS#1 v2.1 with e=65537 left-padded into a 512-byte buffer (`upg_verify.c:46-48,273-317`).
3. **The WS63 port reads its root key from a flash partition, and the fallback is self-referential.** With direct flash access (the shipped setting), `upg_get_root_public_key` maps `PARTITION_FLASH_ROOT_PUBLIC_KEYS_AREA` and returns the `root_key_area` of an on-flash `root_public_key` struct whose header declares algorithm `0x2A13C812` (ECC256) vs `0x2A13C823` (SM2) and curve RFC5639 BrainpoolP256r1 (`upg_common_porting.c:353-378`; `upg_common_porting.h:62-75`). Without direct flash access the "root" key is instead copied from the package's own `key_area.fota_external_public_key` (`upg_common_porting.c:366-376`) — the key area would then be verified with the key embedded in itself. The shipped config avoids this path, but the fallback is a structural trust-boundary fact, stated here as source behavior. A ROM-curve parameter set (32-byte field/generator/order constants, curve type `DRV_PKE_ECC_TYPE_RFC5639`) is additionally embedded for the sample-verify path (`rom_public_key.h:16-42`).
4. **Progress state lives in a 12 KiB tail of the FOTA partition and moves through a three-slot monotonic flag protocol.** The FOTA partition ends with [package storage][buffer 4K][write-status 4K][flag 4K] (`upg_porting.h:52-71,40-43`); the flag area starts at `addr + size - 4K` and the progress-status area at `addr + size - 12K` (`upg_common_porting.c:112-143`). `fota_upgrade_flag_area_t` holds magic pair, head-before offset, package length, firmware count, per-firmware flag triples (`firmware_flag[20][3]`), an NV triple, version-change flag, result, NV offsets/lengths and complete flag (`upg_definitions.h:65-80`). Flags move `0xFF` (not started) to `0x0F` (started) to `0x00` (retry consumed) within a slot, then FINISHED clears all three — transitions only clear bits, so every state change is a single flash write without erase, and a reboot simply resumes at the first non-zero slot (`upg_common.c:30-36,574-660,668-704`). explicit constants are `0xFFFF` half-finished (this program's images done, more remain), `0` all-finished, and `0x5A5A5A5A` abnormal — the value written when no branch applies (`upg_common.c:781-843`, writes at :784,:842); the erased value `0xFFFFFFFF` satisfies the armed check (`complete_flag != 0`) but is never explicitly written. `uapi_upg_get_result` returns the stored result but always writes `UINT32_MAX` into `last_image_index` — the documented out-param is never computed (`upg_common.c:846-861`).
5. **Full-package verification runs once; per-image verification carries resume correctness.** `uapi_upg_start` gates on the armed state (magic pair + non-zero complete flag, `upg_process.c:337-343`), runs the whole-package 8-step verify only when every firmware flag is still `0xFF` (`upg_process.c:351-359`; `upg_check_first_entry` at `upg_process.c:304-314`), and thereafter relies on `uapi_upg_verify_file_image` per image before applying it (`upg_process.c:147-157`). Failure consumes one retry slot and switches to RETRY; success switches to FINISHED and (when enabled) advances the eFuse rollback version (`upg_process.c:160-176`). Firmware count must equal image count or image count minus one (one NV image) and is capped at 20 (`upg_process.c:284-287`; `upg_storage.c:266-292`).
6. **Three image pipelines share one bounds model: image data lengths are treated as 16-byte-aligned.** Every read/verify path clamps to `upg_aligned(image_len, 16)` (`upg_common.c:432-448,460-481`). Full-image update erases first and then copies page-size (4 KiB) chunks (erase-before-write ordering), kicking the watchdog and reporting progress per chunk (`upg_upgrade.c:116-170`). Compressed update reads a 16-byte LZMA header (5 property bytes + 8 size bytes + 3 alignment), initializes an LZMA decoder with dictionary `min(props dicSize, declared unpacked size)`, and streams decode-to-flash in 1 KiB in / 4 KiB out buffers (16 KiB out when re-encryption is enabled), flushing whenever less than 1 KiB of output space remains (`upg_lzmadec.c:89-135,192-270`; `upg_lzmadec.h:20-27`). Resource images (index + data) are filesystem operations driven by a 128-byte-path node table (`upg_resource.c:101-147`; `upg_definitions.h:92-102`) and only compile with resource support enabled (disabled in the shipped config).
7. **The differential pipeline is a byte-wise, in-place delta engine ("KNVD") with its own flash page journal.** The patch stream is LZMA-compressed control/data: 5 property bytes, a 3-byte unpacked-size field, then 16-byte control blocks `{magic "KNVD", copy, extra, seek}` (`upg_patch.c:30-31,181-218`; `upg_patch_info.h:58-67`). Applying a delta writes `old_byte + delta_byte` at the new position, walking bottom-up when the new image is larger (`bottom_up = old_image_len < new_image_len`, `upg_patch.c:1228`) and top-down otherwise (bottom_up walker `:720-742`, top_down `:750-771`). Writes buffer a full 4 KiB page in RAM; a per-page status byte records write type (partial-start `1<<1`, standard `1<<3`, partial-end `1<<5`) plus two journal bits — `0x80` "page staged in buffer", `0x40` "page written to image" — cleared only after the image write succeeds (`upg_patch_info.h:29-35`; `upg_patch.c:508-586`). On restart, a scan finds at most one page with the staged bit set and not yet committed, rewrites it from the buffer page, and refuses to continue if two are found (`upg_patch.c:1075-1136`). The engine also carries dead failure-injection plumbing (`failpoint`/`failfn` declared at `upg_patch.h:106-108`, always zeroed here at `upg_patch.c:1230-1232`).
8. **The patch engine allocates through a size-prefixed wrapper and candidly distrusts LZMA.** `fota_patch_alloc` stores an `int32` size header before each block (`upg_patch.c:47-62`), and `zip_init` sets `z->dec->probs = NULL` with the in-source comment "LZMA is absolute trash. Required to prevent a segfault" (`upg_patch.c:239-240`).
9. **Flash primitives are porting-layer code with real wear-awareness.** `upg_flash_erase` aligns any range out to 4 KiB sectors, reads back and rewrites the partial head and tail bytes so an erase never destroys neighbors (`upg_common_porting.c:289-347`); `upg_flash_write` wraps SFC writes in an IRQ lock with an optional read-back compare debug mode (`upg_common_porting.c:243-287`). The trust anchor comes from the flash `PARTITION_FLASH_ROOT_PUBLIC_KEYS_AREA` partition in direct-access mode (the shipped config); in non-direct mode the "root" key is instead copied from the package's own `fota_external_public_key` field (`upg_common_porting.c:366-376`). When enabled, re-encryption derives both a decrypt channel (key type `ABRK_REE`) and an encrypt channel (`ODRK1`) from a 28-byte salt taken from the image header's `enc_pk_l1` field with KLAD key derivation, and the "encrypt" primitive is implemented as `drv_rom_cipher_symc_decrypt` on both channels (`upg_encry_porting.c:47-148`) — symmetric in fact, a notable source oddity.
10. **Boot orchestration and repair live in flashboot, not the app.** `start_fastboot` calls `ws63_upg_check()` before app verification (`flashboot main.c:397-440`): it initializes UPG with flashboot's own allocator, checks the armed flag area, runs `uapi_upg_start()`, and then unconditionally reboots (`flashboot main.c:260-283`). Application verification failure enters a repair loop: with A/B disabled, after 3 consecutive verification failures it resets the upgrade flag and re-runs `uapi_upg_request_upgrade(false)` (re-applying the stored package), capped at 6 tries (`flashboot main.c:165-168,202-231`); with A/B enabled it switches the run region every 3rd failure (`flashboot main.c:185-199`). A flashboot self-recovery path restores the boot image from its backup partition when a backup-boot register is set (`flashboot main.c:129-163`).
11. **A/B mode splits APP + FOTA partitions into two run regions with a 4 KiB config page at the FOTA end.** Region A is the app partition, region B the FOTA partition; each region is `(a+b-4KiB)/2` rounded up to 4 KiB, and the config block carries check value `0x70746C6C` plus the run-region index (`upg_ab.c:16-18,35-41,181-225`). `upg_set_run_region` refuses a no-op switch (`upg_ab.c:269-287`).
12. **The ingest side is a three-call state machine, and everything else hangs off it.** `uapi_upg_prepare` erases the FOTA area and writes `head_before_offset=0`, `package_length`, then `head_magic` (`upg_storage.c:61-118`); `uapi_upg_write_package_sync/async` stream chunks with bounds against package length (`upg_storage.c:195-244,416-432`); `uapi_upg_request_upgrade` verifies the whole package, writes `firmware_num` and the end magic — arming the flag area — and optionally reboots (`upg_storage.c:298-369`). The OHOS HOTP HAL adapter wraps exactly these calls (`upg_xts_adapt.c:78-128`), and the XFusion OTA port maps `xf_ota_init/start/write/upgrade` onto them in strictly sequential fashion (`port_xf_ota_client.c:59-68,155-185,218-239,203-216`).

## 1. Package format and the verification chain

The public header defines the wire structs (`upg.h:130-287`). The key area carries struct version/length, signature length, owner/key/key-algorithm IDs, key and MSID version/mask pairs, a maintenance-mode field with die-id, a 52-byte reserved block, the 64-byte external public key, and the 64-byte key-area signature. The info area carries the FOTA version pair, MSID pair, hash-table location and length, its own SHA-256, image count, hardware id, a 112-byte user-defined field, and its signature. The `user_defined` doc comment still says 48 bytes while the macro is 112 (`INFO_AREA_USER_LEN`, `upg_definitions_porting.h:82`) — comment/code drift.

The chain is verified top-down (`uapi_upg_verify_file`, `upg_verify.c:545-584`), with each failure recorded into the context's `temporary_result` so the flag area can persist a reason (`upg_verify.c:556-575,769-796`). Hash computation goes through the driver-cipher HASH interface in 4 KiB segments (`VERIFY_BUFF_LEN 0x1000`, `upg_verify.c:588-640`), with a secure-buffer attribute chosen by core role (`upg_verify.c:58-62`). RSA mode hardcodes e=65537 left-padded into a 512-byte buffer, i.e. RSA-4096 with PKCS#1 v2.1 (`upg_verify.c:46-48,295-307`).

The OTP header (`upg_otp_reg.h:17-45`) documents the OTP shadow-register base `0x500E0000` and a lockable word with per-algorithm disable bits (sha1/sm4/sm3/sm2), consumed only as address constants by this module.

## 2. The flag area and its state machine

`fota_upgrade_flag_area_t` (`upg_definitions.h:65-80`) holds the magic pair, `head_before_offset`, package length, firmware count, 20x3 firmware flags, 3 NV flags, a version-change byte, the persisted result, NV data/hash offsets recorded by the UPG processor, and `complete_flag`. Status derivation treats all-`0x00` as finished, all-`0xFF` as not-started, and otherwise inspects the first non-zero slot (`upg_common.c:668-704`). `upg_get_status` (used at init) consumes the version-change byte by writing it back to zero after reporting success/failure (`upg_common.c:874-918`). `uapi_upg_get_result` returns the stored result but always writes `UINT32_MAX` into `last_image_index` — the documented out-param is never computed (`upg_common.c:846-861`).

NV is special-cased end to end: the processor does not flash an NV image but records its package offset/length and header-hash location into the flag area and marks it started (`upg_process.c:180-208`); the NV module later applies it via `nv_upg_upgrade_task_process` using exactly those fields and then sets the final NV flag (`src/middleware/utils/nv/nv_storage_lib/nv_upg.c:40,356-390`, consulted via grep).

## 3. Image application pipelines

Dispatch is by `decompress_flag` (`upg_process.c:88-130`): `0x3C7896E1` (labeled ZIP, the same constant as the encryption-enabled flag), `0x44494646` (ASCII "DIFF"), otherwise full image. Resource images (index/data) are filesystem operations driven by a 128-byte-path index table (`upg_definitions.h:92-102`; `upg_resource.c:22-147`) and only compile with resource support enabled (disabled in the shipped config).

The LZMA pipeline is restart-safe only at whole-image granularity — a failed compressed upgrade consumes a retry slot and restarts from offset 0 on the next pass. The KNVD pipeline is the opposite: it is page-journaled so an interrupted diff resumes mid-image, but note the interaction — the flag slot is only marked finished after `process_patch` returns, and recovery state is keyed by flash, not by the flag area. The buffer-area precondition is explicit: `fota_buffers_has_contents` requires the whole status area to be erased `0xFF` before treating a run as fresh (`upg_patch.c:337-378`).

## 4. Porting layer, and what the boot flow does with it

Beyond finding 9-10: image-id to partition mapping is a three-entry table (flashboot, application, HiLink; `upg_common_porting.c:42-46`) with the upgradeable set fixed to four ids including NV (`upg_common_porting.c:62-67`); anti-rollback reads/writes eFuse offsets keyed only for flashboot and application images (`upg_common_porting.c:422-435`), with board masks fixed to key-mask 0 / code-mask `0xFFFFFFFF` (`upg_common_porting.c:447-453`) — under the shipped config the whole anti-rollback path is compiled out anyway. The chip-level `upg_backup.c` is a one-function stub returning success (`upg_backup.c:20-23`).

## 5. Factual defect census (source-observed, no exploitation analysis)

- `upg_ab_start` rejects exactly the two valid regions: the guard `if (upg_region == UPG_REGION_A || upg_region == UPG_REGION_B) return ERRCODE_INVALID_PARAM;` inverts the intended test, so any valid A/B call fails and the erase never runs (`upg_ab.c:316-326`; enum `upg_ab.h:8-12`). The same text is present in the current official upstream.
- The MSID board-match check logs a mismatch but always returns success — the in-source comment says the MSID eFuse address is not yet determined (`upg_common_porting.c:380-395`, comment at 394).
- `HotaHalGetUpdateIndex` has no return statement when the AB macro is defined — the function body is only conditionally populated (`upg_xts_adapt.c:90-96`).
- `uapi_upg_write_package_async` invokes its "completion" callback inline after a synchronous write; it is synchronous in fact (`upg_storage.c:221-233`).
- The filesystem package writer keeps `static FILE*`/position across calls — single-stream, non-reentrant state (`upg_storage.c:371-414`).
- The LZMA header parser sums 11 bytes (8 size + 3 alignment) into a `uint32` unpacked size; with the 16-byte header the loop's shift counts reach 64-80 bits on 32-bit ints (`upg_lzmadec.c:106-109` with `upg_lzmadec.h:20-21`) — latent undefined behavior if alignment bytes are non-zero.
- `upg_flash_erase_metadata_pages` is an empty stub returning success (`upg_common.c:707-710`); `upg_image_backups_update` likewise (`upg_backup.c:20-23`).
- `upg_get_board_version` falls through to the last-iterated image header when the requested id is absent (`upg_common_porting.c:455-491`).
- The AB-boot path writes into a string literal (`char *boot_str = "A"; *boot_str = ...`, `flashboot main.c:422-426`).
- `UPG_IMAGE_ID_KEY_AREA` and `UPG_IMAGE_ID_FOTA_INFO_AREA` share one constant `0xCB8D154E` (`upg_definitions.h:22-23`), and the encryption/ZIP flags share `0x3C7896E1` (`upg_definitions.h:31-32`; `upg_lzmadec.c:21`).
- The re-encryption salt comment describes `enc_pk_l1`(16)+`enc_pk_l2`(first 12) as a 28-byte salt, but the code copies all 28 bytes from `enc_pk_l1` alone (`upg_encry_porting.c:66-74` vs `upg_encry_porting.h:22`).
- Six differently-named eFuse version offsets all share address `0xF0` (`upg_common_porting.h:55-60`).
- The embedded OHOS adapter validates package version by `strncmp` equality of current vs package version string (`upg_xts_adapt.c:207-210`).

## Boundaries and gaps

- Read-only, source-only: no builds, no flash-image dumps, no device runs. The LZMA producer alignment-byte convention and the root-key partition contents are asserted only by code, not by artifact inspection.
- The fork's chip-porting layer is a 2025-03 snapshot; official upstream had touched those files by 2026-08 (the two quirks re-checked persist). Findings are pinned to the fork revision; blob SHAs make every claim re-checkable against either remote.
- `build_upg_pkg.py`, `fota_format_st.py`, `nv_upg.c`, the app `main.c` and `at_plt.c` were consulted (grep/partial) for integration boundaries, not audited as topics; they are natural follow-up slices (host-side package producer; NV-application half; AT surface).
- The `ssb_fota`/`ota` CMake branches and the BT/SLE UPG consumers are absent from the tree — genuine closures, not unread files.

## Reusable for our stack

- The three-slot monotonic flag protocol is a clean pattern for flash-wear-friendly retry state on the WS63 dongle's firmware-update path — it never needs erase to change state.
- The page-journal + single-in-flight-recovery design (B/W status bits, buffer page, one-recovery-page invariant) is directly transferable to any host-managed firmware write that must survive USB unplugs mid-write.
- The eight-step hash-chain layout explains what our host-side `fwpkg` tooling (see [the web flasher reports](NEW-WEB-FLASHER-FWPKG.md)) must produce for a stock WS63 device to accept an image: key-area + info-area signatures with 64-byte ECC values, a hash table whose own hash lives in the info area, and per-image headers hashed into the table.
- The erase-with-flanking-writeback primitive is the correct mental model for byte-granular writes over 4 KiB-sector flash and explains observed device-side latencies during flashing.

## Comparison anchors (vs existing reports)

- [NEW-XF-BURN](NEW-XF-BURN.md) / [NEW-WS63FLASH-GHIDRA](NEW-WS63FLASH-GHIDRA.md): those cover what the host sends and through which bootloader door; this report covers what the device does with it afterward — the two planes meet at the FOTA partition image this middleware consumes.
- [NEW-HISI-FWPKG-IMAGE-PLANNING](NEW-HISI-FWPKG-IMAGE-PLANNING.md): the Rust planner emits flash plans for the same class of packages; its erase/write range asymmetry contrasts with the UPG side's fixed partition-layout assumption (`upg_porting.h:52-71` documents the FOTA partition layout: package storage, optional 4 KiB buffer and status pages, 4 KiB flag area).
- [NEW-FBB-WS63-XFUSION-PORT](NEW-FBB-WS63-XFUSION-PORT.md): that report's bootstrap section gains its missing half — the OTA client port is the XFusion-side consumer of this middleware's ingest API.
- [NEW-HISI-NVS-PERSISTENCE](NEW-HISI-NVS-PERSISTENCE.md): the NV-image handoff (record offsets now, apply at app boot) is the contract between this engine and the NV module that report covers.
- [NEW-WS63-MIDDLEWARE-OPEN-CLOSED](NEW-WS63-MIDDLEWARE-OPEN-CLOSED.md): the open/closed middleware map extends — `update/` is source-open on both planes (middleware + chip porting), unlike the closed GLE host.
