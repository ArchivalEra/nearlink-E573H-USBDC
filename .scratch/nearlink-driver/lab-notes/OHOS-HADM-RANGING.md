# OHOS HADM Ranging (Channel Sounding) Layer — Deep Dive

Source: `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/cp/bsl/sle/hadm/`

## 1. Ranging command sequence (state-machine driven)
1. CM link connected → remote features (byte 8, mask 0x4 = sounding) → **0x2002 READ_REMOTE_MEASURE_CAPS** (payload = connHandle u16)
2. Result caches phaseCaliOffsetCm/tofCaliOffsetM → SOUNDING_READY
3. User start → CM connection param update → **0x2003 SET_MEASURE_CONFIG_PARAM** (DLI_SetMeasureConfigParam: measureConfigDirect=0x90010004, schedulingTimeslot=4(125us), channelBandwidth=2.4GHz(1<<0), multi-tone + pm2400mBand[10])
4. → **0x2005 SET_MEASURE_EN** (enable=0) → SOUNDING
5. Stop → **0x2005** (enable=1=disable) → back to SOUNDING_READY

Local caps (**0x2001**) consumed only by cfgdb module. FIFO correlation since SET responses lack lcid.

## 2. Channel sounding params (HadmSoundingParam_S)
freqSpace (0=1MHz/1=2MHz), conAnchorNum, refreshRate (1/2/4), acbInterval, csInterval, glpMode, sleHadmMode, isCsParamChg + full timing fields (configId, rttPhy, freqHoppingMode, occurrenceGroupPeriod, pm2400mBand[10]...). posalg_freq is chip-SDK-only.

## 3. IQ report (0x0028 / vendor 0x004A)
- Info-type bitmask DLI_SlemInfoType: aoa/aod/chnlInfo/tof/chnlMeas/sinr/vender...
- 3 bytes/sample 12-bit I/Q: i=b0|((b1&0x0F)<<8), q=(b2<<4)|((b1&0xF0)>>4)
- 10-byte chmap (79 channels), multi-tone ×4 (max 316), tofResult u32 (0.1ns units)
- Pair local (localId==0) + remote IQ by timestamp diff ≤10

## 4. Distance conversion
distance = tofResult × 0.03 m (chip reports 0.1ns → ×3/100); two-ended = (local+remote)×0.03/2 − tofCalib×2. Phase/TOF cal offsets passed to service-side SLEM algorithm — NO distance algo in stack.

## 5. API surface
HadmRegCbk/HadmStartSounding/HadmStopSounding/HadmGetSoundingState; parallel limit 1. Chip SDK equivalent: sle_hadm_manager.h (ws73v100-wifi/include/bsle/sle/).

## 6. GTTT variants (chip-SDK only)
sle_set_channel_sounding_param_ex_gttt: posalg_freq, g_handle, is_gtt, is_tt, tx_tt_id, rx_tt_id; enlarge_ratio. G-T and TT ranging modes.

## 7. mm/ is empty (README only).
