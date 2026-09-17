---
type: intel
title: "研究方向清单 (Research Directions)"
language: zh
created: 2026-08-16
tags: [intel, research, directions]
sources:
  - "https://github.com/OpenSparklink/linux"
  - "https://github.com/OpenSparklink/sparklink"
trust: B
stale_after: 2027-02-16
---

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
  本地克隆: `https://github.com/OpenSparklink/linux`（blob-filter, 2GB）+
  `https://github.com/OpenSparklink/sparklink`（用户态 crates）
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

## 十五、Xinghongpai WS63 固件程序知识（2026-09-11）

- 产出：`NEW-XINGHONGPAI-FIRMWARE-KNOWLEDGE.md`
- 范围：只读梳理 `firmware/examples` 的 OpenHarmony/Hi3863 程序结构；未编译、未操作硬件、未分析 PCB。
- 应用模型：CMSIS-RTOS2 `osThread*`/`osTimer*`/`osMutex*`/`osSemaphore*`/`osMessageQueue*`；
  OpenHarmony `APP_FEATURE_INIT`、`SYS_RUN`，以及新版 `app_run` + `osal_kthread_create`。
- 可复用算法：AHT20（0x38、忙等待/校准/20 位换算）、SSD1306（0x3C、命令/数据前缀、页寻址）、
  ADC 采样、Wi-Fi STA/SoftAP 状态机、lwIP TCP/UDP 基础调用。
- API 方言：老 `IoTGpio*`/`IoTI2c*` 与新 `uapi_gpio_*`/`uapi_i2c_master_init`/`uapi_adc_*` 混用；
  引脚号、I2C 总线、pinmux 必须按目标 SDK 和板卡重新验证。
- 关键缺陷：ADC 回调 `buffer[length-1]` 无长度检查且无自动扫描配置时可能返回未初始化数据；
  网络示例存在 11 字节数组写 `display_data[11]`、UDP `&response`、负 `recvfrom` 下标、
  零地址 UDP 初始发送、失败后继续执行、socket 未关闭和硬编码凭据。
- 协议边界：固件树没有 `SLE`、`SSAP`、`HADM`、`DLI`、`NearLink`、`SparkLink` 或 `USB` 实现，
  不能直接喂 WS73 USB/DLI/SSAP 设计；只能作为 WS63 固件侧协议算法和 SDK API 参考。
- 后续硬件 session 再做：物理引脚映射、PCB/电源/时钟/天线验证、烧录和实机日志；本轮不触碰 PCB。

## 十六、NearLink Toolbox 网站程序知识（2026-09-11）

- 产出：`NEW-NEARLINK-TOOLBOX-WEBSITE.md`
- 实际形态：Next.js 15.1.11 + React 19 的静态 export 网站；没有 Rust、Tauri、`src-tauri`、Cargo、serialport 或烧录后端。
- 文档落差：`docs.md` 描述 Tauri 2 + Rust + React 18 + TanStack Router + React Query + Tokio/serialport，但仓库中只有营销页、组件和截图。
- 下载链路：点击按钮后获取 `https://haohanyh-ctcc.gcxstudio.cn/software-updater.json`，再直接 `window.open` 其中的 Windows URL；无 manifest 运行时校验、签名/哈希/大小校验、平台白名单或安装流程。
- 功能边界：固件商店、固件管理、串口烧写、串口调试、AT 命令、云镜像等均以 UI 文案/卡片呈现，没有对应执行代码。
- 额外边界：`app/layout.tsx` 注入 51.la 第三方脚本并开启 `screenRecord:true`；统计图和 4.8/5.0 评分是硬编码展示数据。
- 对 WS73/HHD-01 的价值：只借鉴信息架构和产品词汇；真实工具必须补 signed manifest、可信下载、设备识别、串口/USB 后端、烧录状态机、日志、回滚和平台打包。

## 十七、NLChat Web 端程序知识（2026-09-12）

- 产出：`NEW-NLCHAT-WEB.md`
- 实际形态：Vite + React 18 + TypeScript 单页应用，通过 Cloudflare Pages 静态部署；没有服务端后端、Rust/Tauri、固件或测试脚本。
- 串口生命周期：`navigator.serial.requestPort()` → `open()` → `readable.getReader()` 循环读取 → `writable.getWriter()` 写入 UTF-8 → reader/port 清理与关闭。
- 终端能力：text/hex/ANSI 显示、时间戳、自动滚动、行尾选择、命令历史、复制、清屏和日志导出。
- 聊天边界：`ChatUI` 只把 `ssapc`、`[sle` 和 `This is the content of the client/server:` 等文本约定转换成气泡；它不是 SSAP/SLE 解析器。
- 关键缺陷：没有显式帧头/长度/序号/校验；按超时合包不等于协议重组；聊天解析只按 CRLF 拆分；关闭连接时流式 UTF-8 decoder 没有最终 flush；没有设备身份、版本协商、安全、重连或二进制载荷验证。
- 可借鉴：浏览器 Web Serial 生命周期、原始终端与高层展示分离、诊断日志导出和用户主动选择串口。
- 不可直接借鉴：把 CRLF/英文标记当协议边界，或把 README 的星闪产品宣传当成该仓库已实现星闪协议。
- 对 WS73 的价值：可作为未来浏览器诊断工具的 UI/串口生命周期参考；WS73 USB/SSAP 的传输、framing、安全和互操作仍必须由独立协议与实机测试确定。

## 十八、汇编/编译器/链接器优化资源（2026-09-12，NEW-NEARLINK-ASSEMBLY-OPTIMIZATION）

- 产出：`NEW-NEARLINK-ASSEMBLY-OPTIMIZATION.md`（网络研究 + 本地程序证据）
- 范围：公开网络检索 NearLink/SparkLink/SLE/WS63/WS73 汇编、编译器、链接器与运行时优化资料，并整理为可复用的通用优化指令集；仅程序/源码/构建知识，未触碰 PCB，未构建或操作硬件。
- 核心结论：
  1. **编译器分级**：主机用户态可用 `-flto -ffunction-sections -fdata-sections -O2` + `-Wl,--gc-sections`（LTO=1 门控）；设备 KOs 默认维持 `-Os`，LTO/gc 只可作为配置门控的受控子集试点，不得 blanket 开启。
  2. **Rust release 分级**：`lto="thin"`（最终 binary 可升 `fat`）、`codegen-units=1`、`panic="abort"`、`strip=true`；dev/test 保持 `codegen-units=16`。
  3. **inline 节制**：`always` 只用于极小纯 helper；packed 线结构体作为 wire 事实保留，不作为优化目标；register/volatile 访问器不能让优化丢弃跨线程/中断状态。
  4. **LTO/链接边界**：Kbuild `ld -r` 下的 KOs 不能直接注入 LTO；固定 ROM 用 `-fno-lto` + `KEEP()` 锚点；PGO 当前不可用，需先有 `.gcno` 与内核 profiling 数据。
  5. **度量门禁**：任何优化增益必须用 `size` / `nm --print-size --size-sort` / `-Wl,-Map` 三要素验证，并断言文本缩减或死符号消除，不能只看字节。
  6. **通用指令集**：已沉淀为可复用通用规则集（编译器分级、inline 与 section 控制、目标 ABI 纪律、Kernel/LTO 边界、SLE/USB 运行时门控、度量验收），适用于 NearLink/SparkLink 共享工具链与宿主用户态/目标设备栈。
- 本地关键证据（已链接，不重复）：
  - `stack/ssap/Makefile:8-10`（主机 `-O2 -std=c11`，无 LTO）；
  - `sdk/.../driver/wifi/Makefile:467-474`（KOs `ccflags-y -fno-pic -Os -DDMAC_ON_HOST`）；
  - `sdk/.../driver/platform/drv/device/romable/include/hi_types.h:143-144` + `td_base.h:77-78`（SoC register `always_inline` 访问器）；
  - `stack/ssap/include/ssap_pkt.h:186-588`（packed PDU 线事实）；
  - `stack/ssap/src/ssap_codec.c:43-53`（`put_u16`/`get_u16` 纯 helper 作为 `always` 候选）。
- 网络外部来源（仅元数据核验，未拉取完整源码）：
  - `hispark-rs/bs2x-svd`（BS21/BS2X CMSIS-SVD + svd2rust，Rust-rv32m 工具链源参考）；
  - `hispark-rs/riscv32-ws63-gcc730-toolchain`（riscv32 ws63 GCC 7.3.0 工具链脚本，目标 ABI 证据）；
  - `hispark-rs/fbb_bs2x-qemu`（QEMU 可跑 BS2x 精简 SDK fork，bare-metal RISC-V 运行优化背景）；
  - `hispark-rs/bs2x-guide`（BS21/BS2X 用户指南，SLE/NearLink 行为指引）；
  - `OpenSparklink/nearlink_sdr_sim`（NearLink SDR 仿真，用于线速率度量验证）；
  - `Hny0305Lin/NLChat`、`Hny0305Lin/Hihope_WS63_NearLink_SDK`（SLE_UART/WS63 程序侧传输与 framing 证据）；
  - `NearLink-ePaper/NearLink-Mesh-ePaper`（H3863 SLE Mesh PoC，AODV/AIMD 运行性能/开销证据）。
- 后续可用：网络资源可继续作为通用规则集的补充，但所有性能主张必须以本地实测 `size`/`nm`/`Map` 与对端实机测量为准，不能把网络资料直接声明为已验证性能结论。

## 十九、UWB-like 多锚点 SLE 测距套件（2026-09-12，NEW-NEARLINK-UWB-LIKE-RANGING）

- 产出：`NEW-NEARLINK-UWB-LIKE-RANGING.md`（热点自主发现：GitHub sort=updated 全网最新推送仓，2026-09-12）
- 甄别：`zhuzhengyan50-spec/nearlink-uwb-like-ranging`，Apache-2.0（新代码）+ SDK 派生文件保留原许可；明确声明"UWB-Like"仅指定位体验，技术是 SLE Channel Sounding（非 UWB 协议）；单 commit 2026-09-12。
- **定位意义**：测距方向（本文方向 6）第一份公开端到端实现——此前 OpenSparklink/OHOS 侧测距完全缺席，只有 DLI 固件级线索（WS63-HADM-LL / OHOS-HADM-FULL）。
- 三角色架构：Anchor（≤4）/ Ranging Client / **Collector（不参与 CS，独立串口输出）**——Collector 拆分使移动端串口不承载数据洪流（串口阻塞会打断测距时序，IQ 输出用编译开关控制）。
- 线协议（PROTOCOL.md 权威）：8B 帧 `{type u32 LE, len u32 LE}`；消息 0xFFFFFFEA（Client→Anchor IQ）+ 0x10/0x11/0x12/0x13/0x14/0x15；32B 距离元数据（anchor/client/rssi/conn_id/dist_mm/双端 timestamp_sn/双端 tof/双端 rssi）；332B IQ 结构 = samp_cnt(≤80)+rssi+es_sn+timestamp_sn+80×(I u16+Q u16 LE)+tof_result（Mode 3）；Collector 串口为大写 hex ≤24B/行、seq 连续重组、32/332 定长校验、丢行即弃样重同步。
- API 方言（BS21E / fbb_bs2x，`standard-bs21e-1100e` 目标）：`sle_hadm_register_callbacks` + `sle_set_channel_sounding_param_ex`/`enable` + `cs_caps/cs_state_changed/cs_param/cs_iq_report/cs_retry` 回调族 + CS 重试调度（client.c:767,827,837）；设备侧距离平滑用 SDK 内置 `slem_smooth`/`slem_alg_smooth_dis`（DIS_ALG_MODE 5，dist_mm=dis_smoothed×1000）；SSAP 通道共存（CCCD=property_handle+1，与我们 P0-6 同约定）。
- **WS73 适配核心发现**：WS73 SDK `include/bsle/sle/sle_hadm_manager.h:282,299,316,333` 暴露**同构 API**（param_ex/enable/disable/register_callbacks），`:68-83` 的 `sle_channel_sounding_iq_report_t` 含同名同型 `es_sn`/`timestamp_sn`/`tof_result`——同族 FBB 栈结论延伸到 CS/HADM 域，WS73 dongle 具备做测距端的头文件级前提；WS73 无 `slem_smooth` 头 → 平滑/定位必须主机侧做（与该仓主机 GnUls+Kalman 架构天然吻合）。
- 对比锚点：OHOS 0x0028 IQ=3B/12bit 解包 vs 此仓 SDK 原生 4B/点 u16；此仓 "slem" 前缀=SDK 测距平滑库（**名字撞 OHOS NAI 的 SLEM 网络层，语义完全不同**，勿混淆）；该仓不含 PHY/MCS/QoS 调参（与 SLE-MEASURE-QOS 正交）。
- 实测边界：BearPi-Pico H2821E + 3dBi 外置天线，空旷 LOS >100m 保留原 SDK 校准；~2Hz/链路、4 anchor→~8 次/秒/client；第 5 anchor 不稳故默认 4；hex 文本协议带宽低，versioned binary+CRC 在其 Roadmap。
- 可直接复用模式：Collector 角色拆分、双向 IQ 按 timestamp 配对+丢行弃样重同步、每连接槽位+断连清槽、CS 重试 FSM、主机侧 GnUls（ULS 初值+一步高斯-牛顿）定位管线。
- 行动项：① WS73 测距客户端原型（stack/ssap 加 sle_hadm seam，复用 8B 帧+IQ 槽位，主机 GnUls 定位）→ 新 issue；② WS73 单报 vs BS21E 80 点聚合的 IQ 结构差异待实机核对；③ 采纳 seq=0 重同步+定长校验契约到未来 Collector/采集工具。

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

## 十五、WiFi 探索（进行中，2026-08-17）

- WS63-WIFI-SERVICE.md — fbb_ws63 WiFi 服务层架构 + BLE/SLE 共存机制（子代理进行中）
- WS73-WIFI-DEADLOCK.md — WS73 WiFi 懒初始化死锁根因分析 + 修复方案（子代理进行中）

**今日收工盘点（2026-08-17）**:
- 情报: 53 份（+6 待归队 = 59）
- 代码: 84 commits，10/11 P0（仅 permissions 需 SM 层）
- 测试: 四套全绿
- 明天: HHD-01 AT 脚本就绪，WS73 dongle + PC 栈互连就绪

## 十八、OKF 时代猎收第一批（2026-09-13，队列仓 4 消化 + 1 甄别空仓，知识束 92→95 concepts）

前置过时确认（GitHub API pushed_at/archived，全部存活）：openharmony/communication_nearlink_service 9-11 推送、tethering_nearlink 9-11、hi3863-sle-1v8-vehicle 8-17、Eironax/Qwac 8-31。

产出（knowledge/harvest/）：
- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`：openharmony 主仓 pull 544 commits（8-14→9-11）。可复用：①SSAP 服务端 `NLSTK_SsapServerReplayConnectedLink`（复用已建链场景注册后补发存量链路状态——我们 SSAP server 同款盲区，直接可移植）；②DTAP_CopyFrame 拷贝后指针重定基（内指针结构深拷贝必须 rebase，防 UAF）；③帧4 广播/扫描双引用计数天线钉扎管理器（SleFrame4AntennaMgr，串行队列免锁）；④DLI_SetICGAutorateParam（同步链路自适应速率）+ DLI_RegisterSnoopSensitiveOpcodes（敏感指令日志隐匿化）；⑤全栈 per-module fuzzer 基建（25+ 模块）。含 OpenHarmony v7.0 Release 标签。
- `NEW-OHOS-TETHERING-SERVICE.md`：xingkaiyueying/tethering_nearlink = OHOS 星闪网络共享服务：本地 socket+epoll 泵（PortInfo port/addr/mtu/fd，50KB 缓冲）→ SleDataTransferService（UUID↔port↔tokenId/uid/pid 映射 + 每应用缓存 + ACB 状态路由）→ SLE Port Profile（portId+manufacturerId+UUID 三元组，SSAP 之上的端口透传）。CACHE_FULL 显式背压枚举。车机连接请求路径（ConnectCarReq）。
- `NEW-SLE-1V8-VEHICLE.md`：cxl0928 竞赛仓。1 拖 8 拓扑 = MAX_CON=8 定长 conn_id 表 + MAC 去重 + 扫描→连接→MTU 交换→重扫描循环 + 断链数组压实。SBUS 25B 帧 16×11bit 解析、MT6816 14bit 磁编码器、PID 闭环。
- Eironax/Qwac：**空仓甄别**（全仓仅 LICENSE，单 Initial commit，蹭 Playjoy 名），不产报告，克隆已删。

可复用/不可复用边界：Replay-on-register、copy-then-rebase、opcode 隐匿化、引用计数资源钉扎、1v8 连接循环均可移植；tethering 的 socket 桥无 IP 转发逻辑（NAT/DHCP 不在仓内），port profile 为部分片段不可独立构建。

队列余量：yanlinkos/fbb_ws63+fbb_bs2x（fork-diff 判定）、Heebu/NearLinkChat（低优先）、openharmony/communication_dsoftbus、openharmony/device_soc_hisilicon。

## 十九、OKF 时代猎收第二批（2026-09-13，热点扫描 3 消化 + 1 fork-diff 判定，95→98 concepts）

热点扫描（sort=updated）+ 甄别：santes210/NearLink（Android 壳仓）、Leiyimei/ws63_sensor_sle（空仓）、bhengubv/aether-protocol（C# 名称撞车）一行跳过；Sky05y/smart_cabinet（BearPi H3863 药柜样例，2.5 月未推）留观察不克隆。

产出（knowledge/harvest/）：
- `NEW-TEKI128-MINIMAL-PAIR.md`：teki128/nearlink = 564 行最小 SLE 客户端/服务端对。服务端 5 步 SSAP 序列（0x00A0/0x01A0/0x02A0 + CCCD + start_service）+ 客户端固定地址直连（G_CAN_NEGO、interval 100、MTU 512）。客户端地址 11:22:33:44:55:66 即 HHD-01 violin 的 SDK 默认值。可作为 WS73 host SSAP 栈 bring-up 冒烟验收基准。
- `NEW-PET-COLLAR-GATEWAY.md`：starflash-pet-collar = 三端系统 + **sle_gateway 双无线电桥**（SLE report/write 双向 ↔ lwIP UDP 512B）。证明 WS63 应用层 SLE+WiFi 并发共存可行（对三模目标 prior art）。证实 sle_uart_1_vs_8 已是 HiSpark 样例家族标配（两仓出现）。硬编码 WiFi 演示凭据按凭据卫生不传播。
- `NEW-YL63-FORK-VERDICT.md`：yanlinkos/fbb_ws63 fork-diff 判定（metadata-only clone + 文件清单 comm + 稀疏检出 684K）：非同字节 rehost，YL63 重构代次。**官方 AT 指南 7791 行/100 命令**稀疏采纳——AT+BLESETNAME=<len,name> 官方证实 HHD-01 逆向格式；SLE 全家族（SLESETMCS/SLESETDEFAULTCONNP/SLEPAIR）首次有官方文档。fbb_bs2x 判定暂缓。

方法论沉淀：大厂商 fork 判定 playbook = `--filter=blob:none --no-checkout` 元数据克隆 → 文件清单 comm 差集 → 高价值路径 sparse-checkout（472M 仓只花 684K）。

## 二十、OKF 时代猎收第三批（2026-09-13，符号搜索轴 + API 同面实证，98→99 concepts）

gh 符号搜索轴（sle_hadm/ssaps_notify_indicate/sle_iso_manager）：新候选多数过时（BearPi H3863 Pico 2024-08、bs2x_sdk 2025-03、sanchuanhehe/fbb_ws63 2025-08、BearPi H2821 Pico 2024-06——按过时规则降级标注不克隆）；sle_iso_manager 全球仍仅 wtsl_app 一家使用（印证 NEW-BS21-WTSL 的下一代 API 稀缺结论）。

产出（knowledge/harvest/）：
- `NEW-OHOS-DEVICE-SOC-WS63.md`：openharmony/device_soc_hisilicon（9-09 推送，465MB）稀疏采纳 3.9M。**核心实证：OHOS ws63v100 与 WS73 1.10.110 三个关键 SLE 头（ssap_server/ssap_client/hadm_manager）的 NLSTK_* 函数集 100% 相同、零单边符号**——设备侧与主机侧同一公共 API 代次，可移植性论证闭环（扩展 UWB-like 报告的 hadm 同构注记至全 SSAP 面）。adapter/hals 为标准 OHOS 粘合。protocol/662 + middleware/516 文件留待未来深挖。
- 同批过时确认重置队列：sle_iso 唯一用户 = wtsl_app（已消化）；符号轴 SDK 镜像仓多为 1-2 年未推。

## 二十一、OKF 时代猎收第四批（2026-09-13，窄词扫描 + dsoftbus 评估，99→101 concepts）

窄词扫描（ws73/h3863/dongle/usb）：twyora/WildLink 对（新）、MakeBlackSheepGreat/BearPi-Pico-H3863（资料库，观察）、gtxaspec/ws73v100-wifi（已知）、FlashKeyboard（已本地）；OpenSparklink/sparklink 本地已最新（0 behind）。

产出（knowledge/harvest/）：
- `NEW-WILDLINK-SENSOR-PAIR.md`：twyora/WildLink = 多属性 SSAP 服务端（每传感器独立 property handle：MAX30102/MAX30205，自定义 UUID base 37BEA880-FC70-11EA）+ **三链路节点**（app_entry 同时起 sle_client + ble_server + atk_lora）。Kconfig-per-task 门控。与 1v8 车（多服务器单属性）构成 WS63 家族两个正交多轴设计。
- `NEW-DSOFTBUS-SLE-STUB.md`：dsoftbus 46MB 队列项以证据结案——公共树 ConnSleInit **显式返回 NULL**（"do not support sle init" 桩），但脚手架完整：net-ledger 带 SLE_MAC/SLE_CAP 能力账本 + MAC 变更同步消息；适配器枚举**同时含 SLE 与 SLB（SparkLink Basic）双栈状态 + TURN_HALF 态**（新词汇）；lnn_sle fuzzer 先于传输实现。"公共桩/私有实现"模式再次实锤。

## 二十二、OKF 时代猎收第五批（2026-09-13，nearlink+mesh 轴 + hispark-rs 增量，101→103 concepts）

产出（knowledge/harvest/）：
- `NEW-WS63E-MESH-AIGC-FRAME.md`：leion-kk WS63E 分布式 AIGC 画框。**SLE Mesh 上的按目的地路由表**（mesh_route_entry_t: dest/next_hop/hop_count/lifetime_ms，AODV 家族带 lifetime 过期）——与 sle_mesh 的 leader 根树路由构成两种公开 mesh 设计。图像管线 = 分块接收→片上 TJpgDec JPEG 解码→RLE→流缓冲→墨水屏 SPI 边收边刷（零本地存储）。网关双无线电分工（WiFi 走云端 AIGC / Mesh 走分发）。
- `NEW-FBB-WS63-QEMU-FORK.md`：fbb_ws63-qemu fork 稀疏采纳（3.5M）。**RF 初始化仿真缺口清单**：BT/WiFi 任务深初始化依赖片上 ROM 数据 + RF 校准 + efuse dump，QEMU 不可建模——正是三模固件仿真需补的三件套。boot 实证：flashboot→liteos→调度器空闲。本地 hisi-riscv-qemu（=ws63-qemu）已建模 xlinx ISA + 全部 35 个 SVD 外设 + mask-ROM 拦截。

hispark-rs 澄清：本地 hispark-rs/hisi-riscv-qemu 即 ws63-qemu（README 交叉证实 boot 状态），fbb_ws63-qemu 为其配套 SDK fork。

## 二十三、OKF 时代猎收第六批（2026-09-13，队列收尾 + 样例家族手册，103→104 concepts）

- Heebu/NearLinkChat **蹭名结案**：pubspec 依赖仅 connectivity_plus/wifi_iot/flutter_webrtc，纯 WiFi/LAN WebRTC 应用，无任何星闪 SDK。不产报告。
- `NEW-BEARPI-H3863-DOCS.md`：MakeBlackSheepGreat/BearPi-Pico-H3863 稀疏采纳 160K（docs/communication 7 篇 1284 行）= 本批已消化样例家族（sle_uart/sle-1-to-8/sle_gateway/ble_uart）的教程手册。新事实：**SLE UART 家族要求两板配对后方可互发**（代码消化未显现的流程前置）；证实 pet-collar 的 sle_gateway 与 1v8 车拓扑均为 BearPi 标配样例而非原创。hardware/datasheets 按边界未拉。

## 二十四、OKF 时代猎收第七批（2026-09-13，openharmony org 全扫 + DK 课程，104→105 concepts）

org 全扫描（search org:openharmony nearlink + org 内 ssaps/sle_enable 代码搜索）：除已消化的 service/device_soc/dsoftbus 外发现 **openharmony/vendor_hihope**（9-12 推送，全会话最新鲜）。

- `NEW-OHOS-DK3863-SLE-CURRICULUM.md`：vendor_hihope nearlink_dk_3863 = 28 样例课程，SLE 应用系列 5 个（23_uart/24_humi/25_led/26_gas/27_oled）共用 UART 骨架，载荷分 taxonomy：透传/传感器上报/执行器命令/模拟量/显示流。配对状态（SlePairStateType）为连接回调一等信号（证实 BearPi 手册配对前置）。OHOS BUILD.gn 集成 = 设备侧 OHOS 化实例。可作为 WS73 SSAP 栈验收测试矩阵。
- 空白轴结论：hi3798×nearlink、nearlink sniffer、sparklink dongle 关键词 GitHub 零结果（嗅探工具空间空白）；WS73 USB host 栈唯一性 9-13 复验成立（仍仅本仓库 + gtxaspec）。

## 二十五、OKF 时代猎收第八批（2026-09-13，中文关键词轴爆发：商用 DTU 生态，105→108 concepts）

中文轴（星闪）扫出商用生态三仓 + 收尾 MeshGatewayAPP：

- `NEW-DS10-SLE-DTU.md`：**WANG-XU-create/DS10-TTL = 首个公开的星闪 SLE 串口透传实测画像**（32B≈14ms / 1400B≈419ms·44% 成功率，分片逐片 ACK 所致；上行 1100B / 下行 4095B 不对称；≥5ms 帧间隔；1 主 ≤15 从；GFSK/Polar 可选 200m）。**第三代 AT 方言**：双平面（运行态/配置态）+ 草稿事务（CFG_NEW/SAVE/DISCARD 原子生效）+ AT+CFG_SLE=<frameType>,<tier> + Modbus over SLE 分站路由。ROS2 驱动（master/slave 角色、Frame 消息、噪声地板诊断）。对我们 SLE 传输：小帧+信用步调的实证依据。
- `NEW-TXSTAR-DS10-REMOTE.md`：La-OHV/tx_star = DS10 控制环（Android 摇杆 CH340 USB 串口 20Hz 二进制帧 + 序号/CRC-8/STM32 自动重同步/掉线判 + 总开关清零安全不变量）。
- `NEW-MESHGATEWAY-APP-PROTOCOL.md`：NearLink-ePaper/MeshGatewayAPP = SLE-mesh 图传协议手机侧规范（v2 检查点 + 30B 缺包位图一次补 240 包；v2.1 FAST/ACK 双流控；14B 图像头；Kotlin RLE 与固件 image_rle.c 位级 round-trip）。与 AIGC frame 仓合读 = 协议两端齐备。

## 二十六、OKF 时代猎收第九批（2026-09-13，topic 轴 + SLE 2.0 首个公开实证，108→109 concepts）

- `NEW-HIDITING-SLE2-EVIDENCE.md`：elfbobo/hs-fbb = 海思谛听 HiDiTing 轻智能终端方案（764MB，稀疏/raw 采纳 4 篇 SLE API 文档）。**SLE 2.0（16Mbps 双向）首次入公档**；新一代 `bs_sle_*` API 族（对比我们 NLSTK_* 1.x）新增 auto_conn 自动重连管理、set_data_length（DLE）、directed_reconnect；**Port 服务成一等 API**（bs_sle_port_create_local/remote_port、write_by_uuid/by_port）——与 tethering Port Profile、MeshGatewayAPP 三源收敛，port 抽象是 SLE 传输层的方向。
- 大仓 playbook 第四次应用：764MB 元数据化，sparse-checkout 因图 blobs 超时改走 raw 直取（4 文件 3.7K 行）。
- rzy0901/sle_measure_sdk1.0.12 与本地 sle_measure_sdk 同构（论文测量代码版本标签），跳过；dxnz-id/pressplay 为媒体应用蹭名，跳过。

## 二十七、OKF 时代猎收第十批（2026-09-13，观察项收尾 1/2，109→110 concepts）

- `NEW-SMART-CABINET-FULLCHAIN.md`：Sky05y/smart_cabinet（6-28 推送，2.5 月静默未归档）全链路多节点 SLE 参考——2 从节点（DHT11/BH1750/MQ 气体/指纹/锁/OLED 六类传感器）→ SLE → 主控（device_no 键多服务器表）→ WiFi STA → lwIP 原生 HTTP POST 上云 → Web 看板。无 MQTT/SDK 的可审计云端出口。与 pet-collar（UDP 单跳）构成网关复杂度两端。

## 二十八、OKF 时代猎收第十一批（2026-09-13，观察项收尾 2/2：protocol 深挖，110→111 concepts）

- `NEW-GLE-HOST-SYMBOL-SURFACE.md`：device_soc ws63v100 protocol 层深挖——设备侧 SLE host **闭源发布为 libbth_gle.a**（bt host 无源码，仅 bgtp 控制器传输开源）。nm 符号面（1035 符号）揭开内部分层：**gle_sm(77)=安全管理器**（authentication g_node/t_node 双角色、number_compare、encrypt_param/block——我们 SSAP 缺失 SM 层的完整参照规范）、sle_at(69)（AT 内部层在 host 库内）、gle_hci/sapi/uapi 双边界、gle_tm/dm/cm/dd/aa。**uapi_ssaps 权威面**含我们栈未建模的 `_ex` 变体与 `update_item_value_by_{handle,uuid}` 服务端主动更新族。
- 大仓 playbook 第五次应用：20MB protocol 层物化 + nm 符号表互操作分析（仅符号名，无反汇编）。

## 二十九、OKF 时代猎收第十二批（2026-09-13，middleware 开闭图谱 + 成熟 mesh 工程，111→113 concepts）

- `NEW-WS63-MIDDLEWARE-OPEN-CLOSED.md`：device_soc middleware 开闭图谱——**HCC 架构头开源**（hcc_flow_ctrl 三阶段 API + DFX 队列丢包计数器=设备侧也在量化丢包）、AT 框架 core/wifi 开源而 **bt_cmd 表体闭源**（与 libbth_gle.a 同一闭源边界）。设备侧三阶段流控（sched_check→pre_proc→process）实证我们主机侧信用门控的架构同构。
- `NEW-SLE-TEAM-MESH-V456.md`：BH4ME/sle_mesh_new（v4.5.56）= 成熟度最高的公开 SLE mesh 工程：254B 有界报文（四种路由类型 FLOOD/DIRECT 混合 + leader term + FW compat 字节）、tick 切片中继优化器、板上 Web API + 契约测试、发布证据 JSON/多板烧录自动化。第三个独立 SLE mesh 设计（与 sle_mesh v4.4.9 tier 路由、AIGC frame 路由表并列）。
- 固件轴新扫出：SlumberMin/SmartEdge-WS63（固件+Flutter）、starbridge-ws63-firmware、ws63-sdk-dev-skill（WS63 SDK 的 AI agent skill）——留观察。

## 三十、OKF 时代猎收第十三批（2026-09-13，固件轴三连，113→116 concepts）

- `NEW-WS63-SDK-DEV-SKILL.md`：独立第三方给 WS63 SDK 做的 785 行 agent skill——痛点画像（build 失败/产物定位/fwpkg/串口日志）+ 方法论（证据优先、单根因轮次、**三次盲改即停**、单写者）+ references 蒸馏（GPIO 上电禁用表、样例卫生清单、多设备连接状态机模式）。
- `NEW-STARBRIDGE-EDGE-BRIDGE.md`：SS928 边缘机 UART JSON line → WS63 主控 → SLE → 远端 IR 节点。三种串口控制切分点之三（AT/二进制帧/JSON line）。
- `NEW-SMARTEDGE-GATEWAY-FRAME.md`：全屋智能网关——统一 0xAA 帧协议（6-253B）+ 分组地址（0x10-0x1F 四域）+ SLE 三 UUID（FF00 服务/FF01 控制/FF02 状态）+ 七入口汇入单控制模型。247B 载荷预算与 DS10 实测下行上限交叉印证。

## 三十一、OKF 时代猎收第十四批（2026-09-13，HiDiTing src 图谱 + IP-over-SLE 首个开源实现，116→117 concepts）

- `NEW-HIDITING-LWIP-SLE-NETIF.md`：hs-fbb src 图谱（10020 个 .c 盘点）——`interim_binary/3322` 为闭源二进制挂载点、`application/wearable`(1579) 开源。核心发现：**lwip_sle_adapter.c（434 行）= 首个开源的 IP-over-SLE 集成**——lwIP 虚拟以太网 netif（etharp_output + sle_chba_send_pkt linkoutput、BROADCAST/ETHARP/IGMP flags、IP_FRAG_MAX_MTU），SLE 链路状态 1:1 映射 netif。对我们 WS73：lwIP netif 模式 = 把 dongle 暴露为真网络接口的现成蓝图（ping/TCP 工具直接可用）；小 MTU 缺口由 IP 分片吸收。
- 大仓方法论：纯 ls-tree 盘点 + 单文件 raw 直取，零 checkout 完成 764MB 树的图谱。

## 三十二、OKF 时代猎收第十五批（2026-09-13，FIND 家族权威参照 + 本地库保鲜扫，117→118 concepts）

- `NEW-SSAPS-FIND-REFERENCE.md`：OHOS ssaps_server_find.c（1498 行）= FIND_BY_UUID/STRUCTURE 的权威实现参照——请求码路由响应、STANDARD/CUSTOMIZE/MIX 三态 UUID 类型、**MTU 作为预算参数传入载荷构建器**、V10 协议版本分叉显式并存。直指我们 assets/stack/ssap 缺失的 FIND_BY_UUID 功能（此前 ssap 审计已标注）。
- **本地库保鲜扫**：45 仓 ls-remote SHA 对照全部 current（API 限额后改走 Git 协议）；已消化仓（uwb-like-ranging/keyboard-cli/nearlink_service）均无新推送。

## 三十三、OKF 时代猎收第十六批（2026-09-13，**修正批**：multi-READ/WRITE 实存，118→119 concepts）

- `NEW-SSAPS-MULTI-OPERATIONS-CORRECTION.md`：**修正本会话早前误判**——OHOS 栈 multi-READ/WRITE 实存（命名 `*MultiRead*`/`*WriteMulti*`，此前 grep MULTI_WRITE 模式漏检）：WriteMulti 子项循环带逐项准入（SSAP_LoopControlType_E）、ReadMulti 按"先授权→再算预算→后序列化"次序、**multi-processing 是 MTU 交换时协商的链路能力位**、CCCD 状态按对端地址存 clientConfigs 向量。原始记忆"multi-READ/WRITE done"说法正确；审计方法失当。教训入库：SSAP 多操作特性命名规范。

## 三十四、OKF 时代猎收第十七批（2026-09-13 收官，Port FSM 三源闭环，119→120 concepts）

- `NEW-PORT-PROFILE-FSM.md`：tethering port_stm.h 八态 FSM（IDLE→REGISTER_APP→CREATE_LINK→GET_SERVICE→FIND_SERVICE→READ_PROPERTY→SET_NET→CONNECTED）+ 用户会话事件与 SSAP 回调同rank 事件集。Port 抽象三源闭环完成（tethering 1.x FSM + HiDiTing 2.0 bs_sle_port API + find/middleware 参照）——Port 作为 SLE 传输抽象已成生态方向，我们 SSAP 栈的 bring-up 状态机蓝图齐备。

## 三十五、OKF 时代猎收第十八批（2026-09-13 续跑，深挖①：port 实现机制，120→121 concepts）

- `NEW-PORT-PROFILE-INTERNALS.md`：port FSM 实现机制——**每状态分发表**（各态自带事件分发器，非迁移矩阵）+ 客户端**三键会话缓存**（addr/appId/UUID 三比较器查同一表）+ 对称启停/注册括号纪律。对我们 SSAP：per-state dispatch 优于全局迁移矩阵；三键缓存对应 server 端 clientConfigs 向量需求。

## 三十六、OKF 时代猎收第十九批（2026-09-13 续跑，深挖②：SSAP servm 模块地图，121→122 concepts）

- `NEW-SSAP-SERVM-MODULE-MAP.md`：SSAP servm 全套 9601 行模块地图（server 4652 + client 4949）——客户端 **peer 服务缓存**（ssapc_cache 1009 行，每链路缓存远端服务/属性免重复发现）+ **客户端应用链路 SM**（ssapc_app_link_sm 623 行，应用级生命周期独立于 ACL）+ 共享链路/句柄平面（ssap_link/handle/manager）。对我们栈的建设路线图：缺口（find/多操作/客户端缓存/app SM）各 600-1500 行，与既有规划工作量假设吻合。

## 三十七、OKF 时代猎收第二十批（2026-09-13 续跑，深挖③：多入口汇入实现，122→123 concepts）

- `NEW-SMARTEDGE-MULTI-ENTRY.md`：SmartEdge 七入口汇入 = **一头文件一入口**（ble/webserver/mqtt/key/gesture/radar/heart_rate 各一，共享 gateway_frame）+ 编译期特性门控（CONFIG_GATEWAY_MQTT_ENABLE + UNUSED_ATTR 桩）+ 命名时序常量（ACK 18s/轮询 20ms/状态上报地板 3s）+ **内建 latency 计量模块**（gateway_latency.h）。csrc/ 为 u8g2 显示中间件（与控制模型分离）。

## 三十八、OKF 时代猎收第二十一批（2026-09-13 续跑，深挖④：AIGC 云端，123→124 concepts）

- `NEW-AIGC-CLOUD-SERVER.md`：云端 3594 行 = 照片审阅 Web 应用（分页/EXIF/_safe_join 防穿越）+ 三 API 链（高德天气→LLM 提示词优化→通义万相异步文生图轮询）+ encoder_epd_wifi 裸机编码节点（OLED/EPD 共享 MOSI 引脚）。**凭据卫生发现**：DashScope API Key 硬编码入库（报告只记事实不复制密钥，上游应轮换）——`getattr(cfg,KEY,None) or "hardcoded"` 是我们工具链要避免的反模式。

## 三十九、OKF 时代猎收第二十二批（2026-09-13 续跑，深挖⑤：客户端对象模型，124→125 concepts）

- `NEW-SSAPC-CLIENT-OBJECT-MODEL.md`：**概念级发现——OHOS SSAP 服务是 property/method/event 三类成员的对象模型**（× STD/VENDOR 变体），非纯 GATT 特征。客户端发现解码时顺带填充 per-address 缓存（Prty/Method/Event 三桶）；V10 响应按 itemType 三态解码；SSAP_DecodeSingleProperty 用计算 needSize 的游标逐字段硬校验。对我们栈：接口描述层（瘦身 IDL）应进入 ssap server/client 设计。

## 四十、OKF 时代猎收第二十三批（2026-09-13 续跑，深挖⑥：共享链路平面，125→126 concepts）

- `NEW-SSAP-LINK-PLANE.md`：ssap_link/link_state（695 行）= 四态链路模型（DISCONNECTED/CONNECTING/CONNECTED/**DISCONNECTING**）+ 显式重试契约写入头文件（DISCONNECTING 期间调用者缓存请求断开后重试）+ 地址键控空安全查询。三层状态组合（连接平面/应用 SM/profile FSM）而非复制。与 OSPL-CONN-FSM 跨方言互证。
- 刷新扫描：无新仓（发现空间持续稳定）。

## 四十一、OKF 时代猎收第二十四批（2026-09-13 续跑，深挖⑦：fuzzer 解剖，126→127 concepts）

- `NEW-STACK-FUZZER-ANATOMY.md`：hydra-fuzz GN 目标解剖——**include 图 = 闭源栈的内部层分类学**（sdf/oal 原语→sdf 框架→dli→dp→cp/bsl 服务面→nai）；共享 stack_fuzz.gni 去重；fuzz 入口走真实内部边界（cm_trans_channel→dli_layer→dtap_scheduler，LLVM FuzzedDataProvider）。对我们：栈目录骨架参照 + 模块边界 fuzz 模板 + 共享 GNI 模式。

## 四十二、OKF 时代猎收第二十五批（2026-09-13 续跑，深挖⑧：节点侧帧收发，127→128 concepts）

- `NEW-SMARTEDGE-ENVIR-NODE.md`：envir 节点 = 五类环境传感器（BH1750/BMP180/DC01/DHT20/GUVA）+ 帧编解码字节级落实（[0]=0xAA、[3]=cmd、reject-early 校验）+ 标准 server/adv 骨架。SmartEdge 故事闭环（网关帧+入口汇入+节点侧）。
- 已消化仓上游检查：uwb-like-ranging current。

## 四十三、OKF 时代猎收第二十六批（2026-09-13 续跑，深挖⑨：OHOS SA/IPC 面，128→129 concepts）

- `NEW-OHOS-SA1190-IPC-SURFACE.md`：**重大架构发现——OHOS NearLink 服务完整 IPC 面谱**：SA 1190（nearlink_service，auto-restart，参数驱动 start-on-demand，HDI proxy 版本地板）+ **28 个 IPC 接口**（SSAP 双端/HADM 测距含 sounding_result parcel/**ASC 音频**/TWS/VCP/HID host/云配对/CDSM/观测器族）+ SSAP 对象模型的 method/event parcel 序列化跨 IPC 边界。音频/TWS/VCP 首次在公共树现身（LE-Audio-out-of-scope 决策获得生态佐证）。

## 四十四、OKF 时代猎收第二十七批（2026-09-13 续跑，对象模型 IPC 闭环，129→130 concepts）

- `NEW-IPC-OPCODE-TAXONOMY.md`：**NL_SSAP_CLIENT_CALL_METHOD 实锤**——property/method/event 三类成员各有独立 IPC 动词，对象模型端到端贯通（app→IPC→service→SSAP CALL_METHOD→栈）。**NL_SET_SLE_COEX_PARAM**（共存参数运行时可调走 IPC）与 NL_VCP_CLIENT_SET_DEVICE_ABSOLUTE_VOLUME（VCP 音量）同面。对我们：daemon 控制面按成员类分动词 + 共存调参能力位。

## 四十五、OKF 时代猎收第二十八批（2026-09-13 续跑，Replay 加固续篇 + Taihe IDL，130→131 concepts）

- `NEW-REPLAY-HARDENING-TAIHE.md`：nearlink_service 48 小时内再动 67 commits（115 文件 +7215）——**ReplayConnectedLink 加固轮**（MTU 补发 onMtuChanged(link->mtu)、并入注册任务同步执行消除重复通知窗口、165 行重放单测，检视 S2/S3/W1 标签）+ **Taihe IDL for SSAP**（@ohos.nearlink.ssap Client/Server promise API + ANI 绑定——ArkTS 面第三层绑定：NAPI/IPC/Taihe）。热仓日更节奏确立（保鲜扫应日检）。

## 四十六、OKF 时代猎收第二十九批（2026-09-13 续跑，第三协议消费端，131→132 concepts）

- `NEW-MESHMAX-DESKTOP.md`：MeshMaxDesktop v2.2.0（Electron+Nuxt+**noble BLE**）= mesh 图传协议第三消费端（手机 App/板载 Web/PC 桌面）。**混合传输策略**：START/END 带应答串行 + IMG_DATA 无应答并行 6 包 in-flight（7B 头 dst/seq/len 分块）。三实现位级 RLE 一致 = 互操作完整性的最强证据集。@abandonware/noble 供应链注意。

## 四十七、OKF 时代猎收第三十批（2026-09-13 续跑，mesh 底座容量学，132→133 concepts）

- `NEW-MESH-TRANSPORT-SUBSTRATE.md`：AIGC mesh 底座 = **双角色连接池**（固定数组，server 入向 1 + client 出向 4，注释标"芯片硬限制"——树形成因）+ conn_id/mesh_addr 双键查找 + **环形去重缓存**（src+seq→msg_id，最新优先反向扫）。分层：transport(谁连着)→route(发哪)→forward(转发+去重)→main。规划硬数字：WS63 家族多连接预算 1+4（车控 8 client 纯角色对照）。

## 四十八、OKF 时代猎收第三十一批（2026-09-13 续跑，datatransfer 缓存内部，133→134 concepts）

- `NEW-DATATRANSFER-CACHE-INTERNALS.md`：AppConnectParamMapping 每应用会话含 **tcid（动态传输信道 ID——feature_mgr 的 FEAT_DYN_TCID 上游实证）**/transMode/frameType + **transState+preTransState 三态前态回溯**（BUSY/AVAILABLE/FAIL，freeze/restore 机制的数据基础）+ operator== 防重插入。对我们：会话记录预留 tcid 等价字段 + 前态影子对。

## 四十九、OKF 时代猎收第三十二批（2h 续跑 1/8，服务注册表+句柄分配器，134→135 concepts）

- `NEW-SSAP-REGISTRY-HANDLE-ALLOC.md`：服务注册 = Cache 三族（Service/Property/Descriptor 带异步回调）+ **按 handle 区间删除**；16-bit 句柄 = **范围块分配器**（整块 [MIN,MAX] 起始、分配切分、容量限碎片数）。对我们：动态服务生命周期三件套（异步注册/区间删除/块分配器）。

## 五十、OKF 时代猎收第三十三批（2h 续跑 2/8，Rust 示例课程，135→136 concepts）

- `NEW-WS63-EXAMPLES-RUST.md`：ws63-examples = 30+ 示例课程（async/rtos-interop/connectivity/hazards 四域）。**ARCHITECTURE.md 自报过时**（称仅 blinky，实况 wifi_connectivity 2180 行 Rust/wifi_softap 1445 行）。核心机制：**wifi_blob_link 的 build.rs 将厂商 WiFi ROM blob（ws63-radio-sys 提供，rv32imfc/ilp32f）以 +whole-archive 链入 .wifi_pkt_ram NOLOAD 段**（0xA00000/0xC000，g_mem_start_addr_cfg 相对 __wifi_pkt_ram_begin__ 寻址）——Rust 裸机跑厂商 WiFi 栈的完整配方。rust-ws73 直接可用。

## 五十一、OKF 时代猎收第三十四批（2h 续跑 3/8，no_std 调度器，136→137 concepts）

- `NEW-HISI-RTOS-SCHEDULER.md`：hisi-rtos = Rust no_std 调度器（三 RunPolicy：Cooperative/**Budgeted CPU 配额补充**/Preemptive 时间片；272 字节统一任务/陷阱帧；TIMER+软中断延迟抢占 mret 尾声重臂 deadline；**start_with_port 能力门控移植**——抢占须板级实证换取；spec/+tests/ 规格驱动）。对 rust-ws73：Budgeted 是与厂商射频任务共存的确定性原语；272B 帧是上下文切换成本的具体尺寸。

## 五十二、OKF 时代猎收第三十五批（2h 续跑 4/8，Cargo-only WiFi 里程碑，137→138 concepts）

- `NEW-HISI-RF-WS63-COMPOSITION.md`：**里程碑——Rust 裸机 WiFi on WS63 是工作代码**：hisi-rf-ws63 组合根 + 五层所有权链（facade→core 契约→ws63 后端→radio-sys ABI→rtos）+ **Cargo 交付归一化归档 + 原生 rust-lld（零厂商 SDK/Python/GCC/后链脚本）** + profile "wpa2-personal,smoltcp"（WPA2+TCP/IP 栈）。归一化归档模式 = 厂商 blob 问题的答案，rust-ws73 WiFi 腿上游已示范。

## 五十三、OKF 时代猎收第三十六批（2h 续跑 5/8，blob 归一化流水线，138→139 concepts）

- `NEW-RADIO-SYS-NORMALIZATION.md`：ws63-radio-sys = **字节可复现 blob 归一化流水线**——三包发布单元（sys: links 元数据 ABI 契约 / blob: 归一化归档零构建期下载 / hisi-rf-link: 重定位清单+验证+兼容 profile 纯 Rust 工具）；CI 从固定 submodule 重建+**字节比对**+hostap 跨编译 ABI 验证+macOS 规范构建器字节一致门；依赖序发布带 registry 可见性等待。**厂商 blob 供应链化的一般参照**。

## 五十四、OKF 时代猎收第三十七批（2h 续跑 6/8，契约层诊断学，139→140 concepts）

- `NEW-HISI-RF-CORE-CONTRACTS.md`：hisi-rf-core = 芯片中立契约（WifiBackend trait@508、wifi/ble/sle 全契约、零 IP 栈所有权——app 选 embassy-net 或 smoltcp::phy::Device）。**诊断 schema v2 为本会话最佳错误报告设计**：零分配+稳定机器码+恢复动作+四条目数值 trace，**刻意排除 SSID/密钥材料/任意后端文本**；README 契约式排除清单（永不依赖 PAC/blob/调度器/分配器/ROM/NVS/TLS/镜像格式）。

## 五十五、OKF 时代猎收第三十八批（2h 续跑 7/8，五层链顶完工，140→141 concepts）

- `NEW-HISI-RF-FACADE.md`：hisi-rf facade 薄选择层（lib.rs+诊断再导出，chip-* + **命名 profile**）——**profile-wifi-wpa3-smoltcp 在列（WPA3 已通）**。五层链（facade→core 契约→ws63 后端→radio-sys blob→rtos）文档闭环。rust-ws73 采用 feature 命名 profile 惯例 + WPA3 从起步规划。

## 五十六、OKF 时代猎收第三十九批（2h 续跑 8/8，21 维 IQ 特征模式，141→142 concepts）

- `NEW-IQ-FEATURES-SCHEMA.md`：parse_iq_raw.py（690 行）= COLLECT 流状态机组装器 + **21 维 IQFeatures 五域特征**（时域幅值/PAPR/过零、星座 spread/相位抖动/IQ 相关、FFT 峰值/带宽/谱质心、SNR、偏度/峰度）+ 三层数据模型（measurement/raw/features）。对我们 WS73 测距：好/坏 IQ 捕获的数值定义先于任何 ML。

## 五十七、OKF 时代猎收第四十批（gitcode 解锁首批，知识生态收敛，142→143 concepts）

- 工具：scripts/gitee CLI（GITEE_TOKEN/token 文件双通道）+ repo launcher v2.5 + gitcode private-token（~/.local/share/，600 权限仓外存储）。
- **GitCode 搜索金矿**：nearlink-vip/hs-wiki + hs-peripheral-knowledge = 第三方 agent 可读 HiSilicon 知识库双仓——hs-wiki 为 OKF 式 markdown（四域 Wearable/AIoT/Vision/Media + runbooks + **products.yaml 披露 HiDiTing 芯片星座：hi3322 主控 + hi2871 GNSS + hi2131e CAT1**）；peripheral-knowledge 为 JSONL 知识图谱（169 peripherals/102 facts，confidence+extraction_method 溯源，verifications 层设计空置）。
- 知识生态收敛信号：第三方独立构建的 agent 知识库与我们 knowledge/ OKF 束同构（frontmatter/域索引/runbooks/结构化数据四要素平行）。

## 五十八、OKF 时代猎收第四十一批（gitcode 金矿王炸，144 concepts）

- `NEW-NLD-DBUS-DAEMON.md`：**本会话最重磅——Nld + nearlinkctl = 星闪界完整 BlueZ 等价物**（goodspeed1，gitcode）：nld 总管 + 每狗一个 nldadapter.<dev> 进程（**eRPC 对接海思 BS2x/WS63 dongle——正是我们硬件**）+ 14 接口 D-Bus API（977 行 XML，BlueZ 一一镜像：Adapter1/Device1/Agent1/SsapManager1/本地 SSAP 服务端对象树/SsapAdvertisingManager1/**Firmware1 D-Bus 固件更新**）+ nearlinkctl（bluetoothctl 式 CLI）。可靠性契约=异步方法固件确认后才完成+看门狗。Linux/Windows 双原生（Win 全静态+私有 D-Bus 总线）。
- **对我们的意义**：WS73 dongle 用户态部署形态的现成参照架构（多狗进程隔离/固件 ACK 绑定/D-Bus API 面/本地 SSAP 服务端）；BLE-WIFI-USERLAND-RESEARCH 之问的答案。
- 同 org 星闪开源社区：skills（AI Agent 技能库+MCP 工具集）、ws63flash-win、firmware_repo、nearlink-contrib 等留档待挖。

## 五十九、OKF 时代猎收第四十二批（2h 续跑，dongle 协议分解，144→145 concepts）

- `NEW-NLD-ERPC-PROTOCOL.md`：Nld dongle 协议 = **九服务组 eRPC**（host/device_manager/seek/announce/connection/ssap_client/ssap_server/firmware/**low_latency**，双向 client/server 生成对）+ 双串口分离（device:serial:event_serial，防队头阻塞）+ IDL 代码生成。对我们：与 HCC-over-USB 并列的厂商 dongle 传输设计；low_latency 模式服务与 D-Bus Firmware1 固件更新为我们未暴露的能力。

## 六十、OKF 时代猎收第四十三批（gitcode 深挖：大赛语料普查，145→146 concepts）

- `NEW-2026-COMPETITION-SURVEY.md`：HiSpark 2026 嵌入式大赛语料（8.6G，**IOT 星闪方向 ~65 队 + AIOT 12 队**，62 README）普查入库。芯片普查：WS63 主导、WS63E×4、BS21E×1、H3863×2。域分类：安全/养老（最大簇）、定位×2、音频×3、可穿戴×4、机器人/车×8、工业仪器×7、基础设施×4。深挖队列 Top6：SLE_Indoor_Locate + BS21E_sle_location（定位对比）/SLE_DLNA_sound/多节点运动感知/DTU/WS63E 电安。
- 流程修正：check-okf 经管道 tail 掩盖退出码导致失败链继续（计数漂移），后续直跑取码；头/体两行计数需同批校验。

## 六十一、OKF 时代猎收第四十四批（2h 续跑，定位双子深挖，146→147 concepts）

- `NEW-CS-POSITIONING-PAIR.md`：18600_SLE_Indoor_Locate = BS21E 多锚点 SLE CS 室内定位全栈——**GTTT 组时分传输一拖八（SDK 调度 CS 事件，G-T 模式）**+ 锚端双 IQ 测距（经验 scale/offset 校准残差偏置）+ 标签端 1310 行**线性最小二乘三边定位**（EMA+GDOP+残差门限+**3D 秩亏自动降 2D**）+ BLE 桥 JSON 输出；11706 双芯片拆分（BS21E 定位 + WS63 显示上联）。官方 SDK 自带 sle_locate 样例路径实证。对我们 CS 测距：GTTT 调度 + 求解器束 + 秩亏守卫三件套直接可用。

## 六十二、OKF 时代猎收第四十五批（无限 harvest 同步 49，浏览器烧录，147→148 concepts）

- `NEW-WEB-FLASHER-FWPKG.md`：StarFish nearlink-web-flasher = **零依赖 Web Serial 烧录器**（WS63/BS2x：fwpkg 解析→LoaderBoot 握手→波特率切换→YMODEM CRC/1024B→复位，903 行 JS）。**fwpkg 容器魔数 0xEFBEADDF**（与 boot-ROM 同步字 0xEFBEADDE 同族差一尼布尔——容器层 vs 传输层）；CRC16 覆盖窗 + bin-info 表。nearlink-contrib = 早期组件集（仅 mpu6050，Step1/Step2 惯例）。烧录协议第三独立实现（ws63flash/xf_burn 后）。

## 六十三、OKF 时代猎收第四十六批（无限 harvest 同步 50，上游归属 + 官方 ArkTS 面，148→149 concepts）

- `NEW-SIG-UPSTREAM-NEARLINKKIT.md`：**归属裁决**——openharmony-sig/communication_nearlink = tethering_nearlink 的上游家族（sa_profile 字节同、services ~500 处分叉，fork 演进属 datatransfer/天线管理）；官方 App 面 = **@kit.NearLinkKit 四模块（scan/ssap/dataTransfer/constant）**，官方样例 SsapClientPage/ScanConfigPage 实操（App 面只暴露 client 侧+传输，server 留内部）。

## 六十四、OKF 时代猎收第四十七批（无限 harvest 同步 51，SLE 音频实证，149→150 concepts）

- `NEW-SLE-AUDIO-SPEAKER.md`：18884 大赛作品 = **SLE 48kHz/16bit 双声道无压缩音频实证**（PHY 4M+功率/速率拉满）+ DLNA/minimp3 网络模式 + 三角色三板部署（发送/播放+网络/控制）+ 微信小程序配网。SLE 容量两极（DS10 小帧 14ms ↔ 本作大流 PHY 4M）均被真实项目占满。I2S 播放 + PCM2706 USB 音频 + 降噪模块。

## 六十五、OKF 时代猎收第四十八批（无限 harvest 同步 52，学生 DTU，150→151 concepts）

- `NEW-STUDENT-DTU-AA55.md`：23778 DTU = CONFIG/RUN/Storage 三门面 + **AA55 二进制配置协议**（状态机 parser+seq/len/CRC16+命令表 GET_MODE_STATUS/REBOOT）+ SLE/UART0/UART1(485) 三路透明桥 + **sle_tree_v1 树形组网预留**。CONFIG/RUN 分面纪律优于语料均值。串口控制切分点家族 +1（二进制 AA55）。

## 六十六、OKF 时代猎收第四十九批（无限 harvest 同步 53，多节点动捕 + gh/HTTPS 切换，151→152 concepts）

- `NEW-MULTI-NODE-MOTION-DETECTION.md`：24897 = BNO085/BMI270 可穿戴动捕（**CD4053 模拟开关单 UART 分时复用双 IMU** 降本 + **SlimeVR 开放体测协议**上 WS63 + 四元数模块），Bridge+UDP WiFi 链（重流选 WiFi 非 SLE——语料级传输选型信号）+ web-studio 康复训练模块。
- 流程切换：移除全局 url.git@github.com:.insteadof（https→ssh 重写，瞬断根因），推送改走 gh 凭据 HTTPS。

## 六十七、OKF 时代猎收第五十批（无限 harvest 同步 54，手语手套，152→153 concepts）

- `NEW-SIGNLANGUAGE-GLOVE.md`：18384 星语手套 = 弯曲传感器 ADC + 双手 JY901P IMU（UART，项目内完成 MPU6050/BNO085→JY901P 迁移）经 SLE UART 方言到 OrangePi **边缘 AI**（~1s 语音输出，训练_pc 全生命周期：collect/balance/train/evaluate/benchmark + systemd 部署）——传感器节点傻、边缘扛模型的算力放置范式 + 语料最佳仓库结构。

## 六十八、OKF 时代猎收第五十一批（无限 harvest 同步 55，WS63E 电安，153→154 concepts）

- `NEW-WS63E-ELECTRIC-SAFETY.md`：16781 = WS63E 一主两从星型（A 主控/B 网关 OLED+WiFi/C 执行）+ MQTT 云出口。WS63E 应用层兼容性实证 + 网关模式第七例。拓扑谱系完备：星（本作）/树（DTU）/路由表（AIGC）。

## 六十九、OKF 时代猎收第五十二批（无限 harvest 同步 56，WS53 新芯片家族，154→155 concepts）

- `NEW-FBB-WS53-SDK.md`：HiSpark/fbb_ws53 = **WS53V100 Wi-Fi/BLE/SLE Combo SoC 官方 SDK**（FBB 统一框架，跨芯片移植为设计目标）。独有样例：**sle_conn_param_tuning**（连接参数调优教学样例）+ **sle_chba**（CHBA 命名跨双芯片族闭合：ws53 样例 ↔ HiDiTing lwip 适配器）。FBB 归一化仓库布局解释社区示例跨芯片移植低摩擦。

## 七十、OKF 时代猎收第五十三批（无限 harvest 同步 57，官方 agent 契约 CLI，155→156 concepts）

- `NEW-HS-FBB-CLI.md`：hs-fbb-cli = fbb 框架族单 CLI（SDK 自动检测 + 组件管理 + 镜像配置）+ **显式 AI agent 契约章节**："fbb 是机制层，skills 是策略层"+ **fbb describe --json 单次态势探针**（schema 版本/工具链路径/SDK chips/targets/verbs 一调全回）+ 自激活环境免 shell 状态。厂商社区 agent-first 工具化实证；nearlink-harvest 类技能应对齐该契约。

## 七十一、OKF 时代猎收第五十四批（无限 harvest 同步 58，社区工具生态，156→157 concepts）

- `NEW-COMMUNITY-TOOLING-ECOSYSTEM.md`：星闪开源社区三仓——**CLAUDE.md 自进化工作记忆引擎**（462 行自引用闭环：读我→载记忆→分流→坑点沉淀，放置契约 ~/fbb_ws63/src）+ MimiClaw（**5 美元芯片上的口袋 AI 助手**，LLM 上 NearLink SoC）+ ws63flash-win（Windows 原生 AT+RST 软复位+loaderboot 后 921600 高速，免 WSL/usbipd）。烧录协议第四独立实现。

## 七十二、OKF 时代猎收第五十五批（无限 harvest 同步 59，OH-on-WS63 版图，157→158 concepts）

- `NEW-OH-WS63-LANDSCAPE.md`：OH-on-WS63 公开版图单薄——hbu-dragon 教学案例集（124M，wifi-iot 样例，**限制性自定义许可**：禁商用/竞赛/教学/论文）+ nearlink_oh_ws63（README 占位）。实质 OH WS63 源仍以 device_soc 稀疏采纳为准。许可扫描先于教学代码采纳。

## 七十三、OKF 时代猎收第六十批（无限 harvest 同步 60，WS63 AI 生态，158→159 concepts）

- `NEW-WS63-AI-ECOSYSTEM.md`：**FBB ModelZoo = 厂商端到端模型转固件流水线**（数据生成→模型转换→SDK 打包一 run.sh 串起；场景：图像分类/音频事件/KWS/音频增强/时序/视觉唤醒/HAR；目标 **WS63 MCU + HiDiTing Nano NPU + Hi1156E Tiny NPU**——第四芯片类 hi1156E 入图）+ BYLE 云之声商用语音 SDK（KWS+LLM 对话，账号门控闭源）。AI 是 NearLink 头号应用方向实证。

## 七十四、OKF 时代猎收第六十一批（无限 harvest 同步 61，306x 排除项，159→160 concepts）

- `NEW-OPEN-MCU-306X.md`：HiSpark/open_mcu = 3061M/3065H/3066M MCU 族（128KB 电机控制，**无 NearLink**——阴性发现防未来误配）。FBB 邻接布局 + ICKEY 购买件号。芯片族图谱修正项。

## 七十五、OKF 时代猎收第六十二批（无限 harvest 同步 62，保鲜轮 + 谱系修正，160→161 concepts）

- `NEW-FRESHNESS-ROUND-0913.md`：uwb-like 上游新增 **Technical Boundary 章节**（贡献=多节点编排/双向聚合/Collector/主机定位/IQ 分析；非目标=厂商低层算法复刻/UWB 波形等价；精度基准未发布——watch 项）。**谱系修正**：sle_mesh 与 sle_mesh_new 为同族两版（sle_team_* 分类学一致，v1.2.2→v4.5.56 时间线），此前"两个独立设计"表述修正。seantran/SparkLink = 4B 长度前缀 TCP 聊天蹭名跳过。
- 流程改进：README 计数更新改为动态读取+自增（消除硬编码断言漂移）。

## 七十六、OKF 时代猎收第六十三批（无限 harvest 同步 63，客户端应用层，161→162 concepts）

- `NEW-SSAPC-APP-LAYER.md`：ssapc_app.c 客户端应用层 = **per-appId 链路状态槽** + 异步注册统一回调形（本地注册也走 onRegisterApp 回调，应用代码形态统一）+ **交互超时一等旋钮**（独立于链路监督）+ 集中清理入口。客户端生命周期三特性补完，与服务端 per-peer 簿记对称。

## 七十七、OKF 时代猎收第六十四批（无限 harvest 同步 64，OHOS 控制台，162→163 concepts）

- `NEW-OHOS-NEARLINK-CONTROL.md`：ohos-nearlinkControl = OHOS 侧星闪开关 CLI（enable/disable + **autoConnPolicy 自动连接策略参数**）+ 机器码+人提示双面错误输出（ERR_NL_INVALID_COMMAND）。SA 1190 的操作员前端。

## 七十八、OKF 时代猎收第五十四批（无限 harvest 同步 65，mesh 操作面，163→164 concepts）

- `NEW-TEAM-MESH-CLI.md`：sle_team_cli.c（469 行）= mesh 操作面 20+ 动词——join/leave/**pairing approve <id> relay|norelay**（批准时决定中继能力）/**allow only|add|del 成员白名单**/hello/hb/pos/alert（丢员告警带最后位置）/ack 注入命令与包类型一一对应（测试场景直接可造）。

## 七十九、OKF 时代猎收第六十五批（无限 harvest 同步 66，服务内存拓扑，164→165 concepts）

- `NEW-SSAP-SERVICE-MEMORY-TOPOLOGY.md`：FreeService 析构器实锤 **SSAP_Service_S 五向量所有权拓扑**（properties/**references**/methods/events/descriptors）——对象模型在服务端 C 结构级同在（+references 含服务引用链接，GATT 次级服务引用类似物）。我们 assets/stack/ssap 需补 methods/events/references 三向量。

## 八十、OKF 时代猎收第六十六批（无限 harvest 同步 67，可运行 API 契约，165→166 概念）

- `NEW-NLD-TOOLS-CONTRACTS.md`：nld tools/ 两脚本 = **可执行 API 规范**——agent-test.py（KeyboardOnly 配对代理+固定 passkey+Multi-adapter 寻址）与 ssap-app.py（ObjectManager 根导出 SsapService1/SsapProperty1/CCCD+onRegisterApplication 树遍历回调）。实现同 D-Bus 回调即可让该测试对自家栈复用——白捡一致性测试台。

## 八十一、OKF 时代猎收（无限 harvest 同步 69，日轮+新工具甄别）

- `NEW-DAILY-CHURN-0914.md`：nearlink_service 日更（微信来电音量修正，ASC/TWS/VCP 活跃使用佐证）；StarFlash-Releases = 编译版 Windows 工具发行仓（无源码，甄别为分发物非知识源）；teki128 日更待观察。热仓日轮节奏持续。

## 八十二、OKF 时代猎收（无限 harvest 同步 70，大赛双子，167→168 concepts）

- `NEW-VISION-GLASSES-DISASTER-RESCUE.md`：10447 视觉导盲眼镜四层（眼镜→WS63E SLE 发送(GPS+ICM42688)→WS63E 接收→ESP32 串口透传→Python/FastAPI 视觉导航）——**WS63→ESP32 串口透传**为算力分配范式；12148 灾难救援三节点 NODE_AUX 类型化协议（主/辅节点区分区域性 vs 单点风险）。

## 八十三、OKF 时代猎收（无限 harvest 同步 71，驱动与 GPS 管线，168→169 concepts）

- `NEW-MPU6050-NMEA-DRIVERS.md`：nearlink-contrib MPU6050 驱动 = WS63 **I2C 引脚图**（I2C0=GPIO17/18，I2C1=GPIO16/15，MODE 2）+ Step1/Step2 集成指南惯例；sle_mesh_new NMEA 解析器（313 行）= GPRMC/GNRMC/GPGGA/GNGGA → **e6 定点坐标**（避免 MCU 浮点）→ mesh 包位置字段管线。Step1/Step2 文档惯例可复用于自家样例贡献。

## 八十四、OKF 时代猎收（无限 harvest 同步 72，竞赛批次 3，169→170 concepts）

- `NEW-COMPETITION-BATCH3.md`：10019 冷链运输（WS63+温湿度+GPS+太阳能+小程序 MQTT）/ 12648 WS63E 智能门锁（NFC+**I2S 语音对讲**+**毫米波雷达**+MQTT+HarmonyOS App，单板外设密度之最）/ 14624 四轴飞行器（README 空占位待深挖）。

## 八十五、OKF 时代猎收（无限 harvest 同步 73，竞赛批次 4，170→171 concepts）

- `NEW-COMPETITION-BATCH4.md`：15239 SparkSafe（WS63E 双板车内儿童遗留监测，`protocol.h` 0xA5+cmd+len+payload32+CRC8 公共协议层，**重连前 `sle_remove_all_pairs()` 清旧配对密钥**，非对称滞回四级风险，单机 WiFi STA+SoftAP+SLE 三射频共存）/ 15792 多端健康监测（**WS63 药盒网关 = SLE Client×2 + BLE Client 手环 + WiFi HTTP 上行三射频聚合**，NV 离线服药计划兜底，Ktor/MySQL/Compose 全栈参照）/ 17966 智能安全帽（SLE 级联自组网：**16-bit DAG 位图拓扑 NOTIFY/MODIFY 同步** + 6dB RSSI 换父迟滞 + 断连 10s 原链路优先，本地库最完整 SLE 多跳参照）。

## 八十六、OKF 时代猎收（无限 harvest 同步 74，竞赛批次 5，171→172 concepts）

- `NEW-COMPETITION-BATCH5.md`：10102 多模态姿态感知（**两级 SLE 组网突破 3 连接上限：主控仅连上肢/下肢子主控+ECG 三个节点**，int16 四元数×10000，0xBB 骨架帧 67B/0xDD 文本帧，四端共用 IoTDA `/realtime_data` topic，WS63 WiFi 射频感知人体存在）/ 10347 智慧药仓（H3863 双板：LVGL 触屏+PID 控温+霍尔服药检测；**语音板经 WebSocket 接 xiaozhi-server 以 MCP 工具控制药盒**；CoAP 上行+SLE 鸿蒙直连）/ 14710 跌倒检测（三重阈值+五态 FSM+双速率采样，300ms 告警，最干净的 WS63 穿戴 sample 骨架）/ 15625 星闪护盾（**单代码库 Kconfig `MYDEMO_SAMPLE_SUPPORT_*` 编译期选 N 种节点固件**，共享 12_sle_uart 通信基座）。

## 八十七、OKF 时代猎收（无限 harvest 同步 75，竞赛批次 6，172→173 concepts）

- `NEW-COMPETITION-BATCH6.md`：15413 双模巡检小车（**App 侧三链路降级：云端/基站 WiFi/小车直连**，RSSI 作为一等遥测字段，云端任务→基站拉取→下发闭环）/ 15303 智能晾衣架（最小双板范式，`sle_window_protocol.h` 仅 3 命令）/ 15307 智能水杯（**单 WS63 上 SLE 设备面 + BLE 用户面双模共存**，AS7341 光谱液体识别，MPU6050 倾斜保持计时喝水事件机）。**生态约定：service UUID 0xABCD + property 0x3344 为 sle_uart demo 原生配对，已扩散为竞赛事实标准 — 扫描指纹。**

## 八十八、OKF 时代猎收（无限 harvest 同步 76，竞赛批次 7 AIOT 赛道，173→174 concepts）

- `NEW-COMPETITION-BATCH7.md`：**10714 SLE 羽毛球 = 全库对 WS73 dongle 目标最相关项目** — BS20 拍柄 10ms **非连接广播** IMU、SS928 上 **WS73 模块扫播 40+ 球拍按 MAC 区分**、用户态工具用与我们 SSAP 栈相同的 SDK 头文件（sle_device_discovery.h 等）；运营铁律：**扫描前必须 mcu nl 复位 + rmmod/insmod，否则 "sle adapter init open fail"**；10ms 广播 vs 100ms 扫描帧重复 → C 层 dedup 槽表 + Qt 层按 MAC 时间戳双层过滤得稳定 10Hz；announce/seek/connection 三模式互斥。20520 无接触演示（ASCII CSV 过 SLE 保端到端 ts_ms 延迟可测，500ms MQTT 节流）。15252/15913 仅为 SS928 视觉外围。

## 八十九、OKF 时代猎收（无限 harvest 同步 77，竞赛批次 8，174→175 concepts）

- `NEW-COMPETITION-BATCH8.md`：**11065 运动助教附带 ws73_sdk_linux_WS73_1.10.111 精简树（比我们 1.10.110 新一版）** — 唯一实质 API 增量：`sle_device_discovery.h:146-200` 新增 **SLE_MCS_00(BPSK1/4)→SLE_MCS_12(8PSK) 十三级调制枚举** + 新头文件 logger_manager.h；announce 参数文档化（125µs 间隔、信道 76/77/78、0x7F 功率哨兵）。视觉-IMU 三态融合 FSM（纯视觉→融合→IMU 兜底）。16010 高原水质无人船 `dataExc.c:226-259` 多船 conn_id 轮换（自动按就绪序切换/手动校验）。

## 九十、OKF 时代猎收（无限 harvest 同步 78，日常轮转+判词，175→176 concepts）

- `NEW-DAILY-CHURN-0915B.md`：teki128/nearlink 两新提交（**配对失败即 `sle_remove_paired_remote_device`，与 SparkSafe 的 remove_all_pairs 规则双向印证**，status 全改 0x%x 十六进制）；三个"SparkLink/NearLink"同名仓判词（seantran=TCP 聊天、Heebu=纯 Flutter 局域网 0 命中、xypasolini=RALLY NFC 徽章）— **同名≠同物，需代码级证据（头文件 include/API 调用）才判 NearLink**。

## 九十一、OKF 时代猎收（无限 harvest 同步 79，本地深挖 sle_mesh_new，176→177 concepts）

- `NEW-SLE-MESH-RELAY-OPTIMIZER.md`：sle_mesh_new 路由内部（补完 sync 68 只覆盖操作面的部分）— **双层包格式（mesh envelope + app packet，逻辑/物理 DIRECT 分离，12 种 payload 类型）**、**稳定性门控优化器（配对中/恢复中/成员缺失/半离线 → 冻结重配置）**、保守调参（**12dB 换父迟滞** vs 头盔 6dB、-92dBm 地板、7 子容量帽、未知 RSSI=-128、一代优化器只动 leader-direct 一层）、容量感知中继树 + 入口物理交付偏好。4193 行可移植 C，与厂商 SDK 无耦合。

## 九十二、OKF 时代猎收（无限 harvest 同步 80，本地深挖 OHOS DLI snoop，177→178 concepts）

- `NEW-OHOS-DLI-SNOOP.md`：communication_nearlink_service 未挖掘的 `SleDliSnoop.cpp`（704 行）= **星闪版 btmon** — 9 字节记录头（8B ms 时间戳 LE + 1B 方向位）+ 类型字节 **0xA1 CMD/0xA2 EVENT/0xA3 ACB/0xA4 ICB**，ACB/ICB 4 字节子头 lcid/handle(2)+len(2)；`dli_opcode.h` 228 条 DLI 命令/事件命名空间（0x0405/06 公共地址、0x0C02-06 广播族、0x1401 建连、0x1C01-28 加密族）**直接注解我们 E573H USB dongle 抓包**；商用版隐私脱敏 = opcode 黑名单（13 cmd + 2 event）双源维护防漂移。文本十六进制日志 + 单线程汇入。

## 九十三、OKF 时代猎收（无限 harvest 同步 81，竞赛批次 9，178→179 concepts）

- `NEW-COMPETITION-BATCH9.md`：17513 跌倒检测（**WS63E 板载 2.4G 雷达 + 18KB ROM/7KB RAM 端侧 CNN**，PyTorch 训练 500ms 窗口，SoftAP 一键采原始中频数据闭环迭代）/ 18007 Rd-03_V2 雷达+舵机本地报警（Node.js 代理 + 微信 CloudBase 14 云函数）/ **18600 BS21E SLE Channel Sounding 室内定位（第二套全栈 CS 参照）** — GTTT 组时分同时连多锚、锚端 IQ 聚合+时间戳匹配+SDK 测距、**Tag 端线性最小二乘三边求解器**：中值跳变拒绝→EMA→A^T A 行列式 GDOP 门限→高残差锚剔除重试，8 锚非共面 3D，BLE GATT JSON 出口。

## 九十四、OKF 时代猎收（无限 harvest 同步 82，本地深挖 Nld daemon 内部，179→180 concepts）

- `NEW-NLD-DAEMON-INTERNALS.md`：**D-Bus 树 15 接口 112 成员全枚举**（新增 Firmware1），Adapter 单实例单 ssapc/ssaps 会话；**eRPC 契约 9 对服务**（announce/connection/device_manager/firmware/host/**low_latency**/seek/ssap_client/ssap_server，bs2x 目标全可构建）；Windows 方案 = 自带 dbus-daemon.exe + **comtcp.c COM↔TCP 回环中继**（stdout `NLD_BRIDGE_PORT` 握手、USB CDC 重枚举自动重开、父进程句柄判活）；**生态关键发现：Nld dongle 是 USB VID 109B CDC 串口，与我们 E573H ffff:3733 vendor-bulk 是两种不同 USB 形态**，工具链不可互相假设。

## 九十五、OKF 时代猎收（无限 harvest 同步 83，竞赛批次 10，180→181 concepts）

- `NEW-COMPETITION-BATCH10.md`：18384 星语手语手套（**首个发布链路 KPI 的竞赛项目：SLE 一主两从 25ms 连接间隔 / 双手 50Hz 同步 / 10 分钟丢帧 <0.01%**；10 通道分压 PCB + WS63 片载 ADC，JY901P 卡尔曼 Yaw 漂移 <±1°，Inception-Transformer 52 类 <30ms）/ 17661 NearMeet 徽章（8×WS63 四对主从 + Node.js 中枢，**SLE 懒加载仅雷达模式**，**每板 1m 实测 RSSI 标定常数**做近距判定，同固件 g_local_mac 区分机队）/ 17664 OROS（常规环境节点，OHOS `$oc/.../properties/report` topic 格式可复用）。

## 九十六、OKF 时代猎收（无限 harvest 同步 84，竞赛批次 11 IOT 赛道扫尾，181→182 concepts）

- `NEW-COMPETITION-BATCH11.md`：17183 眼镜平台（**G/D/C 三节点职责切分：D 节点独占 SLE 链路+云端 ASR/LLM/翻译**，头戴端最小化）/ 17316 癫痫雷达灯（**两节点 27 字节 V2 定长帧** + 节点本地判定后上行、MOSFET PWM 灯带、完整免责声明）/ 扫尾判词：16781 三板星型（第三次确认 Client/Server/网关三件套）、16946 方言语音自组网、17725 手势循迹小车、18459 电机 API 分层、18026 SU-03T 离线语音手环（**SU-03T 第 4 次出现 = 事实语音外设**）。IOT 赛道 README 级全覆盖完成。

## 九十七、OKF 时代猎收（无限 harvest 同步 85，GitCode 官方组织扫描，182→183 concepts）

- `NEW-GITCODE-ORG-SCAN-0915.md`：**hinearlink/firmware_repo = 官方固件索引（10 个 WS63 包）** — `ws63_AT_v1.10.102` 即 HHD-01 同版 AT 固件官方下载源（我们 d68d592 备份 = 官方构建实锤）、sle_throughput 三代、littlefs 系列；StarFish `nl` CLI（path/build/set 短名映射 + menuconfig 一键）；扫描判词：GitHub/Gitee/GitCode 三平台本窗口均无新 NearLink 仓，**GitCode 更新呈波次性（最新 ws63flash-win 2026-08-19）— 监控节奏 GitHub 日常 > GitCode 周级/事件驱动 > Gitee 仅组织遍历**。

## 九十八、OKF 时代猎收（无限 harvest 同步 86，本地深挖 hbu-dragon 案例库枚举，183→184 concepts）

- `NEW-HBU-CASES-ENUM.md`：25 个案例全枚举（radar/sle_hybrid/dicsoss/starBeacon/uart 1v1-1vn/wifi_coexist 等）+ **SparkLink 团体标准 T/XS 引用清单（30001-2023 等 10 项）= SLE 标准合规引用块**；sle_starBeacon = **可信信标防伪造 + 覆盖 cm~3km + 2821E 厘米级**；sle_hybrid_mode = **主从一体节点组线性网络**（第三种 mesh 拓扑：无表无位图的链式）；sle_dicsoss = 分布式采集 + `OpenHarmony/DICSOSS/*/Message` MQTT 出口；sle_delay_optimize = RTT 时延测量标准环。nearlink-contrib 判词：仍只有 MPU6050（成熟度：胚胎期），Step1/Step2 文档约定值得借用。**注意：河北大学版权禁止竞赛/教学复用案例代码，仅记架构事实。**

## 九十九、OKF 时代猎收（无限 harvest 同步 87，本地深挖 WildLink 对板 + xfusion 判词，184→185 concepts）

- `NEW-WILDLINK-XFUSION.md`：WildLinkClient/Server（H3863 野外安全对板，**SLE 配对链 + BLE 手机链 + LoRA 远距链三传输层**，SLE client 53 处 ssap 调用密度 = 厂商 SSAP 表面人机工程学基准；`node_telemetry.h` 定长生命体征记录含 min/max/now + need_help 旗标可复用）；xfusion（x-eks-fusion 跨 SDK 组件框架：xf_hal/xf_nal/xf_ble 齐备，**xf_sle 缺位 = SSAP 可移植抽象的开放贡献位**）；0xAA 同步字节 = 生态 UART 帧低端惯例（ATK LoRA/SparkSafe 0xA5/NearMeet）。

## 一百、OKF 时代猎收（无限 harvest 同步 88，本地深挖 OHOS HADM 测距算法层，185→186 concepts）

- `NEW-OHOS-HADM-RANGING-ADAPTER.md`：`ranging_alogorithm_adapter/`（~400 行）= **官方 CS IQ→距离算法层** — **6 种算法模式**（METHOD_1M 1MHz 跳频 150m / METHOD_2M / METHOD_1M_2M 低复杂度 75m / METHOD_ADJ_R_END V1-V3 动态 r 150m）；`MeasureAlgPara` 双侧 IQ（DUT/RTD uint16 I/Q）+ 双侧 ToF/信道图/IQ 位宽；`DisResult` 输出三级平滑（disOri/disSlightSmoothed/disSmoothed）+ **prob 置信度** + smoothNum 连续有效计数；**SLEM 错误码 0x8000A400-0x8000A800**（TOF_IQ_NOTMATCH = ToF 远大于 IQ 的发散检测器、IQ_LOW_ENERGY、MARIX_INV_FAIL）。CS 故事三端齐备：IQ 采集（uwb-like-ranging）→ 官方算法层（本报告）→ 求解器（18600）。

## 一百零一、OKF 时代猎收（无限 harvest 同步 89，Nld eRPC 契约枚举，186→187 concepts）

- `NEW-NLD-ERPC-CONTRACT.md`：erpc_gen/bs2x 全部接口头文件枚举 — **9 服务 ID 地图**（connection=84/seek=83/ssap_client=85/ssap_server=86/host 回调侧 22 个 on_* 方法）共 60+ 方法；**SSAP 枚举契约级实证**（permission 位掩码 READ1/WRITE2/ENC4/AUTHN8/AUTHZ16、find 六类型含 REFERENCE_SERVICE=2、operate-indication 六位含 BROADCAST=32）；连接控制面 `sle_set_mcs/set_phy_param/set_data_len/set_channel_map/set_nv_smp_keys` 逐连接旋钮；**firmware UPG OTA 状态机**（start→write→get_status/result + build-info/commit/version）；low_latency 仅 2 方法 = QoS 开关非数据通道。主机↔dongle 现在三深度可读（USB bulk → DLI opcode → eRPC method）。

## 一百零二、OKF 时代猎收（无限 harvest 同步 90，DLI 参数结构字典，187→188 concepts）

- `NEW-DLI-STRUCT-TABLES.md`：dli_cmd_struct.h(632)+dli_event_struct.h(597) = 228 条 opcode 的参数字典（含单位与范围注释）— 广播参数远超 BLE 形态（**导频密度/MCS/secondAdvMaxSkip 跳发**、时长 N×10ms）；连接时序 SLE 独有字段（txRxInterval 事件内/eventInterval 事件间/systemTimeUnit/**txRxFlag 先发后发**，间隔 0.25ms 单位 [7.5ms,4s]）；**gFeedback/tFeedback HARQ 反馈码表**（CBG/TB/半可靠组播 m 序列编号）— m 序列反馈机制在 DLI 边界的直接证据；AdvReportEvt.eventType 4 位义、EnableEncryptParam {SM linkKey+cryptoAlgo+keyDerivAlgo}。USB 抓包解码器可从十六进制升级到参数级打印。

## 一百零三、OKF 时代猎收（无限 harvest 同步 91，SLE HID 键盘类 + 全库 frontmatter 规范化，188→189 concepts）

- `NEW-SLE-HID-KEYBOARDS.md`：TP78v3（GPL3.0，Hi2821/E **USB/BLE/SLE 三模键盘，有线 8K/星闪 2K 回报率** — 全库首个 SLE 输入设备回报率实证，VIA 改键+在线配列+固件导出+接收器方案，12 版指导 PDF）/ FlashKeyboard（**bs20/bs21e/bs22/bs2x 四芯片目标固件框架**，共享 keyboard app + BLE HID-over-GATT server 参照，与 Playjoy 键盘 CLI 同源生态两端）/ 星鸿派 HuaqiuOpenHardware 开源板（WS63V100/Hi3863，**CHANGES_FROM_ORIGINAL + MANIFEST 校验 + "示例非独立可编译"诚实声明 = 厂商衍生资产再发布治理范本**）。**同轮：全库 189 份文档 frontmatter 规范化**（字段序 type/title/language/created/tags/sources/trust/stale_after、title 引号化、正文推断 sources、trust/stale_after 默认值），与用户要求的格式修正合并提交。

## 一百零四、OKF 时代猎收（无限 harvest 同步 92，starclaw/MimiClaw 判词，189→190 concepts）

- `NEW-STARCLAW-VERDICT.md`：MimiClaw = **$5 ESP32-S3 纯 C 裸机 AI agent 闭环**（Telegram 长轮询 + WebSocket :18789 + 串口 CLI 三通道 → 双向队列 → agent loop：context→LLM→tool_use→web_search→outbound；12MB SPIFFS 持久记忆；Anthropic/OpenAI 运行时切换；agent/bus/channels/cron/gateway/heartbeat/llm/memory/ota 完整模块分类学）。GitCode hinearlink 组织描述"可运行在星闪WS63开发板"= **意向非现状（树内 0 行 WS63 代码）— WS63 助手开放位**；SSAP 服务 = 片上 agent 的天然工具面。

## 一百零五、OKF 时代猎收（无限 harvest 同步 93，agent 契约 + BYLE + 扫描判词，190→191 concepts）

- `NEW-AGENT-CONTRACT-BYLE-SCAN.md`：skills-nearlink `CLAUDE.md`（462 行）= **最完整的公开 WS63 开发 agent 契约**（自进化记忆闭环：读文件→执行→发现新坑写回；目录访问矩阵：仅 peripheral 可写、仅 include 可引、禁 #define 覆盖 Kconfig；build.py 命令面 + `_all/_load_only.fwpkg` 词汇；反幻觉条款）— 发布我们自己 agent 契约的设计参照。BYLE byleFN 闭源 SDK 文档（KWS+打断+opus+ByleStudio JSON 配置 = 第二家 AI 音频芯片厂商特性清单）。判词：Terrydev5/NearLink = Bonjour+WS 名字党第 4 例（但一次性 token 门控临时端口文件传输模式可借鉴）、Qwac = 仅 LICENSE 占位（Playjoy 替代品预告，列入复查单）。

## 一百零六、OKF 时代猎收（无限 harvest 同步 94，fbb-modelzoo 管线解剖，191→192 concepts）

- `NEW-FBB-MODELZOO-PIPELINE.md`：HiSpark.AI 开放模型库 = **run.sh→config.cfg 统一管线（数据生成→模型转换→SDK 工程打包）**，7 场景（KWS/音频异常/RNNoise 增强/HAR/时序/视觉唤醒/图像分类）；config.cfg 分层（不改块 PLATFORM=RISCV/mindspore-lite/**micro_quant** int8+float32 IO / 必填块 SDK_PATH+ADAPTOR_PATH+TOOLCHAIN_PATH）；**metadata.yaml 芯片→模板绑定表**（WS63 SAMPLE_COMMON 注入 ai_main.c 到 fbb_ws63 — 模型库不发自固件而是缝合进 SDK 树）；per-chip 精度对比工具（nano_accuracy_compare.py）；skills/ 又见 agent 面。生态 agent 三件套（CLI/知识组织/仓契约）+ 模型库 = HiSpark.AI 开发者面全图。

## 一百零七、OKF 时代猎收（无限 harvest 同步 95，Ghidra RISCV31 定制 ISA 全解，192→193 concepts）

- `NEW-GHIDRA-RISCV31-ISA.md`：HiSilicon 定制 RV32 七族指令位级编码表（**C.PUSH/C.POP/C.POPRET 硬件多寄存器压栈、L.LI 48 位长立即数、C.SB/SH 借 FP 槽、MULIADD、ADDSHF~ANDSHF 移位融合 ALU、JAL16/J16 25 位远跳、BEQI/BNEI/BLTI/BGEI 立即数比较分支**）— 原生 RISC-V 反汇编器在 WS63/Hi2821 镜像上必产出乱码的根因；slaspec 模块可直接装回 Ghidra 重跑我们既往 loaderboot 逆向。41 库工具架全覆盖。

## 一百零八、OKF 时代猎收（无限 harvest 同步 96，GitCode 扫描轮 0915b，193→194 concepts）

- `NEW-GITCODE-SCAN-0915B.md`：**xiaohong-ai/ws63flash v4.0.1 = WS63 UART 烧录器 Rust 重构**（WS63/BS21E/ESP32 三芯片 ChipType、Tauri GUI、fwpkg/ymodem/sign 三 crate、flasher+fwpkg+sign 三 CLI 分立，MIT，~1945 行）— 同一 boot-ROM 协议第三次独立实现（C→Python→Rust），后浪补前浪动词；fwpkg 结构体含 offset 换算注释可直接复用。fixedstarheng/ws63-xiangpenpen（14807 上游：`sle_client_one`/`sle_server_many` 字面 1:N 目录命名，老年抑郁多模态早筛）。上游新鲜度全清（uwb-ranging/teki128/keyboard-cli/**communication_nearlink_service 0 领先**）。

## 一百零九、OKF 时代猎收（无限 harvest 同步 97，小鸿 AI 生产级语音助手解剖，194→195 concepts）

- `NEW-XIAOHONG-VOICE-AGENT.md`：xiaohong-fbb_ws63 = **生态内最完整的开源 WS63 AI 产品代码库**（对比竞赛原型）— **WS63 主控 + CI1302 启英泰伦音频协处理器（UART ring + Opus 16k/120ms/1920 样本解码预算 + TTS 下行 + 离线 KWS 固件 bin）**双芯片分工；mongoose WebSocket agent（**每次建连前从 Settings NV 重读 URL/token/协议版本** — OTA 晚于首建的弹性配置）；二进制帧先清后投递的背压纪律；LVGL+ST7789+littlefs+OTA+boards 抽象；fw_protocol/ 发布物布局（两个 dated fwpkg + 命令词协议 xlsx）。**语音助手 + SLE 家控 = 已发布 fwpkg 实证的产品形态**。

## 一百一十、OKF 时代猎收（无限 harvest 同步 98，web-flasher 与协议四重印证，195→196 concepts）

- `NEW-WEB-FLASHER-QUADRUPLE.md`：StarFish nearlink-web-flasher（903 行零依赖 JS，Web Serial API 在 Chrome/Edge 直接烧 fwpkg：解析（0xefbeaddf + CRC + 分区表）→ LoaderBoot 握手 → reqBaudrate 中途切换 → YMODEM → 复位；含 HiSilicon 格式出处声明与 test/）。**协议四重实现收官：C（goodspeed34）→ Python（geekheart）→ Rust v4（xiaohong-ai）→ JS 浏览器（StarFish）四家字节级一致** — 我们 fwpkg/boot 协议文档的最强背书；厂商 AutoBurn 为第五闭源成员。

## 一百一十一、OKF 时代猎收（无限 harvest 同步 99，新扫描轮 0916，196→197 concepts）

- `NEW-FRESH-SCAN-0916.md`：night-fishing-nearlark-agent（**自托管 FastAPI+WebSocket AI Agent 层过 SLE 1vN 控灯** — agent-over-NearLink 第 4 例、首个非厂商云方案；SM2/3/4 国密声明待证）+ hi3863-smart-aquaponics（双板 Client/Server 三件套又一例，PID 卷帘电机，ASRPRO 第 5 次）。**hispark-rs 上游核验：HEAD ce68c14 2026-09-10 与本地一致（NET0 RX-stop 已在 sync 71 报告覆盖）**。GitHub 近 24h 窗口无新近联仓。agent-transport 矩阵四方齐备：厂商云/片上/产品语音/自托管。

## 一百一十二、OKF 时代猎收（无限 harvest 同步 100 🏁 收官，nearlink-firmwares 固件商店，197→198 concepts）

- `NEW-FIRMWARE-STORE-CAPSTONE.md`：星闪工具箱（MiraHikari，**Tauri+Rust 一站式固件管理**：固件商店/本地管理/内置串口烧写/AT 命令收藏）— **分布式 endpoint 镜像注册表**（endpoints.json 多 urlPrefix 含 ctcc/cucc ISP 镜像 + 推荐 flag，无中心服务器）+ 每包 metadata.json（chips/brands/files）；**12 固件族目录 = 生态普查：AT、NLChat 跨芯片 client/server 对（BS21+WS63）、SLE 键盘 dongle+keyboard 对、GFSK、HHD03 AT**。社区商店四类在售固件 = AT/聊天/HID/GFSK。**sync 100 收官：123 份报告，boot-ROM 协议四重验证、三层主机协议深度、四种 mesh、CS 全链、agent 矩阵四方 — 下一百轮开放位：dli 余族/HADM 多锚/Nld 参数布局/AIOT 深挖。**

## 一百一十三、OKF 时代猎收（无限 harvest 同步 101，OHOS 主仓 0915 增量，198→199 concepts）

- `NEW-OHOS-SERVICE-0915-DELTA.md`：主仓 59b50c4→7068bc4 三合并（!254 br1 / !256 qhz-fork-0911 / !257 appexecfwk-base-deps）— **QOSM 降级超时 4000→5000ms（升级 1000ms 不动，快升慢降 hysteresis 实证）**；`nearlink_service_common` 显式依赖 `appexecfwk_base`（传递依赖转显式）；ASC 音频单测 **mock 全符号 hidden 可见性隔离（.so 动态解析劫持根因 + 析构 mock）** — dongle 主机栈单测可直接复用的隔离模式。fork-merge 节奏延续（个人 fork 分支仍是正常合入载体）。

## 一百一十四、OKF 时代猎收（无限 harvest 同步 102，保鲜轮 0916，199→200 concepts）

- `NEW-FRESHNESS-ROUND-0916.md`：teki128 增量 c5cb7ae（删 write-only `link_ready`，`g_conn_id` 单一状态 token + 断开三连：清 id→解配→重扫）— dongle 主机栈禁并行布尔的公开实证；7 热仓 pull 全 current（sle_mesh=BH4ME、web-flasher=GitCode hinearlink、qemu=hispark-rs 三处 owner 纠偏）；**方法论：pushed_at 只触发、pull 才裁决**（pet-collar 跨 ref 推送误报）。

## 一百一十五、OKF 时代猎收（无限 harvest 同步 103，撞名裁决批，200→201 concepts）

- `NEW-NAME-COLLISION-VERDICTS-0916.md`：9 月窗口五 hits 全撞名（THRIVE36 本地服务落地页/jaegermichael+ Toshakarp Vite-TS/Ss2809 Vercel Node 后端/Heebu Flutter 二次确认）— **10 次 API 零克隆定级**；裁决梯：语言→根目录→manifest→克隆；`vercel.json`/`vite.config.ts`/`pubspec.yaml`/落地页 shops-rides-delivery 词表见两项即判。
## 九十九、OKF 时代猎收（无限 harvest 同步 104，HopeRun WS63 SLE demo curriculum，201→202 concepts）

- `NEW-HOPERUN-WS63-SLE-DEMO-CURRICULUM.md`：WS63V100 双板课程把 name-filtered seek、MTU/配对、FIND、CCCD、notify 与 UART 转发串成完整冒烟链；AHT20 示例再以连接态门控周期采样，适合作为 dongle 主机栈的 SSAP 互操作对照，但文本 NUL 与全局缓冲不能直接当传输协议。

## 一百一十七、OKF 时代猎收（无限 harvest 同步 105，LinkNebula evidence correction，202→203 concepts）

- `NEW-LINKNEBULA-RUST-OFFLINE-MESH.md`：旧 LinkNebula 报告的证据纠偏，不计作新发现仓库。273B 外层包、独立 CRC 异或、控制包类型未接入、路由过期未调用、模拟器目标地址不筛选，以及未执行测试的成熟度边界；响应解码已有 11B 最小长度检查。上一条 sync104 的“九十九”为旧临时脚本按匹配节数计数的编号错误，本条恢复既有最高章节号之后的顺序。
