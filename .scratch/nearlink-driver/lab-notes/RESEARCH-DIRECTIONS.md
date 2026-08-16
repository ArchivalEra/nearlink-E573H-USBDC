# 研究方向清单 (Research Directions)

> 整理: 2026-08-16 · 目的: 记录之后要研究的方向，避免资料散落。
> 关联: wayfinder 票 09/10 + lab-notes 各文档。

## 一、短期（双 dongle / 对端实连验证）

1. **WiFi 激活死锁问题**（2026-08-16 踩坑，最高优先）
   - 现象: `wifi_soc.ko` 加载 OK，但 sysfs 懒初始化（echo init > /sys/kernel/wifi）**内核态卡死**，宿主 I/O 阻塞，被迫重启
   - 根因推断: BLE/SLE 已占用 PM 单实例通道，WiFi 再 open 冲突 → PM/固件交互死锁
   - 方向: SDK PM 三服务协调（plat_pm_wlan.c pm_svc_open），WiFi 需独占 PM（先关 BLE/SLE）；初始化要单步可控
   - 参考: 票 10 + EXPLORE 日志死锁记录

2. **双 dongle 实连**（票 09）
   - hcc/PM 多实例 hack，或分时切换
   - 验证: 广播互发现（ADV_REPORT 0x001A）→ 连接（CREATE_CONNECTION）→ SSAP 服务 → 数据面

3. **星闪手机对测**（最快路径）
   - 华为 Mate/P 系列支持星闪；单 dongle 即可

## 二、中期（用户态 SLE 栈实现）

4. **SSAP 服务层**（协议研究子代理报告中）
   - 服务发现/属性模型（类 BLE GATT）、UUID 方案、操作码
   - 复用: OpenHarmony nlstk_ssap_app_*（Apache-2.0）+ HiSilicon SLE API 头

5. **连接建立 + 数据面**
   - adv→scan→conn 参数、ACB/ICB/IOB 通道、tcid 分配
   - 低时延链路（SET_IOG_PARAM 已验证可配）

6. **测距（ranging）**
   - READ_MEASURE_CAPS/SET_MEASURE_EN 已验证，实测需对端

## 三、长期（电视盒三模落地，票 10）

7. **ble_soc 已通**（hci1 + LE 扫描发现设备，56c901e）
8. **wifi_soc 待解死锁**（方向 1）→ wlan0 后接 wpa_supplicant
9. **星闪用户态**：OHOS 栈移植（票 03 GREEN）或自写 HCI 客户端（sle-hci-scan.py 已验证框架）
10. **hi3798 电视盒**：SHIFU-BUILD-LIST.md 给师傅交叉编译

## 四、资料待爬（子代理进行中）

- OpenHarmony 社区星闪资料（上游 gitcode、HDI 驱动接口、社区新闻）
- NearLink 协议细节（SSAP/连接/数据面/测距）
- 产出: OPENHARMONY-COMMUNITY-RESEARCH.md + NEARLINK-PROTOCOL-RESEARCH.md

## 五、经验教训（防再踩）

- **内核模块懒初始化 = 高风险**：必须在隔离环境单步验证，禁止与已占 PM 的服务并行
- **重编译必须 -j1 + free 检查**（OOM 黑屏两次教训）
- **只操作星闪口**（用户红线）；宿主 WiFi/BT 不动
