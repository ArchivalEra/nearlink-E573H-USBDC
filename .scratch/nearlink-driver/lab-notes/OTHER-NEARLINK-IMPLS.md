# Alternative NearLink/SparkLink/SLE Implementations (beyond OHOS & HiSilicon)

Research 2026-08-16.

## Ranked implementations

### 1. OpenSparklink/sparklink — Rust, complete BlueZ-style host stack (MOST RELEVANT)
- https://github.com/OpenSparklink/sparklink (~5200 Rust + 550 Python, 7 crates; org also has OpenSparklink/linux kernel tree + nearlink_sdr_sim)
- Linux host stack mirroring BlueZ:
  - Kernel `net/sparklink/` in Rust (~28.6K lines): sparklink_core.rs (ioctl, /dev/sparklink, multi-controller), sle_conn.rs (CM, TCID 0x02/0x0A/0x1F, credit flow), sle_uapi.rs (80+ ioctl structs, 126 cmds magic 'S'), sle_ssap.rs, sle_usb.rs, sle_dli.rs (DLI abstraction, SleController trait), sle_security.rs (JustWorks/PSK/NumericComparison/OOB/Password, RPA), sle_pdu.rs (PDU, CRC-12), sle_adv.rs, sle_phy.rs (13-level MCS, AFH), sle_uart.rs (H4), sle_spi.rs (SPI register protocol), sle_crypto.rs (SM3/SM4/ECDH), sle_netlink.rs (genl 34 cmds/59 attrs)
  - DLI transports: USB/UART/SPI/Virtual; 5 packet types: CMD 0xA1, EVENT 0xA2, AsyncUcast 0xA3, SyncUcast 0xA4, AsyncMcast 0xA5
  - Userspace: slk-protocol (UAPI bindings), libsparklink (async Adapter, C FFI), slkd (D-Bus daemon), slkconfig, slctl (bluetoothctl analog), slkmon (btmon analog), slkdump
  - Standards cited: T/XS 00001/10002/10003/20001/20002/50003/50004-2025
- Caveat: 0 stars, single author, very new (2026-04), unproven on hardware — reference design to extract protocol structure from, not vendor wholesale.

### 2. hispark-rs ecosystem — Rust bare-metal WS63/BS21 (design patterns)
- https://github.com/orgs/hispark-rs/repositories (~30 repos): hisi-rf (chip-neutral radio facade, profile-sle-ssap), bs2x-guide (SVD/memory/register guide), bs2x-pac (svd2rust), fbb_bs2x-qemu (QEMU boot), hisi-alloc (SRAM arena)
- Reusable: no_std/no-heap feature-gated design, SVD/register maps, QEMU bring-up

### 3. GBCLStudio/LinkNebula — tiny no_std Rust mesh (small)
### 4. Hny0305Lin/NLChat — the real "Android NearLink" today (66 stars): USB-serial CH34x bridge to vendor SLE_UART firmware — NOT a stack
### 5. NearLink-ePaper/NearLink-Mesh-ePaper — SLE mesh app layer (AODV/AIMD over SSAP)
### 6. rzy0901/sle_measure_sdk1.0.12 — measurement firmware (PHY/MCS/CI tuning knobs)
### 7. Vendor mirrors: Ai-BS21_SDK, Hihope_WS63_NearLink_SDK
### 8. Tooling: MiraHikari/nearlink-firmwares (NearLink Toolbox)

## Device-side footprint (fbb_ws63/fbb_bs2x)
| Module | WS63 | BS21e |
|---|---|---|
| Controller libbgtp.a | 1.46 MB | 1.76 MB (stays on radio chip) |
| SLE host libbth_gle.a | 0.82 MB | 0.90 MB |
| BS21 app flash budget | — | ~544 KB |
| BGLE_RAM | 16/32/64K selectable | — |

Feature trims: module-granular Kconfig (FEATURE_GLE_LOW_LATENCY/HADM default n, AT_GLE_MODULE_*); WS63 spi-host mode (hcc_spi_host.c) = reference topology for TV-box SoC + radio.

## Recommendations for lightweight TV-box host stack
1. Study OpenSparklink/sparklink FIRST (sle_dli.rs + sle_uapi.rs contract, sle_pdu.rs, sle_conn.rs, sle_ssap.rs, sle_phy.rs)
2. Copy vendor footprint model (module-granular Kconfig, default-n features, radio RAM selectable)
3. hispark-rs for no_std patterns + QEMU; sle_measure for perf knobs
4. WS63 spi-host mode = our TV-box topology reference
