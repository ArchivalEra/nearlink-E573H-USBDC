---
type: harvest
title: "DLI remaining families closed: CS caps/IQ events, ICB streaming, subrate, per-link quality, RSSI arrays, and frequency-band switch"
language: en
created: 2026-09-17
tags: [harvest, dli, icb, quality, rssi, events, ohos]
sources:
  - "https://github.com/openharmony/communication_nearlink_service/blob/7068bc43961da5e54eba1225a3a8363d85541585/services/stack/src/dli/interface/dli_cmd_struct.h"
  - "https://github.com/openharmony/communication_nearlink_service/blob/7068bc43961da5e54eba1225a3a8363d85541585/services/stack/src/dli/interface/dli_event_struct.h"
trust: A
stale_after: 2027-03-17
---

# DLI remaining families closed: ICB streaming, subrate, per-link quality, RSSI arrays, frequency-band switch

## Executive findings

This completes the queue item left open by [NEW-DLI-STRUCT-TABLES](NEW-DLI-STRUCT-TABLES.md): the read-RSSI, white-list search that turned out to be absent, and the ICB/REM/quality families. Both headers are read in full at revision `7068bc43961da5e54eba1225a3a8363d85541585`. `#pragma pack(1)` wraps the struct region in both files, so recorded layouts are packed.

- **There is no white-list or RNG command family in either struct header.** A full scan for `Rssi|WhiteList|Rng|Random|REM` hits only measurement-target text and the RSSI events. The prior report's guessed family list over-anticipated; the actual uncovered families were measurement-caps details, ICB group/label management, subrate negotiation, per-link quality reporting, and frequency-band switching.
- **Read-RSSI has a single-link and an arrayed variant.** `DLI_ReadRemoteRssiEvt {connHandle, rssi int8, -128..127, 127 = invalid}` (`dli_event_struct.h:328-331`). `DLI_RssiEvt` adds a 32-bit `actualRssiTs` timestamp plus 14-entry `rssiIdx` and `actualRssiValue` arrays (`:373-379`; `SLE_RSSI_REP_SIZE 14` at `:37`) — a sampled RSSI report, not a single reading.
- **CS capability events now expose calibration offsets.** `DLI_ReadCsCapsEvt` (`:196-211`) includes `phaseCaliOffsetCm` and `tofCaliOffsetM` alongside multi-antenna counts, switch interval, and seven minimum-time fields. The private variants (`:213-254`) add `measureReportingCapabilitySupported` and split type1/type2 intra-event minima; the remote-priv form is the richest. Capabilities are 24 bytes (`SLE_MEASURE_LEN 0x18`, `:33`).
- **IQ report event confirms the 3-byte 12-bit IQ packing at the DLI boundary.** `DLI_CsIqReportEvt {status, connHandle, slemIdx, slemInfoType bitfield, timestampSn, data[0]}` (`:304-311`); `DLI_SlemChnlMeas {rssi, iqBitLength, iqData[0]}` with `iqBitLength` "currently 12bit" and the 3-byte I/Q interleave documented at `:266-278`. `DLI_SlemInfoType` is a 16-bit bitfield naming AoA/AoD, per-band channel info, freqDiff, tof, chnlMeas, sinr (`:290-302`). `DLI_SlemVenderData` marks G-side `localId=0` (`:280-288`).
- **ICB establishment answers with concrete air timings.** `DLI_ICBEstablishedEvt` (`:418-441`) returns 3-byte microseconds fields for IMG sync delay, IMB sync delay and per-direction transport latency (range 0x0000EA-0x7FFFFF us), negotiated phy/mcs/pilot per direction, `nse` 0x01-0x1F subevents, per-direction burst counts `bn`, flush timeouts `ft` [0x01,0xFF] in ICB intervals, and `imbInterval` N*0.25ms = [5ms,4s].
- **Quality telemetry is a designed-in 500 ms report.** `DLI_IOBQualityReportEvt` (`:443-457`): diff clock stats, `txFlushed`, `rxLossPktCnt`, `rxLossMaxContPkt`, mean rssi, and `ackRate = tx_ack*100/tx_total` in [0,100] — a ready-made link-quality telemetry record for a host agent. The multicast `DLI_IMBQualityReportEvt` (`:459-480`) adds `errPacketRate`, `missedRate`, tx-node count, and a per-channel tail `{missedRate, errPacketRate, channelRssi}`. `DLI_ReqQualityEvt {lcid, rssi, rate}` closes the set (`:579-583`).
- **Subrating is negotiated on asynchronous links.** Command side `DLI_ACBEnableSubrateParam {subrateMin, subrateMax, maxLatency, continuationNum, supervisionTimeout x10ms}` (`dli_cmd_struct.h:563-569`) and per-`lcid` `DLI_ACBSubrateParam` (`:571-578`); event side `DLI_AcbSetSubrateEvt` and `DLI_AcbReqSubrateEvt` return the factor, peripheral latency, continuation number and timeout (`dli_event_struct.h:545-562`).
- **Frequency-band switching is a first-class event.** Command `DLI_FreqBandExtParam {allowedBand, trigType}` (`dli_cmd_struct.h:559-561`); event `DLI_FreqBandSwitchEvt {status, connHandle, oldFreqBand, newFreqBand}` with 0=2.4G, 1=5.1G, 2=5.8G (`dli_event_struct.h:572-577`), and the capability bitfield reserves `chnlInfo51g/58g` bits (`:293-295`). This is in-header evidence that the SLE CS/measurement plane anticipates 5 GHz bands.
- **Control-plane odds and ends.** `DLI_SetTxPowerParam` splits `bleMaxPower`/`sleMaxPower` (`dli_cmd_struct.h:378-381`); high power is per-connection with `powerLevel` and a frame-type-specific level struct (`:383-392`); `DLI_SetDataLenParam {connHandle, txOctets}` (`:340-343`); ACB event scheduling `{connHandle, eventInterval, eventNumber}` (`:345-349`); `DLI_RemConParamReqReplyParam` mirrors the update layout with slot-unit intervals (`:360-371`); low-latency echo event `DLI_AcbLowLatencyEnableEvt {status, enable, rate}` (`dli_event_struct.h:333-338`).

## Decoder-facing tables

| Family | Event struct | Decoding keys |
|---|---|---|
| RSSI single | `DLI_ReadRemoteRssiEvt` | 127 = invalid sentinel. |
| RSSI sampled | `DLI_RssiEvt` | 14 index/value pairs with `actualRssiTs`. |
| CS caps | `DLI_ReadCsCapsEvt` / priv variants | 24-byte caps blob; calibration offsets in cm and m. |
| CS IQ report | `DLI_CsIqReportEvt` | 12-bit I/Q packed in 3 bytes; slemInfoType bitfield picks tof/chnlMeas/sinr members. |
| ICB established | `DLI_ICBEstablishedEvt` | 3-byte us fields; per-direction phy/mcs/pilot; nse/bn/ft. |
| Quality IOB | `DLI_IOBQualityReportEvt` | 500 ms window; ackRate percent; four reserved u32 tail. |
| Quality multicast | `DLI_IMBQualityReportEvt` | Variable per-channel tail with txCount. |
| Subrate | `DLI_AcbSetSubrateEvt` / `DLI_AcbReqSubrateEvt` | lcid-scoped; 10 ms supervision unit. |
| Band switch | `DLI_FreqBandSwitchEvt` | old/new band codes 0/1/2. |

The command-side measurement config `DLI_SetMeasureConfigParam` (`dli_cmd_struct.h:289-312`) is now fully itemized: 24 fields covering config id, `measureConfigDirect`, occurrence group period, RTT PHY, frequency hopping mode plus initial `fmFreq`, send order, dual-antenna ordering with per-link antenna type, K-bit random antenna order width, per-direction antenna counts/2-tone/switch intervals, channel bandwidth, and a `SLE_MEASURE_PM_24G_BAND_LEN` 2.4 GHz hopping bitmap. Its enable companion is `{connHandle, enable}` (`:314-317`) with `MEASUREMENT_TARGET_START/STOP/PAUSE` semantics (`:278-282`) — pause keeps link resources.

## Boundaries

- Struct-level reading of the OHOS tree at one revision; no dongle capture was taken, so wire offsets, opcode-to-struct mapping and endianness still require capture validation as noted by the prior report.
- White-list and RNG command families do not exist under those names in these two headers; they may exist in vendor SDK headers not present in this tree. Treat absence as OHOS-tree evidence, not protocol absence.
- ICB/ICG/IMG grouping semantics were read at struct level only; the CM-layer usage (ICG scheduling policy) lives elsewhere in the tree and was not traced.
- The file is Apache-2.0; facts are recorded, no code imported.
- The dense MCS/PHY comment blocks contain OCR-degraded glyphs upstream (a "tiao-zhi" (modulation) term appears as "tiao-shi" (debug) in one comment); coding tables were transcribed by value, not by those comments.

## Reusable

1. Extend the dongle capture decoder with the closed families: RSSI sampled reports, CS caps with calibration offsets, ICB established/quality/subrate/band-switch events.
2. Use `DLI_IOBQualityReportEvt` as the template for our own 500 ms link-quality telemetry (ackRate, loss, flush counters) on the WS73 host agent.
3. The 12-bit/3-byte IQ packing confirms the same scheme the uwb-like ranging firmware uses — two independent codebases agree, so a shared decoder is justified.
4. `DLI_FreqBandSwitchEvt` gives the first in-tree hook for tri-band awareness; wire it into capture tooling before any 5.x GHz experiments.
5. `DLI_LocalPrivateFeatures {bitNumber, bitValue}` (`dli_cmd_struct.h:351-354`) is the vendor feature-advertisement knob — pair with `DLI_ReadRemoteFeatsEvt`'s 10-byte feature table.

## Comparison anchors

- [NEW-DLI-STRUCT-TABLES](NEW-DLI-STRUCT-TABLES.md): covered adv/connection/PHY/encryption families; this closes the header pair with ICB, subrate, quality, RSSI arrays and band switching, and corrects the assumption that white-list/RNG families live here.
- [NEW-NEARLINK-UWB-LIKE-RANGING](NEW-NEARLINK-UWB-LIKE-RANGING.md): its firmware-side 332-byte IQ struct matches the DLI `CsIqReportEvt` + `SlemIqData` decomposition now visible at the stack layer.
- [NEW-OHOS-NEARLINK-SEPT-INCREMENT](NEW-OHOS-NEARLINK-SEPT-INCREMENT.md): recorded DLI autorate+hidden-command hardening; this pass supplies the autorate parameter layout (`DLI_ICGAutorateParam`, `dli_cmd_struct.h:476-492`) that report referenced.
