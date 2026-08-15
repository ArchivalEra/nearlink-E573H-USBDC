# 05 — ws73.bin 内部格式与签名分析

Type: research
Status: resolved

## Question

`firmware/us/ws73.bin`（143,956 B，`file` 显示为 data）的内部结构：是否加密/签名、和 SDK 里 SHA256 下载校验的关系、下载前 host 是否需做任何加工（解密/加头/对齐）。

调研项（源：`sdk/ws73_sdk_linux_WS73_1.10.110/` + 本机文件）：
1. `file`/`xxd`/熵分析 ws73.bin 头部与分布；对照 `plat_firmware.c` 里 SHA256_HEADER_LENGTH 与头部字段定义
2. `plat_firmware.c` 中 firmware header 解析代码（`firmware_quit_func`/校验流程）——host 读 blob 后做了什么再发
3. `CONFIG_SUPPORT_STATIC_FIRMWARE_MEM`、`MAX_FIRMWARE_FILE_TX_BUF_LEN` 等是否暗示分段/静态装载
4. 同目录 `e/` vs `us/` 两套 blob 的差异含义（工程版 vs USB 版）
5. 结论：Phase 1 工具直接把原文件灌给设备即可，还是需先加工

产出：blob 格式判定 + 「可直接灌 / 需加工」结论，附证据。

## Why it blocks

Phase 1/2 的固件装载实现正确性（若需加工，工具和驱动都要按同一流程做）。

## Answer

**blob = 64 字节 ASCII 小写 hex SHA256 头 + 明文裸二进制**，不加密、不签名。已验证 `firmware/{us,e}/` 全部 8 个 blob：`sha256(payload) == header` 恒成立（如 us/ws73.bin 头 `1ec2f93a…`，payload 143,892 B）。payload 分段的熵（1.6~7.1 bits/byte，头部 84.6% 零字节、尾部为递增 16 位 LE 向量表）证明是普通链接映像，非密文。全 SDK 无任何 sec_img/secure-boot/RSA 固件签名机制。

**host 加工只有一步：无条件剥掉前 64 字节头，其余原样分块下发。** `plat_firmware.c:1106-1119` `file_len -= SHA256_HEADER_LENGTH; fseek(64)` 后 `read_and_send_file_etc` 按 ≤32KB、8 对齐分块发 `FILES 1 <addr> <len> <state>` → `READY` → 裸数据 → `FILES OK`（1043-1064），目标地址 ws73.bin=0x400000、wifi_cali=0x430000、btc_cali=0x440000（68-71）。SHA256 校验仅是可选的完整性检查：`_PRE_PLAT_SHA256SUM_CHECK` 在 USB 参考配置 `ws73_usb_light.config:118`/v2:121 中**未开启**（android.config:122 才开），设备端不依赖 host 校验。

**结论（Phase 1 libusb 工具）：不能逐字节直接灌原文件**——必须先截掉前 64 字节（`file[64:]`），再按票 01 的握手逐块（≤32KB）发送 payload 到 0x400000 起。无需解密、无需重签名、无需加头、无需对齐填充。

完整报告与证据（file:line、xxd 摘录、熵表）：`.scratch/nearlink-driver/assets/05-firmware-blob-format.md`

关键证据：`plat_sha256_calc.h:20-22`（HEADER_LENGTH=64）、`plat_firmware.c:934-939`（文档化布局）、`plat_firmware.c:1106-1119`（剥头+裸发）、`plat_firmware.c:1043-1064`（FILES 分块线格式）、`plat_firmware.h:84-85`（4K/32K 缓冲）、`hcc_usb_host.c:1166,1343-1350`（boot 态 32KB bulk-out）。
