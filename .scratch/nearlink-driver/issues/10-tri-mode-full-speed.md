# 10 — 三模全速落地：电视盒 WiFi/蓝牙/星闪 全开

Type: task
Status: open
Blocked by:

## Question

单 WS73 dongle 为电视盒提供三模全速无线（WiFi6 + 蓝牙 + 星闪 SLE），这是地图目的地（用户 2026-08-15 澄清）。PM 三服务并存已实锤（plat_pm_wlan.c:140-142 数组），单 dongle 无需 hack（区别于票 09 双设备）。

## 已就绪（x86 侧验证）

- plat_soc.ko + sle_soc.ko：编译、加载、固件下载、SLE 通道握手全通（票 06/08）
- wifi_soc.ko：253 文件源码 + Makefile 已适配 7.x（ccflags-y/isystem/mcmodel）→ 编译中
- ble_soc.ko：driver/bsle/ble_driver/linux（ble_host_hcc.c）待编

## 执行步骤（-j1 安全流程）

1. **编译三模模块**（逐个 -j1，黑屏教训）:
   - wifi_soc（进行中）→ ble_soc → （sle_soc 已有）
2. **加载 + 固件下载**（load-driver.sh 扩展三模块）
3. **用户态栈**（三模各自）:
   - WiFi: open_source/wpa_supplicant_2_10_linux.patch + hostapd（SDK open_source/）——需交叉编 aarch64 或 x86 直接编
   - 蓝牙: 内核自带 BlueZ 栈（HCI 走 ble_soc 的 /dev/hwble？需确认 SDK 是否暴露 HCI 字符设备）
   - 星闪: sparklinkd 系 ARM 二进制 x86 不可用 → OHOS 栈移植（票 03 GREEN）或自写 HCI 客户端（hwsle-probe 已验 /dev/hwsle 通道）
4. **网络接口验证**: wlan0 出现 + 可连 AP；蓝牙 HCI 接口；星闪 SLE 连接
5. **电视盒（hi3798）侧**: 按 SHIFU-BUILD-LIST.md 交叉编译（师傅）

## 未知/待解

- ble_soc 是否注册 /dev/hwble 或走 BlueZ HCI 协议（查 ble_driver/linux）
- wifi_soc 加载后 wlan0 形态（SDK 默认 ws73_cfg_default.ini:30-35 有 wlan0/wlan1/p2p0）
- 三模同开时 USB 带宽/PM 协同（单 480Mbps 链路分三栈）
- 电视盒内核 7.2 闭源 → 师傅编译（资料包已备）

## 阻塞

（无——三模共存架构支持；仅受编译/用户态栈工作量和 OOM 约束）
