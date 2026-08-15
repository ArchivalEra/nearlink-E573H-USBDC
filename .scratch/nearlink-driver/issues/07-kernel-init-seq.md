# 07 — kernel 态初始化序列实验（NV 推送 → SLE_OPEN → 通道建立）

Type: research
Status: open
Blocked by:

## Question

把 kernel 态初始化跑通：host 推 BSLE customize/INI 数据 → 等 CUSTOMIZE_RECEIVED → SLE_OPEN → 等 BOOT_FINISH → 建立 SLE HCI 数据通道。这是 Phase 2 内核驱动 kernel 态部分的前置验证。

已知（SDK 源码 + 真机实验）：
- 只发 H2D_MSG_SLE_OPEN(=29) 控制传输 → 设备重启回 boot（实验 #1 已证）
- 完整序列（sle_host_register.c:145-165, customize_bsle.c:682）:
  1. bulk 数据通道(HCC_CHANNEL_AP) 发 `[hcc头] [bsle_msg_tag{type=0 CUSTOMIZE_DATA, len}] [bfgn_bt_customization_stru]`（≤800B）
  2. 等设备 C2H 状态 `BSLE_STATUS_CUSTOMIZE_RECEIVED`
  3. 控制传输发 SLE_OPEN（0x21/0x00 + 4B LE 1D 00 00 00）
  4. 等 `BSLE_STATUS_BOOT_FINISH`
- HCI 帧格式（子代理 B）: `[5B hcc_header: stype(4):sub(4)|queue_id|pay_len LE]` + HCI 帧（首字节 datatype 0xA1 CMD/0xA2 EVENT/0xA3 ACB/0xA4 ICB），SLE 用 service 0xA / queue 8

未知/待解：
1. `bfgn_bt_customization_stru` 结构体布局与最小合法内容（空结构能不能过？SLE 是否依赖其中的字段）
2. hcc 头 5 字节精确布局（service_type/sub_type/queue_id/pay_len 的位域偏移）——查 hcc.h 里 HCC_TRANS_HEADER 定义
3. C2H 状态消息的接收路径（INT EP 还是 bulk IN？帧格式？）
4. 设备对空/最小 INI 数据是否接受，还是会拒绝

产出：kernel-probe 扩展（libusb: bulk 发 INI + 控制发 OPEN + 监听 C2H），真机在 1-4 实验（1-5 保持对照组），成功则记录通道建立后设备形态（速度/通知/可发 HCI 命令）。

## Blocked by

（无——01 实证完成，可直接开；仅受硬件实验风险约束：1-4 可反复重灌）
