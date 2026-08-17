# NearLink_controller — 星闪游戏手柄固件/协议设计深挖

**Date:** 2026-08-17
**Scope:** Read-only local inspection of `/mnt/hdd/nearlink-stuff/NearLink_controller/` (no network/build/hardware).
**Companion:** `COMMUNITY-PROJECTS.md` (prior profile), our `stack/ssap/`.
**Confirms prior profile (COMMUNITY-PROJECTS §3/§C) and adds:** full source-level breakdown of frame aggregation / dirty retry / reconnect semantics, the spec-vs-implementation discrepancy on reconnect replay, HarmonyOS ABS_* mapping detail, and a concrete host-side composition plan with `stack/ssap/`.

---

## Sources

| 物 | 路径 |
|---|---|
| Project root | `/mnt/hdd/nearlink-stuff/NearLink_controller/` |
| README / AGENTS | `README.md`, `AGENTS.md` |
| P0–P4 specs | `docs/superpowers/specs/2026-07-1{7,8}-*-design.md`, `2026-07-27-p2-driver-layer-design.md`, `2026-07-28-p3-transport-layer-design.md`, `2026-07-28-p4-harmonyos-test-app-design.md` |
| Research | `docs/research/0{1,2,3,4}-*.md` |
| Firmware source | `firmware/src/{controller,transport,driver,logic,config}/**` |
| Firmware tests | `firmware/test/test_{controller,transport}_contract.c`, `test_mocked_end_to_end.c`, `run_tests.ps1`, `Makefile` |
| Host app | `host/entry/src/main/ets/{common,input,haptic,features}/**` |
| 我方对照 | `stack/ssap/include/ssap_link.h`, `stack/ssap/src/{ssap_link.c,ssap_server.c,hwsle_transport.c}` |

Git: local snapshot has a single squashed commit (`git log --oneline --all` = 1). Full history not inspectable.

---

## 1. 项目定位

**不是遥控器，是游戏手柄（gamepad）参考设计。**

- 定位: 基于星闪（SLE）的**开源游戏手柄参考设计**，对标 Betop G6 Pro 级产品，目标 2K–4KHz 轮询（`README.md:3,9`）。AI-agent 主导开发（`AGENTS.md:7,116`）。
- 芯片: 海思 **Hi2821**（利尔达 **EB21** 模组 L-NLEEB21-G5PP4-DK01），RISC-V 64MHz / SRAM 160KB / Flash 1MB / SLE 1.0 12Mbps / USB 2.0（`docs/research/03-eb21-chip-spec.md:16-33`）。KEYSCAN 硬件矩阵、12-bit ADC 6 通道（LX,LY,RX,RY,LT,RT）+VBAT+temp。
- 目标平台: **HarmonyOS 26.0.0 / API 26**（Pura 80 Pro），原生游戏手柄 HID 接入，USB 有线 8K（`README.md:13-14`; P4 spec:17）。
- 阶段: P0–P4 已合并；**无进行中阶段**，下一步（真实硬件集成、真实 OsHidInputSource/DeviceHapticSink、端到端时延）被硬件 gate（`AGENTS.md:116`）。
- 出货证据: Betop G6 Pro 同款 Hi2821 已实现 4K 无线轮询（`docs/research/03-eb21-chip-spec.md:62-63`）。

---

## 2. 传输/协议设计（源码级拆解）

核心在 `firmware/src/controller/controller.c`（171 行）+ `transport/transport.h` + P3 spec。

### 2.1 帧聚合（Frame aggregation）

每个 `controller_poll()` 调用处理**一帧**，非阻塞（`controller.c:50-61`, P3 spec D3:49-53）。聚合=把 ADC（6 通道）+ KEYSCAN（16-bit）一次采样快照合并成**一个 15 字节 HID 报告**（`controller.c:93-146`）。

- **seq 去重**: 每个 driver 返回单调递增 `seq`；`adc_seq != last_adc_seq || ks_seq != last_ks_seq` 才算新帧（`controller.c:108-118`）。seq 未变 → 空帧：不采样处理、不发送、只更新省电计时（`controller.c:115-118`）。单源更新（只 ADC 或只 KEYSCAN 变）也算新帧。
- **首帧规则**: `have_last_frame=0` 时强制生成并发送（`controller.c:109-111`）。
- **非阻塞强制**: `adc_sample`/`keyscan_scan` 以 `timeout_ms=0` 调用；返回 `DRIVER_E_AGAIN` → 空帧（`controller.c:93-97,100-104`）。
- **三份报告缓存**（语义严格区分，勿合并，P3 spec D5:76-79; `controller.h:30-33`）:
  - `last_processed_report[15]` — 最近一次 P1 处理结果，用于空闲/省电字节比较；**帧生成即更新**（即使发送失败）。
  - `last_sent_report[15]` — 最近一次**成功发送**的报告，备重连回放。
  - `pending_report[15]` + `pending_adc_seq/pending_ks_seq` — 发送失败待重试。
- **caller 报告缓冲契约**: `report[15]` 只在**发送成功**（正常或脏重试）时写入；空帧/错误/INACTIVE 时保持不变（`controller.c:88,167`; 契约 #21, P3 spec:570）。
- `frame_id` 仅内部计数，每次成功发送 +1，不写进任何帧（`controller.h:29`; `controller.c:86`）。

### 2.2 脏重试（Dirty retry）

- **触发**: `transport_send_report` 失败（非 OK）→ `dirty=1`，保存 `pending_report + pending_adc_seq + pending_ks_seq`，返回错误码（`controller.c:154-161`）。
- **重试时机/退避**: 下一次 `controller_poll` 的**第一步、采样之前**重试 pending_report（`controller.c:74-90`）——"重试优先，不消费新采样数据"。**没有退避定时器**：轮询节拍（250µs/4K 声明值）本身就是节流；失败时 `dirty` 保持 1，错误码透传（契约 #17-19, P3 spec:566-568）。传输未连接时脏重试直接跳过发送、dirty 保留（`controller.c:76-80`）。
- **重试成功提交**: `dirty=0`、pending seq 提交为 last seq、`frame_id++`、拷贝到 `last_sent_report` + caller report（`controller.c:82-89`）。

### 2.3 重连回放状态机（Reconnect replay）

双层正交状态机（P3 spec D6:81-88）:

```
transport_state_t: DISCONNECTED → PAIRING → CONNECTED      (transport.h:17-21)
controller_mode_t: INACTIVE / ACTIVE / POWER_SAVING         (controller.h:19-23)
```

- **断线**: `transport_get_state()!=CONNECTED` → mode=INACTIVE，**绝不调用 transport_send_report**，但仍采样、跑滤波逻辑（保持 filter 状态），`candidate_report` 仍生成并拷入 `last_processed_report`（`controller.c:16-20,149-150`）。
- **重连**: INACTIVE→ACTIVE（`controller.c:22-26`）。
  - **实现与 spec 的差异（重要发现）**: P3 spec 写"重连回放 `last_sent_report`（cached reconnect）或 pending_report"（P3 spec:116,510），**但 controller.c 实际实现是发送新生成的 `candidate_report`**（`controller.c:153-154`）——因为断线期间仍持续采样，`candidate_report` 就是当前最新状态，等价于（且优于）回放旧帧。`last_sent_report` 缓存保留但未用于重连发送。若 `dirty=1`，步骤 1 会先重放 pending_report。
  - 测试 `test_reconnect_send` 只断言重连后发送计数增加（`test_controller_contract.c:152-163`），不校验回放内容。
- **省电**: 15 字节逐字节相同 → `idle_ms` 累加（饱和至 UINT32_MAX）→ 30s 无变化进 POWER_SAVING（`controller.h:15-17`; `controller.c:28-47`）；输入变化即唤醒并**立即发送当前帧**（`controller.c:37-39`）。"怎么省电"（降采样/关外设/IP5306 sleep）未实现，仅状态转换（P3 spec D6:135）。
- **回调契约**: 仅 CONNECTED/DISCONNECTED 两事件；回调只通知、不得在回调内调 transport API（ISR 上下文安全）（P3 spec D7:137-153）。

### 2.4 传输接口契约（transport.h）

统一 SLE/USB 抽象，编译期二选一（P3 spec D2:47）:
```c
transport_init(h, hid_descriptor, len, cfg, cb, user_data)
transport_connect/disconnect/send_report(report[15])
transport_get_state() / transport_get_poll_interval_us()   // SLE/USB 声明值 250µs (4K)
```
22 条不变式契约（P3 spec:280-305），mock 可测。

---

## 3. HID 映射（鸿蒙 4K）

**15 字节报告、无 Report ID、小端**（`hid_report.c:3-31`; P1 spec:346-362）:

| 字节 | 字段 | 说明 |
|---|---|---|
| 0-1 | buttons 11 bit | bit0=A … bit10=Home（`HidConstants.ets:13-23`），mask 0x07FF |
| 2-3 | LX int16 | 左摇杆 X |
| 4-5 | LY int16 | 左摇杆 Y |
| 6-7 | RX→**Z** | 右摇杆 X 上报为 Z 轴（HarmonyOS 专用） |
| 8-9 | RY→**Rz** | 右摇杆 Y 上报为 Rz 轴 |
| 10-11 | LT→**Brake** | Simulation Controls 页 |
| 12-13 | RT→**Accelerator** | |
| 14 | Hat 4-bit | 0-7 方向，8=null |

描述符: Generic Desktop **Game Pad 0x05**，按钮页 1-11，4 轴 X/Y/Z/Rz，Brake/Accelerator，Hat 0x39（`hid_descriptor.c:3-67`）。

**鸿蒙事件映射**（`docs/research/04-harmonyos-gamepad-api.md:19-30`）: `ABS_X/ABS_Y`（左摇杆）、`ABS_Z/ABS_RZ`（右摇杆）、`ABS_GAS`（RT）、`ABS_BRAKE`（LT）、`ABS_HAT0X/ABS_HAT0Y`（十字键）、`BTN_*`。API 15 起原生支持，API 23+ HID Device Profile 支持自定义描述符；**API 26 上行为未复核**（OQ-P1）。

**跨平台风险（重要）**: 右摇杆用 Z/Rz 是鸿蒙专用；Linux 标准手柄驱动期望 ABS_RX/ABS_RY（P1 spec:387-389,992）。电视盒若跑 Linux，需要不同描述符变体或主机侧重映射。

---

## 4. SLE transport stub

- **现在用什么传输**: 没有任何真传输。`transport/eb21/transport_sle_eb21.c` 和 `transport_usb_eb21.c` 全部函数返回 `DRIVER_E_NOSYS`（`transport_sle_eb21.c:8-18`）。测试走 `transport/mock/transport_mock.c`（模拟事件注入、发送状态注入、seq-hold）。`firmware/src/main/` 与 `firmware/src/sle/` **目录不存在**（firmware README 预留但未建）。
- **SLE 留的接口**: `transport.h` 的 7 个函数 + 22 条不变式 + 回调事件模型 + 编译期后端选择。SLE 具体（GATT HID profile、配对流程、连接参数、ISR 上下文）全部标为 Open Question OQ-T1..T5（P3 spec:309-318）。
- **填补路径（我方）**: 它是**芯片内** Hi2821 SLE host（HiSpark Studio / fbb_bs2x SDK），SLE 代码对我们（WS73 主机侧）**不能直接复用**。但对等的是: 我们 `stack/ssap/` 的 `hwsle_transport`（/dev/hwsle 适配）+ `ssap_link`（DLI 建链，`ssap_link.h:1-110`）就是"transport 后端"；`ssap_server`（`ssap_server.h`，notify 经 `send_frame` 发出）就是"OQ-T1 SLE GATT HID profile"的主机侧实现。即: 他们缺的 OQ-T1 = 我们的 `ssap_server` + HID 属性，直接补上。

---

## 5. 电视盒借鉴（具体可抄项）

### 可抄项（按价值排序）

1. **帧聚合/脏重试状态机（controller.c 整段逻辑）** — 给我们的 `ssap_link` 数据面补"可靠上报"层:
   - seq 去重（只在输入变化时上报）
   - `pending_report`+`dirty` 重试优先（采样前先重试失败帧，错误不吞）
   - 三份报告缓存（processed/sent/pending）语义分离
   - caller 缓冲只在成功时写
   - **无退避定时器**——依赖轮询节拍，天然适配我们 Linux 事件循环
2. **重连回放语义（实现版）**: 断线期间保持采样、重连即发最新状态帧。对应我们 `ssap_link` 重连后 SSAP server 主动 notify 当前状态。
3. **省电空闲检测**: 15 字节逐字节比较判空闲 → 停止上报。遥控器比手柄更需要（遥控闲置不发键）。
4. **HID 报告布局 + 描述符**: 15 字节无 Report ID 的紧凑布局作为 HID-over-SLE 负载；鸿蒙 ABS_* 映射表（§3）作为 TV box 若跑鸿蒙的映射蓝本。
5. **非阻塞单帧 `controller_poll` + 注入 tick** — 与我们的 host 事件循环天然契合。
6. **编译期后端注入**（mock vs 真实现，无 vtable）— 与 `hwsle_transport` 抽象一致，PC 可测。
7. **按键/十字键处理**: 4 态去抖 FSM + SOCD Neutral（`P1 spec D11/D12/D13`）— 遥控按键防抖/矩阵防鬼影可直接搬。

### 与 `stack/ssap/` 组合方式

```
遥控端 (Hi2821/BS21E/WS63 芯片内): controller.c 管线几乎可原样用
   输入 → 滤波 → 15B HID 报告 → transport_send_report(SLE HID / SSAP notify)

WS73 dongle + 我方 host 栈（接收端）:
   /dev/hwsle → hwsle_transport → ssap_link (DLI 建链, ssap_link.h)
   → ssap_server (HID 服务, ssap_server.h) → 帧聚合/去重/重连回放
      （把 controller.c 的聚合状态机搬到 notify 接收侧）
   → uinput / evdev 注入电视盒内核输入子系统
```

关键: `controller.c` 的脏重试/重连逻辑**两端通用**——芯片端用它在发送侧保证低丢帧，我们主机侧用它在 SSAP notify 接收侧做去重与重连状态恢复。我们 `ssap_link` 目前只有 DLI 命令级重试（0x1804 retry，`ssap_link.c:234`），**缺应用数据可靠上报层**——这正是 NearLink_controller 状态机补的位置。

### 现实约束

- tp78_v3（BS21 SLE 2K 键盘+dongle，无源码 fwpkg）和 Betop G6 Pro（同芯片 4K 手柄）已证明 **HID-over-SLE 产品路线成立**（COMMUNITY-PROJECTS §9）。
- 电视盒遥控 = 该路线的"少按键 + 长待机"变体: 手柄的摇杆/扳机处理砍掉，保留按键防抖/Hat/脏重试/省电；HID 报告可缩小到 <15 字节但保持同一布局风格。

---

## 6. 局限

- **无法编译出可用固件**: 所有 `driver/eb21/*` + `transport/eb21/*` 为 NOSYS stub（全仓 21 处 NOSYS）；无 `src/main/`（无 RTOS 调度）；firmware 构建需 HiSpark Studio + fbb_bs2x SDK + EB21 硬件，repo 内无 CMakeLists.txt（firmware README:24 说 P2+ 才有）。**只有 PC 侧 Unity 测试可跑**（P4 spec:13 称 P1–P3 共 212 用例通过）。
- **无任何 SLE/USB 真实现**: 无配对流程、无连接参数、无 HID GATT profile、无 USB 端点配置（OQ-T1/T2）。
- **Host 应用未接通硬件**: `OsHidInputSource`/`DeviceHapticSink` 是抛异常的占位（`OsHidInputSource.ets:4-16`）；instrumented 测试未执行（无模拟器/设备）；LatencyMeter 只测应用内时延（`host/README.md:72-85`）。
- **HID 映射鸿蒙专用**: Z/Rz 右摇杆与 Linux ABS_RX/ABS_RY 不兼容，需描述符变体（P1 spec:388）。
- **快照单提交**: 本地 git 只有 1 个 squashed commit，历史/演进不可查。
- 无 IMU（Phase 2 延迟）；无 IP5306 电源硬件代码（"怎么省电"未定义）。

---

## 7. 结论

**电视盒 + 星闪遥控可行路径：HID over SLE，路线已被市场验证；此项目提供可直接抄的数据面状态机。**

- NearLink_controller 是一个**游戏手柄**（非遥控），SLE 传输全为 stub，代码不能直接进我们 WS73 host——但它把"输入→滤波→HID 报告→可靠上报"的**端到端数据面状态机**写得很干净（seq 去重、脏重试优先、三报告缓存、断线不丢状态、重连发最新帧、30s 空闲省电），且全部 PC 可测。
- 对电视盒：**HID over SLE** 路径有出货产品背书（tp78_v3 SLE 2K 键盘+dongle；Betop G6 Pro 同 Hi2821 4K 手柄）；我们把该状态机两处落地即可：芯片遥控端照搬 `controller.c` 管线，WS73 主机侧在 `ssap_link` 之上用同样语义实现 SSAP HID 服务的 notify 去重/重连回放，再经 uinput 进电视盒输入子系统。
- 若电视盒跑鸿蒙，§3 的 ABS_* 映射表可直接用；跑 Linux 则需右摇杆轴映射变体（遥控场景影响小——遥控基本不用摇杆）。

**Open questions**:
1. OQ-T1（SLE GATT HID profile）是我们 `ssap_server` HID 服务的对等物——但作者未实现，需等作者硬件阶段或自建。
2. spec 的"重连回放 last_sent_report"与实现的"发最新 candidate_report"差异——实现版更好，我们采用实现版。
3. 该状态机在 250µs 节拍下的真实性能（OQ-T3 最坏执行时间）需硬件测量。
