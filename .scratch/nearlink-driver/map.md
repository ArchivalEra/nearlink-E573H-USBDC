# Map: nearlink-driver — WS73 星闪 USB 驱动决策图

> Wayfinder map for `nearlink-E573H-USBDC`. Local-markdown tracker: child tickets are files under `issues/`, numbered from 01.

## Destination

从「USB 上裸 boot 态的 ffff:3733」走到「Linux 上跑通星闪点对点通讯」的**可执行方案**：固件握手、传输形态、开源栈接入、固件处理等决策全部落地，产出可直接开工的方案（执行另开会话，不在地图内）。

## Notes

- 域：WS73 / SLE (SparkLink Low Energy) / 星闪 / HCC-over-USB / OpenHarmony nearlink stack
- 技能：本图用 `/research`（AFK 侦察）、`/grilling`（HITL 拍板）、`/domain-modeling`、`/prototype`
- 素材位置：
  - 硬件：USB 上的 ffff:3733（boot 态，2 bulk EP）——设备情报见 `docs/DEVICE-INTEL.md`
  - 海思 WS73 SDK 源码：`sdk/ws73_sdk_linux_WS73_1.10.110/`（hcc_usb_host.c / plat_firmware.c / sle_driver）
  - OpenHarmony 开源栈：`/mnt/hdd/nearlink-stuff/communication_nearlink_service`（本机克隆）
  - 其余参考仓库：`/mnt/hdd/nearlink-stuff/`（Hi2821 SDK、FlashKeyboard/TIoT 等）
  - 协议/生态文档：`docs/USB-PROTOCOL.md`、`docs/ECOSYSTEM.md`
- 用户偏好：中文交流；每个决策给方案让用户拍板；文档全英文（repo 规则）
- 环境：本机 x86 Linux，kernel headers + gcc 齐（可编内核模块）；无 gh CLI / 无 GH token（tracker 用本地 markdown）

## Decisions so far

<!-- 每解一张票，把 gist + 链接追加到此处（一行）。 -->

（暂无）

## Not yet specified

<!-- 还看不清、尚不能成票的雾区；前沿推进后会graduated成票 -->

- **应用形态**：星闪点对点通讯最终做什么（文本 / 文件传输 / HID 外设 / UART 透传）——等 02/03 明确栈能力后定，Phase 4 范畴
- **内核驱动代码结构**：等 01（握手规范）与 04（传输形态）拍板后，Phase 2 骨架自然成形，现在写不出票
- **测试策略**：握手跑通后（01 验证）再定 —— 单测 / usbmon 抓包 / 双 dongle 对测
- **固件来源合规**：ws73.bin 的分发/版本管理策略——等 06 判断 blob 格式后定

## Out of scope

<!-- 明确排除在目的地之外的工作；永不 graduate -->

- **芯片侧固件开发**（Hi2821/WS63/WS73 固件编写）——本图只做 host 侧驱动与栈
- **WS73 的 WiFi / BLE 功能**——只做 SLE/NearLink 路径
- **自制 dongle / 硬件改造**——用现有 ffff:3733 设备
- **为 OpenHarmony 系统做驱动**——目标是 Linux 主机
