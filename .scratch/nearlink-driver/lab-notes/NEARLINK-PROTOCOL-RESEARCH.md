# NearLink Protocol Research (SSAP / Connection / Data Plane / Ranging)

Compiled 2026-08-16 (subagent B, network research + local clones). Key local sources: OpenHarmony stack clone `/mnt/hdd/nearlink-stuff/communication_nearlink_service`, WS73 SDK `sdk/ws73_sdk_linux_WS73_1.10.110`.

## (a) SSAP service model + opcodes + discovery flow

SSAP (SparkLink Service Access Protocol) rides over the SLE ACB (asynchronous connection-based) data link on a dedicated service-management TCID. Model mirrors BLE GATT with four item types: **service, property (≈characteristic), method, event**, each with descriptors. UUIDs 128-bit base `0x37BEA880-FC70-11EA-B720-000000000000` + 16-bit value LE at offset 14.

**Wire opcodes (definitive, from `ssap_pkt.h` in the local OHOS clone):**
- `0x01` SSAP_ERROR_RSP
- `0x02` EXCHANGE_INFO_REQ / `0x03` RSP (MTU + version; default MTU 251, max 1024; version 0x0101–0x0301)
- `0x04` FIND_STRUCTURE_REQ / `0x05` RSP; `0x06` FIND_BY_UUID_REQ / `0x07` RSP
- `0x08` READ_REQ / `0x09` READ_RSP
- `0x0A` READ_BY_UUID_REQ / `0x0B` RSP
- `0x0C` WRITE_CMD / `0x0D` WRITE_REQ / `0x0E` WRITE_RSP
- `0x0F` VALUE_NTF / `0x10` VALUE_IND / `0x11` VALUE_ACK
- `0x12` CALL_METHOD_CMD / `0x13` CALL_METHOD_REQ / `0x14` CALL_METHOD_RSP

**Find-type ctrl bits**: findType 3b (0=service structure, 1=primary service, 2=reference service, 3=property, 4=method, 5=event), itemType 2b, rspMode 1b. **Property operation bits**: READ=0x01, WRITE_NO_RSP=0x02, WRITE=0x04, NOTIFY=0x08, INDICATE=0x10, BROADCAST=0x20, DESCRIPTOR_WRITE=0x100. Descriptor types: 0=user description, 1=client config (CCCD), 2=server config, 3=presentation format. Special handles: SERVICE_CHANGE_EVENT=0x000E, HASH=0x000F. Fragmentation ctrl: 00 begin/01 mid/10 end/11 no-frag; max value 1024B.

**Discovery flow (sle_uuid_client.c):** register client → enable_sle → scan → connect → pair → exchange_info → find_structure (primary service, handles 1–0xFFFF) → find_property → read/write by handle. Server: ssaps_register_server → add_service → add_property (permissions+ops) → add_descriptor (CCCD `{0x01,0x00}`) → start_service → notify via ssaps_notify_indicate; read/write callbacks + ssaps_send_response.

## (b) Connection establishment flow

Roles: **T node (announcer ≈peripheral)** / **G node (seeker ≈central)**; GT role negotiated.

- **Announce:** set_local_addr/name → set_announce_param (handle=1, mode CONNECTABLE_SCANABLE=0x03, interval 0xC8=100ms, conn_interval 0x10=2ms@125µs, supervision 0x1F4=5s) → set_announce_data (≤251B) → start_announce.
- **Seek:** set_seek_param (filter 0, phys, type 0 passive/1 active, interval/window 100/100) → start_seek → seek_result_cb (event_type, addr, rssi, data) → match MAC → stop_seek → connect_remote_device.
- **Connect:** DLI_CREATE_CONNECTION=0x1401; connect_state_changed_cb with SLE_ACB_STATE_CONNECTED=0x01/DISCONNECTED=0x02. Pair → ssapc_exchange_info_req. Default conn param: interval 0x14=5ms, timeout 0x1F4.
- **Security/SM:** pair_remote_device, pair states PAIRING=0x02/PAIRED=0x03; crypto AC1/AC2/EA1/EA2, HA1/HA2; link_key 16B; DLI security 0x1C0x.
- **Params update:** update_connect_param; PHY/MCS tuning post-connect (frame 1–4, PHY 1M/2M/4M, MCS ≤10, set_data_len).

## (c) Data plane / channel types

Datatypes 0xA1 CMD / 0xA2 EVENT / 0xA3 ACB / 0xA4 ICB (identical DLI/WS73). **TCID at frame offset 5** (2B) demuxes data planes.

**TCID space:** 0x02 SLE_CMTC (mgmt), 0x0A SLE_SMTC (**SSAP rides here**), 0x1F SLE_CUTC (default unicast), 0x50–0x7F dynamic broadcast/multicast, 0x80–0xDF dynamic unicast. Max 101 link entries (SLE_MAX_TCID_NUMS=101).

**Channel kinds:**
- **ACB:** default unicast async link carrying SSAP + app data; low-latency sub-rate (125Hz–8kHz, sle_low_latency_set).
- **ICB:** isochronous channel for audio/streaming; DLI_SETUP_ICB_DATA_PATH=0x280D (verified accepted on WS73).
- **IOB/IOG/IMB/IMG:** sync links; SET_IOG_PARAM=0x2801, CREATE_IOB=0x2803 (verified accepted); events 0x0038/0x0061-0x0064 (IOB), 0x003A/0x0071-0x0074 (IMB).

**DTAP framing:** header = tcid(1B) + typeBits(frameType 4b: basic/aggr/frag/ack) + length(2B) + PI + txSeq/sar + payload + CRC16. Aggregation + fragmentation.

## (d) Ranging (测距)

HADM / Channel Sounding: sle_read_local/remote_channel_sounding_caps, set_param (freq_space, con_anchor_num, refresh_rate, cs_interval, posalg_freq), enable; callbacks cs_state_changed_cb + cs_iq_report_cb (IQ: rssi/freq/i_data/q_data, tof_result, timestamp). GTTT multi-antenna variants; GLP reports CFO. Controller DLI 0x2001-0x2005 (all verified accepted on WS73), events 0x0028/0x0029/0x002B/0x002C. Use cases: car keys, indoor positioning, tags.

## (e) AT-command / SLE-Link alternative paths — KEY FINDING

**`libsle_host.a` in the local WS73 SDK embeds the AT layer** (symbols `sle_at_dd.c`, `sle_at_cm.c`, `sle_at_ssapc.c`, `sle_at_ssaps.c`) — a WS73 dongle can run an AT/SLE-Link bridge over `/dev/hwsle` TODAY, an alternative to a full userspace stack.

AT flow (from Nearlink ToolBox docs): server `AT+SLEENABLE → AT+SLESETADDR → AT+SSAPSADDSRV → AT+SSAPSADDPROPERTY → AT+SSAPSSTARTSERV → AT+SLESETADVPAR → AT+SLESTARTADV`; client `AT+SLEENABLE → AT+SLESETSCANPAR → AT+SLESTARTSCAN → AT+SLECONN → AT+SSAPCFNDSTRU`; data `AT+SSAPCWRITECMD` / `AT+SSAPSSNDNTFY`.

## (f) Third-party code references

- OHOS nearlink stack (local clone `/mnt/hdd/nearlink-stuff/communication_nearlink_service`; gitcode.com/openharmony/communication_nearlink_service)
- fbb_ws63 (WS63): gitee.com/HiSpark/fbb_ws63, mirrors github.com/x-eks-fusion/fbb_ws63, Hny0305Lin/Hihope_WS63_NearLink_SDK
- fbb_bs2x (BS20/BS21E/BS22): gitcode.com/HiSpark/fbb_bs2x, mirrors yanlinkos/fbb_bs2x, sanchuanhehe/fbb_bs2x_dev; docs docs.hisilicon.com/repos/fbb_bs2x/
- rzy0901/sle_measure_sdk1.0.12 (measurement/throughput/latency samples)
- QTDS138/NearLinkSLE (SLE_UART_HE, service 0x3333/char 0x3434, MTU 520)
- NearLink-ePaper/NearLink-Mesh-ePaper (SLE mesh over SSAP)
- OpenSparklink/nearlink_sdr_sim (Python SDR, TXS-10002-2025 data-link procedures)
- MiraHikari/nearlink-firmwares (AT firmware + PDF manuals)
- WS73 SDK local: include/bsle/sle/*.h, application/sample/sle/sle_uuid/, application/sle_android/ (NearlinkManager/Announcer/Seeker/Ssap* — framework layer above libsle_host.a)

## (g) Still unknown / where to find

1. Exact DLI PDU layouts per opcode on WS73 firmware — verify on hardware via /dev/hwsle (dli_cmd_struct.h is the lead)
2. SSAP PDU packing — confirm against libsle_host.a behavior
3. SLE SM pairing handshake details — OHOS cm/ + dli_secu_event.c + sle_connection_manager.h crypto enums
4. ICB isochronous service params — nearlink_sdr_sim + OHOS cm/icb/
5. Standard text TXS-10002-2025 — member-only via isla.org.cn; SDR sim + OHOS are best public proxies
6. WS73 AT dialect specifics — BearPi/HiHope PDFs in nearlink-firmwares
7. Ranging algorithm internals — fbb_bs2x sle_measure_dis_server_alg.c
