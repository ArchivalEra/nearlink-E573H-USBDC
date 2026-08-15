# 05 — ws73.bin internal format & signature analysis

SDK: `sdk/ws73_sdk_linux_WS73_1.10.110` (extraction root `<SDK>` below)
Blob: `<SDK>/firmware/us/ws73.bin` (143,956 B), cross-checked with `<SDK>/firmware/e/ws73.bin` (137,644 B)

## Verdict (TL;DR)

- The blob is **plaintext** (not encrypted, not cryptographically signed).
- Structure: **64-byte lowercase-ASCII hex SHA256 digest** of the remainder, followed by the **raw binary payload**.
- `sha256(payload) == header` verified for **all 8 blobs** in `firmware/{us,e}/`.
- The SHA256 header is an **integrity (corruption) check only** — a bare digest, no HMAC/RSA/ECC, no secure-boot chain in the host SDK.
- Host pre-processing is exactly one step: **strip the 64-byte header**, then push the payload raw.
- For Phase 1 libusb tool: **raw file cannot be pushed byte-for-byte** — it must be truncated by 64 bytes first, then chunk-sent per ticket 01's FILES handshake. No decryption, no re-signing, no padding/alignment of the data itself.

---

## 1. Binary analysis of the blob

### 1.1 Head (first 64 bytes) is an ASCII SHA256 digest

`us/ws73.bin` first 64 bytes:

```
1ec2f93a520d090cffbfb378da4439b59015215bd9cb6cca1e64135724d4cedc
```

`e/ws73.bin` first 64 bytes:

```
2b4ccf4c127caac551d4cf69d272f9a4f01ea91b7f4b32700f07965ea16a8b68
```

`python3` check (all 8 blobs):

```
us/ws73.bin     len=143956 ascii64hdr=True sha256(payload)==hdr=True
us/wifi_cali.bin len=21044  ascii64hdr=True sha256(payload)==hdr=True
us/btc_cali.bin len=7772    ascii64hdr=True sha256(payload)==hdr=True
us/wow.bin      len=22859   ascii64hdr=True sha256(payload)==hdr=True
e/ws73.bin      len=137644  ascii64hdr=True sha256(payload)==hdr=True
e/wifi_cali.bin len=21060   ascii64hdr=True sha256(payload)==hdr=True
e/btc_cali.bin  len=34120   ascii64hdr=True sha256(payload)==hdr=True
e/wow.bin       len=22859   ascii64hdr=True sha256(payload)==hdr=True
```

`sha256(whole_file)` (e.g. `7ac04834...` for us/ws73.bin) is NOT the header; the header is always `sha256(bytes[64:])`.

### 1.2 Payload is plaintext, not encrypted

`us/ws73.bin` payload (bytes 64..end, 143,892 B) segmented entropy:

```
[     0: 16384] entropy=1.647 zeros= 84.6%   <- low-entropy head
[ 16384: 32768] entropy=5.153 zeros= 38.7%
[ 32768: 49152] entropy=7.089 zeros=  5.4%   <- code-like region
[ 49152: 65536] entropy=6.866 zeros=  8.5%
[ 65536: 81920] entropy=7.051 zeros=  4.9%
[ 81920: 98304] entropy=7.097 zeros=  3.8%
[ 98304:114688] entropy=6.906 zeros=  9.4%
[114688:131072] entropy=6.885 zeros= 12.5%
[131072:143892] entropy=6.758 zeros= 16.0%   <- table-like tail
```

Uniformly encrypted data would sit at ~8.0 bits/byte everywhere. This payload has a structured low-entropy head, code-like middle, and a table-like tail.

`us/ws73.bin` payload head (after the 64-byte header):

```
00000000000000000000000000000000 17432f0067002373 17732e00670023c9 ...
```

16 zero bytes followed by what looks like per-section descriptors — consistent with a linker-generated image with a section table, not ciphertext.

`us/ws73.bin` payload tail (last 64 bytes) — monotonically increasing 16-bit LE values, an exception/vector table:

```
be07 c307 c707 cc07 d007 d407 d907 dd07 e107 e507 e907 ed07 f107 f507 f907 fd07
0108 0508 0908 0d08 1008 1408 1808 1c08 1f08 2308 2608 2a08 2d08 3108 3408 3808
```

### 1.3 No signature/secure-boot machinery anywhere in the host SDK

- `grep -rn "sec_img\|secure_boot\|signature\|RSA"` over all `.c/.h/.config/.mk` in the SDK: only unrelated SLE user-space error codes (`include/bsle/common/errcode.h:333-338`) and unrelated "powersave" hits. No firmware signature verification exists on the host side.

---

## 2. SDK download code that consumes the blob

### 2.1 Header constants — `<SDK>/driver/platform/drv/crypto/plat_sha256_calc.h:20-22`

```c
#define SHA256_HEADER_ARR_LEN   32  // 32 Bytes (256 bits).
#define SHA256_HEADER_BYTES_LEN 2   // Each byte is displayed as two characters.
#define SHA256_HEADER_LENGTH    64  // 32 Bytes (256 bits). Each byte is displayed as two characters.
```

Note: these are only defined when `_PRE_PLAT_SHA256SUM_CHECK == 1`. `plat_firmware.c:1074-1076` carries an unconditional fallback:

```c
#ifndef SHA256_HEADER_LENGTH
#define SHA256_HEADER_LENGTH 64
#endif
```

### 2.2 Documented file structure — `plat_firmware.c:934-939`

```c
 *                 File's Structure
 *
 * |<--- sha256sum header --->|<--- file content --->|
 * |         64 Bytes         |      *** Bytes       |
```

### 2.3 SHA256 verification (optional, host-side only) — `plat_firmware.c`

- `firmware_sha256_checksum(path)` (940-1010), compiled only under `_PRE_PLAT_SHA256SUM_CHECK`:
  - size sanity: `tmp <= SHA256_HEADER_LENGTH || tmp > FIRMWARE_FILESIZE_MAX` -> fail (958)
  - `get_file_content()` (866-884): reads 64-byte header, then the rest.
  - `do_check_sha256sum()` (891-914): computes `do_sha256(buf, len)` over payload, hex-encodes to 64 chars, `strncmp` against header.
- Config: `driver/android.config:122` `_PRE_PLAT_SHA256SUM_CHECK=y` (enabled). **But** the USB reference configs disable it:
  - `build/config/ws73_usb_light.config:118` `# _PRE_PLAT_SHA256SUM_CHECK is not set`
  - `build/config/ws73_usb_light_v2.config:121` `# _PRE_PLAT_SHA256SUM_CHECK is not set`
  - => In the USB build the host does NOT verify; the header-skip below is unconditional regardless.

### 2.4 Host transforms the file: strip header, send payload raw — `plat_firmware.c`

`start_parse_file_etc()` (1077-1132), the code executed for every `FILES` download:

```c
1106  /* skip bin's header area */
1107  file_len = file_len - SHA256_HEADER_LENGTH;
...
1116  osal_klib_fseek(SHA256_HEADER_LENGTH, SEEK_SET, fp);
1119  ret = read_and_send_file_etc(&offset, fp, addr, file_len);
```

- The 64-byte ASCII header is **never** sent to the device.
- `read_and_send_file_etc()` (1013-1073) chunks the payload:
  - `transmit_limit = g_fw_data_buf_len`; `per_send_len = min(transmit_limit, remaining)` (1021-1022).
  - buffer sizing `firmware_buf_init()` (1219-1236): `min(cap_max_trans_size, MAX_FIRMWARE_FILE_TX_BUF_LEN)` rounded down to 8. `MIN_FIRMWARE_FILE_TX_BUF_LEN=4096`, `MAX_FIRMWARE_FILE_TX_BUF_LEN=32*1024` (`plat_firmware.h:84-85`).
  - `hcc_usb_host.c:1166/1343-1350` allocates a `MAX_FIRMWARE_FILE_TX_BUF_LEN` bulk-out buffer in boot state.
- Per chunk, wire format is an ASCII command line then raw bytes (1043-1064):

```c
snprintf(..., "%s%c%d%c0x%lx%c0x%lx%c0x%lx%c",
         FILES_CMD_KEYWORD, ' ', FILE_COUNT_PER_SEND, ' ',
         addr_send, ' ', (unsigned long)rdlen, ' ',
         (unsigned long)trans_state, ' ');          // "FILES 1 <addr> <len> <state> "
firmware_send_and_recv_expect_result_func(buff_tx, len, MSG_FROM_DEV_READY_OK);  // expect "READY"
firmware_send_and_recv_expect_result_func(g_fw_data_buf, rdlen, MSG_FROM_DEV_FILES_OK); // expect "FILES OK"
```

  - `trans_state` enum (`plat_firmware.c:49-52`): `START=0, MID=1, END=2, START_END=3`; single-chunk send uses `START_END` (1035-1041).
  - Payload is streamed in file order at monotonically increasing `addr`; the device writes it directly. No re-assembly, no re-encryption, no interleaving.
- Target address for ws73.bin: `FW_BIN_DOWNLOAD_CMD "1,0x400000,"CONFIG_FIRMWARE_BIN_PATH` (`plat_firmware.c:68`); wifi_cali `0x430000`, btc_cali `0x440000` (70-71). `CONFIG_FIRMWARE_BIN_PATH="/etc/ws73/ws73.bin"` (`build/config/ws73_usb_light.config:139`, etc.).

### 2.5 UART path is identical in spirit — `plat_firmware_uart.c`

- `uart_download_file()` / `uart_send_file_data()` (240-310): same `firmware_sha256_checksum()` pre-check under the same flag, same `len -= SHA256_HEADER_LENGTH; fseek(64)` header skip (274-278), then XMODEM-CRC framed transfer. (Not relevant to USB, but confirms the header-skip is universal.)

---

## 3. us/ vs e/ — two target image variants, same format

- `firmware/{e,us}/` are two build-variant image sets. Both contain the same 4 files with identical format.
- `us/ws73.bin` payload 143,892 B vs `e/ws73.bin` payload 137,580 B; only a 17-byte common prefix; `wow.bin` identical (22,859 B both).
- The project targets the USB device path and uses `firmware/us/`: `docs/SDK-INTEL.md:34,72`, `README.md:53-54`, USB configs point at `/etc/ws73/ws73.bin`. `us` = USB variant of the image set.
- Format conclusion applies to both; Phase 1 uses `firmware/us/`.

---

## 4. Conclusion for the Phase 1 libusb tool

Required host-side transformation, in order:

1. Read the whole `firmware/us/ws73.bin` (143,956 B).
2. Optional (recommended): verify `sha256(file[64:])` == `file[0:64]` (ASCII hex) locally — the official kernel driver does this only when `_PRE_PLAT_SHA256SUM_CHECK=y`, which is NOT set in the USB reference config, so the device never depends on the host doing it.
3. **Discard the first 64 bytes** — this is mandatory and unconditional in the driver (`plat_firmware.c:1107,1116`).
4. Chunk the remaining 143,892 payload bytes (chunks ≤ 32 KB, 8-aligned sizes by driver convention, last chunk may be short) and push per ticket 01's handshake: `FILES 1 <addr> <len> <state> ` command → `READY` → payload bulk-out → `FILES OK`, with `addr = 0x400000 + accumulated offset`, state = START/MID/END/START_END (0/1/2/3).

No decryption, no re-signing, no header to add, no padding to apply. The blob is "ready to load" apart from its 64-byte integrity header, which must be stripped.

### Evidence index (file:line)

- `driver/platform/drv/crypto/plat_sha256_calc.h:20-22` — SHA256_HEADER_LENGTH=64 etc.
- `driver/platform/firmware_download/plat_firmware.c:934-939` — documented file layout (64 B header + content)
- `plat_firmware.c:866-884` (get_file_content), `891-914` (do_check_sha256sum), `940-1010` (firmware_sha256_checksum)
- `plat_firmware.c:1106-1119` — unconditional 64-byte header skip, then raw chunked send
- `plat_firmware.c:1013-1073` — chunked FILES framing ("FILES 1 addr len state", "READY", "FILES OK")
- `plat_firmware.c:49-52` — trans_state enum
- `plat_firmware.c:68-71` — download addresses 0x400000/0x430000/0x440000
- `driver/platform/pm/plat_firmware.h:84-85` — MIN/MAX_FIRMWARE_FILE_TX_BUF_LEN
- `driver/platform/hcc/host/hcc_usb_host.c:1166,1343-1350` — 32 KB boot bulk-out buffer
- `build/config/ws73_usb_light.config:118` (+ v2:121) — `_PRE_PLAT_SHA256SUM_CHECK` NOT set for USB build
- `driver/android.config:122` — SHA256 check enabled in the Android (non-USB-ref) config
- `driver/platform/firmware_download/plat_firmware_uart.c:261-310` — UART path, same header-skip
- `docs/SDK-INTEL.md:34,72`, `README.md:53-54` — `firmware/us/` is the deployed set
