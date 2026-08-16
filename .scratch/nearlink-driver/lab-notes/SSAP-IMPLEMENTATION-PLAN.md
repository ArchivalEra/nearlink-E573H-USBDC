# WS73 上实现 SSAP 用户态栈 — 行动文档

> 基于 OpenHarmony nearlink_service 源码深挖（ssap_pkt.h / ssaps_server.c / ssap_manager.c / dtap.c）
> + 我们已在 WS73 硬件上验证的 HCI 层（SLE-CONTROL-PLANE.md）。2026-08-16。

## 一、SSAP 数据路径全景（源码确认）

```
发送: SSAP PDU → SSAP_Send → SSAP_SendBuffToDTAP(lcid)
      → DTAP_DataSend({lcid, tcid=TCID_SLE_SMTC=0x0A, buff})
      → DTAP 传输层 → DLI → HDI → 硬件

接收: 硬件 → HDI → DLI → DTAP 按 tcid 分发
      → DTAP_RegisterDataRecvCb(TCID_SLE_SMTC, SSAP_Recv)  ← ssap_manager.c:326
```

**关键**: SSAP 固定跑在 **TCID 0x0A (SLE_SMTC)** 上，与我们的硬件发现一致
（WS73 帧 tcid 在 offset 5；CHBA 用 TCID 分流转发）。

## 二、SSAP PDU 格式（ssap_pkt.h，字节级）

- **基础**: PDU = msgCode(1B) + msgCtrl(1B) + 载荷；`SSAP_PDU_BASE_LEN=2`
- **TransType**（msgCode 高位）：CMD=0x01, REQ=0x82, RSP=0x03, NOTI=0x04, IND=0x85, ACK=0x06（REPLY_MASK=0x80）
- **Opcode**：0x01 ERROR_RSP, 0x02/03 EXCHANGE_INFO, 0x04/05 FIND_STRUCTURE, 0x06/07 FIND_BY_UUID,
  0x08/09 READ, 0x0A/0B READ_BY_UUID, 0x0C WRITE_CMD, 0x0D/0E WRITE, 0x0F VALUE_NTF, 0x10/11 VALUE_IND/ACK,
  0x12/13/14 CALL_METHOD
- **EXCHANGE_INFO**: MTU u16 + version u16（默认 251，最大 1024，版本 1.0-1.3）
- **FIND_STRUCTURE**: handle范围 + findType(3b) + itemType(2b) + rspMode(1b)；返回 handle+uuid+operation+descriptor数
- **READ**: handle + type；**WRITE_CMD**: handle + type + data
- 属性操作位: READ=1 WRITE_NO_RSP=2 WRITE=4 NOTIFY=8 INDICATE=0x10 BROADCAST=0x20
- 特殊句柄: SERVICE_CHANGE=0x000E, HASH=0x000F

## 三、服务端 API 表面（ssaps_server.c 1508 行）

- `SSAPS_RegisterServer` / `SSAPS_AddService` / `SSAPS_AddProperty` / `SSAPS_AddDescriptor`
- `SSAPS_StartService` / `SSAPS_NotifyIndicate` / `SSAPS_SendReadRsp` / `SSAPS_SendWriteRsp`
- 内部: handle 分配（`SSAPS_IsHandleExist`）、读权限检查（`SSAPS_ReadControlCheck`）、
  多读序列化（`SSAPS_SerializeMultiReadItem`）、分片（READ_RSP 分片控制）

## 四、连接生命周期（ssap_manager.c）

1. CM 连接事件 → `SSAP_CreateSsapLink(addr, lcid, SSAP_Send)`（每连接一个 link）
2. 老协议栈兼容: `SSAP_CreateSsapLinkWithInitReq(..., hasInitReqTask=true)` 先发 find req
3. 30s 超时（`SSAP_TIMEOUT_TIME`）→ `CM_DirectConnectRemove`
4. 关闭: `SSAP_LinkDeInit`

## 五、WS73 适配点（传输层替换）

| OHOS 组件 | WS73 等价物 | 适配方式 |
|---|---|---|
| DLI（命令/事件帧） | /dev/hwsle 字节流 | 已通（sle-hci-scan.py 验证） |
| HDI ISleHciInterface | /dev/hwsle | 内核已暴露 ACB 数据通道（0xA3 帧） |
| DTAP（tcid 分发） | CHBA tcid demux | WS73 内核已按 tcid 分流 |
| **SSAP 层** | 移植 ssap_pkt.h + ssaps_server.c | **直接可用（Apache-2.0）** |

**核心洞察**: WS73 内核驱动已经做了 DTAP 的工作（tcid demux），OHOS 的 SSAP 层
（ssap_pkt + server/client + manager）是纯协议代码，可直接移植——只需写一个
薄适配层把 SSAP_Send/SSAP_Recv 接到 /dev/hwsle 的 0xA3 ACB 帧。

## 六、实现步骤（建议）

1. **移植 ssap_pkt.h**（纯头文件，零依赖）
2. **移植 ssaps_server.c + ssap_link.c + ssap_manager.c**（剥离 SDF/CP 抽象，替换为 POSIX）
3. **写 hwsle 传输适配**：SSAP_Send → write(/dev/hwsle, [0xA3][tcid=0x0A][payload])；
   SSAP_Recv ← read 线程解析 0xA3 帧 → tcid==0x0A 交给 SSAP
4. **测试**：先自环（自己发自己收 PDU），再双 dongle 或手机对连

## 七、风险

- WS73 固件侧 SSAP 方言与 OHOS 是否 100% 一致——需对端实测（唯一残余风险，票 02 已标）
- 分片/多读等复杂 PDU 路径需先测单字节串
- SSAP 需 ACB 连接建立（CREATE_CONNECTION 参数正确化）——这依赖对端
