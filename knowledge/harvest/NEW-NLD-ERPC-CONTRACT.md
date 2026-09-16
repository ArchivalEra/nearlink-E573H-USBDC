---
type: harvest
title: "Nld eRPC dongle contract enumerated: 9 service IDs, 60+ methods, SSAP enum ground truth, UPG and low-latency control planes"
language: en
created: 2026-09-15
tags: [harvest, nld, erpc, ssap, dongle, contract]
sources:
  - ""https://gitcode.com/goodspeed1/Nld""
trust: A
stale_after: 2027-03-15
---

# Nld eRPC dongle contract enumeration

## Executive findings

Reading the generated interface headers completes the wire contract between host and dongle — service IDs and every method, plus the SSAP enum ground truth.

**1. Service ID map (from `m_serviceId` in each interface header).** connection=84, seek=83, ssap_client=85, ssap_server=86, host=8x callback side (host_interface carries 22 callback method IDs: on_power_on, on_sle_enable/disable, on_connect_state_changed, on_pair_complete, on_auth_complete, on_conn_param_update, on_passkey_req/notify, on_seek_result, on_ssap_add_service/property/descriptor, on_ssap_start_service, on_ssap_delete_all_service, on_ssap_read_request, read_by_uuid_request, write_request, indicate_cfm, mtu_changed, notification, indication). Total ≥60 distinct methods across 9 services.

**2. SSAP wire enums confirmed at contract level** (`sle_sle_connection_common.h`): addr types PUBLIC=0/PRIVATE=6; write types NO_RSP=1/WITH_RSP=2; **permission bitmask READ=1/WRITE=2/ENCRYPTION_NEED=4/AUTHENTICATION_NEED=8/AUTHORIZATION_NEED=16**; find types SERVICE_STRUCTURE=0/PRIMARY_SERVICE=1/REFERENCE_SERVICE=2/PROPERTY=3/METHOD=4/EVENT=5; property types VALUE=0 + descriptors USER_DESCRIPTION=1/CLIENT_CONFIGURATION=2/SERVER_CONFIGURATION=3/PRESENTATION_FORMAT=4/RFU=5/CUSTOM=255; operate-indication bits READ=1/WRITE_NO_RSP=2/WRITE=4/NOTIFY=8/INDICATE=16/BROADCAST=32. These are the same values our SSAP stack assumed — now vendor-contract-verified.

**3. Connection control plane is richer than sample code shows.** `sle_connection_interface` includes, beyond the usual connect/pair/disconnect: `sle_set_mcs(conn_id, uint8_t mcs)` (per-connection modulation override — pairs with the 1.10.111 MCS enum), `sle_set_phy_param(conn_id, sle_set_phy_t*)`, `sle_set_data_len(conn_id, tx_octets)`, `sle_set_channel_map`, `sle_set_nv_smp_keys`, `sle_read_remote_device_rssi`, `sle_get_pair_state`, bonded/paired device listing and `sle_remove_all_pairs` — the full pairing-key hygiene API our reconnect rule needs, one call deep.

**4. Two control planes outside the radio link.** `device_manager` (enable/disable SLE, `bpdongle_reset_system`) and `firmware`: UPG state machine (`upg_start` → `upg_write` → `upg_get_status/get_result`, storage-size query, `upg_reset_upgrade_flag`, plus build-info/commit/version/id queries) — a complete dongle OTA contract. `low_latency` has exactly two methods: `sle_low_latency_dongle_enable` + `sle_low_latency_set` — a toggle plus parameter set, confirming it as a QoS profile switch rather than a data path.

**5. announce/seek asymmetry.** `announce` carries local identity (get/set addr and name) plus per-handle announce param/data/remove/start/stop; `seek` is only 3 methods (set_seek_param/start/stop) — seek results arrive via host callbacks (on_seek_result), keeping the data path one-directional.

## Boundaries

- Method IDs and signatures verified; argument struct field layouts live in the _common.hpp files (partially read — connection/low_latency done).
- `bpdongle_*` prefix marks BP-dongle-scoped operations — exact scope vs plain methods not documented in-repo.
- Method numbering gaps (e.g. seek start/stop IDs) not cross-checked against .cpp dispatch tables.

## Reusable

- The 9-service ID map + SSAP enum table — paste-ready ground truth for our SSAP stack docs and any future dongle emulator.
- `sle_set_mcs`/`sle_set_data_len`/`sle_set_phy_param` per-connection — the knobs for a WS73 link-quality experiment suite.
- UPG method sequence as the dongle OTA flow reference.
- Seek results via callback only (3-method seek service) — architectural pattern: control minimal, data on callbacks.

## Comparison anchors

- vs. USB-PROTOCOL.md: this is the semantic layer above the DLI opcodes — host↔dongle now readable at three depths (USB bulk frames, DLI opcodes, eRPC methods).
- vs. our SSAP engine: enums match; the report adds `REFERENCE_SERVICE=2` and `METHOD/EVENT` find types we had not enumerated.
- vs. 10714 tools: their userspace calls map 1:1 onto this contract (seek→on_seek_result, announce commands) — two independent implementations agree.
