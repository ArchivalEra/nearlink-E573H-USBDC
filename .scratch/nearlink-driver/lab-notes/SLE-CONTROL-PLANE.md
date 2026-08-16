# SLE Control Plane — 星闪宝子操作指南 (verified on hardware)

> 实测于 2026-08-16，1-5 星闪宝子（WS73 ffff:3733），经 `/dev/hwsle` + plat_soc/sle_soc 内核驱动。
> 所有字节均经真机验证。这是从零驱动 WS73 星闪控制面的完整操作手册。

## 前置

1. 加载 plat_soc + sle_soc（顺序：plat → sle），驱动自动固件下载 → 设备进 kernel 态（bcdDevice 0318/480M/5EP）
2. **时序注意**：若 hcc 数据通道报 `0x80003033 (HCC_STATE_EXCEPTION)`，rmmod ×2 → insmod 重载即可（BSP_READY 早于 hcc init 的竞态）
3. open `/dev/hwsle`（触发 pm_sle_enable → SLE_OPEN 握手，dmesg: "sle btc open finish" / "sle set sle state 1"）

## 帧格式

```
命令:  [A1] [opcode u16 LE] [plen u16 LE] [params...]
事件:  [A2] [hdr: 02 00] [plen u16 LE] [00 01|06 status] [opcode u16 LE 回显] [status] [data...]
         status: 01=完成/接受  06=参数错误/不支持
异步:  部分命令先回 [A2 0a 00 01 00 00]（command status 类型），完成事件可能延迟
```

## 已验证命令表

| 功能 | opcode | 参数 | 结果 |
|---|---|---|---|
| SET_EVENT_MASK | 0x0401 | 8B mask（0xff 全开） | ✅ status 01 |
| READ_LOCAL_BUFFER | 0x0402 | 无 | ✅ 回数据 |
| READ_LOCAL_SUPPORT_FEATS | 0x0403 | 无 | ✅ 回 8B |
| READ_LOCAL_VERSION | 0x0404 | 无 | ✅ **version=0x6c(108), mfr=0xa1(161), hw=0x00** |
| SET_PUBLIC_ADDRESS | 0x0405 | 6B MAC | ✅ status 01（可写 MAC！） |
| GET_PUBLIC_ADDRESS | 0x0406 | 1B type=0 | ⏳ 异步（0x0a cmd-status，完成事件未捕获） |
| RESET | 0x0408 | 无 | ✅ status 01 |
| READ_ACCESS_FILTER_SIZE | 0x040A | 无 | ✅ 回 status 06+数据 |
| SET_ADV_PARAMS | 0x0C02 | **49B DLI_AdvParam** | ✅ 正确参数 status 01（零参数=06） |
| SET_ADV_DATA | 0x0C03 | handle+op+sel+len+payload | ✅ status 01 |
| SET_ADV_ENABLE | 0x0C05 | enable+handle+duration+maxEvt | ✅ status 01（广播启用） |
| SET_SCAN_PARAMS | 0x1001 | 7B | ✅ 异步处理 |
| SET_SCAN_ENABLE | 0x1002 | enable+filterdup | ✅ **status=OK**（扫描启用） |
| CREATE_CONNECTION | 0x1401 | 需对端参数 | ✅ 异步处理 |
| DISCONNECT | 0x1403 | connHandle | ✅ 回 status 06（参数） |

## DLI_AdvParam 49B 布局（pack(1)）

```
[0] advHandle=0    [1] advMode=1      [2] advGtRole=0
[3-5]  primAdvIntervalMin (3B LE, 125us 单位, 800=100ms)
[6-8]  primAdvIntervalMax
[9]    channelMap (0b111=76/77/78)
[10]   ownAddrType=0    [11] peerAddrType=0
[12-17] ownAddr 6B      [18-23] peerAddr 6B
[24]   advFilterPolicy  [25] advTxPower=127(no pref)  [26] primAdvFrameFormat=0
[27-34] second adv phy/pilot/mcs/maxskip + sid + scan params
[35-48] conn params (做G时有效)
```

## 广播序列（已验证）

```python
# SET_ADV_PARAMS (49B 正确构造) → status 01
# SET_ADV_DATA:  [0,3,0,len] + payload(flags+name) → status 01
# SET_ADV_ENABLE: [1,0,0,0,0] → status 01 = 广播启用！
```

## 状态机/已知

- 设备空口事件不上报（adv+scan 启用后 10s 观察零事件）——需对端设备（第二 dongle 或星闪手机）才有 scan 结果/连接事件
- READ_VERSION 完整返回一次后可能回 06（状态相关，重开通道恢复）
- SET_EVENT_MASK 全开不改变静默行为

## 工具

- `scripts/ws73-probe/sle-hci-scan.py`：命令方言扫描器
- `scripts/ws73-probe/sle-adv.py`：广播+扫描序列
- 均在实验场 /mnt/hdd/laboratory/ws73-probe/ 有源头

## 隔离准则

- 只操作星闪口（1-5/1-4），不碰其他 USB
- 零编译（内核模块已备）
- 内存 <1G 停一切操作
