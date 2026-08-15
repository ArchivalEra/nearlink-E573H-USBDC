# 01 — WS73 Boot-stage Firmware Download Handshake: Byte-Exact Wire Protocol Spec

Source: HiSilicon WS73 Linux SDK `ws73_sdk_linux_WS73_1.10.110`, host-side driver only.
All facts below are **source-derived**; nothing here was observed on physical hardware.
Base directory referenced as `<SDK>` = `/home/archivalera/plum/zcode-projects/nearlink/sdk/ws73_sdk_linux_WS73_1.10.110`.

Key files:
- `<SDK>/driver/platform/firmware_download/plat_firmware.c`
- `<SDK>/driver/platform/firmware_download/plat_firmware_uart.c` (comparison / UART flavor)
- `<SDK>/driver/platform/pm/plat_firmware.h`
- `<SDK>/driver/platform/hcc/host/hcc_usb_host.c`
- `<SDK>/driver/platform/hcc/host/hcc_usb_host_ops.c`
- `<SDK>/driver/platform/hcc/host/hcc_usb_host.h`
- `<SDK>/driver/platform/hcc/comm/hcc_bus_usb_comm.h`
- `<SDK>/driver/platform/drv/crypto/plat_sha256_calc.h`
- `<SDK>/driver/platform/pm/plat_pm_board_ws73.c`, `<SDK>/driver/platform/pm/plat_pm.c`

---

## 1. Scope and transport model

The WS73 device boots into a minimal "boot" USB configuration with exactly **2 bulk
endpoints** (no interrupt, no control-data EPs). The host downloads firmware through these
two bulk pipes using a **half-duplex request/response ASCII command protocol** layered
directly on raw bulk transfers (no USB control transfers, no 92-byte HCC frame header, no
URB/aggregation framing — those only exist in kernel/WORK mode).

The protocol is a *line-oriented command/response* protocol:
- Every host→device command is an **ASCII string** terminated by a **space byte** (`0x20`,
  `COMPART_KEYWORD`), i.e. commands are space-delimited tokens ending in one trailing space.
  There is **no newline** and **no length prefix**.
- After most commands the device replies with an **ASCII acknowledgement string** on the
  bulk-IN pipe. A few commands (QUIT, and the raw chunk data after FILES) have empty or
  special acknowledgement semantics.
- File payload is sent as **raw bytes** (the firmware content) on bulk-OUT, not hex-encoded.

The same command vocabulary is shared with the UART boot downloader
(`plat_firmware_uart.c`), which confirms the wire syntax (space-terminated ASCII keywords).

---

## 2. Boot-state USB endpoint layout

Source: `<SDK>/driver/platform/hcc/host/hcc_usb_host.c`.

- The USB driver matches two VID/PID pairs (`hcc_usb_host.c:28-32`):
  - `{ 0x12D1, 0x897d }`
  - `{ 0xFFFF, 0x3733 }`  ← the `hcc_bus_usb_comm.h:21-22` constants
    `DEIVICE_VENDOR_ID 0xFFFF` / `DEIVICE_PRODUCT_ID 0x3733` suggest this is the boot/DFU
    identity. (Inference only — the code does not print which id matched.)
- `oal_usb_probe()` (`hcc_usb_host.c:1404-1427`) selects the boot configuration purely by
  endpoint count of the current altsetting (`bNumEndpoints`):
  - `bNumEndpoints == DEVICE_KERNEL_EP_NUM (5)` → kernel configuration →
    `oal_usb_system_init_config()` (WORK mode, full HCC).
  - `bNumEndpoints == DEVICE_BOOT_EP_NUM (2)` → **boot configuration** →
    `oal_usb_boot_init_config()`.
- Boot EPs are taken by **index** in the interface descriptor (`hcc_usb_host.h:60-64`):
  - `BULK_EP_IN_IND 0` → `dev->bulk_in_ep` (`oal_usb_boot_bulk_in_probe`,
    `hcc_usb_host.c:1300-1323`).
  - `BULK_EP_OUT_IND 1` → `dev->bulk_out_ep` (`oal_usb_boot_bulk_out_probe`,
    `hcc_usb_host.c:1325-1354`).
  - The absolute addresses (e.g. 0x81 IN / 0x01 OUT) are **not** asserted anywhere in the
    source; only the descriptor indices are. The physical addresses must be read from the
    device descriptor at runtime. (The ticket's "0x81/0x01, 64 B, Full Speed" clue is
    consistent but unverifiable from source.)
- `oal_usb_boot_init_config()` (`hcc_usb_host.c:1356-1401`) on success:
  - `hcc_switch_status(HCC_BUS_FORBID)` — HCC TX path stays disabled during download.
  - allocates a **32 KB** bulk-out staging buffer
    (`dev->buik_out_buffer_size = MAX_FIRMWARE_FILE_TX_BUF_LEN` = 32768,
    `hcc_usb_host.c:1350`).
  - sets `g_usb->connect = TRUE`, bus state = `BUS_USB_BOOT` (=5), `pst_bus->state = 1`,
    then `complete(&g_usb_boot_probe)`.
- The RX (bulk-in) path during boot does **not** use the async URB ring used in WORK mode;
  it is **synchronous** `usb_bulk_msg()` calls made by the firmware-download task itself
  (see §7).

---

## 3. Command/response framing (exact bytes)

### 3.1 Separator and terminator

From `<SDK>/driver/platform/pm/plat_firmware.h:78-79`:

```c
#define COMPART_KEYWORD  ((char)' ')     /* 0x20 — the ONLY separator AND terminator */
#define CMD_LINE_SIGN    ((char)';')     /* defined, not used by the USB/USB boot path */
```

- All commands are built with `snprintf_s(..., "%s%c...%c", KEYWORD, COMPART_KEYWORD, ...)`
  and end with one `COMPART_KEYWORD` (trailing space).
- The sender transmits exactly `strlen(command_buffer)` bytes, so the **trailing 0x20 is
  on the wire**. Example: `firmware_send_and_recv_expect_result_func(buff_tx,
  strlen(buff_tx), ...)` at `plat_firmware.c:1052`.
- `CMD_LINE_SIGN ';'` exists but is not emitted by the boot download path.

### 3.2 Transport calls

Host side send/recv are the `hcc_bus_patch_write` / `hcc_bus_patch_read` bus ops
(`hcc_bus.h:29-30`), which for USB are:

- `hcc_usb_patch_write()` (`hcc_usb_host_ops.c:964-974`) →
  `oal_usb_bulk_out_msg()` (`hcc_usb_host_ops.c:926-962`) →
  `usb_bulk_msg(usb_sndbulkpipe(udev, bulk_out_ep), buf, size, &returnlen, timeout)`.
  `size = min(len, dev->buik_out_buffer_size)` (32 KB). Timeout is the global
  `g_usb_bulk_out_timeout_ms` (default `USB_DOWNLOAD_FW_TIMEOUT` = **30000 ms**,
  `hcc_usb_host_ops.c:23`, `hcc_usb_host.h:98`).
- `hcc_usb_patch_read()` (`hcc_usb_host_ops.c:913-924`) →
  `oal_usb_patch_bulk_in_msg()` (`hcc_usb_host_ops.c:869-911`) →
  `usb_bulk_msg(usb_rcvbulkpipe(udev, bulk_in_ep), buf, size, &returnlen, timeout)`,
  `size = min(len, MAX_FIRMWARE_FILE_RX_BUF_LEN)` (128 KB). Returns actual received length.
- The firmware layer calls these with:
  - `firmware_send_func(data, data_max_len, len)` → `hcc_bus_patch_write` (`plat_firmware.c:184-206`)
  - `firmware_recv_func(data, len)` → `hcc_bus_patch_read` with `READ_MEG_TIMEOUT` (2000)
    (`plat_firmware.c:142-161`)
  - `firmware_recv_timeout_func(data, len, timeout)` → caller-selected timeout
    (`plat_firmware.c:163-182`)
- Both bulk calls are serialized with a host-side claim lock (`usb_claim_host` /
  `usb_release_host` over `reg_rw_lock`, `hcc_usb_host_ops.c:886/941/899/956`) — the wire
  protocol is strictly serialized; never overlap a send with a pending read.

---

## 4. Command catalog — exact wire bytes

Constants at `plat_firmware.h:53-75`. All strings below are byte-exact (ASCII, no NUL
terminators on the wire; trailing `0x20` included).

### 4.1 `WRITEM` — write memory (the first command of the download)

Format produced by `firmware_number_type_cmd_send_etc()` (`plat_firmware.c:390-445`):
`WRITEM` `0x20` `<width>` `0x20` `<addr>` `0x20` `<value>` `0x20`
where each `<field>` is copied verbatim from the comma-separated `cmd_para`, commas
replaced by `0x20`. Full-width format chars (`FORMAT_BYTE '1'`, `FORMAT_WORD '2'`,
`FORMAT_LONG '4'`, `plat_firmware.c:56-58`) exist but the pre-download path uses literal
text fields.

Concrete first command (host-side `pre_download_get_cmu_xo_trim()`,
`plat_firmware.c:120-139`, constants `WR_CMU_XO_TRIM_LEN 4`, `WR_CMU_XO_TRIM_ADDR
0x40019408`, `plat_firmware.h:132-133`):

```
cmd_para = "4,0x40019408,0x%04x"  (trim = board-specific CMU XO trim, see §9)
wire     = "WRITEM 4 0x40019408 0xXXXX "     → 27 bytes when trim is 4 hex digits
```

Expect reply: `"WRITEM OK"` (`MSG_FROM_DEV_WRITEM_OK`, `plat_firmware.h:69`).

Alternate WRITEM encoding — `write_device_reg()` (`plat_firmware.c:552-577`):
`"%s%c%c%c0x%x%c%u%c"` → `WRITEM` `0x20` `4` `0x20` `0x%x` (addr, lowercase hex) `0x20`
`%u` (**decimal** value) `0x20`. Used by the boot-state register write path
(`hcc_usb_write_reg`, `hcc_usb_host_ops.c:1296-1302`). Also expects `"WRITEM OK"`.
Note the two encodings differ: the pre-download WRITEM keeps `0x`-prefixed **hex** value,
`write_device_reg` uses **decimal** value.

### 4.2 `READM` — read memory (boot-state register read / connection check)

Caution: the task brief said `RMEM`, but the actual keyword macro is
`RMEM_CMD_KEYWORD "READM"` (`plat_firmware.h:64`).

`read_device_reg()` (`plat_firmware.c:522-549`):
`"%s%c0x%x%c%d%c"` → `READM` `0x20` `0x%x` (addr, lowercase hex) `0x20` `%d` (width=4)
`0x20` → e.g. `"READM 0x40019380 4 "` (22 bytes).
Reply: **4 raw bytes** = the register value, host-native byte order
(`*((osal_u32 *)(buf_result))`, `plat_firmware.c:543`). No ASCII acknowledgement.

### 4.3 `FILES` — start a file-transfer chunk

`read_and_send_file_etc()` (`plat_firmware.c:1013-1073`):

```c
snprintf_s(buff_tx, SEND_BUF_LEN, SEND_BUF_LEN,
    "%s%c%d%c0x%lx%c0x%lx%c0x%lx%c",
    FILES_CMD_KEYWORD, COMPART_KEYWORD,
    FILE_COUNT_PER_SEND, COMPART_KEYWORD,
    addr_send, COMPART_KEYWORD,
    (unsigned long)rdlen, COMPART_KEYWORD,
    (unsigned long)trans_state, COMPART_KEYWORD);
```

Wire: `FILES` `0x20` `1` `0x20` `0x<addr>` `0x20` `0x<len>` `0x20` `0x<state>` `0x20`
- field 2 = file count, **always `1`** (`FILE_COUNT_PER_SEND`, `plat_firmware.h:83`);
  `firmware_parse_file_cmd_etc()` rejects any other count (`plat_firmware.c:471-474`).
- field 3 = target load address, `0x`-prefixed lowercase hex (`simple_strtoul(...,16)`,
  `plat_firmware.c:489`).
- field 4 = chunk byte length, `0x`-prefixed lowercase hex.
- field 5 = "verify stage" flag (`trans_state`), `0x`-prefixed lowercase hex, one of:
  `0x0` first chunk of a multi-chunk transfer, `0x1` middle chunk, `0x2` last chunk,
  `0x3` single-chunk transfer (see §6.2).

Example first chunk of ws73.bin (32 KB chunks, content length 200 KB−64 B = 204736 B ⇒ 7
chunks: 6×32768 + 1×20128):
```
"FILES 1 0x400000 0x8000 0x0 "
"FILES 1 0x408000 0x8000 0x1 "   (x4)
"FILES 1 0x430000 0x8000 0x1 "   ... addresses continue: 0x400000 + 6*0x8000 = 0x430000
"FILES 1 0x4EA000 0x4EA0 0x2 "   last (0x8000*6 = 0x30000 → 0x430000; 204736−196608=8128=0x1FC0)
```
Sequence per chunk after sending the FILES line:
1. expect `"READY"` (`MSG_FROM_DEV_READY_OK`, `plat_firmware.h:72`)
2. `oal_usleep_range(5000, 5100)` — **≥5 ms** pause (`FILE_CMD_WAIT_TIME_MIN/MAX`,
   `plat_firmware.h:37-38`, `plat_firmware.c:1059`)
3. send `rdlen` **raw bytes** of file content on bulk-OUT (single bulk write)
4. expect `"FILES OK"` (`MSG_FROM_DEV_FILES_OK`, `plat_firmware.h:71`)

### 4.4 `QUIT` — finish download, device reboots to kernel mode

`firmware_cmd_quit_func()` (`plat_firmware.c:783-822`): buffer = `QUIT` `0x20` (5 bytes).
Wire: `"QUIT "`.
Reply: `MSG_FROM_DEV_QUIT_OK` is the **empty string** (`plat_firmware.h:75`), so
`firmware_recv_expect_result_func()` short-circuits and **does not wait** for a reply
(`plat_firmware.c:214-217`). Instead, for USB, `firmware_send_quit_func()`
(`plat_firmware.c:284-313`) immediately calls `hcc_usb_reload()`:
1. `usleep_range(USB_ENUM_DEVICE_TIMEOUT=100000 us = 100 ms)`
2. `wait_for_completion_timeout(&g_usb_wifi_probe, USB_WAIT_TIME * HZ)` =
   **up to 5 s** for the device to re-enumerate with the 5-EP kernel configuration
   (`hcc_usb_host.c:1649-1672`, `hcc_usb_host.h:75`).
Device side is expected to reset/re-enumerate on QUIT; the kernel-config probe
(`oal_usb_system_init_config`) then signals `g_usb_wifi_probe`.

### 4.5 `VERSION` — optional version handshake (NOT used by the USB boot path)

`VER_CMD_KEYWORD "VERSION"`, wire `"VERSION "` then read `VERSION_LEN` (64) bytes and
copy to `g_device_version` (`firmware_check_version()`, `plat_firmware.c:342-388`).
It is wired as a generic NUM command (`firmware_cmd_number_func`, `plat_firmware.c:770`)
but is **not present in the USB boot download command set** (see §5). The UART boot path
does call it explicitly (`plat_firmware_uart.c:124-146`, expecting a fixed
`VER_EXPECT_VALUE`). It exists in the shared vocabulary; treat as optional/available.

### 4.6 Other keywords defined but not used in the USB boot download

`JUMP`, `SETPM`, `SETBUCK`, `SETSYSLDO`, `SETNFCRETLDO`, `SETPD`, `SETNFCCRG`, `SETABB`,
`SETTCXODIV` (`plat_firmware.h:54-63`) — same space-terminated encoding via
`firmware_number_type_cmd_send_etc()`, replies `"JUMP OK"` / `"SET OK"`
(`plat_firmware.h:73-74`). `JUMP` uses a 15 s reply timeout
(`READ_MEG_JUMP_TIMEOUT 15000`, `plat_firmware.h:35`). Not part of the E573H USB boot
download flow.

---

## 5. The complete ordered handshake

Command sequence is the compile-time array `g_firmware_download_set[]`
(`plat_firmware.c:79-109`), executed in order by `firmware_download_etc()`
(`plat_firmware.c:1201-1306`).

### Stage 0 — boot probe (device already in boot mode when host driver loads)

1. Device enumerated in boot config (2 EPs). `oal_usb_boot_init_config()` runs, allocates
   32 KB bulk-OUT buffer, sets `BUS_USB_BOOT`, `complete(g_usb_boot_probe)`.
2. `ws73_board_service_enter()` (`plat_pm_board_ws73.c:368+`):
   - `plat_pm_power_action(POWER_UP)` then `POWER_PATCH_LOAD_PREPARE` (disables HCC,
     `plat_pm_board_ws73.c:222-225`).
   - `ws73_board_reinit_hcc_bus()` → `hcc_bus_reinit()` → `hcc_usb_reinit()` waits for
     `g_usb_boot_probe` up to **5 s** (`hcc_usb_host.c:1674-1686`).

### Stage 1 — HCC connection check (register read through boot protocol)

`hcc_connect_state_check()` (`plat_firmware.c:587-607`, compiled under
`CONFIG_SUPPORT_HCC_CONN_CHECK`), invoked from wlan power-on (`plat_pm.c:279`):
1. Save current bulk-OUT timeout; **temporarily set it to 500 ms**
   (`hcc_usb_bulk_out_timeout_set(500)`).
2. `read_device_reg(REG_GP_REG0 = 0x40019380)` → send `"READM 0x40019380 4 "`,
   read back 4 bytes (reg value). Success ⇒ device alive.
3. Restore timeout to 30000 ms.
On failure the board is reset (`plat_pm.c:283-288`: `hcc_bus_reinit`,
`hcc_switch_status(FORBID)`, `board_power_reset`).

### Stage 2 — firmware_download_etc() (the boot download)

`firmware_download_etc()` (`plat_firmware.c:1253-1306`):
1. `firmware_buf_init()` — allocate chunk buffer: for USB, `g_fw_data_buf_len` =
   min(`cap_max_trans_size`=0x7fffffff, `MAX_FIRMWARE_FILE_TX_BUF_LEN`=32768), then
   **round down to 8** (`OAL_ROUND_DOWN(...,8)`, `plat_firmware.c:1227-1230`). ⇒ 32768 B.
2. `pre_download_get_cmu_xo_trim()` fills the WRITEM parameter string
   (`plat_firmware.c:131-133`).
3. Iterate `g_firmware_download_set[]`:

| # | type | command | parameter | action |
|---|------|---------|-----------|--------|
| 1 | NUM  | `WRITEM` | `4,0x40019408,0x<trim>` | send, expect `WRITEM OK` |
| 2* | FILE | `FILES` | `1,0x106400,/etc/ws73/ws73_rom.bin` | chunked download (**only if `ROMBIN_OPEN`**) |
| 3 | FILE | `FILES` | `1,0x400000,/etc/ws73/ws73.bin` | chunked download |
| 4 | FILE | `FILES` | `1,0x430000,/etc/ws73/ws73_wifi_cali.bin` | chunked download |
| 5† | FILE | `FILES` | `1,0x440000,/etc/ws73/btc_cali.bin` | chunked download (**only if `BT_EM_BUFFER_CALI_SUPPORT`**) |

  (paths from `android_autoconfig.h:97-99`; `FW_*_DOWNLOAD_CMD` macros at
  `plat_firmware.c:68-76`. *ROMBIN entry `FW_ROMBIN_DOWNLOAD_CMD` is
  `1,0x106400,/etc/ws73/ws73_rom.bin` for Linux.)
4. `firmware_cmd_func(QUIT_TYPE_CMD, "", "")` → `"QUIT "` → re-enumeration wait (§4.4).
5. On any failure (`firmware_cmd_func` returns < 0), abort; free buffers; return
   `-OAL_FAIL`. `firmware_download_etc()` is guarded by `g_firmware_mutex`.

### Stage 3 — post-download bring-up

1. `plat_pm_power_action(HCC_BUS_POWER_PATCH_LAUCH)` — waits for device BSP-ready
   completion: first up to `HOST_WAIT_BOTTOM_INIT_TIMEOUT` **1000 ms**, then one retry of
   `BUS_POWER_PATCH_LAUCH_TIME` **5000 ms** (`plat_pm_board_ws73.c:56-57,228-239`).
2. `hcc_enable()`, heartbeat start, etc.

---

## 6. Firmware file → wire mapping

### 6.1 On-disk file layout (SHA-256 header)

`plat_sha256_calc.h:20-24`:
```c
#define SHA256_HEADER_ARR_LEN   32   // 32 raw digest bytes
#define SHA256_HEADER_BYTES_LEN 2    // each digest byte printed as 2 hex chars
#define SHA256_HEADER_LENGTH    64   // 64 ASCII hex chars
#define FILE_READ_SIZE          (32 * 1024)
```

File structure (`plat_firmware.c:934-939`):
```
|<-- 64 bytes: hex SHA-256 of content -->|<-- content bytes (rest of file) -->|
```
- Offset 0..63: ASCII lower-case hex digest of **the file content only** (header
  excluded). Host verifies by computing SHA-256 over content and comparing with the
  header (`firmware_sha256_checksum()`, `do_check_sha256sum()`, `plat_firmware.c:891-914,
  940-1009`; digest byte `i` → chars at offset `2*i` via `"%02x"`).
- Offset 64..EOF: the actual binary content that is placed at the target RAM address.
- Constraint: total file size (incl. 64-byte header) must satisfy
  `SHA256_HEADER_LENGTH < size <= FIRMWARE_FILESIZE_MAX` where
  `FIRMWARE_FILESIZE_MAX = 200*1024` (`plat_firmware.h:107`). So **content ≤ 204736 B**.
- The host checks the header checksum *before* sending any bytes; a mismatch aborts the
  whole download (`start_parse_file_etc`, `plat_firmware.c:1084-1090`).

### 6.2 Wire transfer (what actually goes over USB)

- The 64-byte SHA-256 header is **stripped and never transmitted** (`plat_firmware.c:1106-1116`).
- Only the content is sent, in chunks of `g_fw_data_buf_len` = **32 KB**
  (`MAX_FIRMWARE_FILE_TX_BUF_LEN`, `plat_firmware.h:85`); last chunk is whatever remains
  (`per_send_len = min(transmit_limit, file_len)`, `plat_firmware.c:1022`). Host-side chunk
  buffer is 8-byte aligned (length rounded down, `plat_firmware.c:1230`), but the *sent*
  length is the exact `rdlen` read from the file — no padding bytes are added to the wire
  data.
- **No encryption**, no CRC, no sequence numbers on the data path — integrity is by the
  SHA-256 header check (host-side) and the `READY`/`FILES OK` handshake per chunk.
- `trans_state` per chunk (enum `firmware_verify_stage`, `plat_firmware.c:48-54`):
  `FIRMWARE_VERIFY_TRANS_START=0`, `FIRMWARE_VERIFY_TRANS=1`,
  `FIRMWARE_VERIFY_TRANS_END=2`, `FIRMWARE_VERIFY_TRANS_START_END=3`.
  Assigned in `read_and_send_file_etc()` (`plat_firmware.c:1026-1041`):
  - 1 chunk total → `0x3`
  - first of many → `0x0`, middle → `0x1`, last → `0x2`.
  Presumably lets the device-side receiver know when it can validate the whole-image hash;
  the device-side interpretation is not in this SDK (host-only).

---

## 7. Timeouts and retry table

Source constants:

| constant | value | meaning | ref |
|---|---|---|---|
| `USB_DOWNLOAD_FW_TIMEOUT` | 30000 ms | bulk-OUT timeout (every command AND every data chunk write) | `hcc_usb_host.h:98`, `hcc_usb_host_ops.c:23` |
| `READ_MEG_TIMEOUT` | 2000 ms | default bulk-IN (reply) read timeout | `plat_firmware.h:34` (comment says 200ms; value is 2000) |
| `READ_MEG_JUMP_TIMEOUT` | 15000 ms | JUMP reply timeout (not used in USB boot download) | `plat_firmware.h:35` |
| `HOST_DEV_TIMEOUT` | 3 | retry count for send/receive expectation loops | `plat_firmware.h:46` |
| `FILE_CMD_WAIT_TIME_MIN/MAX` | 5000/5100 µs | interleave delay between FILES ack and data chunk | `plat_firmware.h:37-38` |
| `USB_ENUM_DEVICE_TIMEOUT` | 100000 µs (100 ms) | sleep before waiting for re-enumeration after QUIT | `hcc_usb_host.h:100` |
| `USB_WAIT_TIME` | 5 s | wait-for-completion bound (boot probe, wifi probe, disconnect) | `hcc_usb_host.h:75` |
| `HOST_WAIT_BOTTOM_INIT_TIMEOUT` | 1000 ms | post-download device BSP-ready wait | `plat_pm_board_ws73.c:56` |
| `BUS_POWER_PATCH_LAUCH_TIME` | 5000 ms | retry bound for device BSP-ready | `plat_pm_board_ws73.c:57` |
| 500 ms | — | temporary bulk-OUT timeout during `hcc_connect_state_check` | `plat_firmware.c:595` |

Retry semantics:
- `firmware_recv_expect_result_func()` loops up to `HOST_DEV_TIMEOUT` (3) times, each
  iteration one bulk-IN read (2000 ms); it keeps reading on mismatch but **fails the whole
  step after 3** (`plat_firmware.c:220-235`). The reply buffer is 512 B (`RECV_BUF_LEN`,
  `plat_firmware.h:41`); the ack is matched as a **prefix** (`memcmp(auc_buf, expect,
  strlen(expect))`).
- `firmware_send_and_recv_expect_result_func()` retries the send+expect pair up to 3
  times (`plat_firmware.c:270-279`).
- `firmware_send_quit_func()` retries QUIT+hcc_usb_reload up to 3 times
  (`plat_firmware.c:289-309`).
- `usb_bulk_msg()` returning `-EPIPE` triggers `usb_clear_halt()` then the caller sees the
  error (`hcc_usb_host_ops.c:267-270`).

---

## 8. Device behaviors that interrupt the flow

Source: `hcc_usb_host.c`.

- `oal_usb_disconnect()` (`hcc_usb_host.c:1582-1621`): sets `g_usb->connect = FALSE`,
  `pst_bus->state = 0`; if state was `BUS_USB_BOOT` it sets
  `BUS_USB_DISCONNECT` and clears `interface_wlan`, then `complete(g_usb_disconnect)` and
  re-inits both probe completions. Any in-flight firmware bulk op on a disconnected device
  fails → `firmware_download_etc()` aborts.
- The device is **expected** to disconnect/re-enumerate after `QUIT`. The host does not
  treat that disconnect as an error path — it is the *success* path that leads to the
  kernel-mode probe (`g_usb_wifi_probe`). `hcc_usb_reload()` returns failure only if the
  wifi probe completion does not fire within 5 s.
- `oal_usb_boot_init_config()` explicitly calls `hcc_switch_status(HCC_BUS_FORBID)` at
  probe time so the normal HCC TX path cannot interleave with the boot download.
- Bus state guard: `hcc_usb_patch_get_usb_info()` returns NULL unless state is
  `BUS_USB_BOOT` (`hcc_usb_host_ops.c:862-865`) — patch (firmware) I/O is only legal in
  boot state.
- Suspend/resume: `oal_usb_suspend`/`oal_usb_resume` set `usb_suspend` atomic; a suspended
  device (e.g. autosuspend) would stall bulk transfers and trip the 30 s timeout. The
  driver holds a wakelock around `firmware_download_function()` (`plat_pm.c:74,82-88`).

---

## 9. Baud / speed / version negotiation

- **No baud negotiation on USB.** UART boot does an explicit baud-rate write
  (`UART_BAUD_RATE_REG 0x40019410`, `HCC_UART_FW_BAUDRATE`) in `pre_download_writem()`
  (`plat_firmware_uart.c:23-24,390-393`); the USB boot path has no such step.
- USB speed is inferred on the kernel (5-EP) config only: `wMaxPacketSize == 0x400`
  ⇒ SUPER, else HIGH (`oal_usb_system_init_bulk_ep`, `hcc_usb_host.c:949-953`). Boot config
  does not read speed.
- **Version handshake:** the `VERSION` command exists but is **not executed in the USB
  boot download set** (only on the UART path). On USB the only pre-download liveness probe
  is the `READM 0x40019380 4` connection check (§5 Stage 1).
- The only board-specific value on the wire is the CMU XO trim in the first WRITEM
  (`0x40019408`), sourced from an INI key `cmu_xo_trim` / default `CONFIG_INI_CMU_XO_TRIM
  0x83c` (`cfg/ini.h:106`, `cfg/customize_wifi.c:209-...`). This is calibration data that
  is **device-specific and unverifiable from source** — the actual value for the E573H
  must be read empirically (or from the vendor INI).

---

## 10. Byte-exact walkthrough (reference transcript, source-derived)

Everything below is what the *host* emits; device reply bytes are the constants quoted.

```
# stage 1 (only if CONFIG_SUPPORT_HCC_CONN_CHECK)
TX(bulk-out)  READM 0x40019380 4 <0x20>          # 22 bytes
RX(bulk-in)   <4 bytes: register value, native endian>

# stage 2a
TX(bulk-out)  WRITEM 4 0x40019408 0x083c <0x20>  # example trim 0x083c
RX(bulk-in)   WRITEM OK

# stage 2b — for each file, per chunk (chunk <= 0x8000, state 0/1/2/3)
TX(bulk-out)  FILES 1 0x400000 0x8000 0x0 <0x20>
RX(bulk-in)   READY
#   (>=5ms pause)
TX(bulk-out)  <0x8000 raw content bytes>
RX(bulk-in)   FILES OK

# stage 2c
TX(bulk-out)  QUIT <0x20>                        # 5 bytes, no reply expected
#   device re-enumerates as 5-EP kernel config; host waits up to 5s
```

---

## 11. Implementation notes for a libusb/Phase-1 replicator

1. Open the boot interface, claim it, find bulk IN (ep index 0) and bulk OUT (ep index 1)
   by scanning `bNumEndpoints == 2` (do not hard-code addresses 0x81/0x01 without reading
   the descriptor — the source only pins the indices).
2. Match VID/PID 0x12D1:0x897D or 0xFFFF:0x3733.
3. Implement exactly: one synchronous bulk-OUT per command string (incl. trailing 0x20),
   then one synchronous bulk-IN of up to 512 B for the ack (prefix match), 2 s read
   timeout, 3 retries; 30 s write timeout; 5 ms interleave before data chunks.
4. Chunk buffer 32 KB; do not pad the last chunk; strip the 64-byte SHA-256 header from
   the file before sending; verify the header matches SHA-256 of the content first.
5. After `QUIT` wait for re-enumeration (5 s budget) and expect the 5-EP kernel config.

---

## 12. Must-confirm-empirically (hardware unknowns)

- Absolute boot EP addresses and actual wMaxPacketSize of the E573H boot config.
- Exact `cmu_xo_trim` value sent in the first WRITEM (INI/calibration data).
- Device ack strings `WRITEM OK` / `READY` / `FILES OK` exact casing/spacing (host matches
  as prefix; assumed space-separated as in the host constants).
- Whether the device requires a `VERSION` exchange on the E573H boot (not sent by the USB
  host path in this SDK) and the expected reply content (`VER_EXPECT_VALUE` lives in
  device-side or UART header, not in this file).
- Whether QUIT triggers a physical disconnect or a silent re-enumeration, and the timing.
- Exact content layout/address expectations of `ws73.bin` (0x400000), `wifi_cali.bin`
  (0x430000), `btc_cali.bin` (0x440000), and the 0x106400 ROMBIN address (only if the
  device ROMBIN_OPEN flavor is used).
```
