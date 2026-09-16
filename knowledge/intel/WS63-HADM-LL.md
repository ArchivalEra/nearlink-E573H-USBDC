---
type: intel
title: "WS63 (fbb_ws63) HADM Ranging + Low-Latency API — Semantic Gap Analysis vs. our DLI Layer"
language: zh
created: 2026-08-17
tags: [intel, ws63, hadm, ranging]
sources:
  - "https://github.com/x-eks-fusion/fbb_ws63"
  - "https://github.com/gtxaspec/ws73v100-wifi"
trust: B
stale_after: 2027-02-17
---

# WS63 (fbb_ws63) HADM Ranging + Low-Latency API — Semantic Gap Analysis vs. our DLI Layer

**Date:** 2026-08-17
**Author:** research subagent (nearlink-driver)

## Sources

- WS63 SDK (primary): `https://github.com/x-eks-fusion/fbb_ws63/tree/master/src/`
  - `include/middleware/services/bts/sle/sle_hadm_manager.h` (342 lines)
  - `include/middleware/services/bts/sle/sle_low_latency.h` (347 lines)
  - `include/middleware/services/bts/sle/sle_glp_manager.h` (44 lines)
  - `include/middleware/services/bts/sle/sle_connection_manager.h` (low-latency callback, lines 496-577)
  - `protocol/bt/host/gle/ws63-liteos-app/libbth_gle.a` (prebuilt GLE stack; symbols via `strings`/`ar t`)
  - `protocol/bt/host/gle/Kconfig` (FEATURE_GLE_LOW_LATENCY / FEATURE_GLE_HADM, default n)
- WS73 SDK (byte-identical headers): `https://github.com/gtxaspec/ws73v100-wifi,sle_low_latency,sle_glp_manager}.h`
- OHOS Nearlink service (semantic reference):
  - `communication_nearlink_service/services/stack/src/cp/bsl/sle/hadm/include/hadm_api.h`
  - `.../hadm/src/hadm_config_dli.c` (HadmSoundingParam_S → DLI_SetMeasureConfigParam mapping)
  - `.../hadm/src/hadm_parser_iq.c` (IQ report parser, 3-byte/12-bit I/Q)
  - `.../hadm/src/hadm_sm.c` (state machine)
  - `.../dli/interface/dli_cmd_struct.h` (DLI_SetMeasureConfigParam, DLI_SetMeasureEnableParam, DLI_MeasurementTargetState, DLI_FrequencyBand, DLI_ICGParam)
  - `.../dli/interface/dli_event_struct.h` (DLI_ReadCsCapsEvt/Priv, DLI_CsIqReportEvt, DLI_SlemInfoType, DLI_AcbLowLatencyEnableEvt)
  - `.../dli/interface/dli_def.h` (MEASURE_CONFIG_DIRECT=0x90010004, SCHEDULING_TIME_SLOT_125=4, SLE_MEASURE_PM_24G_BAND_LEN=10)
  - `.../dli/interface/dli_opcode.h` (0x2001-0x2005, 0x2801-0x280D)
- Our prior notes: `OHOS-HADM-RANGING.md`, `SLE-CONTROL-PLANE.md`

## Key finding up front

The WS63 and WS73 chip-SDK HADM/low-latency headers are **byte-identical** (verified by full read of both trees). The WS63 SDK ships **no HADM/measure/ranging application example** — `application/` only has `sle_speed_*` and `sle_uuid_*`; the HADM/GLP/low-latency code lives only in the prebuilt `libbth_gle.a` (objects `gle_aa_hadm_ui.c.obj`, `gle_glp_core.c.obj`, `gle_sample_low_latency_mouse.c.obj`, `gle_sample_low_latency_dongle.c.obj`). So the header surface below is the full normative API we can rely on.

## 1. HADM (channel sounding) API inventory

From `fbb_ws63/src/include/middleware/services/bts/sle/sle_hadm_manager.h` (=== `ws73v100-wifi/include/bsle/sle/sle_hadm_manager.h`):

| API | Signature | Notes |
|---|---|---|
| read local caps | `errcode_t sle_read_local_channel_sounding_caps(void)` (hdr:240) | result via callback |
| read remote caps | `errcode_t sle_read_remote_channel_sounding_caps(uint16_t conn_id)` (hdr:259) | |
| set CS param | `errcode_t sle_set_channel_sounding_param_ex(uint16_t conn_id, sle_set_channel_sounding_param_ex_t *param)` (hdr:282) | the "ex" (extended) variant |
| enable CS | `errcode_t sle_set_channel_sounding_enable(uint16_t conn_id)` (hdr:299) | |
| disable CS | `errcode_t sle_set_channel_sounding_disable(uint16_t conn_id)` (hdr:316) | |
| register callbacks | `errcode_t sle_hadm_register_callbacks(sle_hadm_callbacks_t *func)` (hdr:333) | |

Callbacks struct `sle_hadm_callbacks_t` (hdr:206-223):
- `read_local_cs_caps_cb(caps, status)` — `sle_channel_sounding_caps_t*`
- `read_remote_cs_caps_cb(conn_id, caps, status)`
- `cs_state_changed_cb(status)` — enable/disable completion
- `cs_iq_report_cb(conn_id, report)` — the ranging result channel

Data types:
- `sle_channel_sounding_caps_t` = opaque `uint8_t caps[SLE_CS_CAPS_LEN]`, `SLE_CS_CAPS_LEN = 0x15` (21 bytes) (hdr:23,92-95). **Same as OHOS `SLE_MEASURE_LEN` caps blob.**
- `sle_set_channel_sounding_param_ex_t` (hdr:47-64):
  `is_cs_param_chg, freq_space, con_anchor_num, refresh_rate, acb_interval(u16), cs_interval(u16), posalg_freq(u16), glp_mode`
- `sle_channel_sounding_iq_report_t` (hdr:73-83):
  `samp_cnt, report_idx, es_sn(u16), timestamp_sn(u32 aligned to master), rssi[20], freq[20], i_data[20], q_data[20], tof_result(u32)` — `SLE_CS_IQ_REPORT_COUNT=20`.

### Difference vs. OHOS-HADM-RANGING.md semantics

- **Param richness**: WS63 `_ex_t` is a *simplified subset* — it carries ONLY the tail fields (isCsParamChg, freqSpace, conAnchorNum, refreshRate, acbInterval, csInterval, posalgFreq, glpMode) of the full OHOS `HadmSoundingParam_S` (hadm_api.h:43-72). The full timing/PHY fields (configId, schedulingTimeslot, rttPhy, freqHoppingMode, fmFreq, fmInteractionType, occurrenceGroupPeriod, fmOccurrenceGroupInterval, pmMeasureType1/2Interval, fmTIp1Time, pmTIp2Time, fmTGuard, fmSignal2Length, pmInitAntCount, pmInitSignal2Tone, pmReflAntCount, pmReflSignal2Tone, pmFreqHoppingBand, pm2400mBand[10], sleHadmMode) are **not exposed** by the chip SDK header. They are filled inside the GLE/HADM service with defaults; on WS73 Android the JNI header is the same simplified struct (`ws73v100-wifi/.../Nearlink/jni/sle_hadm_manager.h:47-64`).
- `posalg_freq` = chip-SDK-only (algorithm frequency) — confirmed by OHOS-HADM-RANGING.md §2 ("posalg_freq is chip-SDK-only"); it has no DLI counterpart in `HadmSoundingParam_S`.
- Caps callback: OHOS stack parses the caps into a full struct (`DLI_ReadCsCapsEvt`/`DLI_ReadRemoteCsCapsPrivEvt`, dli_event_struct.h:196-233) including `multiAntennasSupported`, `multiAntennasSwitchInterval`, `type1MinTimeIp1..4`, `type1MinTimeInterEvt`, `type2MinTimeInterEvt`, `minTimeInitializeInterEvt`, `minTimeIntraEvt(Group)`, `phaseCaliOffsetCm`, `tofCaliOffsetM`; the chip SDK hands you the raw 21-byte blob.
- **No angle field** in the chip-SDK IQ report. AoA/AoD is only present as a bit in the DLI `slemInfoType` mask; angle is computed service-side from I/Q by the SLEM algorithm (see OHOS-HADM-RANGING.md §3).

## 2. Mapping CS params to DLI 0x2001-0x2005 (we verified commands accepted)

The OHOS `hadm_config_dli.c` `BuildMeasureParam()` (lines 93-123) is the authoritative mapping from the param struct to `DLI_SetMeasureConfigParam` (dli_cmd_struct.h:289-312):

| DLI command (verified accepted on WS73) | payload semantics now known |
|---|---|
| `0x2001 READ_LOCAL_MEASURE_CAPS` | no payload; event `DLI_ReadLocalCsCapsEvt` = 21B caps blob (dli_event_struct.h:186-188) |
| `0x2002 READ_REMOTE_MEASURE_CAPS` | payload `{u16 connHandle}` (dli_cmd_struct.h:268-270); event `DLI_ReadRemoteCsCapsPrivEvt` (status, connHandle, + full field breakdown incl. `measureSignalCapabilitySupported`, `phaseCaliOffsetCm`, `tofCaliOffsetM`) (dli_event_struct.h:213-233) |
| `0x2003 SET_MEASURE_CONFIG_PARAM` | `DLI_SetMeasureConfigParam`: connHandle, configId, **measureConfigDirect=0x90010004** (dli_def.h:35), occurrenceGroupPeriod, **schedulingTimeslot=4 → 125us** (dli_def.h:31), rttPhy, freqHoppingMode, fmFreq, sendDirect=TX_ORDER_SECOND(1), antennaOrderConfig, first/secondAntennaTypeConfig, eventsCount, bitWidth, pmInitAntCount, pmInitSignal2Tone, firstNodeInterval, pmReflAntCount, pmReflSignal2Tone, secondNodeInterval, **channelBandwidth=FREQUENCY_BAND_2_4GHZ=(1<<0)** (dli_cmd_struct.h:272-276), **pm2400mBand[10]** |
| `0x2005 SET_MEASURE_EN` | `DLI_SetMeasureEnableParam {connHandle, enable}` (dli_cmd_struct.h:314-317). **Counterintuitive: enable=0 = START, enable=1 = STOP/PAUSE** — `DLI_MeasurementTargetState` (dli_cmd_struct.h:278-282): START=0, STOP=1, PAUSE=2. Matches our OHOS-HADM-RANGING.md §1 step 4-5. |

Note: the chip-SDK `sle_set_channel_sounding_param_ex()` param `_ex_t` maps onto the *subset* of `DLI_SetMeasureConfigParam` (is_cs_param_chg ↔ `isCsParamChg` etc.); the WS73 GLE firmware computes `acb_interval`/`cs_interval` from the other params (header comment "根据参数计算得到的acb链路周期/测距周期", hdr:56-59). This matches OHOS `HadmSoundingParam_S` tail fields (hadm_api.h:66-71). The OHOS stack ALSO sends a CM connection-parameter update (HadmConnectionParam_S) before SET_MEASURE_CONFIG_PARAM (hadm_sm.c:349-381, `HadmSetConnectionParamToCm`), then 0x2003 then 0x2005 — that ordering is required for a real ranging session.

The GLE low-latency scheduling API and IOG (0x2801) are *separate* from HADM — see §4.

## 3. Ranging result callback fields (chip SDK vs. DLI wire)

Chip SDK `sle_channel_sounding_iq_report_t` (hdr:73-83) fields vs. wire:

- `samp_cnt`, `report_idx`, `es_sn` — housekeeping
- `timestamp_sn` — u32, aligned to master clock (hdr:77)
- `rssi[20]`, `freq[20]` — per-sample RSSI and frequency index
- `i_data[20]`/`q_data[20]` — u16 I/Q per sample
- `tof_result` u32 — **0.1ns units → distance = tof_result × 0.03 m**; two-ended `(local+remote)×0.03/2 − tofCalib×2` (OHOS-HADM-RANGING.md §4).

Wire format (OHOS parser, `hadm_parser_iq.c`):
- Event header `DLI_CsIqReportEvt` (dli_event_struct.h:304-311): status, connHandle, slemIdx, `DLI_SlemInfoType` bitmask (aoa/aod/chnlInfo24g/51g/58g/freqDiff/tof/chnlMeas/sinr/vender, dli_event_struct.h:290-302), timestampSn, then data.
- **3 bytes per sample, 12-bit I/Q**: `i = b0 | ((b1 & 0x0F)<<8)`, `q = (b2<<4) | ((b1 & 0xF0)>>4)` (hadm_parser_iq.c:94-96).
- 10-byte chmap (79 channels), multi-tone ×4 → max 316 channel samples (hadm_parser_iq.c:61-65).
- tofResult u32, rssi, iqBitLen; multi-tone bit = bit10 of slemInfoType (hadm_parser_iq.c:115-117).
- Service-side pairing: local (localId==0) + remote IQ by timestamp diff ≤10; distance computed at service layer, **no distance algorithm in stack** (OHOS-HADM-RANGING.md §4).
- Calibration offsets from caps: `phaseCaliOffsetCm`, `tofCaliOffsetM` (dli_event_struct.h:209-210).

**Gap for us**: our driver validated 0x2001/0x2002/0x2005 accepted but we have not (a) parsed the 21-byte caps blob, (b) exercised the IQ report event (0x0028 / vendor 0x004A per OHOS-HADM-RANGING.md §3), (c) applied distance conversion, (d) handled enable=0↔START semantics.

## 4. Low-latency API (sle_low_latency.h) vs. our DLI low-latency commands

From `fbb_ws63/src/include/middleware/services/bts/sle/sle_low_latency.h` (=== WS73 copy):

- Rate enum `sle_low_latency_rate_t` (hdr:32-54): 125Hz, 500Hz, 1K, 2K, 3K, 4K, 5K, 6K, 7K, 8K.
- `sle_set_acb_low_latency_t {conn_id, enable, rate}` (hdr:87-94) — **ACB = connected (basic) link low-latency scheduling**, not IOG.
- Core setter: `errcode_t sle_low_latency_set(uint16_t conn_id, uint8_t enable, uint16_t rate)` (hdr:268).
- Role-based init + callback registration:
  - Mouse (host device): `sle_low_latency_mouse_enable()` (hdr:165), `sle_low_latency_mouse_register_callbacks()` (hdr:180) with `low_latency_key_value_set_callback(button_mask,x,y,wheel)` (hdr:113).
  - Dongle (receiver): `sle_low_latency_dongle_enable()` (hdr:249), `sle_low_latency_dongle_register_callbacks()` (hdr:283) with `low_latency_report_callback(data,len)` (hdr:195).
  - TX/RX generic: `sle_low_latency_tx_enable()` (hdr:326) + `tx_register_callbacks()` with `low_latency_general_tx_callback(uint8_t *len)` returning TLV data pointer (hdr:128); `sle_low_latency_rx_enable()` (hdr:339) + `rx_register_callbacks()` with `low_latency_general_rx_callback(len, value)` (hdr:210).
- Status enums: `sle_low_latency_status_t {DISABLE, ENABLE}` (hdr:63-68); `sle_low_latency_value_set_status_t {SUCCESS, FAIL}` (hdr:77-82, "获取鼠标数据失败,失败后不会发送数据").
- Connection manager also exposes `sle_low_latency_callback(status, addr, rate)` (sle_connection_manager.h:513) in `sle_connection_callbacks_t` (low_latency_cb, :562) — confirmation path.

**DLI mapping**: this maps to `DLI_ACB_LOW_LATENCY_EN` (opcode `0x0013` evt, dli_opcode.h:177) → `DLI_AcbLowLatencyEnableEvt {connHandle, status, enable, rate}` (dli_event_struct.h:333-338, "下发低时延时调度最大能力"). GLE prebuilt lib symbols confirm: `gle_set_acb_low_latency_request`, `sapi_gle_set_acb_low_latency_request`, `uapi_gle_acb_low_latency_set`, `gle_read_low_latency_param`.

**vs. our verified DLI low-latency commands** (SLE-CONTROL-PLANE.md §补充验证):
- Our `SET_IOG_PARAM 0x2801 / 0x2802 TEST` (accepted) are the **IOG synchronous link** commands — i.e. `DLI_SetICGParam` (dli_cmd.c:1110-1158), struct `DLI_ICGParam` (dli_cmd_struct.h:417-426): type, opCode, id, `sduIntervalG2T/T2G` (us), sca, packing, framing, `maxLatencyG2T/T2G` (ms), icbCnt, paramCnt, per-ICB `{maxSduG2T, maxSduT2G, rtnG2T, rtnT2G}`. IOG=point-to-point sync unicast group, IMG (0x2807)=multicast. `DLI_CREATE_IOB 0x2803` and `DLI_SETUP_ICB_DATA_PATH 0x280D` (both verified accepted) are the IOG data-channel commands.
- The WS63 `sle_low_latency.h` API is the **ACB (connected-link) low-latency scheduling** feature (125Hz-8K report rate) — a *different* mechanism from IOG. IOG is the "同步链路" (synchronous link / low-latency data plane) family; ACB low-latency is per-connection scheduling-rate control. The GLE lib has both (`gle_set_acb_low_latency_request`, `gle_link_set_acb_latency`, plus `gle_aa_cmd_create_icb`-family under 0x28xx).

## 5. GLP (sle_glp_manager.h) — brief

`sle_glp_manager.h` (44 lines): `sle_cs_glp_report_t {status, con_hdl, cfo(int32)}` + `sle_cs_glp_report_callback(conn_id, report)` + `sle_glp_register_callbacks()` (hdr:23-36). This is **not** BLE's Glucose Profile. It is a **channel-sounding companion report** carrying per-connection **CFO (carrier frequency offset)** for the CS engine — "cs_" prefixed, sits beside hadm, GLE lib has `gle_glp_core.c.obj`/`gle_glp_ui.c.obj`/`gle_glp_cmd_proc`/`gle_glp_init`. `glp_mode` in the CS param struct = "2.4GHz 模式" (hdr:62-63); OHOS `fmInteractionType` comment says "和glp有关 stepmode" (hadm_api.h:49). errcode.h reserves a GLP error block 0x8000_9400 (ws73 errcode.h:640).

## 6. 补全清单 — what our ranging layer is missing

Based on comparison of our verified DLI commands (SLE-CONTROL-PLANE.md) vs. WS63/WS73 official API + OHOS stack:

1. **Caps parsing**: decode the 21-byte `SLE_CS_CAPS_LEN` blob per `DLI_ReadCsCapsEvt`/`DLI_ReadRemoteCsCapsPrivEvt` field layout (multiAntennasSupported, switch interval, type1/2 min time fields, phaseCaliOffsetCm, tofCaliOffsetM) — required for param validation and distance calibration.
2. **SET_MEASURE_CONFIG_PARAM (0x2003) full payload**: we now have the complete `DLI_SetMeasureConfigParam` field semantics (measureConfigDirect=0x90010004, schedulingTimeslot=4=125us, channelBandwidth bit0=2.4GHz, pm2400mBand[10] hop channels, antenna counts/tones, sendDirect=1=后发). Note `_ex_t` exposes only the high-level subset; the full timing fields are firmware-computed.
3. **Enable polarity**: 0x2005 enable=0 means START, enable=1 means STOP/PAUSE — fix any assumption in our state machine.
4. **Start sequence ordering**: remote caps → CM connection-param update → SET_MEASURE_CONFIG_PARAM → SET_MEASURE_EN(0). One ranging session at a time (OHOS parallel limit 1).
5. **IQ report consumption**: handle 0x0028/vendor 0x004A event; 3-byte 12-bit I/Q unpacking, 10-byte chmap, slemInfoType bitmask (tof/sinr/aoa/aod bits), timestamp pairing local+remote (diff ≤10).
6. **Distance conversion**: tof_result×0.03m single-ended; two-ended average with tofCaliOffsetM/phaseCaliOffsetCm; no on-device distance algorithm (service side).
7. **Low-latency split**: keep IOG (0x2801-0x280D sync-link) separate from ACB low-latency scheduling (125Hz-8K). If we add low-latency scheduling, the WS63 API is conn_id+enable+rate with mouse/dongle/TX/RX role callbacks and `DLI_AcbLowLatencyEnableEvt {status, enable, rate}` confirmation.
8. **GLP CFO report**: register `sle_glp_register_callbacks` equivalent if CFO per link is needed (useful for frequency-difference based ranging compensation).

## 7. Open questions

- Which DLI opcode underlies `sle_set_channel_sounding_param_ex()` exactly — does WS73 GLE map `_ex_t` to 0x2003's full `DLI_SetMeasureConfigParam` with firmware-computed timing, or does it use the older `DLI_MeasureConfigExtParam` (which equals `HadmSoundingParam_S`+connHandle, dli_def.h:200-230)? OHOS stack branches on `DLI_IsSupportNewDisMeasure()` — the WS73 firmware we have may be the "old" variant, in which case the wire payload for 0x2003 is the full 40+ byte `HadmSoundingParam_S` layout, not the 30-byte `DLI_SetMeasureConfigParam`.
- Caps blob bit layout within the 21 bytes is not documented in the chip SDK; OHOS `DLI_ReadCsCapsPrivEvt` order is the best-guess decoder but needs hardware confirmation with a peer device.
- `refresh_rate` values (1/2/4 = high/mid/low frequency refresh, hadm_api.h:69) and `freq_space` (0=1MHz, 1=2MHz) — how they scale acb_interval/cs_interval is not documented.
- `con_anchor_num` (>1) implies multi-anchor sessions; whether our single-dongle WS73 path supports >1 anchors is untested.
- Low-latency rates: does the WS73 (ffff:3733) firmware actually support 8K scheduling on ACB links, or is the practical ceiling lower? Needs peer measurement.
