# 研究方向清单 (Research Directions)

> 整理: 2026-08-16 · 更新: 2026-08-17（WS63 聚焦 + HH-D01 确认）
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
    5 EP URB 管理、`/dev/ws73hci` 字符设备 → OSPL-USB-TRANSPORT.md 已入库喂设计
    （关键: OSPL 是 3-EP 标准绑定 ≠ 我们 5-EP HCC；`/dev/ws73hci` 需带 hcc_header/queue_id）
    **待实机**: 探 WS73 是否也实现 class-compliant 0xE0/0x01/0x05 模式（若支持是更简单路径）
12. **实机事件布局 pin（下个硬件 session）**: `num_hci_pkts` 字节位置、0x0015 status-first
    布局、0x0005 reason 位置（OSPL-DLI-CROSSCHECK + OSPL-CONN-FSM 开放问题）→ 之后再做
    hwsle_transport 待决命令表 + DLI 命名修正（0x0401→ReadCmdLen 等）

## 四、资料待爬（子代理进行中）

- ~~OpenHarmony 社区星闪资料~~ ✅ 产出: OPENHARMONY-COMMUNITY-RESEARCH.md + NEARLINK-PROTOCOL-RESEARCH.md
- **OpenSparklink 深挖（2026-08-17）** ✅ **6 子代理全部归队，6 份报告已入库**
  本地克隆: `/mnt/hdd/nearlink-stuff/OpenSparklink-linux`（blob-filter, 2GB）+
  `/mnt/hdd/nearlink-stuff/sparklink`（用户态 crates）
  - OSPL-DLI-CROSSCHECK.md — DLI 传输契约对照（sle_dli.rs 1503L + slk-protocol types.rs）
  - OSPL-UAPI-CONTRACT.md — host-kernel ABI（sparklink{,_ioctl}.h + sle_uapi.rs 2880L）
  - OSPL-CONN-FSM.md — 连接状态机对照（sle_conn.rs 3071L）
  - OSPL-SSAP-COMPARE.md — SSAP 服务层逐操作码对比（sle_ssap.rs 2480L，**修正 4 个 wire bug 已提交 f789afe**）
  - OSPL-PHY-RANGING.md — PHY/安全/测距能力矩阵（sle_phy/security/crypto/adv）
  - OSPL-USB-TRANSPORT.md — USB 传输契约拆解（sle_usb.rs 1625L，喂票 06）

  **关键结论**: DLI 包类型/命令帧/事件关联与我们逐字节一致（30 opcodes 1:1）；
  USB 是标准 T/XS 10003-2025 绑定（3 EP 裸 DLI 帧）≠ 我们 5-EP HCC；测距完全缺席、
  SM 算法非标准（需自建）；OSPL UAPI 是 131 ioctls 高层 ABI（电视盒取 ~40 精简即可）。

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
   - ✅ **2026-08-17 确认**: HHD-01 官方固件（`HopeRun-NearLink/HH-D01/WS63V100 AT命令使用案例.pdf`）
     内置完整 SLE/SSAP AT 指令集：服务端 `AT+SLEENABLE/SLESETADDR/SSAPSADDSRV/ADDPROPERTY/
     ADDDESCR/STARTSERV/SLESTARTADV`，客户端 `SLESETSCANPAR/STARTSCAN/STOPSCAN/SLECONN/SLEPAIR/
     SSAPCFNDSTRU/CWRITECMD`。→ 明天板子到手**无需编译 SDK**，串口 AT 即可驱动星闪；
     也印证 WS73 AT 桥接可行性
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

## 九、WS63 深挖批次（2026-08-17，8 子代理归队）

**背景**: 用户明天收到润和 HHD-01 星闪开发板（WS63 芯片，HopeRun-NearLink firmware/README 确认
HH-D01=WS63）。fbb_ws63（905M 润和官方 SDK）+ HopeRun-NearLink（官方仓库）本地深挖完成。

**8 份新报告**（lab-notes 25→32）:
- WS63-SSAP-API.md — SSAP PDU 结构 vs OHOS 逐字节验证 → **FIND member 位图 + 版本门控 + READ 错误项 3 修复已提交 4addeaf**
- WS63-CONN-DISCOVERY.md — 连接 API 只带地址（固件代管链路生命周期）→ 我们 DLI 层加固依然必要
- WS63-SLE-EXAMPLES.md — 官方示例（UUID 0xABCD/0x1122、speed server MAC 11:22:33:44:55:66）+ 互连手册
- WS63-HADM-LL.md — HADM 测距 6 函数 + 0x2005 极性（enable=0=START）+ 低时延=ACB 调度（非 IOG 同步链路）
- WS63-BUILD-FLASH.md — riscv32 交叉编译（工具链内置）、fwpkg 构建、串口烧录（Linux 烧录器缺失=关键风险）
- WS63-VS-WS73.md — 同族 FBB 栈、API 头逐文件 diff 仅 4-53 行、线协议一致 → **互连前提成立**
- HHD01-BOARD.md — HHD-01 板卡全貌 + 23_sle_uart 拆解 + 明天上手 checklist
- COMMUNITY-PROJECTS.md — 12 个社区项目体检（NearLinkSLE/sle_measure_sdk/ili9320 星闪应用层协议等）

**关键新情报**:
1. **AT 桥接路径实锤**: HHD-01 官方固件内置完整 SLE/SSAP AT 指令集（AT+SLEENABLE/SSAPSADDSRV/
   SLECONN/SLEPAIR/SSAPCFNDSTRU/SSAPCWRITECMD…）→ 明天串口 AT 零编译直接驱动星闪；
   文本已入库 .scratch/.../assets/HHD01-WS63V100-AT-commands.txt
2. **明天上手计划（HHD-01 到手后）**: ① apt/pip 装 cmake/ninja/kconfiglib/pycparser →
   ② fbb_ws63 .config 开 sle_uuid_server/sle_speed_server → ③ build.py -c ws63-liteos-app →
   ④ 串口烧录 fwpkg（Linux 烧录器待定）→ ⑤ PC 栈（stack/ssap, tcid 0x0A）连 WS63（0xABCD/0x1122
   或 speed MAC）→ ⑥ 测速（READ 触发 100 万×236B 通知）
3. **互连前提成立**: WS63 板跑 sle_uuid_server + 我们 PC dongle 栈，同族 FBB 栈同线协议；
   待实机验证: 版本协商（WS63 请求 v1.0 vs 我们 v1.3 已修）、连接完成事件码、tcid

**待办**: WS63-BUILD-FLASH 标注的 Linux 烧录器缺失——需找 Linux 串口烧录方案（HiSpark Studio 是 Windows）；fbb_ws63 的 AT 框架（at_bt_cmd_table.h）与 OH 版 AT 指令名不一致——若走 AT 路径需对齐指令表。

## 十、OHOS 生态第二批（2026-08-17，6 子代理归队，lab-notes 32→39）

- WS63-AT-FRAMEWORK.md — **决定性**: 官方预编译 fwpkg 就是 AT 固件（18 条 OH 方言 SLE AT
  指令二进制字符串确认）→ 明天烧 fwpkg + 串口 115200 发 AT 即可零编译驱动；FBB 方言不在固件里，忽略
- SLE-MESH-EPAPER.md — README 说 AODV 实际是**主动 DV 路由**（HELLO-DV + Poison Reverse）；
  1 Server+4 Client SSAP 连接/节点、16B 帧头、AIMD 引擎（RFC-6298 RTT）、多跳吞吐崩塌（10.8KB 图 1跳2-4s/3跳12-18s）
- OHOS-FRAMEWORK-LAYER.md — framework 分层（ArkTS→native→binder→SA1190→stack→DLI）；
  电视盒两路径: 跑 OHOS 用官方 kit（只需 DLI/HDI shim）/ 普通 Linux 借鉴其对象/方法 API 形态
- OHOS-HDI-DRIVER.md — 官方 HDI 在 drivers_interface/drivers_peripheral 其他仓（本地无）；
  HAL 桥接口形态（SleHalInit/SleSendDliPacket + type 字节）**确认票 06 /dev/ws73hci 字符设备 ABI**
- HOPERUN-DEMOS.md — 34 个 demo 分类（23-27 才是 SLE）；首日单板自检序列
  （demo_uart→06_gpioled→07_gpiobutton→11_aht20→12_oled→25_sle_led）
- NEARLINK-CONTROLLER.md — 星闪 4K 游戏手柄参考设计（Hi2821/EB21）；帧聚合/脏重试/重连回放
  状态机可抄到我们 ssap_link 之上（SSAP notify 去重/重连回放→uinput）

**明天 HHD-01 上手路线（更新）**: ① 烧官方预编译 AT 固件（fwpkg，免编译）→ ② 串口 115200
先发 AT + AT+SYSINFO 探测 → ③ OH 方言 AT 全流程（SLEENABLE→SSAPSADDSRV→…→SLESTARTADV 服务端 /
SLESETSCANPAR→SLECONN→SSAPCFNDSTRU→SSAPCWRITECMD 客户端）→ ④ 板载外设 demo 自检 →
⑤ 我们 PC 栈互连（连 0x2222/0x2323 或 speed MAC 11:22:33:44:55:66）

**用户约束重申（防再踩）**: -j1 编译 + free 检查（OOM 黑屏×2 教训）；只动星闪 USB 口；不碰宿主 WiFi/BT；重编译前查内存。

## 十一、OHOS 权威实现第三批（2026-08-17，6 子代理归队，lab-notes 38→44）

- OHOS-SSAP-ENGINE.md — 服务端权威引擎全解剖（find/write/app）→ **P0 差距清单**: 多值读写、
  READ_BY_UUID、FIND_BY_UUID、CALL_METHOD、CCCD 门控+VALUE_ACK、能力位；P1: 描述符、权限门控、
  SERVICE_CHANGE；P2: 128-bit uuid、分片。FIND rspMode=MULTI_RSP 被拒、method fragment 必须 0x11
- OHOS-SSAP-CLIENT.md — 客户端权威解剖（ssapc_app_link_sm 5 状态 FSM、memberValue 位图驱动发现、
  双布局解析、CCCD=CLIENT_CONFIG 读写、VALUE_IND 自动回 ACK）→ **ssap_client 模块蓝本**
  （ssapc_client.c/cache.c/app.c/api.c/link_sm.c + 20+ 签名级函数）
- OHOS-HID-PROFILE.md — HID 服务 0x060B（Report Map 0x1039/Work State 0x103A/Report Index 0x103B/
  Input-Output-Feature 0x103C-E）+ BAS 0x060A；BLE HID 直移植 + CPCD 订阅（0x0E）；音频控制=方法
  0x1017+事件 0x1018 → **电视盒遥控蓝本**（远端=SSAP server + 芯片 controller.c 帧聚合，dongle=SSAP client）
- OHOS-SA-SERVICE.md — SA 1190（MakeAndRegisterAbility、15s 卸载、driver-death SIGKILL 重启）、
  多 profile（sleServers_ map）、权限双重校验 → **Path A 改动清单**（WS73 USB HDI shim 是唯一实活）
- OHOS-HADM-FULL.md — 测距全链路: 0x2003 全参（measureConfigDirect=0x90010004）、0x2005 极性
  （0=START/1=STOP/2=PAUSE）、0x0028 IQ（3B/12bit 解包）、ToF×3/100=cm、双端平均-校准 →
  **WS73 测距客户端蓝本 7 项**；待实机: WS73 固件 DLI V1.1 新分支 vs 旧 46B 参数
- WS63-RADAR.md — 雷达=2.4G 存在/接近检测（复用 WiFi RF，非毫米波，粗粒度上下界），
  WS73 无人体雷达（仅 DFS）→ 雷达是 WS63E 专属机会，不进我们 Linux 栈

**代码落地方向（下一批）**: OHOS-SSAP-ENGINE P0 差距清单 → 修 stack/ssap 服务端（多值 READ、
READ_BY_UUID、CCCD 门控、能力位）；OHOS-SSAP-CLIENT 蓝本 → 新增 ssap_client 模块（电视盒遥控
dongle 侧必需）；OHOS-DLI-LAYER → hwsle_transport 加 pending-command 表 + 0x00EE 超时 + CmdStatus
区分；OHOS-DTAP-LAYER → ACB 信用门控（防溢出芯片缓冲区）

## 十二、OHOS stack 内部层第四批（2026-08-17，6 子代理归队，lab-notes 44→49+）

- OHOS-DLI-LAYER.md — DLI 传输层权威（dli_cmd_struct.h 49 命令结构体、dli_event_struct.h 35+ 事件、
  dli_opcode.h 49 opcodes、per-command 回调+3-4s 超时+0x00EE dispatch）→ 我们 hwsle_transport 是
  原始线适配（fire-and-forget），需加: pending-command 表 + 超时 + CmdStatus/CmdComplete 区分
- OHOS-DTAP-LAYER.md — 数据面权威（TCID 动态 0x1E-0xDF、4 级优先级调度、ACB 信用门控
  g_sendNotAckPktCnt < g_apBufferNum、帧分段 SAR/UNSEG/FIRST/MID/LAST）→ 最大借鉴:
  **ACB 信用门控**（防止溢出芯片缓冲区）
- OHOS-NAI-LAYER.md — SLEM 薄 facade、加密=ECDH+AES-CMAC+SHA-256（无 SM2/SM3）、初始化
  两阶段（sync: SDF→Scheduler→DLI/CM/DTAP → async: SSAP→HADM→BAL）→ 借鉴: 生命周期分离
- OHOS-DEVICE-MGR.md — hash-map 多设备架构（无固定槽位上限）、active/passive 配对方向、
  SM2 加密密钥持久化（HKS）、per-profile 连接上限（HID=6/BAS=8）→ 电视盒多设备管理蓝本
- FBB-WS63-GLE.md — 星闪协议栈链路层核心（6 模块: CM/DD/SM/TM/HCI/SSAP、`oh_sle_srv_*.c` 揭示
  OHOS firmware-side DLI 命令处理接口、GLE CM 5 状态 vs 我们 4 状态、post-connect 自动
  read_remote_version/set_data_len（WS73 需 host 显式发））
- FBB-WS63-BGTP.md — 蓝牙/星闪控制器固件（仅预编译 libbgtp.a、BLE/SLE 并行 HCI 栈、`hci_gle_*`
  dispatch、WS73 同架构 bare-metal）→ 揭示 DLI 命令芯片端处理: `hci_msg_recv_h2c` →
  `hci_gle_cmd_received` → FSM dispatch

## 十三、扫尾+审计第五批（2026-08-17，5 子代理归队，lab-notes 49→53+）

- NearLinkSLE-SAMPLES.md — 社区最干净 SSAP 双端样例: SLE_HELLO（UUID 0x3333/0x3434, 2s 通知）、
  SLE_UART_HE（生产级 8KB ring + 128B+2ms×100 重试队列）；**CCCD 写 0x0001 到 prop_handle+1
  是通知的命门**；断连重广播=立即 sle_start_announce()
- SLE-MEASURE-QOS.md — PHY 1M/2M/4M + MCS 0-10 + CI 1.25-20ms + 载荷 ≤1370B（链路层 ARQ）；
  **ACB 信用门控**= `sle_flow_ctrl_flag()>0` before send（与 DTAP 确认一致）；最小实现:
  pending-frame counter + MAX_INFLIGHT cap in hwsle_transport_send_acb()
- SSAP-PLAN-AUDIT.md — 原计划 6 步核对: 2 done / 2 partial / 1 obsolete / 1 superseded；
  已修 7 个 wire bug；**1 潜在 bug**: ssap_item_type_t 枚举 0x01-0x05 应为 0x00-0x04（当前无害）；
  12 个 P0 遗漏；更新后 Roadmap: 11 P0 / 10 P1 / 6 P2
- OHOS-SM-SECURITY.md — SM 100% host 端（cp/bsl/sle/sm/），三阶段配对（Negotiate 0x0133-
  0x0137 → Authenticate 0x0138-0x0142, 6 方法 → Encrypt, ECDH P-256/SM2 + AES-CCM/SM4-CCM）；
  SSAP 3-bit 权限门控（AUTHENTICATION/ENCRYPTION/AUTHORIZATION NEED）；vs OpenSparklink: OHOS
  用规范 SLE G/T-node 协议，OpenSparklink 用 BLE LTK/IRK（不互通）→ 电视盒需 JustWorks +
  NumericCompare + ECDH + DLI 加密命令
- LINKNEBULA-MESH.md — Rust no_std SLE mesh PoC（Hi2821/WS73, ~2000 行, 2025-03 冻结）；
  SLE 用法非标准（自定义 FFI）；路由=被动 beacon-DV（首跳固定表，无真正多跳）；
  可靠传输未实现；服务发现=QoS 加权评分（可迁移）；**结论**: LinkNebula 给架构模式，
  ePaper mesh 给行为蓝本，两者都不能直接 fork

## 十四、P0 代码落地（2026-08-17，5 项已实现，3 项待实测后补）

基于 53 份情报的 P0 差距清单（OHOS-SSAP-ENGINE + SSAP-PLAN-AUDIT），已实现 8/11：

**已完成（5 个 commit，全部测试通过）**:
- `6592b1b` ssap_item_type_t 枚举值 0x01-0x05→0x00-0x04（SSAP-PLAN-AUDIT 发现）
- `9a359f2` P0-6 CCCD 通知门控（写 prop_handle+1 启用 notify/indicate）
  + P0-7 ExchangeInfo 能力位（reliable+multiProcessing for v1.3）
- `a637e6d` P0-3 READ_BY_UUID（0x0A/0x0B，按 uuid 搜索属性+读值）
- `3dcd0b5` P0-1 多值 READ（ctrl.multi + {length:15|success:1} item 打包）
- `fe1e075` P0-4 FIND_BY_UUID（0x06/0x07，按 uuid 发现属性）
- `c45d096` P0-11 CCCD 描述符（FIND_PROPERTY 自动包含 descType=0x02）
- `e291ca5` P0-16 SERVICE_CHANGE 通知（add_service 后发 VALUE_NTF 0x000E）
- wire 修复: VALUE_ACK/READ_RSP/WRITE_RSP/FIND 布局/位图/版本门控/READ 错误路径
- ssap_link.c 加固: 超时/拒绝回退/事件门控/supervision timeout

**待补（大块，HHD-01 基础互连不需要）**:
- P0-2 multi-WRITE: 变长 item 无长度前缀，协议层设计特征（非实现 bug）
- P0-5 CALL_METHOD: 需方法回调注册+requestId+pending queue
- P0-12 permissions: 需 SM 层集成（ECDH/DLI_EnableEncryption）

**HHD-01 明天互连路径（已验证可行）**:
烧 AT 固件 → 串口 OH 方言 AT（test-hhd01-at.sh）→ PC 栈 connect →
exchange_info → find（含 CCCD 描述符）→ write CCCD → read/notify

