# 01 — 固件下载握手字节流精确还原

Type: research
Status: open

## Question

把 WS73 boot 态固件下载握手还原成**逐字节可实现**的协议规范，供 Phase 1 验证工具（libusb）与 Phase 2 内核驱动直接照抄。

已知线索（sdk/ws73_sdk_linux_WS73_1.10.110）：
- boot 态 = 2 bulk EP（0x81 IN / 0x01 OUT，64B，Full Speed）
- `plat_firmware.c`：ASCII 命令 `WRITEM/WMEM/RMEM/FILES/QUIT`，`MSG_FROM_DEV_*` 应答，SHA256 头，`FIRMWARE_FILESIZE_MAX 200KB`，分块 32KB
- `hcc_usb_host.c`：`oal_usb_boot_init_config` 分配 32KB 缓冲走 bulk-out；固件后 `QUIT` → 重枚举 5 EP

需要确认/还原：
1. 握手第一条命令的**精确字节**（命令字 + 参数格式、`1,0x400000,/path` 的编码）
2. 应答的**精确字节**（`MSG_FROM_DEV_*` 具体值、长度/超时）
3. 固件文件到线格式的**逐字段映射**（SHA256 头布局、是否加密/分段、对齐）
4. 顺序与超时表（每一步的先后、等待窗口 `USB_DOWNLOAD_FW_TIMEOUT 30000ms`）
5. 设备侧哪些行为会中断流程（disconnect/重枚举触发点）

产出：一份「boot 握手线格式规范」附到本票（`.scratch/nearlink-driver/assets/` 或指向 docs 位置），Phase 1 直接照抄实现。

## Why it blocks

04（传输形态拍板）依赖本票：握手协议形态（短命令+大块 bulk 流）决定内核 vs userspace 实现方式的选择空间。
