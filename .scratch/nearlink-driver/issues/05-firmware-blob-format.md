# 05 — ws73.bin 内部格式与签名分析

Type: research
Status: open

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
