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

- [05 — ws73.bin 内部格式与签名分析](issues/05-firmware-blob-format.md) — 固件无加密无签名：64B 小写 hex SHA256 头 + 明文负载（`sha256(file[64:])==file[0:64]` 全 8 blob 验证通过）；host 唯一加工 = 剥 64B 头后按 `FILES 1 <addr> <len> <state>` 分块 ≤32KB 原样发到 0x400000（ws73.bin）。Phase 1 工具须先截 `file[64:]` 再按 01 的 FILES 握手发，不能整文件直灌。
- [01 — 固件下载握手字节流精确还原](issues/01-firmware-handshake-spec.md) — 握手 = 纯 ASCII 行命令 + 原始 bulk 流，逐字节规范见 assets/01。顺序：可选 `READM 0x40019380 4`(连通探测,回4字节) → `WRITEM 4 0x40019408 0x<trim>`(回 `WRITEM OK`) → 每文件分块 `FILES 1 0x<addr> 0x<len> 0x<state>`(回 `READY`,停≥5ms,发原始字节,回 `FILES OK`) → `QUIT`(不回包,设备重枚举 5EP,host 等≤5s)。64B SHA256 头只校验不上传。超时: bulk-out 30s / bulk-in 2s / 重试 3 次。待实测: XO trim 值、应答大小写、boot EP 绝对地址。

- [02 — DLI↔WS73 HCC 方言对照](issues/02-dli-hcc-dialect.md) — 需要转换层，但形态是**传输适配器**非协议转换器：DLI datatype 字节（CMD/EVENT/ACB/ICB = 0xA1-0xA4）与 WS73 `HCI_DATATYPE_*` **完全一致**，DLI 帧可逐字节原样作 HCC payload 送达；`/dev/hwsle` 是"帧搬运工"（不解析 opcode）。需适配 3 个 HAL 缝差异（RX 拆 type 字节 / 合成 initializationComplete / 生命周期）。残余风险：固件侧 HCI opcode 编号匹配需实机验证。
- [03 — 开源栈 Linux 移植面清单](issues/03-ohos-stack-port.md) — **GREEN 可行**：核心栈 `services/stack/{cp,dp,dli,sdf,nai}` ~73KLOC 纯 C + POSIX 线程，可干净剥离成 Linux 库，2-4 人周。栈路径只碰 6 个 OHOS 依赖（hilog/hisysevent/securec/init/parameter 纯 stub，openssl 直接保留——SLE 加密走 OpenSSL 非 huks）；HAL 缝 = 5 函数 C ABI（+2 回调）可本地化；SA/IPC/SAMGR 层整体剥掉；最小子集 = stack 六目录 + 重写 hardware 后端。硬耦合：4 处 `kill(SIGKILL)`（含芯片复位路径，需重设计）、/data/log 路径。

- [04 — 传输形态拍板：内核 usb_driver vs userspace](issues/04-transport-form.md) — **内核 usb_driver 最终形态**（SLE 12Mbps=USB FS 线速，userspace 拷贝开销卡吞吐）+ **混合节奏**（Phase 1 userspace 验握手先行，Phase 2 内核定稿 /dev/ws73hci）。

- [07 — kernel 态初始化序列实验（NV 推送 → SLE_OPEN → 通道建立）](issues/07-kernel-init-seq.md) — userspace 盲试到边界：BSP_READY 稳定抓到（INT bit0），INI 推送三种帧格式 CUSTOMIZE_RECEIVED 从未返回，SLE_OPEN 后设备必 panic 断连。根因 = kernel 态需 hcc 框架上下文（service/队列/netbuf），libusb 裸推帧不够。→ 验证票 04 内核形态决策；kernel 态移交票 06。

- [06 — 内核 usb_driver 骨架设计（ws73usb）](issues/06-kernel-driver-skeleton.md) — **x86 移植成功**：SDK `driver/platform` 在 clang21 编译出 plat_soc.ko（EXTRA_CFLAGS→ccflags-y、set_fs 全家 no-op、del_timer→timer_delete、摘诊断通道+stub）；insmod 成功、`wireless_usb` 注册、双设备绑定 boot probe 成功。固件自动下载需 sle_soc+用户态（下一步）。

## Not yet specified

<!-- 还看不清、尚不能成票的雾区；前沿推进后会graduated成票 -->

- **应用形态**：星闪点对点通讯最终做什么（文本 / 文件传输 / HID 外设 / UART 透传）——等 kernel 态通道建立（票 07）后定，Phase 4 范畴
- **测试策略**：通道实验持续（1-4 实验 / 1-5 对照）；usbmon 受限（xanmod debugfs 未开）；双 dongle 对测在 07 通道建立后
- **固件来源合规**：4 blob 全 SHA256 头+明文已确认，分发合规待定

## Out of scope

<!-- 明确排除在目的地之外的工作；永不 graduate -->

- **芯片侧固件开发**（Hi2821/WS63/WS73 固件编写）——本图只做 host 侧驱动与栈
- **WS73 的 WiFi / BLE 功能**——只做 SLE/NearLink 路径
- **自制 dongle / 硬件改造**——用现有 ffff:3733 设备
- **为 OpenHarmony 系统做驱动**——目标是 Linux 主机
