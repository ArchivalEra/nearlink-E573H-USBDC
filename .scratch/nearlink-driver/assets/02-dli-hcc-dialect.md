# 02 — DLI ↔ WS73 HCC 方言对照

Sources:
- A) OpenHarmony stack: `/mnt/hdd/nearlink-stuff/communication_nearlink_service` (base `<OH>`)
- B) WS73 SDK: `<SDK>` = `/home/archivalera/plum/zcode-projects/nearlink/sdk/ws73_sdk_linux_WS73_1.10.110`

## Verdict (TL;DR)

**(b) 需要一个转换层——但不是“协议转换”，而是一个很薄的传输适配层。**

- DLI 帧（0xA1/0xA2/0xA3/0xA4 打头）**逐字节原样**作为 HCC 的 payload 放进 hcc 包就能送达 controller——HCC 头是传输复用头，DLI 帧是协议帧，两层正交、不冲突。
- 关键证据：DLI 的 4 个 datatype 字节（`DLI_DATATYPE_CMD=0xA1 / EVENT=0xA2 / ACB=0xA3 / ICB=0xA4`，`<OH> services/stack/src/dli/interface/dli_layer_stru.h:81-86`）与 WS73 侧 `HCI_DATATYPE_SLE_CMD=0xA1 / EVENT=0xA2 / ACB=0xA3 / ICB=0xA4`（`<SDK> driver/bsle/sle_driver/sle_chba/sle_hci_chba_proc.h:19-24`）**完全一致**；数据帧 TCID 定位 `HCI_DATA_TCID_POS=5`（`sle_hci_chba_proc.h:11-12`）与 DLI 5 字节数据头 + DTAP 头首字节=tcid 的布局吻合。
- WS73 主机侧驱动**不解析、不构建**任何 SLE HCI 命令——`/dev/hwsle` 写路径、`sle_hci_send_frame`、`hcc_bt_tx_data` 都是“不透明帧搬运”（opaque pass-through）。opcode 解释发生在固件侧（BTC/CHBA），不在主机驱动源码里。
- 因此 Phase 3 的接入点是把 OpenHarmony 栈的 6 函数 HAL（`SleHalInit/SleReset/SleSendDliPacket/SleHalClose/GetDliVersion` + `hciPacketReceived` 回调）的 HDI 后端替换成 `/dev/hwsle` 的 open/write/read/poll，并处理三个小差异：(1) RX 侧 type 字节约定；(2) `initializationComplete` 的合成；(3) 生命周期（open→`H2D_MSG_SLE_OPEN` 已由内核驱动自动处理）。
- 唯一无法从源码验证的残余风险：WS73 固件侧 HCI 解析器是否逐 opcode 匹配 DLI 的 0x0401/0x0C02/0x1401 编号。SDK 主机侧无固件 HCI 源码；但 datatype 常量一致、帧头布局一致（1B type + 2B opcode + 2B len）、TCID 位置一致，强烈提示同一套海思 SLE-HCI 方言。

---

## 1. Command framing：DLI 5 字节头 vs HCC 5 字节头

两层不是同一个东西：DLI 头是 **SLE-HCI 协议帧头**，HCC 头是**传输复用头**（挂在 DLI 帧外面）。92 字节 `HIUSB_PACKAGE_HEARDER_SIZE` 是更底层的 USB 总线散射头，与协议无关。

### 1.1 DLI 帧格式（线格式，字节序 LE）

| 字节 | 字段 | 说明 | 证据 |
|---|---|---|---|
| 0 | type | 0xA1=CMD / 0xA2=EVENT / 0xA3=ACB / 0xA4=ICB | `dli_layer_stru.h:28,81-86` |
| 1-2 | opcode (LE) | 命令 opcode，如 0x1401=DLI_CREATE_CONNECTION | `dli_layer.c:133-135`; `dli_opcode.h:252` |
| 3-4 | payload len (LE) | `parLen` | `dli_layer.c:135` |
| 5+ | payload | 命令参数（`DLI_CmdStru.par[]`） | |

数据帧（ACB/ICB，`DLI_DataStru`，`dli_layer_stru.h:73-79` + `dli_layer.c:296-305`）：

| 字节 | 字段 | 说明 |
|---|---|---|
| 0 | type | 0xA3=ACB / 0xA4=ICB |
| 1-2 | bit15=prio, bit14=ts, bit13-12=pbFlag, bit11-0=lcid | pbFlag: 0=full 1=first 2=middle 3=last |
| 3-4 | payload len | |
| 5+ | DTAP 帧 | 首字节 = tcid（见 §4） |

事件帧（RX）：type 由 `SlePacketType` 参数单独携带（`SleDliCallbacks.cpp:57-70`），data 内是 4 字节 `event(2)+len(2)` + 参数（`dli_data_stub.c:45,152-162`；`dli_layer.c:577-586` 剥 `DLI_HEADER_WITHOUT_TYPE_SIZE=4`）。

### 1.2 HCC 包格式

| 层 | 头 | 大小 | 说明 | 证据 |
|---|---|---|---|---|
| HCC 逻辑层 | `hcc_header { sub_type:4, service_type:4, queue_id:8, pay_len:16 }` | 5 B | `hcc_comm.h:123-128`；`hcc_bt_tx_data` 前置此头并填 `service_type=HCC_ACTION_TYPE_SLE(0xA), sub_type=0, queue_id=SLE_DATA_QUEUE(8), pay_len=len` | `hcc.c:1012-1069` |
| USB 总线层 | `usb_package {msg_type,len,reserve}` + `aggr_len[24]` | 92 B (`HIUSB_PACKAGE_HEARDER_SIZE`) | 散射/聚合描述头，`hcc_usb_host.c:328,394`；`hcc_bus_usb_comm.h:17-19`；`hcc_usb_host.h:272-276` |

**结论：DLI 5 字节头 与 HCC 5 字节头 完全可比但不同层**——DLI 帧原样塞进 HCC 的 payload 区（`hcc_bt_tx_data` 直接 `memcpy(buf+sizeof(hcc_header), data_buf, len)`，`hcc.c:1050`），零转换。92 字节 USB 头是 bus 层，与协议无关，双方不接触。

## 2. Command set：DLI opcode vs WS73 hcc 消息

### 2.1 DLI opcode（OpenHarmony 侧，主机构建）

`DLI_CmdOpcode`（`<OH> services/stack/src/dli/interface/dli_opcode.h:228-297`）：

| OGF 分组 | opcode | 命令 |
|---|---|---|
| 0x04 (controller 基础) | 0x0401-0x040D | SET_EVENT_MASK / READ_LOCAL_BUFFER / SET_PUBLIC_ADDRESS / RESET / ACCESS_FILTER_LIST… |
| 0x0C (广播) | 0x0C02-0x0C08 | SET_ADVERTISING_PARAMETERS / DATA / SCAN_RESPONSE / ENABLE / REMOVE_SET… |
| 0x10 (扫描) | 0x1001-0x1003 | SET_SCAN_PARAMETERS / ENABLE / DATA |
| 0x14 (连接) | 0x1401-0x1403 | CREATE_CONNECTION / CREATE_CONNECTION_CANCEL / DISCONNECT |
| 0x18 (连接管理) | 0x1801-0x180C | READ_REMOTE_FEATURES / VERSION / SET_PHY / CONNECTION_UPDATE / SET_MCS / READ_REMOTE_RSSI / SET_CONTROLLER_DATA… |
| 0x1C (安全) | 0x1C01-0x1C28 | ENCRYPT / RANDOM / ENABLE_ENCRYPTION / IMG_ENCRYPTION… |
| 0x20 (测距) | 0x2001-0x2005 | READ_LOCAL/REMOTE_MEASURE_CAPS / SET_MEASURE_CONFIG… |
| 0x28 (组播/同步) | 0x2801-0x280E | SET_IOG_PARAM / CREATE_IOB / SET_IMG_PARAM / CREATE_IMB / ICB_DATA_PATH… |

事件 opcode：`DLI_EventOpcode`（`dli_opcode.h:166-226`），如 0x0001 STATUS / 0x0002 COMPLETE / 0x0015 CONNECTION_COMPLETE / 0x001A ADVERTISING_REPORT。

### 2.2 WS73 hcc 侧

- **HCC 消息**（`H2D_MSG_*`，`<SDK> driver/platform/drv/device/romable/include/hcc_cfg_comm.h:93-127`）：`H2D_MSG_SLE_OPEN=29`、`H2D_MSG_SLE_CLOSE=30`、`H2D_MSG_BT_OPEN=25`、`H2D_MSG_FLOWCTRL_ON/OFF`…这些是 **HCC 传输层的开关/PM/流控控制消息**，不是 SLE 链路命令。SLE 业务使能 = `/dev/hwsle` open → `pm_sle_enable()` → `bsle_open_close_cmd(PM_SVC_SLE, TRUE)` → `hcc_send_message(HCC_CHANNEL_AP, H2D_MSG_SLE_OPEN, HCC_ACTION_TYPE_TEST)`（`sle_dev.c:90-119`；`plat_pm_wlan.c:459-487`）。
- **HCC 通道/队列**：`HCC_CHANNEL_AP=0x2`（没有 `HCC_CHANNEL_SLE`；`hcc_cfg.h:37-43`），SLE 业务跑在 AP 通道的 `SLE_DATA_QUEUE=8`（`hcc_cfg_comm.h:41-56`；`hcc_cfg.c:99-120`）。`hcc_service_type` 的 `HCC_ACTION_TYPE_SLE=0xA`（`hcc_cfg_comm.h:37`）。
- **SLE HCI 帧路由**：`sle_hci_send_frame(data_buf, len)` → `hcc_bt_tx_data(HCC_CHANNEL_AP, data_buf, len, {service_type=SLE, queue_id=SLE_DATA_QUEUE})`（`sle_hcc_proc.c:107-141`）。**data_buf 是什么，就传什么**——不解释、不重排。

### 2.3 结论

**DLI 命令可以直接包进 hcc 包送达 controller**。WS73 主机驱动是“帧搬运工”：`/dev/hwsle` → `sle_dev_tx_proc` → `sle_hci_send_frame` → hcc payload，全程不解析 opcode。命令集是否匹配由**固件侧**（BTC/CHBA）决定，SDK 不含固件 HCI 源码，故 opcode 级匹配只能在上硬件后验证；但 datatype 字节一致 + 帧头布局一致，是同一套 HCI 方言的有力证据。`H2D_MSG_*` 与 DLI opcode 不在同一维度：前者是“把 SLE 传输打开”，后者是“做 SLE 链路操作”。

## 3. HAL 缝：SleSendDliPacket 上线的字节 vs /dev/hwsle 期望

### 3.1 OpenHarmony 侧（`SleDliLayerAdapter.cpp:129-149`）

```cpp
int SleSendDliPacket(const SlePacket *packet) {
    std::vector<uint8_t> data;
    data.assign(packet->data, packet->data + packet->size);
    uint32_t type = static_cast<uint32_t>(data[0]);   // 0xA1
    SleDliSnoop::GetInstance().DliSnoopCapture(type, data, false);
    return g_iSleDli->SleSendHciPacket(data);          // HDI 透传
}
```

即：**DLI 帧字节（含 0xA1 type 字节）原封不动经 HDI 送下去**。RX 侧 `SleDliCallbacks::hciPacketReceived(uint32_t type, const vector<uint8_t>& data)` 把 type 单独透传回栈（`SleDliCallbacks.cpp:57-70`）。

HAL 6 函数契约（`SleDliLayerAdapter.h:61-74`）：`SleHalInit(callbacks)`、`SleReset()`、`SleSendDliPacket(packet)`、`SleHalClose()`、`GetDliVersion()` + 回调 `initializationComplete(SleInitStatus)`、`dliPacketReceived(SlePacketType, const SlePacket*)`。

### 3.2 WS73 侧（`sle_dev.c:121-163`）

```c
static ssize_t sle_misc_dev_write(...) {
    if (type != SLE_TYPE_UNKNOWN) {           // type 静态 = SLE_TYPE_UNKNOWN，无 setter → 恒不成立
        tx_buf[0] = type; copy_from_user(tx_buf+1, buf, count); g_sle_dev.tx(tx_buf, count+1);
    } else {
        copy_from_user(tx_buf, buf, count);   // 原始字节直通
        g_sle_dev.tx(tx_buf, count);
    }
}
```

`type` 是 file-static 且从未被改写（无 ioctl setter），所以 `/dev/hwsle` 的写路径是**原始字节直通**：`g_sle_dev.tx` → `sle_dev_tx_proc`（`sle_host_register.c:49-57`）→ `sle_hci_send_frame` → hcc。

### 3.3 对比

| 维度 | SleSendDliPacket | /dev/hwsle write |
|---|---|---|
| 线上字节 | DLI 帧原样（首字节 0xA1） | 期望原始 HCI 帧（首字节 0xA1）——**一致** |
| 前置处理 | 无 | 无（type=UNKNOWN 直通） |
| 生命周期 | SleHalInit → HDI 服务 | open → `pm_sle_enable()` → `H2D_MSG_SLE_OPEN`（内核驱动自动做） |

**唯一差异在 RX 约定**：OHOS 侧 `hciPacketReceived` 把 type 字节与 data 分离（`SleDliCallbacks.cpp` 回调签名 `uint32_t type`），WS73 侧 `/dev/hwsle` read 返回设备发来的完整帧（含 type 字节；例：DFR 事件 `sle_dfr_event = {0xa2, 0xaa, 0xfc, 0x02, 0x00, 0x00, 0x00}` 首字节就是 0xA2，`sle_host_register.c:14`）。适配层在读路径上把首字节拆出来当 `type` 即可（约几行代码）。TX 无需任何变换。

## 4. Data plane：DTAP vs hcc data channel

- **DTAP（OpenHarmony 栈内）**：`dp/dtap` 模块，`DTAP_BasicHeader { tcid:8, frameType:4+ocpf bits:4, length:16 }`（`<OH> services/stack/src/dp/dtap/include/dtap_frame.h:108-121`）。TCID 语义：`TCID_SLE_CMTC=0x02`、`TCID_SLE_CUTC=0x1F`、动态单播 0x80-0xDF（`dtap_tcid.h:31-57`）。DTAP 帧作为 ACB/ICB 的 payload 承载。
- **WS73 侧**：`HCI_DATA_TCID_POS=5`（`sle_hci_chba_proc.h:11-12`）——数据帧第 6 字节读 tcid；`sle_link_info_t { lcid, tcid, addr }` 维护 lcid↔tcid 映射（`sle_data_type.h:16-20`；`sle_hci_chba_proc.c:12-31`）。CHBA 用 datatype 0xA3/0xA4 过滤自己的数据帧（`sle_hcc_proc.c:88-91`：`sle_hci_send_to_chba(buf,len)==0` 时才继续上抛）。
- **映射**：DLI ACB 数据帧 = 5B DLI 头 + DTAP 帧（tcid 在 payload 首字节=第 6 字节），与 `HCI_DATA_TCID_POS=5` 精确吻合；hcc 的 `SLE_DATA_QUEUE` 只是承载队列，不区分控制/数据。**DTAP 帧 1:1 直接进 hcc payload，无转换**。CHBA 侧负责 tcid/lcid 到固件链路的映射，主机侧不动。

## 5. Verdict

**(b) 需要转换层，但形态是「传输适配器」，不是「协议转换器」。**

需要转换的 3 个点（都是 HAL 缝层面的胶水）：
1. **RX type 约定**：`/dev/hwsle` 读回完整帧（含 0xA2/0xA3 首字节）→ 拆出首字节作为 `SlePacketType`，余下交给 `dliPacketReceived`；TX 原样写。
2. **initializationComplete 合成**：OHOS 栈 `DLI_SapiInit` 会等 `SleHalInit` 触发的初始化完成信号（`dli_sapi.c:43-78`）；适配层在 `/dev/hwsle` open 成功后回调。
3. **生命周期映射**：`SleReset()` 在 OHOS 里是 `kill(SIGKILL)`（`SleDliLayerAdapter.cpp:123-126`）——若需软复位，映射到 `/dev/hwsle` close+reopen（触发 `H2D_MSG_SLE_CLOSE`/`H2D_MSG_SLE_OPEN`）；`GetDliVersion` 返回 DLI_VERSION_1_0 即可。

**不需要转换的东西**：DLI 命令集（0x1401 等）原样作为 hcc payload 发送；DTAP 数据原样承载；datatype 字节、帧头布局、TCID 位置全部一致。

**残余风险（硬件验证项）**：WS73 固件侧 SLE HCI opcode 是否与 DLI 的 0x0401/0x0C02/0x1401 编号逐一匹配。SDK 不含固件 HCI 源码，此项只能在 Phase 4 用实机（`usbmon` 抓 `/dev/hwsle` 写出的帧 vs 设备回帧）验证。datatype 常量、帧头布局、TCID 约定三者一致是本结论的主要依据。

关键证据索引：
- `dli_layer_stru.h:28,81-86`（DLI_HEADER=5、datatype 0xA1-A4）
- `dli_layer.c:129-137,296-305`（CMD/数据帧组装）
- `dli_opcode.h:228-297`（DLI_CmdOpcode）
- `SleDliLayerAdapter.cpp:129-149`、`SleDliCallbacks.cpp:57-70`（HAL 透传/type 分离）
- `hcc_comm.h:121-129`（hcc_header）、`hcc.c:1012-1069`（hcc_bt_tx_data 前置头+原样拷贝）
- `sle_hcc_proc.c:107-141`（sle_hci_send_frame）、`sle_dev.c:121-163`（写路径直通）
- `sle_hci_chba_proc.h:11-12,19-24`（HCI_DATA_TCID_POS=5、HCI_DATATYPE 0xA1-A4）
- `hcc_cfg_comm.h:93-127`（H2D_MSG_SLE_OPEN=29）、`plat_pm_wlan.c:459-487`（open→SLE_OPEN 消息）
- `dtap_frame.h:108-121`、`dtap_tcid.h:31-57`（DTAP 头/tcid）
