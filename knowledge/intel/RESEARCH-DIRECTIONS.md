---
type: intel
title: 研究方向清单 (Research Directions)
language: zh
created: 2026-08-16
tags: []
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
