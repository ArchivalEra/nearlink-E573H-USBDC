---
type: harvest
title: tethering OHOS integration — SA 1190 nearlink_service profile and the complete 28-interface IPC surface: SSAP, HADM ranging, ASC audio, TWS, VCP, HID host, cloud pair
language: en
created: 2026-09-13
tags: [ohos, sa-profile, ipc, ssap, hadm, asc, audio, tws, vcp, hid, interface-map, harvest]
sources:
  - url: https://github.com/xingkaiyueying/tethering_nearlink
    note: local clone (pushed 2026-09-11, current); read-only inspection of sa_profile/ and ipc_parcel/
trust: verified
stale_after: 2026-12-13
---

# tethering OHOS integration — SA 1190 nearlink_service profile and the complete 28-interface IPC surface: SSAP, HADM ranging, ASC audio, TWS, VCP, HID host, cloud pair

- Inspection date: 2026-09-13 (same current clone as `NEW-OHOS-TETHERING-SERVICE.md`)
- Mode: read-only local program inspection; no build, network, hardware, or PCB access
- Scope: the OHOS system-ability packaging of the NearLink service and its full IPC interface/parcel taxonomy

## Executive findings

1. The service is packaged as **OHOS system ability 1190**: `sa_profile/1190.json` declares process `nearlink_service`, library `libnearlink_server.z.so`, `auto-restart: true`, `run-on-create: false`, and **parameter-driven start-on-demand** — it starts when `persist.nearlink.switch_enable == 1` conditioned on `const.nearlink.enable == true`, and pins an HDI proxy version floor (`min_hdi_proxy_version: libnearlink_hci_proxy_1.0.z.so`). [`sa_profile/1190.json`]
2. The IPC surface is **28 interface headers**, far beyond the tethering scope of the service: `i_nearlink_ssap_client.h` + `i_nearlink_ssap_server.h` (+ both callback headers), `i_nearlink_sle_advertiser.h`, `i_nearlink_sle_central_manager.h`, `i_nearlink_sle_controller.h`, `i_nearlink_sle_datatransfer.h`, **`i_nearlink_hadm_client.h`** (ranging, with a `nearlink_hadm_client_sounding_result` parcel), **`i_nearlink_asc.h`** (+ `asc_audio_stream_info` / `asc_audio_control_result` parcels — an audio service component), **`i_nearlink_tws_client.h`** (TWS earbuds), **`i_nearlink_vcp_client.h`** (volume control profile), `i_nearlink_hid_host.h`, `i_nearlink_cloud_pair.h`, `i_nearlink_cdsm_client.h` (device sync), and battery/RSSI/host observers. [`ipc_parcel/interface/` listing]
3. The **parcel taxonomy serializes the SSAP object model over IPC**: `nearlink_ssap_service_parcel`, `nearlink_ssap_property_parcel`, `nearlink_ssap_method_parcel`, `nearlink_ssap_event_parcel` (wait — method/event parcels exist as files), `nearlink_ssap_descriptor_parcel`, plus scan filter/result/settings, advertiser data/settings, datatransfer connection params, cloud-pair device, and raw address. The property/method/event member classes from `NEW-SSAPC-CLIENT-OBJECT-MODEL.md` have their own IPC parcel types. [`ipc_parcel/parcel/` listing]
4. Net architectural picture: the OHOS NearLink service is a **single SA exposing the whole NearLink feature family** (SSAP, advertising, scan, ranging, audio, TWS, VCP, HID, cloud pairing) over one IPC surface — tethering (datatransfer) is one consumer slice of a much larger capability map that the public tree documents via its interface headers.

## Boundaries and gaps

- Interface headers were enumerated; method/opcode constants inside `nearlink_service_ipc_interface_code.h` were not dumped this pass.
- The ASC/TWS/VCP implementations live behind the service library; only their IPC contracts are public here.
- The four start-on-demand param entries include unnamed placeholders (None names in the JSON dump) — param details beyond the switch_enable gate were not fully parsed.

## Reusable for our stack

- The 28-interface map is the complete capability checklist an OHOS NearLink SA exposes — the checklist our own daemon should mirror if we ever ship an OHOS-compatible control surface (SSAP + adv + scan + ranging + transfer are the five we already cover conceptually).
- SA packaging facts (1190, auto-restart, param-driven start, HDI proxy floor) are the concrete OHOS integration recipe for our daemon.
- The audio/TWS/VCP interface presence publicly confirms LE-Audio-adjacent NearLink audio profiles are moving through the ecosystem — relevant context for our LE-Audio-out-of-scope decision (still out of scope, but now evidenced).

## Comparison anchors (vs existing reports)

- `OHOS-SSAP-CLIENT.md` / `OHOS-SSAP-ENGINE.md` / `OHOS-HDI-DRIVER.md`: the IPC surface is the layer between those framework docs and the stack — now mapped end to end.
- `NEW-SSAPC-CLIENT-OBJECT-MODEL.md`: the SSAP method/event parcels confirm the object model crosses the IPC boundary, not just the stack boundary.
- `NEW-OHOS-NEARLINK-SEPT-INCREMENT.md`: same ecosystem, different repo; the ASC audio interfaces here complement that report's audio-adjacent commits (karaoke ear-return).
