# BS21/WS63 SDK vs OHOS — SSAP Dialect Comparison

Sources (cloned to /mnt/hdd/nearlink-stuff/): fbb_ws63, fbb_bs2x, sle_measure_sdk.

## KEY FINDING: device-side SSAP is the SAME wire protocol as OHOS

Device firmware SSAP server flow (identical across BS2x/WS63/measure):
```
enable_sle() → ssaps_register_server(app_uuid,&sid)
→ ssaps_add_service_sync(sid, uuid, 1, &sh)
→ ssaps_add_property_sync(sid, sh, {uuid, perm, op_ind, val}, &ph)
→ ssaps_add_descriptor_sync(sid, sh, ph, {CLIENT_CONFIGURATION, {0x01,0x00}})
→ ssaps_start_service(sid, sh) → ssaps_set_info(sid, {mtu, ver})
→ sle_set_announce_param/data → sle_start_announce
→ data: ssaps_notify_indicate(sid, conn, {ph, TYPE_VALUE, data, len})
```
Client: seek → connect → pair → ssapc_exchange_info_req → ssapc_find_structure(PRIMARY_SERVICE 0x01) → read/write.

Constants identical: SSAP_FIND_TYPE_PRIMARY_SERVICE=0x01, PROPERTY=0x03, DESCRIPTOR_CLIENT_CONFIGURATION=0x02, SSAP_PROPERTY_TYPE_VALUE=0x00, TCID_SLE_SMTC=0x0A.

OHOS PDU constants (SSAP_PDU_BASE_LEN=2, EXCHANGE_INFO_PKT_LEN=6, REPLY_MASK=0x80, opcodes, error codes) map 1:1 onto device-side ssap_errcode_t (0x01-0x15). **CONFIRMED: the wire protocol is shared — BS2x firmware is the authoritative reference for the WS73 host to drive.**

## Differences WS63 vs BS2x (device side only)
BS2x adds: ssap_errcode_t, ssaps_register_multi_callbacks, read_by_uuid request cbks, ssaps_indicate_cfm, descriptor-write op bits, access-filter-list, passkey, channel-sounding GTTT, sle_device_manager.h (enable_sle moved there).

## Measurement SDK (throughput/latency/CE/DD samples)
- Params (menuconfig): conn interval 0x14=2.5ms (up to 20ms), MTU 1500, pkt len 1370, frame 0/1, PHY 1M/2M/4M, MCS 0-10, tx power NV 0x20A0, scan interval 24ms
- Throughput: sle_set_data_len + sle_set_phy_param + sle_set_mcs → ssaps_notify_indicate gated on gle_tx_acb_data_num_get() QoS; client counts + RSSI per packet
- Latency: GPIO pulses + uapi_tcxo_get_us() timestamps; DD = seek time; CE = connect time over N cycles

## Implication
Our slim SSAP server (stack/ssap/) matches the device-side flow — when a WS73 dongle (running our host) connects to a BS2x/WS63 device, the SSAP handshake will interoperate. Sample asset paths: fbb_bs2x/src/application/samples/products/{sle_uart,sle_measure_dis,sle_ota_dongle}/, fbb_ws63/src/application/samples/bt/sle/{sle_uuid_server,sle_speed_server}/.
