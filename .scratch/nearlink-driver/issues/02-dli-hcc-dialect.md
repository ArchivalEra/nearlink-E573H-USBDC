# 02 — DLI↔WS73 HCC 方言对照

Type: research
Status: resolved

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

## Answer

**（b）需要转换层，但形态是「传输适配器」而非「协议转换器」——DLI 帧逐字节原样作为 HCC payload 送达 controller 是可行的。** 双方 datatype 字节完全一致：DLI `DLI_DATATYPE_CMD=0xA1/EVENT=0xA2/ACB=0xA3/ICB=0xA4`（dli_layer_stru.h:81-86）== WS73 `HCI_DATATYPE_SLE_CMD/EVENT/ACB/ICB`（sle_hci_chba_proc.h:19-24）；数据帧 TCID 定位 `HCI_DATA_TCID_POS=5`（sle_hci_chba_proc.h:11-12）与 DLI 5B 头+DTAP 头首字节=tcid 的布局吻合。WS73 主机驱动是「帧搬运工」：/dev/hwsle 写路径、sle_hci_send_frame、hcc_bt_tx_data 均不解析 opcode，DLI 帧（0x0401/0x0C02/0x1401…）原样进 hcc payload（hcc.c:1012-1069）；H2D_MSG_SLE_OPEN=29 只是传输开关消息（sle_dev.c open → plat_pm_wlan.c:459-487），与链路命令不同维度。适配层需处理 3 个 HAL 缝差异：RX 侧拆 type 字节（/dev/hwsle 读回含 0xA2 首字节，OHOS hciPacketReceived 要求 type 分离）、合成 initializationComplete、生命周期映射（open→H2D_MSG_SLE_OPEN）。DTAP 数据面 1:1 承载（DTAP 头 tcid 首字节 = 数据帧第 6 字节）。残余风险仅一项：WS73 固件侧 HCI opcode 是否逐一匹配 DLI 编号——SDK 无固件 HCI 源码，需 Phase 4 实机验证（datatype/帧头/TCID 三者一致是本结论依据）。

完整对照报告：`.scratch/nearlink-driver/assets/02-dli-hcc-dialect.md`

关键证据：`dli_layer_stru.h:28,81-86`、`dli_opcode.h:228-297`、`SleDliLayerAdapter.cpp:129-149`、`SleDliCallbacks.cpp:57-70`、`hcc_comm.h:121-129`、`hcc.c:1012-1069`、`sle_hcc_proc.c:107-141`、`sle_dev.c:121-163`、`sle_hci_chba_proc.h:11-12,19-24`、`hcc_cfg_comm.h:93-127`、`plat_pm_wlan.c:459-487`、`dtap_frame.h:108-121`。
