# 09 — 双 dongle 互测前置：hcc/PM 多实例化 hack

Type: task
Status: open
Blocked by:

## Question

让两个 ffff:3733 dongle 能互测（星闪 SLE 对连、蓝牙对连、WiFi 对连——测各自极限延迟/速度）。当前硬障碍：**hcc/PM 全局单例**（SDK 架构 = 一个 host 连一个 WS73）。

## 已达成（2026-08-15 实验）

- **双设备同时 kernel 态 ✅**（零编译路径）：libusb 先灌 1-4+1-5 三文件 → 后 insmod plat_soc+sle_soc，驱动 probe 双 5EP 走 kernel 配置（dmesg "kernel usb" ×2），绕过单实例固件下载
- **但 /dev/hwsle 单实例 ❌**：第二次 open 超时（"sle open device finish fail, time out"）——hcc/PM 状态是全局单例

## 前置差距（各模式）

| 模式 | 卡点 |
|---|---|
| 星闪 SLE | hcc 多实例 + SLE 连接建立（advertiser/scanner + SSAP）+ 数据面（HCI 方言未验证，票 02 残余风险） |
| 蓝牙 | ble_soc.ko 未编 + 用户态栈（ARM 二进制 x86 不可用，需移植/自写） |
| WiFi | wifi_soc.ko 未编 + hostapd/wpa_supplicant 未编（SDK open_source/ 有源码） |

## hack 方向（调研起点，未实施）

1. `driver/platform/hcc/`：`g_usb`/`g_hcc_*` 全局单例 → 按 device 索引实例化（hcc_handler per udev）
2. `driver/platform/pm/plat_pm_wlan.c`：`pm_svc_power_on`/`pm_sle_open` 单实例 → per-device
3. `sle_driver/sle_dev.c`：`/dev/hwsle` 按 device 分配（`/dev/hwsle0`/`hwsle1`）
4. 风险：改动面大（hcc core + pm + sle_dev），-j1 重编，OOM 谨慎（7.6G RAM，黑屏教训）

## 更快的替代路径（若用户有对端设备）

单 dongle 对其他设备（支持星闪的华为手机/蓝牙设备/WiFi 路由器）测速**立刻可行**——不需 hack：
- 星闪：SLE 连接手机（HCI 方言验证先行）
- 蓝牙：编 ble_soc + 用户态栈后连手机
- WiFi：编 wifi_soc + hostapd 后连路由器

## 结论

双 dongle 互测 = 架构级 hack（数小时+）；有对端设备则单 dongle 测速是捷径。收束于本次探索，待用户决策路径。
