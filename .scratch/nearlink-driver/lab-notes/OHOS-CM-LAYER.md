# OHOS NearLink CM Layer (SLE Connection Manager) — Deep Dive

Root: `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/cp/bsl/sle/cm/`

## 1. Connection establishment state machine

- State enum: CM_LINK_STATE_* (cm_def.h:81-86: CONNECTED/CONNECTING/DISCONNECTED/DISCONNECTTING) + CM_ConnectState_E (adds CONNECT_UPDATED).
- Active connect: `CM_ConnectEstablishReq()` (cm_api.c:325) → `CM_EstablishLinkInit()` → alloc SleLogicLink_S, status=CONNECTING → `SleAccessLinkEstablishReq()` (sle_access_dli.c:883) fills DLI_ConnectionCreateParam → **`DLI_CreateConnection` → 0x1401**.
- Whitelist path: CM_DirectConnectAdd (10s timeout), CM_StartAllowListConnReq → CM_ClearAcceptFilterList/CM_AddDeviceToAcceptFilterList + CM_ConnectSetParamReq; timeout → CM_ConnectCancelReq → **DLI_CancelCreateConnection 0x1402**.
- Connect complete: **DLI_CONNECTION_COMPLETE_EVT 0x0015** (DLI_ConnectionCompleteEvt: role, connHandle, connInterval, supervisionTimeout, cca, connCompleteType, advHandle) → CM_DLIConnectCbk → SleAccessConnectCbkProc → CM_LINK_STATE_CONNECTED, lcid=connHandle.
- Post-connect chain (cm_event_core.c:62): read remote version (**0x1802**), activate fixed trans channels, then **0x1801** read features (G role), **0x1804** set data len; CM_NotifyLogicLinkCbks fans out per moduleId.
- Disconnect: **0x1403** DLI_DISCONNECT {connHandle, reason}; event **0x0005** DISCONNECTION_COMPLETE.
- Update: **0x1807** CONNECTION_UPDATE; peer-initiated → **0x1808** CONNECTION_PARAM_REQ_REPLY; complete **0x0019**.

## 2. Signaling channel (CMTC, TCID 0x02)

- DTAP wires: DTAP_RegisterDataRecvCb(TCID_SLE_CMTC, CM_RecvSignalingData); send CM_SendBuffToDtap → DTAP_DataSend.
- Framing: CM_SignalingHead_S {code(1), identifier(1), length(2 LE), data[0]}.
- Codes: CAPABILITY_REQ/RSP=0x1/0x2, TC_CONNECT_REQ/RSP=0x10/0x11, TC_DISCONNECT=0x12/0x13, TC_RECONFIG 0x14/0x15, relay 0x20-0x25, QOS 0x30-0x37, measure 0x40-0x46. RSP = REQ+1. 21s timeout.
- Capability exchange right after connect: protocolVersion/deviceType/rxWnd/supportTransMode/mtu onto the link.

## 3. Dynamic TCID allocation (dyntrans/)

- Per-LCID pool: CM_DynTcidPool_S {lcid, unicastTcidUsed bitmap}; pool activated on CONNECTED.
- Allocation: CM_DynTcidAllocate(lcid, mode) — only UNICAST; first free slot in **0x80-0xDF** (UC_BEGIN/UC_END).
- TCID ranges: CMTC 0x02 / SMTC 0x0A / CUTC 0x1F fixed; broadcast 0x20-0x4F; multicast MC 0x50-0x7F (ICB); dynamic unicast 0x80-0xDF.
- Channel state machine: INIT→ACTIVATING (alloc tcid, send TC_CONNECT_REQ)→ACTIVATED; passive INIT→ACTIVATED via TC_CONNECT_RSP; release via TC_DISCONNECT_REQ.

## 4. ICB manager (isochronous)

- DLI: SET_IOG_PARAM 0x2801, SET_IMG_PARAM 0x2807, CREATE_IOB 0x2803, CREATE_IMB 0x2809, ACCEPT 0x2805/0x280B, REJECT 0x2806/0x280C, **SETUP_ICB_DATA_PATH 0x280D / REMOVE 0x280E** (all verified accepted on WS73!).
- Sequence: ICGSetParam → ICBMgrSetParam → DLI_SetICGParam → CreateICB → (passive: AcceptICBReq) → SetupDataPath 0x280D → state DATA_PATH_SETUP.
- Event-group scheduling: CM_ICGParam {sduIntervalG2T/T2G(us), maxLatencyG2T/T2G(ms), sca, packing, framing}.
- DTAP integration: stream-mode channel at CM_TCID_MC_BEGIN 0x50, mtu=4096, crcInit 0x5555, reorder 1000ms.

## 5. API surface a host stack calls

CM_Init/DeInit/Enable/Disable; CM_RegConnectCbks (required: connRemoteUpdateParamReqCbk, connUpdateParamCbk, readRemoteFeatureVersionCbk, setPhyCbk); CM_ConnectEstablishReq/ReleaseReq/SetParamReq/UpdateParamReq/CancelReq; whitelist (CM_DirectConnectAdd/Remove, CM_BackgroundConnectAdd); CM_RegLogicLinkListener, CM_GetLogicLinkByAddr/ByLcid; CM_RegTransChannelListener; CM_RegDynTransChannelCbks, CM_DynTransChannelEstablishReq/ReleaseReq; CM_SetPhy, CM_GetRssi, CM_SetRealACBSubrate; ICB: CM_ICBRegisterCbk, CM_ICGSetParam, CM_ICBAddConnection, CM_ICBSetupDataPath.

## 6. CM hookup into SSAP/DTAP/QOSM

- Module IDs: ADPT=0, CM_SIGNALING=1, DTAP=2, SM=3, **SSAP=4**, HADM=5, CM_DYNTRANS=6, QOSM=7, BNL=8.
- SSAP: CM_RegLogicLinkListener(CM_MODULE_SSAP), drives connections via CM_DirectConnectAdd/Remove; app data on TCID_SLE_SMTC 0x0A.
- DTAP hosts CMTC signaling path; QOSM consumes dyn-trans + ICB APIs.

## Implication for WS73

All connection DLI opcodes (0x1401/1402/1403, 0x1801/02/04/07/08, 0x2801-0x280E) verified accepted on our hardware. The slim host stack needs: connect (0x1401 with DLI_ConnectionCreateParam) → wait 0x0015 → 0x1802/0x1804 → then SSAP on SMTC 0x0A. Signaling on CMTC 0x02 with {code,id,len} framing is the peer-negotiation path (capability + dyn channel).
