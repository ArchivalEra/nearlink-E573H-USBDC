# 10 — 三模全速落地：电视盒 WiFi/蓝牙/星闪 全开

Type: task
Status: open
Blocked by:

## Question

单 WS73 dongle 为电视盒提供三模全速无线（WiFi6 + 蓝牙 + 星闪 SLE），这是地图目的地（用户 2026-08-15 澄清）。PM 三服务并存已实锤（plat_pm_wlan.c:140-142 数组），单 dongle 无需 hack（区别于票 09 双设备）。

## 已就绪（x86 侧，2026-08-16 全部编译完成）

- plat_soc.ko + sle_soc.ko：编译、加载、固件下载、SLE 通道握手全通（票 06/08）
- **wifi_soc.ko 编译成功**（253 文件，e79ad8a）：cfg80211 完整驱动 → wlan0/wlan1/p2p0；
  7.x 适配 = dev_addr_set×4 + netif_rx_ni→netif_rx + cfg80211_new_sta/del_sta(wireless_dev*) +
  ch_switch_notify(link_id) + preset_chandef(u.ap) + roam_info(links[0]) + 去 VFS ns + 去 internal.h
- **ble_soc.ko 编译成功**（7676d6a）：BlueZ hci0 注册（hci_alloc_dev/register）；
  7.x 适配 = asm/unaligned.h→linux/unaligned.h + 去 dev_type/HCI_PRIMARY
- **加载顺序**: plat → ble → sle → wifi（首个服务做固件下载）
- ⚠️ 真机三模验证待 dongle 插回 USB（停电后两 dongle 不在总线）

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

## 侦察结论（子代理 B，2026-08-15 已落盘 lab-notes/BLE-WIFI-USERLAND-RESEARCH.md）

- **ble_soc.ko = 真 BlueZ HCI 设备**（hci_alloc_dev/hci_register_dev → hci0）→ **系统 BlueZ 直接驱动**，无 /dev/hwble（Android 版才有）
- **wifi_soc.ko = 完整 cfg80211 驱动**（wiphy_new/register + 标准 ops）→ **系统 wpa_supplicant/hostapd 直接驱动**（nl80211）；wlan0/wlan1/p2p0
- **三模共存完全支持**：pm_svc_state[] 数组，首个服务做固件下载，无互斥；BT/WLAN 共存编译进 _PRE_WLAN_FEATURE_BTCOEX
- **用户态**: BLE=系统 BlueZ ✅ / WiFi=系统 wpa_supplicant ✅ / SLE=闭源 ARM lib 不可用 → 自写 /dev/hwsle 客户端 或 OHOS 栈（票 03 GREEN）
- 加载顺序: plat_soc → ble_soc → sle_soc → wifi_soc
- 剩余未知: 三模同开时单 480Mbps USB 链路带宽分配；电视盒 7.2 闭源内核（师傅编译）

## 阻塞

（无——三模共存架构支持；仅受编译/用户态栈工作量和 OOM 约束）
