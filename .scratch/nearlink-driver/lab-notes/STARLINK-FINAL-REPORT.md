# 星闪宝子功能探索最终报告 (2026-08-16)

> WS73 三模 dongle (ffff:3733) 星闪/SLE 侧完整功能验证。全程只操作 1-5 星闪口、零编译、隔离遵守。

## 一、打通的关键链路

1. **固件下载**（票 01/05/08 完整）：驱动自动触发，三文件灌入 → kernel 5EP 态
2. **HCC 数据通道**（本轮破局）：BSP_READY 早于 hcc init 的时序竞态 → `hcc_switch_status(HCC_ON)` 静默失败 → hcc_state 卡 FORBID → `0x80003033`。**修复 = 驱动重载**
3. **SLE HCI 方言**（本轮破局）：DLI 风格帧格式，14+ 命令全响应

## 二、已验证全命令面（status 0x00 成功）

| 面 | 命令 | 证据 |
|---|---|---|
| 控制 | READ_VERSION (version=0x6c/0xa1/0x00)、READ_PHY、SET_PUBLIC_ADDRESS、SET_EVENT_MASK、READ_BUFFER/FEATS | ✅ |
| 广播 | SET_ADV_PARAMS(49B)/DATA/ENABLE；**251B 满容量**、8 组广播集 | ✅ |
| 扫描 | SET_SCAN_PARAMS(8B DLI_ScanParam)/ENABLE | ✅ |
| 连接 | CREATE_CONNECTION、DISCONNECT、CONNECTION_UPDATE、READ_REMOTE_VERSION | ✅ 异步 |
| 测距 | READ_MEASURE_CAPS、SET_MEASURE_EN | ✅ |
| 数据链路 | **ICB_DATA_PATH、CREATE_IOB、SET_IOG_PARAM**（星闪独有同步/异步/数据通道） | ✅ |
| 安全 | READ_CRYPTO_ALGO（3 算法）、ENABLE/DISABLE_ENCRYPTION | ✅ |

## 三、关键协议知识

- 命令帧 `[A1][opcode u16 LE][plen u16 LE][params]`
- 事件帧 `[A2][hdr][num_pkts=01][opcode 回显][status][return...]` — status 0x00 成功
- DLI_AdvParam 49B / DLI_ScanParam 8B 布局（见 SLE-CONTROL-PLANE.md）
- 广播分片: COMPLETE=0 FIRST=1 INTERMEDIATE=2 LAST=3
- 错误码: 0x1E=UNKNOWN_ADVERTISING_IDENTIFIER(未配 handle) 0x0F=INVALID_PARAMS
- 设备空口静默（60s 扫描零事件）；自扫不回报自己

## 四、未完成（需对端设备）

- **广播可被发现性实锤**：需第二 dongle（hcc 单实例限制，票 09 hack）或星闪手机
- **连接建立 + 数据面传输**：需对端
- **测距实测**：需对端

## 五、工具（已入库 scripts/ws73-probe/）

- ws73-probe.c（固件下载）、kernel-observe/kernel-init（内核态）
- sle-hci-scan.py（命令方言扫描）、sle-adv.py（广播+扫描序列）

## 结论

星闪宝子控制面/能力面**全功能可编程**（唯一缺对端实连）。这是从零驱动 WS73 星闪的完整技术手册，为电视盒三模落地（票 10）与双 dongle 对测（票 09）提供了全部前置知识。
