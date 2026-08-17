# 研究方向清单 (Research Directions)

> 整理: 2026-08-16 · 更新: 2026-08-17（最终整合盘点）
> 目的: 记录之后要研究的方向，避免资料散落。
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

4. ~~**SSAP 服务层**~~ ✅ **已完成**（2026-08-16 → 17）
   - 产出: `stack/ssap/` 五模块（codec/transport/server/link/feature_mgr），三套测试全绿
   - 蓝本: ssap_pkt.h（OHOS, Apache-2.0）+ OHOS ssaps_server.c 语义 + OSPL sle_ssap.rs 交叉验证（进行中）

5. ~~**连接建立 + 数据面**~~ ✅ **初版已完成**
   - 产出: `ssap_link.c` DLI 状态机（0x1401→0x0015→0x1802/1804→0x1403）+ hwsle_transport ACB 适配
   - 待办: OSPL sle_conn.rs 交叉核对（子代理进行中）→ 补 LINK_LOSS/超时重试

6. **测距（ranging）**
   - READ_MEASURE_CAPS/SET_MEASURE_EN 已验证，实测需对端
   - OSPL 侧: sle_phy.rs 调查中（确认 OpenSparklink 是否有任何 ranging 实现）

## 三、长期（电视盒三模落地，票 10）

7. ~~**ble_soc 已通**~~ ✅（hci1 + LE 扫描发现设备，56c901e）
8. **wifi_soc 待解死锁**（方向 1）→ wlan0 后接 wpa_supplicant
9. ~~**星闪用户态**~~ ✅ **SSAP 栈已完成初版**（票 03 GREEN → stack/ssap/）
10. **hi3798 电视盒**：SHIFU-BUILD-LIST.md 给师傅交叉编译
11. **ws73usb 内核驱动骨架**（票 06，open）：boot/kernel 双模式 probe、固件下载状态机、
    5 EP URB 管理、`/dev/ws73hci` 字符设备 → OSPL sle_usb.rs 传输契约拆解（子代理进行中）喂设计

## 四、资料待爬（子代理进行中）

- ~~OpenHarmony 社区星闪资料~~ ✅ 产出: OPENHARMONY-COMMUNITY-RESEARCH.md + NEARLINK-PROTOCOL-RESEARCH.md
- **OpenSparklink 深挖（2026-08-17，6 子代理后台进行中）**:
  本地克隆: `/mnt/hdd/nearlink-stuff/OpenSparklink-linux`（blob-filter, 2GB）+
  `/mnt/hdd/nearlink-stuff/sparklink`（用户态 crates）
  - OSPL-DLI-CROSSCHECK.md — DLI 传输契约对照（sle_dli.rs 1503L + slk-protocol types.rs）
  - OSPL-UAPI-CONTRACT.md — host-kernel ABI（sparklink{,_ioctl}.h + sle_uapi.rs 2880L）
  - OSPL-CONN-FSM.md — 连接状态机对照（sle_conn.rs 3071L）
  - OSPL-SSAP-COMPARE.md — SSAP 服务层逐操作码对比（sle_ssap.rs 2480L）
  - OSPL-PHY-RANGING.md — PHY/安全/测距能力矩阵（sle_phy/security/crypto/adv）
  - OSPL-USB-TRANSPORT.md — USB 传输契约拆解（sle_usb.rs 1625L，喂票 06）

## 五、经验教训（防再踩）

- **内核模块懒初始化 = 高风险**：必须在隔离环境单步验证，禁止与已占 PM 的服务并行
- **重编译必须 -j1 + free 检查**（OOM 黑屏两次教训）
- **只操作星闪口**（用户红线）；宿主 WiFi/BT 不动

## 六、新情报整合（2026-08-16 社区+协议研究完成）

**新增资产**:
- `OPENHARMONY-COMMUNITY-RESEARCH.md` — OpenHarmony 上游/HDI 契约/HDF 驱动/社区时间线
- `NEARLINK-PROTOCOL-RESEARCH.md` — SSAP 协议/连接流程/数据面/测距/AT 路径

**关键新方向**:
1. **AT/SLE-Link 桥接路径（重大捷径）**: `libsle_host.a` 内嵌 AT 层（sle_at_* 符号）→
   WS73 dongle 可跑 `AT+SLEENABLE/AT+SSAPS*` 桥接，**无需移植完整用户态栈**即可验证 SSAP/连接
2. **SSAP 实现蓝本**: ssap_pkt.h 完整 PDU 定义 + OHOS ssaps_server.c/ssapc_client.c（Apache-2.0）
   → 直接移植/参考实现 SSAP 服务端
3. **DLI HCI 对齐**: 我们的 WS73 HCI 与 dli_opcode.h 几乎同集，可字节级 diff 补齐
4. **WS73 Linux 移植博客**: CSDN eayayaya（USB/i.MX6ULL）+ iikat（SDIO/Hi3516CV610, 含 sle_soc）
   → 可联系作者获取 sle_soc 细节/官方指南 PDF
5. **测距**: HADM channel sounding 0x2001-0x2005 已验证 accepted → 需对端实测

**优先级调整**: 短期 = AT 桥接验证（最快）+ WiFi 死锁修复；中期 = SSAP 栈（有蓝本）

## 七、SLB 方向评估结论（2026-08-16）

- **WS73 无 SLB 能力**（已确认）: SDK 全树零 SLB 栈；ws73.bin 固件为剥离二进制
  无可读字符串（411 行全是噪声），无 SLB 痕迹；芯片定位 = WiFi6+BLE+SLE 三模
- SLB（SparkLink Basic 高速）需要专用芯片/基带，标准文档会员制（TXS-10002-2025），
  公开实现仅 nearlink_sdr_sim（Python SDR 仿真）
- **替代方向（推荐）**: hi3798mv310 + SDIO 3.0 + WS73 = 三模落地最佳载体
  （SDK 原生 SDIO + hi3798 板级），SDIO 直连优于 USB 480M（WiFi6 受益）
- 师傅清单已加 SDIO 变体（SHIFU-BUILD-LIST.md 增补 2）

## 八、最终整合盘点（2026-08-17）

**仓库状态（d150569，29 commits，工作树干净）**:
- 情报库: lab-notes 19 份（OSPL-* 6 份进行中 → 归队后 25 份）；docs/ 4 份英文；issues 01-10
- 核心资产: `stack/ssap/` 五模块（codec/transport/server/link/feature_mgr）+ 三套测试全绿
- 脚本: ws73-probe×3 + load-driver/flash-dongle/hwsle-probe + check-docs/check-fw
- README 双语交叉链 + check-docs 工作区检查（README zh/en 同步强校验）

**待办优先级（盘点后）**:
1. OSPL-* 6 份报告归队 → README 「17 research docs」→ 25 更新 + 提交（等子代理）
2. SSAP 栈实连验证（双 dongle / 星闪手机）——唯一卡点: 硬件/用户批准
3. WiFi 死锁修复（独占 PM）——需隔离环境
4. ws73usb 驱动骨架（票 06）——等 OSPL-USB-TRANSPORT 报告喂设计
5. hi3798 电视盒交叉编译（SHIFU-BUILD-LIST 已备）

**用户约束重申**: -j1 编译 + free 检查（OOM 黑屏×2 教训）；只动星闪 USB 口；不碰宿主 WiFi/BT。

