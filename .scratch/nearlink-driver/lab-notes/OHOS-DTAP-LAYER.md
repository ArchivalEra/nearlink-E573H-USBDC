# OpenHarmony NearLink Data Plane (DTAP + Transport) — Deep Dive

Root: `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/`

## 1. DTAP Frame Format (byte-level)

Defined in `dtap/dtap_frame.h` (packed, length fields LE):

- **Basic header** (DTAP_BasicHeader_S, 4 bytes, dtap_frame.h:108-121): byte0=tcid; byte1=frameType:4 + optionBit:1 + crcBit:1 + pBit:1 + fBit:1; bytes2-3=length (counts bytes *after* this field). CRC optional in basic, mandatory in enhanced.
- **Frame types** (dtap_frame.h:37-48): BASIC=0b0000, SIMPLEX_AGGR=0b0001, SIMPLEX_FRAG=0b0010, DUPLEX_AGGR=0b0011, DUPLEX_FRAG=0b0100, ACK=0b0101, MEAS_REQ=0b0110, MEAS_RSP=0b0111.
- **PI** (protocol indicator): 1 byte after basic header, present only when tcid > 0x1D (dtap_frame.h:137).
- **SAR** (dtap_frame.h:50-55): UNSEG=0b00/FIRST=0b01/MID=0b10/LAST=0b11, in bits 14-15 of seq word for frag frames.
- **Simplex frag** (7 bytes = 4+1pi+2seq): tcid|type|bits|len|pi|txSeq:14|sar:2|payload|crc(2).
- **Duplex frag** (9 bytes): adds reqSeq:14|rfu:2. ACK: reqSeq:14|sBit:1|rfu:1.
- **Aggregation**: aggregate frames hold DTAP_AggregateSdu_S = uint16 length + data[0]. Builders currently return NOT_SUPPORT_ERR — only frag building implemented.
- **Fragmentation**: DTAP_FragmentFrame splits at mps, max 128 fragments, SAR assigned (dtap_frame_enhance.c:185-254). Max payload 32K.

## 2. Transport Modes

CM_TRANS_MODE_BASIC=0x00/TRANSPARENT/STREAM/RELIABLE/MAX (cm_def.h:108-112). Registered as DTAP_TransMode_S vtable per mode; mode comes from channel config `channel->mode = param->config.transMode` (dtap_channel.c:351), delivered by CM channel-state callback.

- **Basic**: no seq/ACK/CRC; RX caches up to 64 frames.
- **Transparent**: no frame build — raw send/recv.
- **Stream**: simplex-frag + txSeq + CRC + reorder timer, no retrans/ACK.
- **Reliable**: full windowed ARQ — tx/rx windows (default 200 = CM_CAP_WND), CRC16 verify, ACK/NACK, retransmission w/ exponential backoff, reorder timer, pBit polling, NACK teardown.

## 3. DTAP_DataSend path

DTAP_DataSend (dtap.c:313-352): validate → ChannelSearch(lcid,tcid) → MTU check → transMode->sendFrame → buildFrame (prepend header, append CRC) → DTAP_SendFrame → priority scheduler → DLI_DataSend (DLI_DATATYPE_ACB) → DLI splits into ACB fragments → SleSendDliPacket to /dev/hwsle.

## 4. CLTP transport

CLTP = Light Weight Connectionless-mode Transport Protocol (socket-style, UDP-like). Header (transport_cltp.h:36-76): TRANS_ProtoBasicHeader_S (version:3=1, optionLen:5 in 4-byte units, srcPort, dstPort — big-endian) + option bitmap + payloadLen. 8-byte header prepended. Used only when pi == DTAP_PI_LWCLTP.

## 5. tcid → connection mapping

Two-level: g_dtapLogicLinksMap (lcid → DTAP_Logic_Link_S), each holds dlist of DTAP_Channel_S; DTAP_ChannelSearch(lcid,tcid) matches srcTcid. RX callbacks per tcid (TCID_SLE_CMTC→CM signaling) or per PI (DTAP_PI_LWCLTP→TRANS). RX entry: DTAP_DataRecv(lcid, buf) registered at nlstk_init.c:140.

## 6. QoS / flow control

- ChannelPriority (FRAGMENT/CMD/HIGH/NORMAL) indexes 4 scheduler queues.
- ACB buffer quota: g_apBufferNum tracks chip ACB buffer count via DLI_DataNumGet; per-lcid quotas; DTAP_CanSend gates on sendNotAckPktCnt < apBufferNum && node quota.
- Reliable window 200 PDU; pBit polling every window/4.

## 7. Wire format on /dev/hwsle (0xA3 frames)

TX: socket → TRANS_SendData → CLTP 8B header → DTAP_DataSend → mode buildFrame (DTAP header + CRC) → scheduler → DLI_DataSend → DLI 5-byte header `[0xA3 | handle(12b)+pb(2b)+ts+prio | len(16b)]` → /dev/hwsle.

So a wire 0xA3 frame's payload is the DTAP frame, whose payload is CLTP header + app data. Basic mode: `[0xA3][hdl][len] | tcid | frameType+bits | len | payload`.

## Implication for our WS73 transport

**IMPORTANT**: this is the OHOS stack's framing. The WS73 SDK's /dev/hwsle kernel driver has its OWN framing (CHBA: tcid@offset5 per sle_hci_chba_proc.h) — we verified this on hardware. Our hwsle_transport.c uses the WS73 framing (0xA3 + tcid u16 + len u16), which is correct for WS73. The DTAP layer would only matter if we port the OHOS stack wholesale; for a slim SSAP-over-WS73 path, SSAP PDUs ride directly in WS73 ACB frames on tcid 0x0A.
