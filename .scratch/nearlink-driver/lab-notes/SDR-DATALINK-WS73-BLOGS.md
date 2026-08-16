# SDR Data-Link & WS73 Blog Research

Sources:
- https://github.com/OpenSparklink/nearlink_sdr_sim (master, TXS-10002-2025 SparkLink SLE link-level simulator, AGPL)
- https://blog.csdn.net/eayayaya/article/details/156680047 (WS73 USB / i.MX6ULL)
- https://blog.csdn.net/iikat/article/details/147385864 (WS73 SDIO / Hi3516CV610)

---

## PART 1: NearLink SDR sim — data-link layer (standard ch. 6.5, 7.3)

Key file: `src/nearlink_sdr/phy/data_link.py` (standard clauses 6.5.1~6.5.3).

### 1.1 Transmission modes & adaptation
- TransmissionMode: UNICAST=0, MULTICAST=1, BIDIRECTIONAL_MULTICAST=2, FEEDBACK_MULTICAST=3, BROADCAST_ASYNC=4, BROADCAST_SYNC=5.
- AdaptMode: PERIODIC=0, APERIODIC=1 (sync data-link only).

### 1.2 Async data-link params — AsyncDataLinkParams
- Timings (us): event_group_start, event_group_period, event_period, intra_event_interval, inter_event_interval, event_group_interval, tx/rx_max_time_offset.
- tx_pdu_max / rx_pdu_max default 251 bytes; tx_sdu_max / rx_sdu_max default 0.

### 1.3 Async flow control — AsyncFlowControl
- Unicast first-transmit stop when: tx flow_ctrl==0 AND rx flow_ctrl==0 AND rx ACK AND rx data_len==0; ALSO stops when no rx feedback.
- Multicast semi-reliable: leader stops when tx flow_ctrl==0 and no NACK.

### 1.4 Sync data-link params — SyncDataLinkParams
- Adds event_count (default 1), new_pkt_count (default 1), discard_period (default 3), adapt_mode, sync_anchor_delay_us, sync_ref_delay_us. PDU maxima 251.

### 1.5 Sync flow control / 1.6 Sync data discard
- Unicast stop same as async BUT no-feedback does NOT stop (differs from async).
- local_baseline = 0 when event_group_index < discard_period, else new_pkt_count * (event_group_index - discard_period + 1).
- Passive discard: payload_id < local_baseline; Active discard (FT3/FT4): tx_sn++ on NACK.

### 1.7 Event group set — multiple event groups, offsets = i * spacing_slots * 125us.

### 1.8 Periodic adaptation — segments_per_sdu = ceil(sdu_max/pdu_max); sdus_per_event_group = period/sdu_period; TX accept t0 = start - (n-1)*sdu_period - ref_delay; RX deliver r1 = start + anchor_delay + (discard_period-1)*period.

### 1.9 Aperiodic adaptation — SDU fragmented to PDUs of (pdu_max-4); first fragment carries 24-bit time_offset_us.

### 1.10 MAC data-plane frames (standard 7.3.3) — mac/frame.py
- ControlFrame: [data_type_index 2B][data_length 1B][payload]. 130 signaling types: 109 link-control (0x0000-0x0070), 3 power-control, 18 security pairing.
- AsyncDataFrame: header 2B = segment_type(2b) + length(11b, max 2047) + reserved(3b) + payload. SegmentType: COMPLETE=0/FIRST=1/MIDDLE=2/LAST=3.
- SyncDataFrame: header 4B = pdu_seq(5b) + event_group(8b) + frame_format(1b) + segment_type(2b) + length(11b); +3B time_offset (aperiodic) and/or +1B sdu_seq (periodic).
- MuxFrame: concatenation of control frames + optional async/sync data frame.

### 1.11 PHY frame (standard 6.3) — preamble | sync | control info | payload(+pilots)
- FT1: GFSK 10us, 32b Gold, A-group CRC12, no Polar, no pilots, MCS8 uncoded.
- FT2: QPSK 10us, 64b, A Polar(64,K), pilots every 16.
- FT3: QPSK 12us, 62b BPSK m31, B Polar(256,K), pilots every 4.
- FT4: BPSK 16us, 126b m63, B Polar(256,K), pilots every 4.
- Control info A2 (async data): packet_type+empty_packet+tx_sn+rx_sn+flow_ctrl+sys_mgmt_rx+data_length(11b); A5 (sync) swaps sys_mgmt_rx for async_sched.
- CRC12 (A-group), CRC24A/B (B-group seed 0x555555^LLID), CRC32; Polar N=32..1024; scrambler x^7+x^4+1.

### 1.12 QoS / retransmission
- ARQ SN modulus: FT1/FT2=2, FT3/FT4=32. Async default max_retransmit=0 (unlimited).
- HARQ TB/CBG modes; FlowController high/low watermarks 16/4; 5 priority queues.

### 1.13 MAC layer (ch. 7): LinkManager 8-state FSM (IDLE/BROADCASTING/SCANNING/ACCESSING/CONNECTED/PAIRING/DORMANT/DISCONNECTED), 21 events, 25 control-plane procedures; T_sys=125us base slot.

### 1.14 Build: `uv sync`; 2652 pytest tests; target HW USRP E310.

## PART 2: WS73 Linux porting blogs

### 2.1 SDIO / Hi3516CV610 (iikat)
Module load order: `insmod plat_soc.ko; wifi_soc.ko; ble_soc.ko; sle_soc.ko`. Firmware /opt/109sdk/firmware/us/*.bin → /etc/ws73/. Configs: wakeup=GPIO7_5=61, poweron=GPIO7_4=60. Kernel: CONFIG_BT/CFG80211/MAC80211/netfilter; DTS sdio1 okay. WiFi userland: libnl + wpa_supplicant/hostapd; wpa_cli scan/connect; udhcpc. **SLE untested by author** — deferred to HiSilicon guides.

### 2.2 USB / i.MX6ULL (eayayaya)
No DTS/kernel changes. WSCFG_BUS_USB=y. Load: BLE = plat_soc+ble_soc; WiFi = plat_soc+wifi_soc. Firmware → /etc/tr5330/. BLE = full BlueZ 5.64 cross-build. WiFi = libnl+openssl+wpa_supplicant 2.10+hostapd from SDK open_source/. **SLE untested**.

### 2.3 Blog gap
Neither blog verifies sle_soc.ko runtime or SSAP userspace. Our project (sdk/ + stack/ + lab-notes) is deeper on SLE/SSAP than the public blogs.
