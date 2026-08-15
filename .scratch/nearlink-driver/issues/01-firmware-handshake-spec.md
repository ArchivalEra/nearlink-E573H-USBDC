# 01 — 固件下载握手字节流精确还原

Type: research
Status: resolved

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

## Answer

Boot 态（2 bulk EP）上的固件下载是一个**纯 ASCII 行命令 + 原始 bulk 流**协议，全部字节在 `.scratch/nearlink-driver/assets/01-firmware-handshake-spec.md`。每一条命令都是大写关键字 + 空格分隔参数 + **一个结尾空格 0x20**，无长度前缀、无换行。顺序固定：可选 `READM 0x40019380 4`（HCC 连通性探测，回 4 字节寄存器值，超时临时 500ms）→ `WRITEM 4 0x40019408 0x<trim>`（回 `WRITEM OK`）→ 每个文件分块发 `FILES 1 0x<addr> 0x<len> 0x<state>`（回 `READY`，停 5ms，发原始内容字节，回 `FILES OK`）→ `QUIT`（不回包，设备重枚举为 5 EP 内核态，host 等最多 5s）。文件映射：磁盘文件头 64 字节是内容（不含头）的 SHA-256 小写 hex，host 先校验、传输时剥掉；内容按 32KB 分块，无加密无填充，chunk 数>1 时 state=0/1/2（首/中/尾），单块=3。超时：bulk-out 30000ms（`USB_DOWNLOAD_FW_TIMEOUT`），bulk-in 2000ms/次（`READ_MEG_TIMEOUT`），每步最多重试 3 次（`HOST_DEV_TIMEOUT`）。中断点：任何 disconnect 置 `connect=FALSE` 直接失败；QUIT 后重枚举是成功路径，5s 内不出 5-EP probe 才算失败。

- Asset：`.scratch/nearlink-driver/assets/01-firmware-handshake-spec.md`
- 需实测确认：CMU XO trim 具体值（INI 校准数据）、设备应答串确切大小写、boot EP 绝对地址（源码只锁死描述符索引，未断言 0x81/0x01）。

## Hardware verification (2026-08-15, Phase-1 ws73-probe on real devices)

待实测项全部实测，结果如下（两台 ffff:3733，1-4 / 1-5）：
- **boot EP 绝对地址确认**：0x81 IN / 0x01 OUT bulk 64B（与描述符索引 0/1 对应）
- **READM 连通检查通过**：`READM 0x40019380 4 ` → 4 字节 raw，reg=0x00000000
- **WRITEM trim 接受**：trim 0x083c（SDK 默认）→ 应答 `WRITEM OK:WRITEM 4 0x40019408 0x083c \0`（复合格式 = "应答:回显命令:NUL"；前缀匹配生效）
- **FILES 分块全通**：应答 `READY:FILES01:FILES 1 0x... \0`；5 块 ws73.bin（0x400000-0x420000）+ 1 块 wifi_cali（0x430000）+ 1 块 btc_cali（0x440000）全部 FILES OK
- **关键发现：必须三文件一起灌**——只灌 ws73.bin 后 QUIT，设备重枚举仍回 boot 态（2EP）；三文件齐全才进 kernel 态
- **QUIT 触发重枚举**：设备号变化（005→008），≤5s 内出现 5EP
- **kernel 态形态**：bcdDevice 0100→0318、bcdUSB 1.10→2.0、wMaxPacketSize 64→512、EP 布局 = 数据 IN/OUT(0x81/0x01) + INT IN(0x83,8B) + RW_REG OUT/IN(0x02/0x82) —— 与 SDK hcc_usb_host.h 5EP 布局完全吻合
- 两台设备均灌成功，现处于 kernel 态

**票 01 规范实证有效，无未决项。**
