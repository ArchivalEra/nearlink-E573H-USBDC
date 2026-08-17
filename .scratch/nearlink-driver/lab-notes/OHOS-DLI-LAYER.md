# OHOS DLI Layer -- Complete Dissection & Comparison with Our hwsle_transport

> Date: 2026-08-17
> Status: Read-only analysis. One file created per task spec.
> Verdict: OHOS DLI is a full command/event framework with async correlation, multi-module callback dispatch, timeout management, and data fragmentation. Our hwsle_transport.c is a raw wire adapter. The gap is architectural, not byte-level.

## Sources Read

**OHOS DLI** (`/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dli/`):

| File | Purpose |
|------|---------|
| `interface/dli.h` | Public API: Init/DeInit/Enable/Disable, callback registration |
| `interface/dli_cmd.h` | All DLI command functions (CreateConnection, SetScanParam, etc.) |
| `interface/dli_cmd_struct.h` | All command parameter structures (packed) |
| `interface/dli_def.h` | Wire-format types, encode/decode macros, AdvParam/ScanParam defs |
| `interface/dli_errno.h` | Error code definitions (controller + stack) |
| `interface/dli_layer.h` | Layer abstraction: CmdSend, DataSend, LayerInit/Deinit |
| `interface/dli_layer_stru.h` | CmdStru/DataStru wire headers, data types (0xA1-0xA4), fragmentation |
| `interface/dli_event_struct.h` | All event structures (ConnectionComplete, AdvReport, etc.) |
| `interface/dli_opcode.h` | CmdOpcode, EventOpcode, RegOpcode, SubEventOpcode enums |
| `interface/dli_callback.h` | Callback registration struct and NOCP event callback |
| `interface/dli_common_func.h` | Open function list (cmdCbkReg, setCmd, getCbk) |
| `dli.c` | Main DLI module: init/deinit/enable, inner callback tables per module |
| `cmd/src/dli_cmd.c` | Command execution: DLI_ExecuteCommand, all DLI_* command wrappers |

**Our code** (`/home/archivalera/plum/zcode-projects/nearlink/`):

| File | Purpose |
|------|---------|
| `stack/ssap/src/hwsle_transport.c` | Raw wire adapter: send HCI cmd/ACB data, poll loop |
| `stack/ssap/include/hwsle_transport.h` | Wire constants (0xA1-0xA4), TCID definitions |
| `stack/ssap/src/ssap_link.c` | Connection state machine: connect/disconnect/event handling |
| `stack/ssap/include/ssap_link.h` | Link state, DLI opcode defines, event defines |
| `.scratch/nearlink-driver/lab-notes/SLE-CONTROL-PLANE.md` | Hardware-verified DLI command/event bytes |
| `.scratch/nearlink-driver/lab-notes/OSPL-DLI-CROSSCHECK.md` | OpenSparklink DLI cross-check |

---

## 1. DLI Layer Architecture

### 1.1 Initialization and Lifecycle (`dli.c:130-217`)

The DLI module has a two-phase lifecycle:

**Phase 1: Init** (`DLI_Init`, `dli.c:130-147`)
```
g_dliIsInited guard --> DLI_PostOtherBlockedThread(DLI_InitInner)
  --> DLI_SetRecvEventCallback(RecvEventHandler)    // register event dispatch
  --> DLI_InitEventCbkList()                        // init callback tables
  --> DLI_InnerEventCbkReg(g_commInnerCbkList)      // register common events
  --> DLI_LayerInit()                               // create thread + resources
g_dliIsInited = true
```

**Phase 2: Enable** (`DLI_Enable`, `dli.c:183-201`)
```
DLI_BlockPostTask(DLI_EnableTask) --> DLI_LayerEnable()  // register driver receive callback
```

**Shutdown** (`DLI_DeInit`, `dli.c:158-174`): inverse of init -- LayerDeinit, unregister callbacks, clear callback list, set recv callback to NULL.

**Key design point**: Init and Enable are separate. Init creates the layer (thread, resources); Enable connects to the driver. This allows the stack to configure callbacks between init and enable.

### 1.2 Command Execution Flow (`dli_cmd.c:180-217`)

Every DLI command goes through `DLI_ExecuteCommand()`:

```
DLI_ExecuteCommand(cmd, event, inParam, paramLen, cbk, cbkContext, cbkContextLen)
  |
  +--> Validate: cmd range [DLI_SET_EVENT_MASK..DLI_TEST_END]
  +--> DLI_DefaultCmdStruCreate(cmd, event, inParam, paramLen)  // build DLI_CmdStru
  +--> DLI_CreateManagerContext(cmd, event, cbk, cbkContext)
  |      |
  |      +--> If event == DLI_CMD_COMPLETE_EVT:
  |      |      Create DLI_InnerCbkLineStru with opcode=cmd
  |      |      DLI_InnerEventCbkReg(innerCbkTable, 1)  // dynamic registration
  |      |      This ensures CmdComplete events route to DLI_ExecuteCommandCbk
  |      |
  |      +--> Copy cbkContext into managerContext->cbkContext
  |
  +--> cmdStruct->context = managerContext
  +--> cmdStruct->timeoutCallback = TimeoutCallback
  +--> cmdStruct->contextFree = ContextFree
  +--> DLI_CmdSend(cmdStruct)  // async send via layer
```

**This is the critical pattern**: every command creates a `DLI_ManagerContext` that holds the callback and context. When the CmdComplete event arrives, the layer dispatches it to `DLI_ExecuteCommandCbk` which:
1. Extracts `status` from `parameters[0]` of `DLI_CommandComplete` (`dli_cmd.c:80`)
2. Unregisters the dynamic inner callback (`dli_cmd.c:82`)
3. Calls `context->cbk(context->cbkContext, status, &par)` (`dli_cmd.c:67`)

**Two event model types**:
- `DLI_CMD_COMPLETE_EVT` (0x0002): used by read commands and short-lived ops. The callback receives both status and return data.
- Custom events (e.g., `DLI_CONNECTION_COMPLETE_EVT = 0x0015`): used by async commands (connect, disconnect, etc.). No CmdComplete callback; the event handler directly processes the result.

### 1.3 Timeout Handling (`dli_cmd.c:126-151`)

```c
void TimeoutCallback(void *param)
{
    DLI_CmdTxNode *cmdNode = (DLI_CmdTxNode *)param;
    DLI_CommandErrorStru errEvt = {0};
    errEvt.status = DLI_COMMAND_TIMEOUT;  // 0x0D
    errEvt.cmd = cmdNode->info->cmd;
    errEvt.event = cmdNode->info->event;
    // Update command flow control number
    if (cmdNode->info->event == DLI_CMD_COMPLETE_EVT) {
        DLI_CmdNumSet(DLI_DEFAULT_CMD_NUM);
    } else {
        if (!cmdNode->isRecvStatusEvt) {
            DLI_CmdNumSet(DLI_DEFAULT_CMD_NUM);
        }
    }
    // Notify upper layer via DLI_CMD_ERROR_EVT (0x00EE)
    RecvEventHandler(DLI_CMD_ERROR_EVT, cmdNode->info->context, &errEvt, sizeof(errEvt));
}
```

**Key insight**: The layer tracks whether a status event was already received (`isRecvStatusEvt`). If yes, only the final event needs timeout handling (the command flow control number was already decremented by the status event).

### 1.4 Layer Abstraction (`dli_layer.h`, `dli_layer_stru.h`)

The layer provides:
- **`DLI_CmdSend(DLI_CmdStru *cmd)`** -- async command send
- **`DLI_DataSend(DLI_DataStru *data)`** -- async data send (max 102400 bytes)
- **`DLI_SplitData()`** -- data fragmentation (up to 128 fragments)
- **`DLI_GetDataFragmentNums()`** -- compute fragment count
- **`DLI_GetFragmentMaxLen()`** -- get single fragment max length
- **`DLI_PostNextTask(DLI_TaskType type)`** -- trigger next command/data task

**DLI_CmdStru wire header** (`dli_layer_stru.h:41-61`):
```
Byte:  0        1-2          3-4         5+
Field: type(1)  cmd(2 LE)    len(2 LE)   params...
       0xA1
```

**DLI_DataStru wire header** (`dli_layer_stru.h:66-86`):
```
Byte:  0        1-2             3-4           5+
Field: type(1)  handle(2 LE)    len(2 LE)     data...
       0xA3/0xA4
```

**Data types** (`dli_layer_stru.h:81-86`):
- `DLI_DATATYPE_CMD = 0xA1`
- `DLI_DATATYPE_EVENT = 0xA2`
- `DLI_DATATYPE_ACB = 0xA3` (async connection-based data)
- `DLI_DATATYPE_ICB = 0xA4` (isochronous data)

### 1.5 Module Architecture (`dli.c:41-103`)

The DLI layer dispatches events to modules via static callback tables:

| Module | ModuleType | Inner Callbacks | Purpose |
|--------|-----------|-----------------|---------|
| DEVD | 0 | AdvReportCbk, AdvTerminatedCbk | Device discovery |
| CM | 1 | ConnectionCbk, DisconnectionCbk, DataLengthChangeCbk, ReadRemoteFeaturesCbk, ReadRemoteVersionCbk, ConnectionUpdateCbk, RemoteConnParamReqCbk, AcbLowLatencyEnableCbk, SetPhyCbk, SetAcbEvtParamCbk, FreqBandSwitchCbk, IOB/IMB callbacks | Connection management |
| CM_ICB | 2 | (same as CM) | ICB connection management |
| CM_COMMON | 3 | (same as CM) | Common CM operations |
| SM | 4 | EncryptParamReqCbk, EncryptChangeCbk, ControllerDataCbk | Security management |
| HADM | 5 | ReadRemoteCsCapsCbk, CsIqReportCbk, ReadLocalCsCapsCbk, MeasureStateChangeCbk | High-accuracy distance measurement |
| NBC | 6 | ChipResetNotifyCbk | Network bootstrapping |
| QOSM | 7 | (empty) | Quality of service |
| EXT_START..EXT_END | 10-17 | (user-provided innerTable) | Extension modules |

**Common events** (registered at init for all modules, `dli.c:98-103`):
- `DLI_CMD_ERROR_EVT` (0x00EE) -> `DLI_CommandErrorCbk`
- `DLI_CMD_STATUS_EVT` (0x0001) -> `DLI_CommandStatusCbk`
- `DLI_NUMBER_OF_COMPLETED_PACKETS_EVT` (0x0009) -> `DLI_NumberOfCompletedPacketsCbk`
- `DLI_VENDOR_EVENT_EVT` (0xFC07) -> `DLI_VendorEventCbk`

---

## 2. Command Structures (`dli_cmd_struct.h`)

All structures are `#pragma pack(1)`.

### 2.1 Advertising Commands

| Structure | Size | Fields | Our Equivalent |
|-----------|------|--------|----------------|
| `DLI_AdvParam` | 49B | advHandle, advMode, advGtRole, primAdvIntervalMin/Max(3B each), primAdvChannelMap, ownAddrType, peerAddrType, ownAddr(6B), peerAddr(6B), advFilterPolicy, advTxPower, primAdvFrameFormat, secondAdvFrameFormat/Phy/Pilot/Mcs/MaxSkip, advSid, scanReqNotifEnable, scanReqRecvNumberMax, scanReqRxDurMax, connIntervalMin/Max, maxLatency, supervisionTimeout, minCeLength, maxCeLength | None (we build inline) |
| `DLI_AdvData` | 5B+ | advHandle, operation, selection, advDataLen, advData[0] | None |
| `DLI_ScanRspData` | 5B+ | advHandle, operation, selection, scanRspDataLen, scanRspData[0] | None |
| `DLI_AdvEnable` | 4B | enable, duration(2B), maxAdvEvents | None |
| `DLI_RemoveAdvertisingSet` | 1B | advHandle | None |

### 2.2 Scanning Commands

| Structure | Size | Fields | Our Equivalent |
|-----------|------|--------|----------------|
| `DLI_ScanParam` | 3B+ | ownAddrType, scanFilterPolicy, frameFormatInd, param[0](scanType+scanInterval+scanWindow per PHY) | None |
| `DLI_ScanEnable` | 2B | enable, filterDuplicates | None |

### 2.3 Connection Commands

| Structure | Size | Fields | Our Equivalent |
|-----------|------|--------|----------------|
| `DLI_ConnectionCreateParam` | 26B | bitFrameType, initiatorFilterPolicy, ownAddressType, peerAddressType, peerAddress[6], gtNegotiateInd, initiatingPhys, scanInterval, scanWindow, connectionIntervalMin/Max, maxLatency, supervisionTimeout, minCeLength, maxCeLength | `ssap_conn_param_t` (ssap_link.h:45-62) -- **we are missing**: gtNegotiateInd, initiatingPhys, connPilot, connFrameFormatMSeqInd, connFrameFormatInd |
| `DLI_ConnectionCreateParamMultiInd` | larger | Same + 2x scanParam[0] + 2x connParam[0] for dual-frame-type | None |
| `DLI_ConnectionCreateParamSingleInd` | larger | Single-frame-type variant | None |
| `DLI_DisconnectParam` | 3B | connHandle(2B), reason | We build inline: `[handle_lo, handle_hi, 0x13]` (ssap_link.c:87-90) |
| `DLI_ConnectionUpdateParam` | 12B | connHandle, connIntervalMin/Max, txRxInterval, eventInterval, maxLatency, supervisionTimeout, systemTimeUnit, txRxFlag | None |
| `DLI_ConnHandleStru` | 2B | connHandle | We build inline: `[handle_lo, handle_hi]` |
| `DLI_RemConParamReqReplyParam` | 16B | connHandle, reason, connIntervalMin/Max, txRxInterval, eventInterval, maxLatency, supervisionTimeout, systemTimeUnit, txRxFlag | None |

### 2.4 PHY/Link Commands

| Structure | Size | Fields | Our Equivalent |
|-----------|------|--------|----------------|
| `DLI_SetPhyParam` | 10B | connHandle, txFormat, rxFormat, txPhy, rxPhy, txPilotDensity, rxPilotDensity, gFeedback, tFeedback | None |
| `DLI_SetDataLenParam` | 4B | connHandle(2B), txOctets(2B) | We build inline: `[handle_lo, handle_hi, 0xFF, 0x00]` (ssap_link.c:177) |
| `DLI_SetMcsParam` | 3B | connHandle(2B), mcs | None |

### 2.5 Security Commands

| Structure | Size | Fields | Our Equivalent |
|-----------|------|--------|----------------|
| `DLI_EnableEncryptParam` | 21B | connHandle(2B), linkKey[16], cryptoAlgo, keyDerivAlgo, integrChkInd | None |
| `DLI_ControllerData` | 4B+ | connHandle(2B), opcode(2B), dataLength, dataBuffer[0] | None |
| `DLI_EncryptParam` | 33B | key[16], plaintext[16], algorithm | None |
| `DLI_EncryptReqReplyParam` | 20B | connHandle(2B), linkKey[16], cryptoAlgo, keyDerivAlgo | None |
| `DLI_IMGEncryptParam` | 28B | handler(2B), algo, iv[8], key[16] | None |

### 2.6 Measurement Commands

| Structure | Size | Fields | Our Equivalent |
|-----------|------|--------|----------------|
| `DLI_SetMeasureConfigParam` | 40B+ | connHandle, configId, measureConfigDirect, occurrenceGroupPeriod, schedulingTimeslot, rttPhy, freqHoppingMode, fmFreq, sendDirect, antenna configs, eventsCount, bitWidth, pmInitAntCount, pmInitSignal2Tone, firstNodeInterval, pmReflAntCount, pmReflSignal2Tone, secondNodeInterval, channelBandwidth, pm2400mBand[10] | None |
| `DLI_SetMeasureEnableParam` | 3B | connHandle(2B), enable | None |

### 2.7 ICB/ICG Commands (Synchronous Links)

| Structure | Size | Fields |
|-----------|------|--------|
| `DLI_ICGParam` | variable | type, opCode, id, sduIntervalG2T/T2G, sca, packing, framing, maxLatencyG2T/T2G, icbCnt, paramCnt, icbParam* |
| `DLI_ICBConnectionParam` | variable | type, opCode, id, labelId, channelCnt, channel* |
| `DLI_SetupICBDataPathParam` | variable | connHandle, direction, pathId, codec{codecId, vendorId, vendorCodecId}, controllerDelay, codecConfigLen, codecConfigData* |
| `DLI_ACBEnableSubrateParam` | 10B | subrateMin/Max, maxLatency, continuationNum, supervisionTimeout |
| `DLI_ACBSubrateParam` | 12B | lcid, subrateMin/Max, maxLatency, continuationNum, supervisionTimeout |

### 2.8 Generic Command Interface

```c
// dli_cmd_struct.h:606-614
typedef struct DLI_CmdParams {
    uint16_t cmd;
    uint16_t event;
    void *inParam;
    uint16_t paramLen;
    DLI_ExecuteCmdCbk cbk;
    void *cbkContext;
    uint16_t cbkContextLen;
} DLI_CmdParams;
```

`DLI_SetCmd()` (`dli_cmd.c:1352-1359`) is the generic command sender -- any module can issue any command through this interface without needing a dedicated wrapper function.

---

## 3. Event Structures (`dli_event_struct.h`)

All structures are `#pragma pack(1)`.

### 3.1 Connection Events

| Structure | Size | Fields | Our Handling |
|-----------|------|--------|-------------|
| `DLI_ConnectionCompleteEvt` | 20B | status, connHandle(2B), role, peerAddressType, peerAddress[6], localResolvablePrivateAddress[6], peerResolvablePrivateAddress[6], connectionInterval(2B), peripheralLatency(2B), supervisionTimeout(2B), centralClockAccuracy, connCompleteType, advHandle | `ssap_link_on_event` case 0x0015 (ssap_link.c:144-181) -- we read status at data[0], connHandle at data[1..2] |
| `DLI_DisconnectEvt` | 4B | status, connHandle(2B), reason | `ssap_link_on_event` case 0x0005 (ssap_link.c:183-200) -- we read data[0] as reason |
| `DLI_ConnectionUpdateCmpEvt` | 12B | status, connHandle(2B), connInterval(2B), txRxInterval, eventInterval(2B), maxLatency(2B), supervisionTimeout(2B), systemTimeUnit, txRxFlag | Not handled |
| `DLI_DataLenChangeEvt` | 6B | connHandle(2B), maxTxOctets(2B), maxRxOctets(2B) | Not handled |
| `DLI_RemoteConnParamReqEvt` | 14B | connHandle(2B), connIntervalMin/Max(2B each), txRxInterval, eventInterval(2B), maxLatency(2B), supervisionTimeout(2B), systemTimeUnit, txRxFlag | Not handled |

### 3.2 Command Events

| Structure | Size | Fields | Our Handling |
|-----------|------|--------|-------------|
| `DLI_CommandComplete` | 4B+ | cmdOpcode(2B), numDliCommandPackets, parameters[0] | `find_cmd_echo()` in ssap_link.c:124-135 -- we scan for opcode echo at offsets 0..2 |
| `DLI_CommandStatus` | 4B | cmdOpcode(2B), numDliCommandPackets, status | Handled in same `find_cmd_echo()` path |
| `DLI_CommandErrorStru` | 6B+ | status(2B), cmd(2B), event(2B), req[1] | Not handled (we don't process 0x00EE) |

### 3.3 Advertising/Scanning Events

| Structure | Size | Fields | Our Handling |
|-----------|------|--------|-------------|
| `DLI_AdvReportEvt` | 15B+ | eventType, addrType, addr[6], directAddrType, directAddr[6], primFrameType, secondPhy, secondFrameType, secondPilotRatio, secondMcs, rssi, dataLength, data[0] | Not handled |
| `DLI_AdvertisingTerminatedEvt` | 4B | status, advHandle, connHandle(2B) | Not handled |
| `DLI_AdvPowerChangeEvt` | 2B | advHandle, powerLevel | Not handled |

### 3.4 Security Events

| Structure | Size | Fields | Our Handling |
|-----------|------|--------|-------------|
| `DLI_EncryptChangeEvt` | 4B | status, connHandle(2B), encryptChange | Not handled |
| `DLI_EncryptParamReqEvt` | 2B | connHandle(2B) | Not handled |
| `DLI_ControllerDataEvt` | 5B+ | connHandle(2B), ctrlDataIndex(2B), len, data[0] | Not handled |

### 3.5 Measurement Events

| Structure | Size | Fields | Our Handling |
|-----------|------|--------|-------------|
| `DLI_CsIqReportEvt` | 10B+ | status, connHandle(2B), slemIdx, slemInfoType, timestampSn(4B), data[0] | Not handled |
| `DLI_MeasureStateChangeEvt` | 3B | status, posMeasureSigConfigIdx, measureState | Not handled |
| `DLI_ReadLocalCsCapsEvt` | 24B | caps[24] | Not handled |
| `DLI_ReadRemoteCsCapsEvt` | 27B | status, connHandle(2B), caps[24] | Not handled |

### 3.6 Data Events

| Structure | Size | Fields | Our Handling |
|-----------|------|--------|-------------|
| `DLI_NumberOfCompletedPacketsEvt` | 3B | connHandle(2B), numCompletedPackets | Not handled (but registered as common event in OHOS) |

### 3.7 ICB/ICG Events

| Structure | Size | Fields |
|-----------|------|--------|
| `DLI_ICBEstablishedEvt` | 40B+ | status, labelId, lcid(2B), connHandle(2B), imgSyncDelay[3], imbSyncDelay[3], transLatencyG2T/T2G[3], phyG2T/T2G, mcsG2T/T2G, pilotG2T/T2G, nse, bnG2T/T2G, ftG2T/T2G, maxPduG2T/T2G(2B each), imbInterval(2B) |
| `DLI_ICBConnectReqEvt` | 6B | lcid(2B), connHandle(2B), icgId, icbId |
| `DLI_ICGLabelReportEvt` | 8B+ | status, lcid(2B), connHandle(2B), icgId, icbId, icbInterval(2B), latencyG2T/T2G[3], bnG2T/T2G, ftG2T/T2G, labelCnt, label[0] |
| `DLI_IOBQualityReportEvt` | 36B | diffTotal(4B), diffMax(4B), diffAvg(4B), connHandle(2B), txFlushed(2B), rxLossPktCnt(2B), rxLossMaxContPkt(2B), rssi, ackRate, reserve1-4(4B each) |
| `DLI_AcbSetSubrateEvt` | 9B | status, lcid(2B), subrateFactor(2B), peripheralLatency(2B), continuationNum(2B) -- wait, that's 10B. Actually: status(1)+lcid(2)+subrateFactor(2)+peripheralLatency(2)+continuationNum(2)+supervisionTimeout(2) = 11B |

---

## 4. Opcode Enumeration (`dli_opcode.h`)

### 4.1 Command Opcodes (`DLI_CmdOpcode`, `dli_opcode.h:228-297`)

| Range | Group | Opcodes |
|-------|-------|---------|
| 0x04xx | Host Control | SET_EVENT_MASK(0x0401), READ_LOCAL_BUFFER(0x0402), READ_LOCAL_SUPPORT_FEATS(0x0403), READ_LOCAL_VERSION(0x0404), SET_PUBLIC_ADDRESS(0x0405), GET_PUBLIC_ADDRESS(0x0406), RESET(0x0408), SET_HOST_CHANNEL_CLASSIFICATION(0x0409), READ/CLEAR/ADD/REMOVE_ACCESS_FILTER_LIST(0x040A-0D) |
| 0x0Cxx | Advertising | SET_ADV_PARAMETERS(0x0C02), SET_ADV_DATA(0x0C03), SET_SCAN_RESPONSE_DATA(0x0C04), SET_ADV_ENABLE(0x0C05), READ_MAX_ADV_DATA_LEN(0x0C06), READ_ADV_SETS_NUM(0x0C07), REMOVE_ADV_SET(0x0C08) |
| 0x10xx | Scanning | SET_SCAN_PARAMETERS(0x1001), SET_SCAN_ENABLE(0x1002), SET_SCAN_DATA(0x1003) |
| 0x14xx | Connection | CREATE_CONNECTION(0x1401), CREATE_CONNECTION_CANCEL(0x1402), DISCONNECT(0x1403) |
| 0x18xx | Information | READ_REMOTE_FEATURES(0x1801), READ_REMOTE_VERSION(0x1802), SET_DATA_LEN(0x1804), READ_PHY(0x1805), SET_PHY(0x1806), CONNECTION_UPDATE(0x1807), CONNECTION_PARAM_REQ_REPLY(0x1808), SET_MCS(0x180A), READ_REMOTE_RSSI(0x180C), SET_CONTROLLER_DATA(0x1812) |
| 0x1Cxx | Security | ENCRYPT(0x1C01), RANDOM(0x1C02), ENABLE_ENCRYPTION(0x1C03), DISABLE_ENCRYPTION(0x1C04), ENCRYPTION_PARAMETER_REQUEST_REPLY(0x1C05), ENCRYPTION_PARAMETER_REQUEST_NEGATIVE_REPLY(0x1C06), READ_SUPPORT_CRYPTOGRAPHY_ALGORITHM(0x1C07), ENABLE_IMG_ENCRYPTION(0x1C28) |
| 0x20xx | Measurement | READ_LOCAL_MEASURE_CAPS(0x2001), READ_REMOTE_MEASURE_CAPS(0x2002), SET_MEASURE_CONFIG_PARAM(0x2003), SET_MEASURE_EN(0x2005) |
| 0x28xx | Synchronous Links | SET_IOG_PARAM(0x2801), SET_IOG_PARAM_TEST(0x2802), CREATE_IOB(0x2803), REMOVE_IOG_PARAM(0x2804), ACCEPT_IOB_REQ(0x2805), REJECT_IOB_REQ(0x2806), SET_IMG_PARAM(0x2807), SET_IMG_PARAM_TEST(0x2808), CREATE_IMB(0x2809), REMOVE_IMG_PARAM(0x280A), ACCEPT_IMB_REQ(0x280B), REJECT_IMB_REQ(0x280C), SETUP_ICB_DATA_PATH(0x280D), REMOVE_ICB_DATA_PATH(0x280E) |

**Total**: 49 command opcodes defined.

**Our verified count**: 30 opcodes verified on WS73 hardware (see OSPL-DLI-CROSSCHECK.md). The remaining 19 are ICB/ICG/sync-link commands, encryption commands, and host-control commands we have not exercised.

### 4.2 Event Opcodes (`DLI_EventOpcode`, `dli_opcode.h:166-226`)

| Code | Name | Used In |
|------|------|---------|
| 0x0001 | DLI_CMD_STATUS_EVT | Common -- command status |
| 0x0002 | DLI_CMD_COMPLETE_EVT | Common -- command complete |
| 0x0003 | DLI_DATA_LENGTH_CHANGE_EVT | CM -- data length negotiation |
| 0x0004 | DLI_ADVERTISING_TERMINATED_EVT | DEVD -- broadcast stopped |
| 0x0005 | DLI_DISCONNECTION_COMPLETE_EVT | CM -- link down |
| 0x0007 | DLI_REMOTE_CONNECTION_PARAMETER_REQUEST_EVT | CM -- peer wants param change |
| 0x0009 | DLI_NUMBER_OF_COMPLETED_PACKETS_EVT | Common -- flow control |
| 0x000E | DLI_ENCRYPTION_PARAMETER_REQUEST_EVT | SM |
| 0x0011 | DLI_ENCRYPTION_CHANGE_EVT | SM |
| 0x0013 | DLI_ACB_LOW_LATENCY_EN_EVT | CM |
| 0x0014 | DLI_CONTROLLER_DATA_EVT | SM |
| 0x0015 | DLI_CONNECTION_COMPLETE_EVT | CM -- link up |
| 0x0016 | DLI_READ_REMOTE_FEATURES_COMPLETE_EVT | CM |
| 0x0017 | DLI_READ_REMOTE_VERSION_COMPLETE_EVT | CM |
| 0x0018 | DLI_SET_PHY_COMPLETE_EVT | CM |
| 0x0019 | DLI_CONNECTION_UPDATE_EVT | CM |
| 0x001A | DLI_ADVERTISING_REPORT_EVT | DEVD -- scan result |
| 0x0028 | DLI_MEASURE_IQ_REPORT_EVT | HADM |
| 0x0029 | DLI_MEASURE_STATE_CHANGE_EVT | HADM |
| 0x002B | DLI_READ_LOCAL_MEASURE_CAPS_STATUS_EVT | HADM |
| 0x002C | DLI_READ_REMOTE_MEASURE_CAPS_STATUS_EVT | HADM |
| 0x0038-0x003B | IOB/IMB connect/establish (std) | CM |
| 0x004A | DLI_MEASURE_IQ_REPORT_VENDOR_EVT | HADM |
| 0x0053 | DLI_FREQ_BAND_SWITCH_VENDOR_EVENT | CM |
| 0x0061-0x0074 | ICG/IMG label/update/report | CM |
| 0x00EE | DLI_CMD_ERROR_EVT | Common -- command error/timeout |
| 0x1405 | DLI_DIS_CONNECTION_COMPLETE_EVT | Extended disconnect |
| 0x180C | DLI_READ_REMOTE_RSSI_EVT | CM |
| 0x1811 | DLI_SET_ACB_EVT_PARAM_EVT | CM |
| 0x1812 | DLI_READ_REMOTE_MEASURE_CAPS_EVT | HADM |
| 0xFC06 | DLI_CHIP_RESET_NOTIFY_EVT | NBC |
| 0xFC07 | DLI_VENDOR_EVENT_EVT | Common -- vendor sub-dispatch |
| 0xFE01 | DLI_SET_CONN_FRAME_POWER_LEVEL_EVT | CM |
| 0xFFFF | DLI_INVALID_EVT | Sentinel |

### 4.3 Registration Opcodes (`DLI_RegOpcode`, `dli_opcode.h:31-164`)

128 registration opcodes (0 through DLI_CBK_MAX) mapping logical callbacks to physical events. This is the DLI layer's internal dispatch table -- each module registers which callbacks handle which logical operations.

### 4.4 Our Opcode Coverage (DLI_CmdOpcode values only)

| Our Opcode | OHOS CmdOpcode | Verified? |
|-----------|---------------|-----------|
| 0x0401 | DLI_SET_EVENT_MASK (we call it SET_EVENT_MASK) | Yes |
| 0x0402 | DLI_READ_LOCAL_BUFFER | Yes |
| 0x0403 | DLI_READ_LOCAL_SUPPORT_FEATS | Yes |
| 0x0404 | DLI_READ_LOCAL_VERSION_INFORMATION | Yes |
| 0x0405 | DLI_SET_PUBLIC_ADDRESS | Yes |
| 0x0406 | DLI_GET_PUBLIC_ADDRESS | Yes |
| 0x0408 | DLI_RESET | Yes |
| 0x040A | DLI_READ_ACCESS_FILTER_LIST_SIZE | Yes |
| 0x0C02 | DLI_SET_ADVERTISING_PARAMETERS | Yes |
| 0x0C03 | DLI_SET_ADVERTISING_DATA | Yes |
| 0x0C04 | DLI_SET_SCAN_RESPONSE_DATA | Yes |
| 0x0C05 | DLI_SET_ADVERTISING_ENABLE | Yes |
| 0x0C06 | DLI_READ_MAXIMUM_ADVERTISING_DATA_LENGTH | Yes |
| 0x1001 | DLI_SET_SCAN_PARAMETERS | Yes |
| 0x1002 | DLI_SET_SCAN_ENABLE | Yes |
| 0x1401 | DLI_CREATE_CONNECTION | Yes |
| 0x1403 | DLI_DISCONNECT | Yes |
| 0x1802 | DLI_READ_REMOTE_VERSION | Yes |
| 0x1804 | DLI_SET_DATA_LEN | Yes |
| 0x1805 | DLI_READ_PHY | Yes |
| 0x1806 | DLI_SET_PHY | Yes |
| 0x1807 | DLI_CONNECTION_UPDATE | Yes |
| 0x180A | DLI_SET_MCS | Yes |
| 0x180C | DLI_READ_REMOTER_RSSI | Yes |
| 0x1C01 | DLI_ENCRYPT | Yes |
| 0x1C02 | DLI_RANDOM | Yes |
| 0x2001 | DLI_READ_LOCAL_MEASURE_CAPS | Yes |
| 0x2005 | DLI_SET_MEASURE_EN | Yes |
| 0x2801 | DLI_SET_IOG_PARAM | Yes |
| 0x2803 | DLI_CREATE_IOB | Yes |
| 0x280D | DLI_SETUP_ICB_DATA_PATH | Yes |

**Coverage: 31 of 49 opcodes verified.** Missing: 0x1402 (cancel connect), 0x1801 (read remote features), 0x1808 (conn param req reply), all 0x1Cxx encryption except 01/02, 0x2002-03 (remote measure caps, set measure config), all 0x28xx except 01/03/0D.

---

## 5. Callback Mechanism (`dli_callback.h`, `dli.c:295-330`)

### 5.1 External Callback Registration

```c
// dli.h:58-59
uint32_t DLI_CmdCbkReg(const ModuleType module,
    const DLI_InnerCbkLineStru *innerTable, const uint32_t innerSize,
    const DLI_CbkLineStru *table, const uint32_t size);
```

Each module (DEVD, CM, SM, HADM, NBC) registers two types of callbacks:

**Inner callbacks** (`DLI_InnerCbkLineStru`, `dli_def.h:236-239`):
```c
typedef struct DLI_InnerCbkLineStru {
    uint16_t opcode;  // event opcode (for auto events) or cmd opcode (for CmdComplete)
    DLI_InnerExecuteCmdCbk func;  // callback: void (*)(void *context, void *arg, uint32_t len, uint16_t evtOpcode)
} DLI_InnerCbkLineStru;
```

These are the internal event handlers that parse event structures and forward to the external callbacks. They are registered at init time from static tables in `dli.c:47-103`.

**External callbacks** (`DLI_CbkLineStru`, `dli_cmd_struct.h:601-604`):
```c
typedef struct DLI_CbkLineStru {
    DLI_RegOpcode opcode;   // logical callback ID (e.g., DLI_CBK_SET_ADV_PARAMS)
    DLI_ExecuteCmdCbk func; // callback: void (*)(void *context, uint16_t status, DLI_ExecuteCmdRetParam *cmdRes)
} DLI_CbkLineStru;
```

These are the per-operation callbacks registered by modules via `DLI_CmdCbkReg()`. They receive the final (status, return-data) pair.

**Lookup**: `DLI_GetCbk(DLI_CBK_xxx)` returns the registered callback for that logical opcode.

### 5.2 Internal Callback Dispatch

The event flow is:

```
Driver delivers raw event bytes
  --> RecvEventHandler (registered via DLI_SetRecvEventCallback)
  --> DLI dispatches by event opcode to registered InnerCbkLineStru entries
  --> Inner callback parses event struct, calls external DLI_CbkLineStru callback
  --> External callback notifies the module (e.g., "scan result arrived", "connection complete")
```

For CmdComplete events specifically (`dli_cmd.c:71-84`):
```
CmdComplete event arrives
  --> DLI_ExecuteCommandCbk (registered dynamically per-command)
  --> Extracts status from parameters[0]
  --> Unregisters the dynamic inner callback
  --> Calls context->cbk(context->cbkContext, status, &par)
```

### 5.3 Platform Callbacks (`dli_callback.h:48-56`)

```c
typedef struct {
    DLI_PostOtherThreadPtr postOtherThread;               // Required: post work to DLI thread
    DLI_PostOtherBlockedThreadPtr postOtherBlockedThread;  // Required: post work and block
    DLI_RecvAcbHandlerPtr recvAcbHandler;                 // Required: async data receive
    DLI_DftReportKillPtr dftReportKill;                   // Optional: DFT report
    DLI_GetExtRegOpcodePtr getExtRegOpcode;               // Optional: custom opcode mapping
    HADM_ProcessCsCapsPtr hadmProcessCsCaps;              // Optional: CS capability processing
} DLI_Callback;
```

This is how the DLI layer decouples from the platform: the platform (OHOS stack) provides thread scheduling (`postOtherThread`), blocking waits (`postOtherBlockedThread`), and data receive (`recvAcbHandler`).

### 5.4 NOCP (Number Of Completed Packets) Callback

```c
// dli_event_struct.h:587
typedef void (*DLI_NOCPEventCbk)(uint16_t connHandle, uint8_t numCompletedPackets);
```

Registered via `DLI_RegNOCPEventCbk(DLI_RegModuleType module, DLI_NOCPEventCbk cbk)` (`dli.h:78`). This is a separate callback path specifically for flow control notification.

### 5.5 Comparison with Our hwsle_transport

| Aspect | OHOS DLI | Our hwsle_transport |
|--------|----------|-------------------|
| Command send | `DLI_ExecuteCommand()` -- async, with callback, timeout, correlation | `hwsle_transport_send_hci_cmd()` -- fire-and-forget, no callback |
| Event receive | `RecvEventHandler` -> inner callback table -> external callback | `hwsle_transport_run()` poll loop -> `g_recv_cb` for ACB only, events skipped |
| Command correlation | Dynamic `DLI_InnerCbkLineStru` per command, opcode-echo matching | `find_cmd_echo()` brute-force scan at offsets 0..2 (ssap_link.c:124-135) |
| Timeout | `TimeoutCallback` fires `DLI_CMD_ERROR_EVT` (0x00EE) | `ssap_link_tick()` polls timestamps, manually cancels |
| Data send | `DLI_DataSend()` with fragmentation, priority, flow control | `hwsle_transport_send_acb()` raw write, no fragmentation |
| Callback registration | Module-based (`DLI_CmdCbkReg`) with 128 logical opcodes | Single `g_recv_cb` function pointer |
| Thread model | Dedicated DLI thread with work queue | Caller's thread (blocking poll loop) |

---

## 6. Error Codes (`dli_errno.h`)

### 6.1 Controller Error Codes (`dli_errno.h:26-58`)

Direct mapping to the SLE specification error codes:

| Code | Name | Description |
|------|------|-------------|
| 0x00 | DLI_SUCCESS | Success |
| 0x01 | DLI_UNKNOWN_COMMAND | Unknown command opcode |
| 0x02 | DLI_UNKNOWN_CONNECTION_IDENTIFIER | Invalid connection handle |
| 0x03 | DLI_HARDWARE_FAILURE | Hardware failure |
| 0x04 | DLI_AUTHENTICATION_FAILURE | Authentication failed |
| 0x05 | DLI_PIN_OR_KEY_MISSING | PIN/key missing |
| 0x06 | DLI_MEMORY_CAPACITY_EXCEEDED | Memory full |
| 0x07 | DLI_CONNECTION_TIMEOUT | Connection timed out |
| 0x08 | DLI_CONNECTION_LIMIT_EXCEEDED | Too many connections |
| 0x09 | DLI_SYNC_CONNECTION_LIMIT_EXCEEDED | Too many sync connections |
| 0x0A | DLI_CONNECTION_ALREADY_EXISTS | Connection already exists |
| 0x0B | DLI_COMMAND_DISALLOWED | Command not allowed in current state |
| 0x0C | DLI_UNACCEPTABLE_BDADDR | Unacceptable address |
| 0x0D | DLI_COMMAND_TIMEOUT | Command timed out (host-side) |
| 0x0E | DLI_UNSUPPORTED_REMOTE_FEATURE | Peer doesn't support feature |
| 0x0F | DLI_INVALID_PARAMETERS | Invalid parameters |
| 0x10 | DLI_REMOTE_USER_TERMINATED_CONNECTION | Peer terminated |
| 0x11 | DLI_CONNECTION_TERMINATED_BY_LOCAL_HOST | Local terminated |
| 0x12 | DLI_ROLE_CHANGE_NOT_ALLOWED | Role change denied |
| 0x13 | DLI_ENCRYPTION_MODE_NOT_ACCEPTABLE | Encryption mode rejected |
| 0x14 | DLI_LINK_KEY_CANNOT_BE_CHANGED | Link key immutable |
| 0x15 | DLI_INSTANT_PASSED | Timing instant passed |
| 0x16 | DLI_CHANNEL_CLASSIFICATION_NOT_SUPPORTED | Channel classification unsupported |
| 0x17 | DLI_INSUFFICIENT_SECURITY | Not enough security |
| 0x18 | DLI_ROLE_SWITCH_FAILED | Role switch failed |
| 0x19 | DLI_CONTROLLER_BUSY | Controller busy |
| 0x1A | DLI_ADVERTISING_TIMEOUT | Broadcast timed out |
| 0x1B | DLI_CONNECTION_TERMINATED_MIC_FAILURE | MIC check failed |
| 0x1C | DLI_CONNECTION_FAILED_TO_BE_ESTABLISHED | Connection setup failed |
| 0x1D | DLI_CCA_REJECTED_BUT_ADJUST_USING_CLOCK_DRAGGING | CCA rejected, clock adjusted |
| 0x1E | DLI_UNKNOWN_ADVERTISING_IDENTIFIER | Unknown broadcast handle |
| 0x1F | DLI_PACKET_TOO_LONG | Packet too long |
| 0x20 | DLI_UNSPECIFIED_ERROR | Unspecified error |

### 6.2 Stack-Internal Error Codes (`dli_errno.h:60-75`)

These use `DLI_MAKE_ERRNO(id)` macro (wraps `SDF_MAKE_ERRNO(COMP_DLI, 1, 1, id)`):

| Code | Name | Description |
|------|------|-------------|
| 101 | DLI_STACK_NOINIT_ERRNO | DLI not initialized |
| 102 | DLI_STACK_INITED_ERRNO | DLI already initialized |
| 103 | DLI_STACK_PARAMS_ERRNO | Invalid parameters |
| 104 | DLI_STACK_MEM_ERRNO | Memory allocation failed |
| 105 | DLI_STACK_EVC_CREATE_ERRNO | Event channel creation failed |
| 106 | DLI_STACK_WORKER_CREATE_ERRNO | Worker thread creation failed |
| 107 | DLI_STACK_EVENT_ADD_ERRNO | Event add failed |
| 108 | DLI_STACK_INIT_SEM_ERRNO | Semaphore init failed |
| 109 | DLI_STACK_HAL_INIT_ERRNO | HAL init failed |
| 110 | DLI_STACK_INIT_TIMEOUT_ERRNO | Init timeout |
| 111 | DLI_STACK_INIT_LOCK_ERRNO | Lock init failed |
| 112 | DLI_STACK_READ_COMM_CONFIG_VAL_ERRNO | Read common config failed |
| 113 | DLI_STACK_POST_BLOCK_ERROR | Block post failed |
| 114 | DLI_STACK_TASK_TIMEOUT | Task timeout |
| 115 | DLI_STACK_OUT_MAXCNT_ERRNO | Max count exceeded |

### 6.3 Comparison with Our SSAP_ERRCODE_*

Our SSAP error codes (`ssap_pkt.h:189-201`) are at the **application layer**, not the DLI/HCI layer:

| Our SSAP_ERRCODE | DLI Equivalent | Layer |
|-----------------|---------------|-------|
| SSAP_ERRCODE_SUCCESS (0x0) | DLI_SUCCESS (0x00) | Both success |
| SSAP_ERRCODE_INVALID_PDU (0x1) | DLI_INVALID_PARAMETERS (0x0F) | Different layers |
| SSAP_ERRCODE_INVALID_HANDLE (0x4) | DLI_UNKNOWN_CONNECTION_IDENTIFIER (0x02) | Different layers |
| SSAP_ERRCODE_NO_RESOURCE (0x5) | DLI_MEMORY_CAPACITY_EXCEEDED (0x06) | Different layers |

**Critical distinction**: Our SSAP_ERRCODE_* codes are SSAP protocol error codes returned in SSAP error response PDUs. DLI error codes are HCI-layer status bytes returned in CmdComplete/CmdStatus events. They operate at different protocol layers and must not be confused.

---

## 7. Takeaway List: What We Should Adopt from OHOS DLI

### 7.1 Must-Have (Critical Gaps)

1. **Command-Event Correlation**: OHOS tracks every in-flight command with `DLI_ManagerContext`, matching CmdComplete/CommandStatus by opcode echo (`dli_cmd.c:86-124`). Our `find_cmd_echo()` (ssap_link.c:124-135) is a brute-force scan with offset guessing. We need a proper pending-command table with timeout.

2. **Command Timeout Handler**: OHOS fires `DLI_CMD_ERROR_EVT` (0x00EE) with cmd/event/status on timeout (`dli_cmd.c:126-151`). We poll timestamps in `ssap_link_tick()` but only for connect/disconnect -- no per-command timeout for SET_DATA_LEN, READ_REMOTE_VERSION, etc.

3. **CmdStatus vs CmdComplete Distinction**: OHOS explicitly tracks `isRecvStatusEvt` to know whether the command flow control number was already decremented (`dli_cmd.c:144-148`). We treat CmdStatus and CmdComplete identically in `find_cmd_echo()`.

4. **0x00EE (DLI_CMD_ERROR_EVT) Processing**: We completely ignore this event. It carries the original cmd/event opcodes plus the error status -- essential for timeout notification.

### 7.2 Should-Have (Architectural Improvements)

5. **Module-Based Callback Registration**: OHOS uses `DLI_CmdCbkReg()` with ModuleType + inner/outer tables (`dli.c:295-315`). We have a single `g_recv_cb` that must be extended for each new event type. A table-driven dispatch would be cleaner.

6. **DLI_SetCmd Generic Interface**: OHOS exposes `DLI_SetCmd(DLI_CmdParams *params)` (`dli_cmd.c:1352-1359`) for sending arbitrary commands. We hardcode each command in `ssap_link.c` with inline byte arrays. A generic interface would reduce code duplication.

7. **Platform Callback Decoupling**: OHOS's `DLI_Callback` struct (`dli_callback.h:48-56`) separates thread scheduling from DLI logic. Our `hwsle_transport_run()` poll loop is tightly coupled to the calling thread.

8. **Data Fragmentation**: OHOS supports `DLI_SplitData()` with up to 128 fragments and priority levels (`dli_layer_stru.h:33-38, 88-93`). We have no fragmentation -- raw `write()` of the full payload.

### 7.3 Nice-to-Have (Future Features)

9. **Number Of Completed Packets Flow Control**: OHOS registers `DLI_NOCPEventCbk` for per-connection flow control (`dli.h:78`). We ignore event 0x0009 entirely.

10. **Connection Update Handling**: OHOS handles `DLI_CONNECTION_UPDATE_EVT` (0x0019) and `DLI_REMOTE_CONNECTION_PARAMETER_REQUEST_EVT` (0x0007). We only handle connect/disconnect.

11. **Encryption State Machine**: OHOS has `DLI_ENCRYPTION_CHANGE_EVT` (0x0011), `DLI_ENCRYPTION_PARAMETER_REQUEST_EVT` (0x000E), and dedicated SM module callbacks (`dli.c:47-51`). We send ENCRYPT/RANDOM commands but ignore the responses.

12. **Broadcast/Scan Event Processing**: OHOS handles `DLI_ADVERTISING_REPORT_EVT` (0x001A) and `DLI_ADVERTISING_TERMINATED_EVT` (0x0004). We skip all events in `hwsle_transport_run()`.

13. **ICB/ICG Synchronous Link Support**: OHOS has full ICB lifecycle (SET_IOG/IMG_PARAM, CREATE_IOB/IMB, data path setup/removal, quality reports). We have the opcodes verified but no implementation.

14. **Subrate Negotiation**: OHOS handles `DLI_ACB_LOW_LATENCY_EN_EVT` (0x0013) and subrate change events. We have no subrate support.

---

## 8. Open Questions

1. **`num_hci_pkts` byte in WS73 CmdComplete**: OHOS `DLI_CommandComplete` (`dli_event_struct.h:160-164`) has `cmdOpcode(2B) + numDliCommandPackets(1B) + parameters[0]`. Our WS73 decode sees this byte but `SLE-CONTROL-PLANE.md` and `sle-adv.py` disagree on its position. Needs re-verification on hardware.

2. **WS73 event code for async "command status"**: Our notes captured `[A2 0a 00 01 00 00]` suggesting event code 0x000A, but OHOS uses 0x0001 (`DLI_CMD_STATUS_EVT`). The 0x0A code is not defined in `DLI_EventOpcode`. Re-capture needed.

3. **DLI_AdvParam layout difference**: OHOS `DLI_AdvParam` is 49 bytes (`dli_def.h:81-110`), but some fields (advHandle, advMode, advGtRole) are in the OHOS struct while our verified 49-byte layout (`SLE-CONTROL-PLANE.md:52-64`) starts with `advHandle` at offset 0. Are these the same struct or does WS73 use a different variant?

4. **DLI_ConnectionCreateParam**: OHOS has `bitFrameType` as first byte (`dli_cmd_struct.h:189`), but we never verified this field on WS73 hardware. Our `ssap_conn_param_t` starts with `version` + `localIndex` which does not match OHOS. The wire format for CREATE_CONNECTION on WS73 remains unverified beyond "accepted".

5. **Stack error codes 101-115**: These use `SDF_MAKE_ERRNO(COMP_DLI, 1, 1, id)` macro. The exact error code values depend on `COMP_DLI` and the SDF error framework. We have no equivalent -- our stack uses plain -1 returns.

6. **DLI version negotiation**: OHOS checks `DLI_GetDliVersion()` against `DLI_VERSION_1_1` to decide feature support (`dli_cmd.c:165-172`). We have no version negotiation -- all commands are sent assuming a single firmware version.
