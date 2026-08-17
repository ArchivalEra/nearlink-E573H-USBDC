# FBB WS63 BGTP -- Chip-side Bluetooth/SLE Controller Firmware

> Date: 2026-08-17
> Source: `/mnt/hdd/nearlink-stuff/fbb_ws63/src/protocol/bt/controller/bgtp/`
> Cross-ref: `SLE-CONTROL-PLANE.md`, WS73 SDK `driver/bsle/`

## What is BGTP

BGTP stands for **Bluetooth/Generic TeleProtocol** (not a standard acronym -- HiSilicon internal naming). It is the **chip-side controller firmware layer** for the WS63/WS73 BLE+SLE (Bluetooth Low Energy + SparkLink Enhanced) radio.

BGTP sits between:
- **Host HCI commands** (from `bth_sdk.a` host layer) above
- **RF hardware** (BLE/SLE baseband, radio, calibration) below

It is the firmware that runs **inside the dongle chip** and executes all radio operations. When we send DLI commands to the WS73 dongle via `/dev/hwsle`, the firmware on the chip is running BGTP-equivalent code to handle those commands.

## Source Status

**No source code available.** The BGTP directory contains only:

```
bgtp/
  CMakeLists.txt          -- build system definition
  Kconfig                 -- feature toggles (BLE adv v1, scan v1, etc.)
  ws63.cmake              -- WS63-specific build config
  ws63-liteos-app/
    libbgtp.a             -- 237 object files, 2085 exported symbols
    libbgtp_rom_data.a    -- 93 data symbols (ROM data overlay)
  ws63-liteos-xts/
    libbgtp.a             -- XTS variant (same structure)
    libbgtp_rom_data.a
```

This is a **prebuilt binary distribution**. The source is proprietary HiSilicon code, compiled into static libraries. We can analyze behavior through symbol tables but cannot read the actual implementation.

### Build Architecture (CMakeLists.txt:1-128)

The build system compiles BGTP into three components:
1. **bgtp** (RAM component) -- main controller code (`BTC_RAM_LIST`)
2. **bgtp_rom** (ROM component) -- ROM-resident code (`BTC_ROM_LIST`)
3. **bgtp_rom_data** (ROM data) -- initialized data (`BTC_ROM_DATA_LIST`)

Key defines: `BTC_SYS_PART=100`, GmSSL3.0 crypto integration, `AUTO_DEF_FILE_ID`.

### Multi-Platform Support (CMakeLists.txt:11-63)

BGTP supports multiple chip families from a single codebase:
- **ws63**: LiteOS mode (`ws63.cmake`)
- **ws73**: NONOS mode (`ws73.cmake`) -- our dongle
- **bs21/bs22/bs26**: LiteOS mode
- **bs25**: LiteOS mode
- **ws53**: NONOS mode
- **mp17c (hi1107)**: LiteOS mode
- **sw39, sw21**: LiteOS mode

This confirms WS73 runs the same BGTP architecture as WS63, just with NONOS scheduling instead of LiteOS.

## Module Breakdown (from libbgtp.a object files)

### HCI Command Interface
- `dts_hci.c.obj` -- DTS HCI transport layer (`dts_hci_init`, `dts_hci_read`, `dts_hci_write`)
- `hci_if.c.obj` -- HCI interface abstraction (`hci_if_init`, `hci_if_handle_return_msg`)
- `hci_bt.c.obj` -- BLE HCI command handling (`hci_bt_cmd_received`, `hci_bt_cmd_get_dest_id`, `hci_bt_cmd_reject`)
- `hci_gle.c.obj` -- GLE (SLE) HCI command handling (`hci_gle_cmd_received`, `hci_gle_cmd_get_dest_id`, `hci_gle_cmd_reject`)
- `hci_msg_recv_h2c` -- Host-to-Controller message reception entry point

### Event Task System (Controller-side state machines)
- `evt_task_fsm.c.obj` -- Core FSM scheduler
- `evt_task_comm.c.obj` -- Common event task handling (`evt_task_comm_task_start`, `evt_task_comm_task_update`)

#### BLE Event Tasks
- `evt_task_ble_acl_fsm_tbl.c.obj` + `evt_task_ble_acl_ram.c.obj` -- ACL connection FSM
- `evt_task_ble_adv_fsm_tbl.c.obj` + `evt_task_ble_adv_ram.c.obj` -- Advertising FSM
- `evt_task_ble_scan_fsm_tbl.c.obj` + `evt_task_ble_scan_ram.c.obj` -- Scanning FSM
- `evt_task_ble_initiate_fsm_tbl.c.obj` + `evt_task_ble_initiate_ram.c.obj` -- Connection initiation FSM
- `evt_task_ble_per_adv.c.obj` -- Periodic advertising
- `evt_task_ble_per_sync.c.obj` -- Periodic sync
- `evt_task_ble_cs.c.obj` -- Channel Sounding (distance measurement)
- `evt_task_ble_test_mode.c.obj` -- HCI test mode

#### GLE (SLE) Event Tasks
- `evt_task_gle_acb.c.obj` + `evt_task_gle_acb_fsm_tbl.c.obj` -- ACB (Asynchronous Connection-Based) link FSM
- `evt_task_gle_scan.c.obj` + `evt_task_gle_scan_fsm_tbl.c.obj` -- GLE scanning
- `evt_task_gle_initiate.c.obj` + `evt_task_gle_initiate_fsm_tbl.c.obj` -- GLE connection initiation
- `evt_task_gle_adv.c.obj` + `evt_task_gle_adv_fsm_tbl.c.obj` -- GLE advertising
- `evt_task_gle_iob.c.obj` + `evt_task_gle_iob_fsm_tbl.c.obj` -- IOB (Isochronous Out-Band) data
- `evt_task_gle_imb.c.obj` + `evt_task_gle_imb_fsm_tbl.c.obj` -- IMB (Isochronous In-Band) data
- `evt_task_gle_test_mode.c.obj` -- GLE test mode
- `evt_task_chnl_scan.c.obj` -- Channel scan (frequency hopping)

### Link Manager
- `lm_ble.c.obj` -- BLE link manager (adv, scan, ACL, power control, PHY update, etc.)
- `lm_gle_acb.c.obj` -- GLE ACB link manager (`lm_gle_acb_create_desc`, `lm_gle_acb_start_master`, `lm_gle_acb_start_slave`)

### Device Manager
- `dm_co.c` -- Device manager common (`dm_co_init`, `dm_co_reset`, `dm_co_get_local_supp_cmds_tbl`)
- `dm_ble.c` -- BLE device manager
- `dm_gle.c` -- GLE device manager (`dm_gle_init`, `dm_gle_hci_reset_act`, `dm_gle_hci_set_public_addr_act`)

### RF/Calibration
- `hal_bt_cali.c.obj`, `cali_*.c.obj` -- RF calibration (TX DC, TX IQ, RX DC, power)
- `bt_rf_config.c.obj`, `bt_rf_table.c.obj` -- RF configuration
- `bt_pll.c.obj` -- PLL initialization
- `bt_phy_init` -- PHY initialization

### Low Power
- `bgtp_sleep_hw.c.obj`, `dts_lowpower.c.obj` -- Sleep/power management
- `bgtp_lowpower_*` -- Low power clock, IRQ, time config

### Coexistence
- `coex_adapter.c.obj`, `dts_coex.c.obj` -- WiFi/BT coexistence
- `coex_ble_*.c.obj`, `coex_gle_*.c.obj` -- Per-protocol coexistence handlers

## DLI Command Processing

### Entry Point Chain

When the host sends a DLI command via HCC (Host Controller Communication):

1. **`hci_msg_recv_h2c`** -- receives raw bytes from HCC transport
2. **`hci_bt_cmd_received`** (BLE) or **`hci_gle_cmd_received`** (SLE) -- parses HCI command header (opcode, parameter length)
3. **`hci_bt_cmd_get_dest_id`** / **`hci_gle_cmd_get_dest_id`** -- maps opcode to internal destination ID
4. Command dispatch to appropriate FSM (evt_task_*)

For rejected commands: **`hci_bt_cmd_reject`** / **`hci_gle_cmd_reject`** sends error response.

### Two Parallel HCI Stacks

BGTP maintains **two completely separate HCI command paths**:

| Path | Prefix | Protocol | Object files |
|------|--------|----------|-------------|
| BLE | `hci_bt_*`, `evt_task_ble_*`, `lm_ble_*`, `dm_ble_*` | Bluetooth Low Energy | 40+ objects |
| GLE | `hci_gle_*`, `evt_task_gle_*`, `lm_gle_*`, `dm_gle_*` | SparkLink Enhanced (星闪) | 40+ objects |

This explains the WS73 behavior: the `/dev/hwsle` device routes to the **GLE path** (`hci_gle_cmd_received`), while `/dev/hwble` would route to the **BLE path** (`hci_bt_cmd_received`).

### Command Response Flow

1. Host sends HCI command via HCC
2. `hci_msg_recv_h2c` receives it
3. `hci_gle_cmd_received` (or `hci_bt_cmd_received`) parses opcode
4. `hci_gle_cmd_get_dest_id` maps opcode to FSM task ID
5. Command is dispatched to the appropriate evt_task handler
6. Handler processes command, generates response
7. `hci_gle_build_cc_evt` (or `hci_bt_build_cc_evt`) builds Command Complete event
8. `hci_gle_send_evt_msg` / `hci_bt_send_evt_msg` sends response back via HCC

### Key Symbol: `dts_hci_check_bgle_data_type`

This function in `dts_hci.c.obj` validates incoming data types, likely distinguishing between BLE and GLE commands on the shared HCC transport.

## WS73 Relevance

### Architecture Mapping

Our WS73 dongle (`ffff:3733`) runs BGTP firmware. The host-side driver stack is:

```
User space:  sle-hci-scan.py / sle-adv.py (our tools)
    |
Kernel:      sle_soc.ko -> /dev/hwsle -> HCC transport -> chip
    |
Chip:        BGTP firmware (ws73 variant, NONOS mode)
    |          |- hci_gle_cmd_received (GLE command path)
    |          |- evt_task_gle_* (GLE state machines)
    |          |- lm_gle_acb (GLE link manager)
    |          |- RF hardware
```

### What This Explains

1. **Why `/dev/hwsle` works**: The WS73 firmware routes SLE commands to the GLE HCI path (`hci_gle_cmd_received`), which is a complete, independent protocol stack from BLE.

2. **Command opcode mapping**: The opcodes we discovered (0x0C02 = SET_ADV_PARAMS, 0x1401 = CREATE_CONNECTION, etc.) are handled by `hci_gle_cmd_get_dest_id` which maps them to specific GLE FSM tasks.

3. **Dual-stack operation**: The WS73 supports simultaneous BLE + SLE operation because BGTP maintains two complete HCI stacks. The driver `sle_soc.ko` and `ble_soc.ko` are separate kernel modules that talk to the same chip firmware through different logical channels.

4. **ACB (Asynchronous Connection-Based) links**: The `evt_task_gle_acb_*` symbols correspond to the SLE connection-oriented data links we tested with CREATE_IOB (opcode 0x2803).

5. **IOB/IMB data paths**: `evt_task_gle_iob` and `evt_task_gle_imb` handle isochronous data -- these are the low-latency synchronous data channels for SLE audio/streaming.

6. **Channel Sounding**: `evt_task_ble_cs` confirms BLE Channel Sounding (distance measurement) support, matching our READ_MEASURE_CAPS (opcode 0x2001/02) results.

### WS73 vs WS63 Differences

From `CMakeLists.txt:15-23`:
- **WS63**: `BGTP_OS_MODE = "LITEOS"` -- runs on LiteOS RTOS
- **WS73**: `BGTP_OS_MODE = "NONOS"` -- runs bare-metal (no OS)

This means WS73 BGTP is simpler in scheduling (no RTOS overhead) but the HCI command handling logic is the same.

### Host Layer Comparison

WS73 SDK provides:
- `sle_driver/sle_soc.mod.c` -- SLE kernel module (sends HCI commands via HCC)
- `ble_driver/linux/ble_soc.mod.c` -- BLE kernel module
- `sle_driver/sle_hcc/sle_hcc_proc.c` -- HCC transport implementation
- `sle_driver/sle_dev/sle_dev.c` -- Device node management (`/dev/hwsle`)

The driver layer sends raw HCI command bytes to the chip, and BGTP firmware processes them through the GLE HCI command path.

## Open Questions

1. **Source availability**: Is the WS73 BGTP source available under NDA? The CMakeLists.txt references `ws73.cmake` which would define the exact source file list, but it is not included in the prebuilt distribution.

2. **GLE opcode table**: Can we extract the full opcode-to-destination mapping from `hci_gle_cmd_get_dest_id`? This would give us the complete command reference. The function is in the prebuilt library -- reverse engineering the dispatch table might be possible with a disassembler.

3. **BGTP firmware binary**: Where is the actual firmware binary (`ws73.bin` or equivalent) that gets loaded into the chip? The `sle_soc.ko` driver must have a firmware loading path (`request_firmware` call) that we could trace.

4. **ACB vs IOB vs IMB**: What is the precise relationship between these three GLE data link types? ACB appears connection-oriented (like BLE ACL), IOB appears isochronous out-band, IMB appears isochronous in-band. Are they mutually exclusive or can they coexist on one connection?

5. **Channel Sounding**: `evt_task_ble_cs` exists in the BLE path, not GLE. Does the WS73 support distance measurement over SLE as well, or only over BLE?

6. **Coexistence**: `coex_adapter`, `dts_coex`, and WiFi coexistence modules suggest the WS63/WS73 are WiFi+BT+SLE combo chips. How does WiFi activity affect SLE timing?

## Key Files

| File | Path |
|------|------|
| BGTP CMakeLists.txt | `fbb_ws63/src/protocol/bt/controller/bgtp/CMakeLists.txt` |
| WS63 cmake | `fbb_ws63/src/protocol/bt/controller/bgtp/ws63.cmake` |
| Kconfig (features) | `fbb_ws63/src/protocol/bt/controller/bgtp/Kconfig` |
| Prebuilt libbgtp.a | `fbb_ws63/src/protocol/bt/controller/bgtp/ws63-liteos-app/libbgtp.a` |
| ROM data | `fbb_ws63/src/protocol/bt/controller/bgtp/ws63-liteos-app/libbgtp_rom_data.a` |
| SLE control plane | `.scratch/nearlink-driver/lab-notes/SLE-CONTROL-PLANE.md` |
| WS73 SDK SLE driver | `sdk/ws73_sdk_linux_WS73_1.10.110/driver/bsle/sle_driver/` |
