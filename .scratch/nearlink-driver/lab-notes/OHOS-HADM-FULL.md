# OHOS HADM Ranging — Full-Chain Anatomy (App → Algorithm Adapter → DLI 0x2003/0x2005 → IQ/ToF Parse → Distance)

**Date:** 2026-08-17
**Author:** research subagent (nearlink-driver)

## Sources

Primary (OHOS `communication_nearlink_service/` at `/mnt/hdd/nearlink-stuff/communication_nearlink_service/`):

- `interfaces/inner_api/include/nearlink_sle_ranging.h` (public app API)
- `frameworks/native/nearlink_sle_ranging.cpp` (native impl + algorithm-adapter call site)
- `frameworks/ranging_alogorithm_adapter/` (`ranging_alogorithm_adapter_def.h`, `src/ranging_alogorithm_adapter.cpp/.h`) — DL-loaded algorithm `.so`
- `utils/include/nearlink_hadm_sounding_result.h` + `utils/src/nearlink_hadm_sounding_result.cpp` — IQ carrier struct
- `ipc_parcel/interface/i_nearlink_hadm_client.h`, `ipc_parcel/parcel/nearlink_hadm_client_sounding_result.h` — IPC surface
- `services/server/src/nearlink_hadm_client_server.cpp`, `services/service/src/hadm/nearlink_hadm_client_service.cpp`, `.../hadm/nearlink_hadm_stack_adapter.cpp`, `.../hadm/hadm_defines.h` — service layer
- `services/stack/src/cp/bsl/sle/hadm/` — `hadm_api.h`, `hadm_user_proc.c`, `hadm_sm.c`, `hadm_config_dli.c`, `hadm_config_cm.c`, `hadm_listen_dli.c`, `hadm_parser_iq.c/.h`, `hadm_link_manager.c`
- `services/stack/src/dli/interface/` — `dli_opcode.h`, `dli_cmd_struct.h`, `dli_event_struct.h`, `dli_def.h`
- `services/stack/src/dli/cmd/src/dli_cmd.c`, `services/stack/src/dli/event/src/dli_hadm_event.c`, `services/stack/src/dli/sapi/src/dli_sapi.c`
- `services/stack/src/cp/nlstkfwk/cfgdb/src/nlstk_cfgdb.c`, `services/stack/src/adapter/src/{hadm,dli}_reg_ext_func.c`, `services/stack/src/adapter/src/hadm_ext_func_wrapper.c`, `services/stack/src/nai/nlm/src/nlstk_init.c`

Our side references:
- `WS63-HADM-LL.md` (chip-SDK API + 0x2001-0x2005 mapping, enable polarity)
- `OHOS-HADM-RANGING.md` (earliest OHOS ranging study)
- `SLE-CONTROL-PLANE.md` (our DLI command verification)

## 全链路数据流 (end-to-end data flow)

```
App / caller
  NearlinkSleRanging::StartSleRanging(device, RangingConfig{refreshRate, algoMode, toneControl})
  ├─ HadmRangingAdapter::InitHadmAlgo(algoMode)              [ranging_alogorithm_adapter.cpp:55-86]
  │    dlopen("libnearlink_measure.z.so") → dlsym measure_init/measure_set_algo_mode/measure_alg_func
  └─ proxy->StartSounding(hadmId, addr, toneControl)         [nearlink_sle_ranging.cpp:145-162]
      IPC → NearlinkHadmClientServer::StartSounding            [server:239-254]
        check AcbConnected, toneControl<=SINGLE_TONE
        → InterfaceHadmClientService::StartSounding(hadmId, addr, toneControl)
           HadmClientService::StartSounding                    [service:251-312]
             verification (HADM_FULL_SCENARIO) / carkey preemption / low-latency check / priority allowlist
             → pimpl->stackAdapter_.StartSounding(addr, callerName, toneControl)   [stack_adapter.cpp:147-165]
                HadmStartSounding(addr, &connParam, &soundingParam)                 [hadm_api.h:150]
  → stack: HadmUserStartSounding → HadmUserStartInvaidCheck(parallel limit=1)      [user_proc.c:43-108,110-153]
  → state machine HadmTriggerStateMachine(USER_START_SOUNDING_EVENT)               [hadm_sm.c:78-162]
      (pre-work: CM features event → HadmReadRemoteMeasureCaps (0x2002) → SOUNDING_READY)
      1. HadmProcUserStartWhenSoundingReady → HadmSetConnectionParamToCm (CM conn update) [sm:351-370, config_cm.c:21-39]
      2. HadmProcUpdateConnParamEvent → HadmSetMeasureParam(lcid, soundingParam)   [sm:372-400, config_dli.c:125-152]
           BuildMeasureParam → DLI_SetMeasureParam (DLI 0x2003)                    [config_dli.c:93-123]
      3. HadmProcConfigResultEventInConfigSounding → HadmSetMeasureEnable(lcid, 0=ENABLE) [sm:409-437]
           DLI_SetMeasureEnable (DLI 0x2005, enable=0)          [config_dli.c:167-195]
      4. HadmProcEnableResultEventInEnableSounding → state=SOUNDING, report START success [sm:439-467]
  chip
     0x2001 READ_LOCAL_MEASURE_CAPS (at stack init, nlstk_cfgdb.c:158-190)
     0x2002 READ_REMOTE_MEASURE_CAPS
     0x2003 SET_MEASURE_CONFIG_PARAM  (DLI_SetMeasureConfigParam, 30-byte field struct)
     0x2005 SET_MEASURE_EN             (DLI_SetMeasureEnableParam{connHandle, enable=0})
     events: 0x0028 MEASURE_IQ_REPORT, 0x0029 MEASURE_STATE_CHANGE          [dli_opcode.h:204-211]
  up-path
     DLI_CsIqReportCbk → HadmListenDli EvtReportSlemIQ                       [dli_hadm_event.c:32-42, listen_dli.c:111-143]
        HadmPaserIqInfoFromDli (byte parse)                                  [parser_iq.c:101-147]
        HadmReportSoundingIqInfoFromDli (local/remote pairing, ts diff<=10)  [sm.c:718-755]
        HadmBuildIqDataToService → HadmSoundingIqData_S (tof→cm, offsets)    [sm.c:618-680]
     stack adapter onReportSoundingIQResult → NearlinkHadmSoundingResult     [stack_adapter.cpp:60-98]
     service OnSoundingResult → ReportSoundingIQResult → IPC                  [service:139-150,388-399]
     server OnSoundingResult → observer callback                              [server:108-126]
     app HadmClientCallbackStubImpl::OnSoundingResult                        [sle_ranging.cpp:52-66]
        HadmRangingAdapter::CalculateHadmDistance(result, DisResult)          [adapter.cpp:132-153]
           TransferSoundingToAlgPara → MeasureAlgPara                         [adapter.cpp:89-130]
           calcHadmDis_(&result, &algPara)   // libnearlink_measure.z.so
        → RangingResult(distance=disSmoothed, prob, rssi) → OnSleRangingResult [sle_ranging.cpp:60-65]
```

Result path: **chip I/Q+ToF → stack parse/pair → service IPC → app-side DL algorithm → `disSmoothed` m**. There is **no distance algorithm inside the stack**; the stack only converts ToF to cm (see 结果解析). All smoothing/algorithm lives in `libnearlink_measure.z.so` loaded in the client framework process.

## DLI 命令参数语义 (0x2003 config / 0x2005 enable)

### HadmSoundingParam_S → DLI_SetMeasureConfigParam (BuildMeasureParam, hadm_config_dli.c:93-123)

| `DLI_SetMeasureConfigParam` field | source | value / semantics |
|---|---|---|
| `connHandle` | lcid | |
| `configId` | args->configId | 0 (stack_adapter.c:117) |
| `measureConfigDirect` | const | **0x90010004** (dli_def.h:35) — 测量量配置指示 bitmask |
| `occurrenceGroupPeriod` | args->occurrenceGroupPeriod | 0x0000 default (stack_adapter.c:123) |
| `schedulingTimeslot` | const | **SCHEDULING_TIME_SLOT_125 = 4** → 125us 系统调度时隙 (dli_def.h:31) |
| `rttPhy` | args->rttPhy | 0 (测量信号带宽指示) |
| `freqHoppingMode` | args->freqHoppingMode | 0 |
| `fmFreq` | args->fmFreq | **0x0015** (init freq, hadm_defines.h:49) |
| `sendDirect` | const | **TX_ORDER_SECOND = 1** (dli_cmd_struct.h:284-287) — 对端先发后发指示: 0=先发,1=后发; always 后发 |
| `antennaOrderConfig`/`first/secondAntennaTypeConfig` | const | 0 |
| `eventsCount`/`bitWidth` | const | 0 |
| `pmInitAntCount`/`pmReflAntCount` | args | 0 (default single antenna) |
| `pmInitSignal2Tone`/`pmReflSignal2Tone` | args, passed through **HADM_ExtCheckAndUpdateMultiToneConfig** (config_dli.c:99-100, wrapper:30-40) | 多音指示, ext plugin may override per toneControl |
| `first/secondNodeInterval` | const | 0 |
| `channelBandwidth` | const | **FREQUENCY_BAND_2_4GHZ = (1<<0)** (dli_cmd_struct.h:272-276) — 2.4GHz hop-band bit present |
| `pm2400mBand[10]` | args->pm2400mBand | 80-bit channel map; default **{0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF,0x7F}** (79 channels) (stack_adapter.c:116) |

`HadmSoundingParam_S` (hadm_api.h:43-72) carries the full timing fields that the OHOS service fills with **fixed product defaults** in `SetSoundingParam` (stack_adapter.c:114-145): `fmInteractionType=0x0002`, `fmOccurrenceGroupInterval=0x0078`, `pmMeasureType1Interval=0x0064`, `pmMeasureType2Interval=0x003C`, `fmTIp1Time=pmTIp2Time=0x0078`, `fmTGuard=0x0A`, `fmSignal2Length=0x64`, `pmFreqHoppingBand=0x01`, `glpMode=0`, `sleHadmMode=toneControl`, `isCsParamChg=0`, `freqSpace=0`, `conAnchorNum=1`, `refreshRate=0`, `acbInterval=0`, `csInterval=0`. Most of these have **no DLI_NewMeasure 0x2003 counterpart** — they are the *old* wire layout (`DLI_MeasureConfigExtParam`, dli_def.h:200-230, "与HadmSoundingParam_S相差一个connHandle") used only on the ext-func branch (`DLI_GetExtFuncList()->setMeasureParamExt`, config_dli.c:137-144), i.e. **DLI_VERSION != 1.1**. On the new branch (DLI_VERSION_1_1, dli_cmd.c:165-170), only the subset in the table above is sent; firmware computes the timing.

### 0x2005 SET_MEASURE_EN semantics

`DLI_SetMeasureEnableParam{connHandle, enable}` (dli_cmd_struct.h:314-317). Polarity is **inverted vs. intuitive**:

- `enable = 0` = `HADM_SOUNDING_ENABLE` = **START** (hadm_sm.c:27-29)
- `enable = 1` = `HADM_SOUNDING_DISABLE` = **STOP** (release link resources)
- `enable = 2` = PAUSE (defined `HADM_SOUNDING_PAUSE`, sm.c:29, and `DLI_MeasurementTargetState`, dli_cmd_struct.h:278-282)

Start sequence in `HadmProcUserStartWhenSoundingReady` → `HadmProcUpdateConnParamEvent` → `HadmProcConfigResultEventInConfigSounding` (hadm_sm.c:351-437): **CM connection-param update first, then 0x2003, then 0x2005(0)**. Each DLI command result (0x2003 config-cmpl, 0x2005 enable-cmpl) is matched back to the sender via the FIFO `g_hadmDliCmdVec` (`HadmPushDliCmd`/`HadmPopLastDliCmd`, config_dli.c:57-91, listen_dli.c:63-105) because DLI results don't carry the link.

Caps probing happens before all this: on CM features event, `HadmProcFeatruesEventInIdle` checks peer sounding feature then `HadmReadRemoteMeasureCaps(lcid)` (0x2002) → `HadmProcRemoteCsEventInReadRemoteCs` caches remote CS caps and moves to SOUNDING_READY (sm.c:284-349). Local caps are read once at stack init via cfgdb (`DLI_ReadLocalMeasureCaps` 0x2001, nlstk_cfgdb.c:158-190, DLI_ReadLocalCsCapsCbk stores 21-byte blob).

## 结果解析 (IQ/ToF result parsing)

### Event header — 0x0028 MEASURE_IQ_REPORT

Wire `DLI_CsIqReportEvt` (dli_event_struct.h:304-311): `status, connHandle, slemIdx, DLI_SlemInfoType(2B bitmask), timestampSn(u32), data[]`.

`DLI_SlemInfoType` bitmask (dli_event_struct.h:290-302): aoa:1, aod:1, chnlInfo24g:1, chnlInfo51g:1, chnlInfo58g:1, freqDiff:1, **tof:1**, **chnlMeas:1**, sinr:1, rsvd:6, **vender:1**. Optional section length computed per bit in `DLI_GetSlemInfoDataLen` (dli_hadm_event.c:150-162): tof→+`DLI_SlemTof`(u32), chnlMeas→+`DLI_SlemChnlMeas`(rssi, iqBitLen, iqData), vender→+`DLI_SlemVender`.

### hadm_parser_iq.c byte layout (HadmPaserIqInfoFromDli, parser_iq.c:101-147)

Sequential parse:
1. `status`(1), `connHandle`(2), `slemIdx`(1), `slemInfoType`(2), `timeStampSn`(4), `slemChmap`(10B = `HadmSlemChmap_S`, 80-bit 2.4G channel map)
2. **multi-tone = bit10 of slemInfoType** (`isBit10Set`, parser_iq.c:115-117); `iqChnlNum = popcount(chmap) × (multiTone?4:1)` (parser_iq.c:37-66, `HADM_MULTI_CHNL_NUM=4`)
3. `tofResult`(u32), `rssi`(1), `iqBitLen`(1)
4. if multi-tone: skip 1 byte 多音信息
5. **IQ samples: 3 bytes each, 12-bit packed** (`HadmGetIqData`, parser_iq.c:81-99):
   `i = b0 | ((b1 & 0x0F) << 8)`, `q = (b2 << 4) | ((b1 & 0xF0) >> 4)` — matches DLI_SlemIqData doc (dli_event_struct.h:271-278)
6. tail: multi-tone order `localId, remoteId, venderLen`; single-tone order `venderLen, localId, remoteId` (parser_iq.c:136-144)

Max channel count `HADM_IQ_MAX_CHNL_NUM = 316` = 79×4 (parser_iq.h:31).

### ToF → distance conversion (hadm_sm.c:633-644, authoritative comment)

- `tofResult` is in **0.1ns units** (chip multiplies raw ns ×10). Distance single-ended:
  **`distance = tof_result × 10^-9 × 3×10^8 × 10^-1 m = tof_result × 0.03 m`**
- Stack converts to **cm** before handing up: `dutTof = localIqInfo->tofResult * 3 / 100` (hadm_sm.c:642-644). **So `HadmSoundingIqData_S.dutTof/rtdTof` are already centimeters**, NOT raw counts — this is a critical semantic for our parser.
- **Two-ended average** (comment, sm.c:637-639): `双端测距值 = (localTof + remoteTof) × 0.03 / 2 − tofCalib × 2`, where tofCalib = ToF calibration value. In practice the algorithm adapter passes `tofDut`/`tofRtd` (cm) plus the four offsets into `MeasureAlgPara` and lets the `.so` do the final math (adapter.cpp:113-114,123-126).
- **Calibration offsets**: from local caps cfgdb + remote caps cache, defaulted when out-of-range:
  - `phaseCaliOffsetCm` default **300** cm (range 1-900) — "相位校准offset"
  - `tofCaliOffsetM` default **1385** m (max 3000) — "TOF校准offset" (hadm_sm.c:32-37,574-616)
  Both local+remote offsets are attached to every `HadmSoundingIqData_S`/`NearlinkHadmSoundingResult` (sm.c:662-678) and passed into the algorithm.

### IQ pairing (local/remote)

`HadmReportSoundingIqInfoFromDli` (sm.c:718-755): report with `localId==0` cached as local; else require local present and **`|ts_remote − ts_local| ≤ HDAM_SOUNDING_TS_MAX_DIFF (10)`**, then cache as remote and build combined result. `HadmBuildIqDataToService` requires equal `iqChnlNum` on both ends (sm.c:623-626). Combined report clears both caches each round (sm.c:711-713).

## 算法适配器 (ranging_algorithm_adapter)

Interface (`ranging_alogorithm_adapter.h:24-28`):
- `typedef errcode_slem (*CalcHadmDis)(DisResult*, const MeasureAlgPara*)` ← symbol **`measure_alg_func`**
- `typedef void (*InitHadmAlg)(void)` ← symbol **`measure_init`**
- `typedef void (*SetAlgoModeFunc)(uint32_t)` ← symbol **`measure_set_algo_mode`**

Load mechanism (adapter.cpp:55-77): singleton `HadmRangingAdapter`; `dlopen("libnearlink_measure.z.so", RTLD_NOW)`; `dlsym` the 3 symbols; `setAlgoMode_(algoMode)` optional (null-safe); `initHadmAlg_()` called from `InitHadmAlgo` (adapter.cpp:79-86) with default `RANGING_ALGO_MODE_ONE`. `CleanUp()` dlcloses (adapter.cpp:155-166). Invoked from app side (`InitHadmAlgo` on StartSleRanging, `CleanUp` on StopSleRanging, sle_ranging.cpp:158,177) and from `CalculateHadmDistance` on each sounding result (sle_ranging.cpp:57).

Input `MeasureAlgPara` (def.h:85-107): `curTime(ms), iqDut[], iqRtd[] (algIq{iData,qData u16}), iqChnlNum, tofDut, tofRtd, totalCount, rssiDut, rssiRtd, keyId, paraLimit{PararPair}, flagInter(DisAlgType), isMultiTone, dutIqBitLen/rtdIqBitLen (default 12), dutSlemChmap/rtdSlemChmap[10], localNvOffset, remoteNvOffset, localTofOffset, remoteTofOffset`.

Adapter constants: `PARA_LIMIT_VALUE = {-98, 2, 20.0f}` (rssiLimit, rStart, thresholdCond2), `flagInter = METHOD_ADJ_R_END` (def.h:33, adapter.cpp:112).

Output `DisResult` (def.h:29-44): `disSmoothed, disOri, disSlightSmoothed, prob, rssi, height, smoothNum` — `disSmoothed` is the distance reported to app.

Algorithm enum `DisAlgType` (def.h:55-72): METHOD_1M (1, 150m cap), METHOD_2M, METHOD_1M_2M (75m cap), **METHOD_ADJ_R_END** (1MHz FH dynamic-r, 150m), _V2, _V3. Error codes `errcode_slem` (def.h:116-154): `ERRCODE_SLEM_TOF_IQ_NOTMATCH`, `ERRCODE_SLEM_IQ_LOW_ENERGY`, `ERRCODE_SLEM_TOA_ABNORMAL`, etc. `CalculateHadmDistance` returns `NL_RANGING_RESULT_ERR` when the algorithm fails (adapter.cpp:147-148).

`TransferSoundingToAlgPara` (adapter.cpp:89-130) converts `NearlinkHadmSoundingResult` (I/Q vectors u16, rssi, tof cm, isMultiTone, iqBitLen, chmaps, offsets) into `MeasureAlgPara`. `IsCompleteData()` gate requires valid addr, nonzero rssi both ends, non-empty and equal-size I/Q vectors (sounding_result.cpp:138-150).

## 实现蓝本 (our WS73 ranging client blueprint)

To add an HADM ranging client to our stack we need, bottom-up (all semantics directly reusable from OHOS):

1. **DLI command layer** (we already have 0x2001/0x2002/0x2005 accepted; verified in SLE-CONTROL-PLANE.md):
   - `read_local_measure_caps()` → 0x2001, no payload
   - `read_remote_measure_caps(connHandle)` → 0x2002, payload `{u16 connHandle}`
   - `set_measure_config_param(DLI_SetMeasureConfigParam)` → 0x2003, payload = the 30-byte struct (connHandle, configId, measureConfigDirect=0x90010004, occurrenceGroupPeriod, schedulingTimeslot=4, rttPhy, freqHoppingMode, fmFreq=0x0015, sendDirect=1, antenna fields, eventsCount, bitWidth, pmInitAntCount, pmInitSignal2Tone, firstNodeInterval, pmReflAntCount, pmReflSignal2Tone, secondNodeInterval, channelBandwidth=0x01, pm2400mBand[10]=79-ch default)
   - `set_measure_enable(connHandle, enable)` → 0x2005, **enable=0 START / 1 STOP / 2 PAUSE** (fix any inverted assumption)
2. **Command-result association**: FIFO queue of `{lcid, expectCbkType}` pushed before each command, popped on `DLI_CMD_COMPLETE_EVT` (copy `HadmPushDliCmd`/`HadmPopLastDliCmd`). Confirmation events are `DLI_CMD_COMPLETE_EVT (0x0002)` for 0x2003/0x2005 (dli_cmd.c:1077-1103); caps use status events `0x040F`/`0x002B`/`0x002C`.
3. **Caps parsing**: decode local 21-byte blob and remote priv event per `DLI_ReadLocal/RemoteCsCapsPrivEvt` field order (dli_event_struct.h:213-254): measureSignalCapabilitySupported(u32), measureReportingCapabilitySupported(u32), multiAntennasSupported, multiAntennasSwitchInterval, type1MinTimeIp1..4, type1MinTimeInterEvt, type2MinTimeInterEvt, minTimeInitializeInterEvt, type1MinTimeIntraEvt, type2MinTimeIntraEvt, minTimeInitializeIntraEvt, minTimeIntraEvtGroup, **phaseCaliOffsetCm, tofCaliOffsetM** (needed for calibration).
4. **IQ report parser** (copy `HadmPaserIqInfoFromDli`): header + 10B chmap + tofResult(u32 0.1ns) + rssi + iqBitLen + 3B/12-bit I/Q unpack + multi-tone ×4 + localId/remoteId tail; multi-tone bit = slemInfoType bit10.
5. **Pairing + ToF→cm**: pair local(localId==0) & remote by `|ts diff|<=10`, equal channel count; convert each `tofResult` to cm with `*3/100`; attach local/remote `phaseCaliOffsetCm`/`tofCaliOffsetM` (defaults 300cm / 1385m).
6. **Service state machine** (copy `hadm_sm.c` table): IDLE→(features+remote caps)→SOUNDING_READY→(CM conn update)→CONFIG_CONNECTION→(0x2003)→CONFIG_SOUNDING→(0x2005=0)→ENABLE_SOUNDING→SOUNDING→(0x2005=1)→DISABLE_SOUNDING; single parallel session (`HADM_MAX_PARALLEL_SOUNDING_NUM=1`, user_proc.h:30).
7. **Algorithm side (optional)**: either port the `libnearlink_measure.z.so` interface (measure_init/measure_set_algo_mode/measure_alg_func, `MeasureAlgPara`/`DisResult`) or — for our embedded driver — implement a minimal ToF-based fallback: single-end `tof×0.03 m`, two-end `(local+remote)×0.03/2 − tofCalib`, since the stack already hands cm + offsets. This is the only place distance math happens.

Function list to implement (OHOS names → ours): `HadmStartSounding`/`HadmStopSounding`/`HadmGetSoundingState`/`HadmGetSoundingAddrInfo`/`HadmRegCbk` (hadm_api.h:123-174) + `HadmReadRemoteMeasureCaps`/`HadmSetMeasureParam`/`HadmSetMeasureEnable` (config_dli.h:39-49). Structs: `HadmConnectionParam_S`, `HadmSoundingParam_S`, `DLI_SetMeasureConfigParam`, `DLI_SetMeasureEnableParam`, `HadmSoundingIqData_S`, `HadmIqInfoFromDli_S`, `HadmRemoteCsParam_S` — all copyable field-for-field.

## 结论

**Ranging end-to-end is feasible on our WS73 dongle.** We already verified 0x2001/0x2002/0x2005 accepted; OHOS now supplies everything missing:

- Full 0x2003 payload semantics (measureConfigDirect=0x90010004, schedulingTimeslot=4=125us, channelBandwidth=2.4GHz bit0, 79-channel pm2400mBand, sendDirect=后发) and 0x2005 enable=0=START.
- Correct command order: CM connection-param update → 0x2003 → 0x2005(0), one session at a time.
- 0x0028 IQ report byte layout (3B/12-bit I/Q, 10B chmap, multi-tone ×4, tofResult u32 0.1ns), 0x0029 measure-state event.
- ToF→cm (`*3/100`) at stack, distance math (`×0.03`, two-end avg, tofCalib) in the algorithm adapter — we can replicate with a minimal fallback if we skip the proprietary `.so`.
- Caps decode (incl. phaseCaliOffsetCm/tofCaliOffsetM) for calibration.
- Key nuance: on new-DLI firmware (V1.1) only the 0x2003 subset is on the wire; on old firmware the wire payload is the full `HadmSoundingParam_S` layout via ext-func branch — our WS73 validation should confirm which variant (see Open questions, also WS63-HADM-LL §7).

待实现清单 = the 7 blue-print items above. No OHOS dependency: everything is byte-format and state-machine semantics we can re-implement in C for our driver.

## Open questions

- **New vs. old DLI branch on WS73**: `DLI_IsSupportNewDisMeasure()` keys on DLI version (1.1 = new, dli_cmd.c:165-170). Our firmware's version determines whether 0x2003 carries `DLI_SetMeasureConfigParam` (~30B) or `DLI_MeasureConfigExtParam`/`HadmSoundingParam_S` (~46B) — must confirm from the 0x2003 command-complete round-trip size.
- Whether our dongle firmware emits **0x0028 (standard) or 0x004A (vendor)** IQ report opcode — OHOS registers both constants (dli_opcode.h:200) but only 0x0028 is consumed by `DLI_CsIqReportCbk` in this tree.
- Whether a peer device will actually start transmitting sounding for us: ranging requires both ends enabled, and the remote side must accept our 0x2003 config — we have no NearLink peer yet.
- `toneControl`/`sleHadmMode` exact meaning of `HADM_ExtCheckAndUpdateMultiToneConfig` (ext plugin, not in this tree) — single-tone vs multi-tone override semantics are firmware/plugin-defined.
- `freqSpace` (0=1M/1=2M) and `refreshRate` (1/2/4) mapping to actual acb/cs interval computation is done in firmware/plugin, not in host code — for now we reuse OHOS defaults.
