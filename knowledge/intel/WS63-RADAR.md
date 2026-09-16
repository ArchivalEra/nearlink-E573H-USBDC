---
type: intel
title: "WS63/WS63E Radar — Capability Assessment for TV-Box"
language: zh
created: 2026-08-17
tags: [intel, ws63, ws63e, radar]
sources:
  - "https://github.com/x-eks-fusion/fbb_ws63"
  - "https://github.com/HopeRunORG/NearLink"
trust: B
stale_after: 2027-02-17
---

# WS63/WS63E Radar — Capability Assessment for TV-Box

- **Date**: 2026-08-17
- **Author**: research subagent (read-only local survey; no network/build/hardware)

## Sources

| Source | Path | Notes |
|---|---|---|
| Radar service API | `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/include/middleware/services/radar/radar_service.h` | Public user API (the only radar header exposed to apps) |
| Radar protocol build tree | `https://github.com/x-eks-fusion/fbb_ws63,alg_ai}/build/ws63/{radar_entry,radar_ai_entry}.cmake`, `plat/Kconfig` | Source is prebuilt (`.a`); cmake lists the internal module layout |
| Radar samples | `https://github.com/x-eks-fusion/fbb_ws63,sta_sample,softap_sample,sta_connect_sample}/` | 4 sample apps |
| Vendor radar demo | `https://github.com/x-eks-fusion/fbb_ws63/tree/master/vendor/HiHope_NearLink_DK_WS63E_V03/demo/radar_led/` (+`README.md`) | "运动感知1.0" (motion-sensing LED) |
| AT radar cmd table | `https://github.com/x-eks-fusion/fbb_ws63/blob/master/src/middleware/utils/at/at_radar_cmd/at/at_radar_cmd_table.h` | Full AT command set (SDK, sources for handlers are prebuilt `libradar_at.a`) |
| AT guide (PDF) | `https://github.com/HopeRunORG/NearLink AT命令 使用指南_03.pdf` | Radar AT chapter (extracted with pdftotext; line refs below are to the pdftotext output) |
| Radar quick-start (PDF) | `https://github.com/HopeRunORG/NearLink 雷达快速入门指南_03.pdf` | Only doc with hard performance numbers |
| AT case PDF text | `.scratch/nearlink-driver/assets/HHD01-WS63V100-AT-commands.txt` | Contains NO radar section (WiFi/SLE/BLE only) |
| Local notes | `HOPERUN-DEMOS.md`, `WS63-AT-FRAMEWORK.md`, `WS63-VS-WS73.md` (lab-notes dir) | Cross-refs |
| WS73 host SDK | `sdk/ws73_sdk_linux_WS73_1.10.110/` | Radar-on-WS73 check |

> Note: `fbb_ws63/src/protocol/radar/` contains **no C sources** — only `libradar_sensing.a`, `libradar_ai.a` (prebuilt) plus cmake. The radar AT handler sources (`at_radar.c`, `at_radar_cmd_register.c`) are likewise prebuilt (`libradar_at.a`). The `radar_entry.cmake` file lists the internal file names.

---

## 1. Radar capability (what the WS63 radar actually is)

The WS63 radar is a **2.4 GHz monostatic presence/proximity radar reusing the WiFi RF chain** (not mmWave), with an extra receive-only RFI antenna for radar RX.

- **Band**: RFIO/RFI antennas are specced at **2.4–2.5 GHz** (`WS63V100 雷达快速入门指南_03.pdf`, antenna constraint table: RFIO/RFI antenna VSWR<2 @2.4–2.5GHz). So radar runs in the same 2.4 GHz WiFi band; the chip is a "2.4GHz SoC WiFi6+BLE+SLE" (fbb_ws63 `README.md:11`).
- **Working mode**: time-shared with WiFi. Radar subframe interval default **5000 µs (5 ms)**, range 3000–100000 µs (`radar_service.h:153-154`); duty cycle <2%, fastest result report every **0.32 s** (`雷达快速入门指南_03.pdf` §1.2, extracted text l.415-418).
- **Two detection capabilities** (`雷达快速入门指南_03.pdf` §1.1, extracted l.357-400):
  1. **靠近检测 / proximity**: detects moving human within **2.5 m**, split into two bands `[0,1.5 m]` and `[1.5 m, 2.5 m]`, report delay <1 s, accuracy >99% (targets ≥1.4 m tall).
  2. **存在检测 / presence**: detects **moving human within 6 m** (single presence band), report delay <2 s, accuracy >99%; with the AI anti-false-positive path (curtains swaying, plants, plastic fan blades) recognition delay <5 s, accuracy >95%.
- **Result granularity**: the public result is coarse — `radar_result_t { lower_boundary, upper_boundary, is_human_presence }` (`radar_service.h:69-79`). Samples interpret boundaries in cm: `[0,100]=1m`, `[100,200]=2m`, `[200,600]=6m` (`radar_sta_sample.c:38-40`, `vendor/.../radar_led/README.md:5`). **No angle/azimuth/velocity in the public API** — no gesture/pointing output. Raw IQ/ADC is only exported as a debug dump over UART1 (`radar_service.h:117-129`).
- **Environment/installation config**: mounting height 0–1.5 / 1.5–2.5 / 2.5–3.5 m; home vs hall scenario; plastic vs metal/diplay-panel covering; toggles to fuse distance-tracking and AI results (`radar_service.h:164-227`).
- **Algorithm thresholds** (tunable, per `radar_service.h:238-255`): 1 m/2 m proximity thresholds, 6 m presence threshold, distance-tracking thresholds (dm), anti-spectrum-symmetric-interference ratio/count, AI human-recognition similarity threshold.
- **Hardware constraints** (`雷达快速入门指南_03.pdf` §2.2-2.3): needs a dedicated RFI radar antenna, RFIO↔RFI isolation ≥20 dBc (typical measured 20–32), RFIO↔RFI group delay <3 ns, antenna clearance ≥15 mm, and the radar antenna must be kept away from EMI sources. Radar is **unavailable during WiFi channel switch / scan** (§1.1 note, l.370-373).
- **Underlying algorithm pipeline** (from `radar_entry.cmake`): preproc (hal_radar_wave, rx-gain-cali, one-rx-ant) → ppl (data-cube mgr, preproc mgr, smooth, cali) → spl (CFAR, plot extraction, doppler detection [presence], proximity detection, bitmap) → fpl (Kalman tracker, multi-target mgr, target FSM) → adl (track proximity) → plus an optional **on-chip AI model** (`radar_ai_entry.cmake`: conv/conv2/fc/LSTM/attention layers — human-vs-clutter discrimination). FFT is a separate ROM component (`alg_radar_sensor/romable/alg_radar_fft.h`).

## 2. Radar samples (what the demos do)

All samples boot WiFi first, then run radar; differences are in transport of the result.

- **`m_sample/radar_m_sample.c`** — presence-gated **smart-appliance IR controller**. Registers result callback; on `is_human_presence==0` sends an appliance "close" frame; on presence picks 1 m/2 m/6 m gear and sends vendor-specific IR/byte frames over the debug UART (`radar_m_sample.c:403-425`, byte arrays at `:50-177`), including 4-step vendor handshakes for some appliances (`:428-463`). Init sequence: register cb → `radar_set_driver_para` → 10 s delay → `uapi_radar_set_status(RADAR_STATUS_CALI_ISO)` (isolate-cali) → poll status/delay/iso forever (`:504-532`).
- **`sta_sample/radar_sta_sample.c`** — motion LED demo: result callback drives GPIO LED (on/off) per 1 m/2 m/6 m gear (`:92-118`). Full param init via the public API: `uapi_radar_set_debug_para`, `uapi_radar_select_alg_para`, `uapi_radar_set_alg_para` (`:142-171`), then `RADAR_STATUS_CALI_ISO` (`:183`).
- **`softap_sample/radar_softap_sample.c`** — same, but SoftAP mode; starts radar with `RADAR_STATUS_START` (`:146`); prints result (`:108-111`).
- **`sta_connect_sample/`** — presence-gated controller + **TCP host bridge**: `radar_socket.c` runs a TCP server on port 9999 exposing a TLV protocol (header `0xAA`; set commands `0xA1` status, `0xA2` delay, `0xA3` alg-select, `0xA4` driver-para, `0xA5` alg-para; get commands `0xB1` result, `0xB2` debug; `radar_socket.c:25-62,178-212,285-310`) and pushes result/debug frames to a connected host (`:312-356`). This is the **only sample that pushes radar data off-device over TCP**.

Shared sequence: `wifi_*_enable()` → `uapi_radar_register_result_cb()` → set driver/alg params → `uapi_radar_set_status(START|CALI_ISO)` → poll `uapi_radar_get_status/get_delay_time/get_isolation` (e.g. `radar_sta_sample.c:173-198`).

## 3. Protocol layer (how radar data flows)

- **In-chip service**: `src/protocol/radar/` is a closed prebuilt service (`libradar_sensing.a` + `libradar_ai.a`). `radar_entry.cmake` shows internals: `radar_service.c` (public API), `radar_driver_mgr.c`, `radar_feature_mgr.c`, `spec/ws63/radar_uart.c` (**debug data out on UART1**), `radar_event.c`, `radar_mips.c`, `radar_tcxo.c`, `radar_tsensor.c`, `radar_dfx.c`, `radar_alg_param.c`, `radar_industry_info.c`, `mfg/radar_mfg.c`.
- **Upward paths for radar data** (no standard/custom host protocol in the SDK):
  1. **In-app callback** — `uapi_radar_register_result_cb` on the radar feature thread (`radar_service.h:81-95,316`); samples use this.
  2. **UART debug dump** — `radar_dbg_type_t` 0–4 selects what UART0/UART1 print (pulse-compressed data, ADC raw, per-frame result; `radar_service.h:117-129`); the m/sta_connect samples even use this UART to push appliance IR bytes.
  3. **TCP TLV bridge** — sample-level, in `radar_socket.c` (see §2), not part of `protocol/radar`.
  4. **MFG hooks** — `CONFIG_RADAR_MFG` + `mfg/radar_mfg.c` for isolation/noise production test.
- **Driver/PHY integration**: the radar TX/RX path runs through the WiFi host driver feature `hmac_radar_sensor.c` (`src/protocol/wifi/source/host/feature/hmac_radar_sensor.{c,h}`): configures TX waveform buffer (≤2 KB at 0xA98000), RX memory (default 8 KB, `CONFIG_RADAR_SENSOR_RX_MEM_8K`), LNA/VGA/LPF/PA gains, subframe period (1–10 ms), channels **1–13** with **40 MHz bandwidth** (`hmac_radar_sensor.c:294` "雷达感知默认使用40M带宽"), and auto-disables radar during scan / STA disconnect / STA connecting / VAP delete (`hmac_radar_sensor.h` `radar_sensor_disable_reason_enum`). Radar results are delivered to the algorithm layer via a registered handle callback (`hmac_radar_sensor.h:70-71`).

## 4. AT radar command set

The HopeRun AT-basic firmware embeds the radar AT set (confirmed present in `ws63-liteos-app_all.fwpkg` strings, `WS63-AT-FRAMEWORK.md:7,163`). Full set from `at_radar_cmd_table.h:327-441` and `WS63V100 AT命令 使用指南_03.pdf` §4:

| Command | Params (range) | Purpose |
|---|---|---|
| `AT+RADARSETST=<status>` | 0 stop / 1 start / 2 reset / 3 resume / **4 iso-cali** | Radar state (`at_radar_cmd_table.h:328-337`; PDF §4.1). Requires STA or SoftAP up first. |
| `AT+RADARGETST` | — | Query idle(0)/running(1) (PDF §4.2) |
| `AT+RADARSETDLY=<dly_time>` | 1–43200 s | "target→no-target" exit delay (`at_radar_cmd_table.h:379-387`, `:273-282`; PDF §4.3) |
| `AT+RADARGETDLY` | — | Query exit delay (PDF §4.4) |
| `AT+RADARGETISO` | — | Query antenna isolation on current channel (PDF §4.5) |
| `AT+RADARSETPARA=<times>,<loop>,<ant>,<wave>,<dbg_type>,<period>` | times 0–20, loop 2–32, ant 0–3, wave 0–10, dbg_type 0–4, period 3000–100000 µs | Waveform/duty/debug config (`at_radar_cmd_table.h:339-347,121-165`; PDF §4.6). `wave` selects waveform type (sample comment: 0=320M/40M CTA, 1=160M/20M CW, default 2; `radar_m_sample.c:210`). |
| `AT+RADARALGCTRL=<height>,<material>,<scenario>,<fusion_track>,<fusion_ai>` | height 0–2, material 0–2, scenario 0–1, fusion 0–1 | Algorithm parameter-suite selection (16 suites = 3 params × ap/sta mode; `at_radar_cmd_table.h:348-357`; PDF §4.7) |
| `AT+RADARALGPARA=<d_th_1m>,<d_th_2m>,<p_th>,<t_th_1m>,<t_th_2m>,<b_th_ratio>,<b_th_cnt>,<a_th>,<write_to_flash>` | 0–99/0–100, write 0–1 | Tune per-suite thresholds, persist to flash (`at_radar_cmd_table.h:432-440`; PDF §4.8) |
| `AT+RADARSETLED=<gear>` | 0–2 | LED gear (1 m/2 m/6 m) — sample/demo support (`at_radar_cmd_table.h:399-407`) |
| `AT+RADARTESTISO=<target_iso>,<iso_win>` *(MFG)* | 0–40, 0–20 | Production isolation test (`at_radar_cmd_table.h:411-419`; PDF 雷达快速入门 §3, typical ISO 20–32) |
| `AT+RADARTESTNOISE=<noise_thres>` *(MFG)* | 0–40 | Production noise-floor test (`at_radar_cmd_table.h:421-429`) |

Naming is `AT+RADAR*`, not `AT+RR`. The AT case PDF (`HHD01-WS63V100-AT-commands.txt`) has **no radar chapter** — it only covers WiFi/SLE/BLE; radar AT lives in the 使用指南 PDF. `RADARALGCTRL` appears once as `RADARLAGCTRL` in the PDF (§4.7 heading) — a doc typo; the command table and syntax define `RADARALGCTRL` (`at_radar_cmd_table.h:348-357`).

## 5. TV-box value assessment

**Feasibility per use-case** (radar data is coarse: 3 distance bands + presence flag):

- **人走息屏 / presence-driven screen blanking**: feasible. 6 m presence, <2 s delay, >99% accuracy is well matched to "nobody in the room → dim/sleep". Needs a mount at 0–3.5 m height with the RFI antenna aimed at the sofa area, ≥1.4 m human targets.
- **儿童距离提醒**: feasible via proximity bands [0–1.5 m] / [1.5–2.5 m] (<1 s, >99%) — "too close to TV" reminder.
- **手势控制**: **not available**. The public API exposes only range-bin presence, no angle/velocity/gesture; the prebuilt AI is human-vs-clutter discrimination only. Raw IQ/ADC debug dump exists (UART1, `dbg_type` 2) but the SDK ships no gesture classifier, and a TV box would have to reimplement DSP+ML on raw data — high cost, unsupported.
- **Watch-outs**: radar pauses during WiFi scan/channel switch (a box that scans channels periodically gets detection gaps); needs a dedicated RFI antenna + ≥15 mm clearance + ≥20 dBc RFIO↔RFI isolation — real board-design constraints inside a metal TV-box chassis.

**Relationship to the WS73 dongle (our project)**: **WS73 has no human-sensing radar.** The WS73 tree contains only `driver/wifi/hmac/hmac_radar.c` which is **DFS radar-pulse detection** (FCC/ETSI pulse patterns for 5 GHz DFS compliance, `hmac_radar.c:7,37-50`), plus a leftover shared header `feature/hmac_radar_sensor.h` with no implementation; there is no `radar_sensing`/`radar_ai` component and the `ws73.bin` firmware blob contains no radar-sensor/presence/RFI/isolation strings. So radar is **exclusively a WS63/WS63E SoC feature** — the `ffff:3733` WS73 dongle cannot do presence sensing.

**Integration path for TV box**: radar requires a WS63/WS63E board/module (e.g. HH-SPARK-WS63E, per `HOPERUN-DEMOS.md:115`) with its own LiteOS/OpenHarmony firmware. The realistic architecture: a WS63E module inside the box (or as a pogo/USB dongle accessory) that simultaneously acts as NearLink (SLE) peripheral + presence radar, reporting the presence/range flag to the box over UART/USB or over SLE. Radar data is small (one `radar_result_t` per event) — it would ride an existing link, not the SSAP protocol; our `stack/ssap/` needs **no changes** for radar itself (grep of `stack/ssap/src` shows no radar/sensor code; SSAP carries arbitrary app data, so a presence attribute could be exposed over SSAP if desired).

## 6. Conclusion & roadmap position

- **Radar is a genuine product differentiator for TV boxes** (presence blanking, child-distance alert) and is cheap (reuses the 2.4 GHz radio, no extra sensor).
- **It is out of scope for the WS73 Linux-driver roadmap**: the WS73 dongle hardware cannot sense presence. Putting radar in scope would mean designing a separate WS63E-based companion module — a hardware + firmware project on the fbb_ws63 SDK (LiteOS/OH), not on our Linux/WS73 stack.
- **Recommendation**: park radar as a **"TV-box companion-module" opportunity**, not a driver feature. Position it as a WS63E module used as NearLink+radar combo; deliver presence + child-distance first (both are off-the-shelf), and treat gesture control as out-of-scope (not supported by the SDK API). Gate on: (a) HopeRun/HH-SPARK-WS63E module carrying the RFI radar antenna (module spec PDFs unparsed — [VERIFY]), (b) TV-box board integration (antenna clearance, isolation), (c) a decision on the host link (UART/USB vs SLE-over-SSAP). No change needed in `stack/ssap/`.

## Open questions

1. Can the 6 m presence mode run reliably in a typical living room with a large TV display in front of the module (the `material=1` metal/PCB/display-panel option hints this is a real concern)? Needs field test.
2. Does the HH-SPARK-WS63E module (or any WS63E module HopeRun offers) actually bring out the **RFI radar antenna**? The two antenna references (RFIO + RFI) imply a 2-antenna design — [VERIFY] against module spec PDFs (unparsed).
3. Radar + BLE/SLE concurrency: the quick-start only documents WiFi+radar time-sharing; whether BLE/SLE (both active on a TV-box combo) degrade radar further is undocumented.
4. The m_sample's vendor IR handshake/control frames show appliance-control is a supported commercial use; what reference app designs does HopeRun have for **presence → screen-off** in a TV/set-top context (any "smart TV" demo)?
5. WS63E die is WS63-partitioned (per `WS63-VS-WS73.md:45`); is the radar feature identical on WS63 and WS63E, or does the WS63E "E" partition trim radar (e.g., no RFI antenna pin)?
