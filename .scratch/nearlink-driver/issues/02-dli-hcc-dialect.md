# 02 — DLI↔WS73 HCC 方言对照

Type: research
Status: open

## Question

OpenHarmony 开源栈（communication_nearlink_service）的 **DLI 命令集/帧格式** 与 WS73 SDK 的 **HCC 包格式** 能否直接对接，还是必须加转换层？这决定 Phase 3 的开源栈接入路径。

对照项：
1. OpenHarmony 栈侧（`/mnt/hdd/nearlink-stuff/communication_nearlink_service`）：
   - `services/stack/src/dli/interface/dli_opcode.h` 的 opcode 集（如 `DLI_CREATE_CONNECTION=0x1401`）
   - `dli_layer.c` 的 5 字节帧头（type/lcid/pbFlag/ts/prio/length）+ 分片 + 命令队列
   - `SleDliLayerAdapter` 的 6 函数 HAL 缝（SleHalInit/SleSendDliPacket/...）
2. WS73 SDK 侧（`sdk/.../driver/platform/hcc/`）：
   - `hcc_usb_host.c` 的 HCC 包结构（92B 散射头 `HIUSB_PACKAGE_HEARDER_SIZE`、channel/queue 机制）
   - `hcc_cfg_comm.h` 的 channel 定义（AP/BT/SLE/...）
   - `sle_hcc_proc.c` 的 SLE HCI 数据路由
3. 判断：DLI 命令能不能经 WS73 hcc 通道送达 controller？`H2D_MSG_SLE_OPEN`(=29) 等消息对应关系？数据面（DTAP vs hcc data）怎么映射？

产出：对照表 + 「直连可行 / 需要转换层」结论，附证据。

## Why it blocks

Phase 3（开源栈移植）的接入方案；也影响 04（若需要转换层，内核侧要不要暴露 HCI 原语）。
