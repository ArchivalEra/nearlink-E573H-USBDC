# OHOS DTAP Layer Deep Dive — Data Plane Reference for WS73 Host Stack

**Date:** 2026-08-17
**Scope:** Read-only analysis of the OpenHarmony NearLink DTAP (Data Transmission and Adaptation Protocol) layer + comparison with our WS73 `hwsle_transport.c`.
**Read-only constraint:** No network, build, or hardware operations.

---

## Sources

| # | File | Role |
|---|------|------|
| 1 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/interface/dtap.h` | Module API, priority enum, PI enum |
| 2 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/interface/dtap_tcid.h` | TCID channel numbering |
| 3 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/interface/dtap_errno.h` | Error codes |
| 4 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/include/dtap_frame.h` | Frame format structs, bit layout |
| 5 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/include/dtap_channel.h` | Channel structs (Basic/Stream/Reliable) |
| 6 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/include/dtap_scheduler.h` | Scheduler API |
| 7 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/include/dtap_trans.h` | Transport mode vtable |
| 8 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/src/dtap.c` | Module init/deinit, send/recv entry points |
| 9 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/src/dtap_channel.c` | Channel lifecycle, logic-link map |
| 10 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/src/dtap_frame.c` | Frame dispatch, CRC, extension parsing |
| 11 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/src/dtap_frame_basic.c` | Basic frame build/parse |
| 12 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/src/dtap_frame_enhance.c` | Enhanced frames (frag/aggr/ACK) |
| 13 | `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/dp/dtap/src/dtap_scheduler.c` | Priority scheduler, credit gating |
| 14 | `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/include/hwsle_transport.h` | Our WS73 transport header |
| 15 | `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/src/hwsle_transport.c` | Our WS73 transport implementation |
| 16 | `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/COMMUNITY-PROJECTS.md` | sle_measure_sdk credit-gating reference |

---

## 1. DTAP Architecture

DTAP ("Data Transmission and Adaptation Protocol") is the OHOS NearLink stack's data-plane layer, sitting between the CM (Connection Manager) signaling layer above and the DLI (Data Link Interface) layer below. It provides multi-channel multiplexing, frame encapsulation, fragmentation/aggregation, scheduling, and credit-based flow control.

### 1.1 Module Responsibilities

**dtap.c** — module entry point and data routing:

- `DTAP_Init()` (dtap.c:365-411): Registers frame contexts (basic + enhanced), transport modes (basic/transparent/stream/reliable), initializes CM signaling, registers CM recv callback on `TCID_SLE_CMTC`, initializes channel subsystem.
- `DTAP_DataSend()` (dtap.c:313-352): The send entry point. Validates data, searches the channel by `(lcid, tcid)`, checks MTU, dispatches to the appropriate transport mode's `sendFrame`.
- `DTAP_DataRecv()` (dtap.c:244-286): The recv entry point. Reads tcid from first byte, searches channel, dispatches to `transFrame` (transparent) or `DTAP_DataRecvFrame` (all others). Transparent mode passes data through without frame parsing.
- `DTAP_RegisterDataRecvCb()` / `DTAP_RegisterProtoRecvCbk()` (dtap.c:74-91): Two-level callback registration — per-tcid for fixed channels (e.g., CM signaling on 0x02), per-PI for protocol-indicator-routed channels (e.g., CLTP/IPv4/IPv6 on dynamic tcid > 0x1D).

### 1.2 dtap_channel — Channel Lifecycle Management

**dtap_channel.c** manages the two-level channel hierarchy:

- `g_dtapLogicLinksMap` (dtap_channel.c:50): A `SDF_Map` keyed by `lcid` (uint16), each value is a `DTAP_Logic_Link_S` containing a doubly-linked list of `DTAP_Channel_S` nodes.
- `DTAP_ChannelSearch(lcid, tcid)` (dtap_channel.c:204-213): Two-level lookup — first find the logic link by lcid, then iterate its channel list matching `srcTcid`. Transparent-mode channels return immediately without tcid check (dtap_channel.c:193-195).
- Channel creation (`DTAP_ChannelCreate`, dtap_channel.c:327-370): Allocates `DTAP_Channel_S`, sets priority based on tcid (CMTC=CMD, <=CUTC=HIGH, >=BC_BEGIN=NORMAL), creates mode-specific attr (Basic/Stream/Reliable).
- `DTAP_ChannelStateChangeCbk` (dtap_channel.c:448-466): Called by CM when channels are activated or released. Activated -> `DTAP_ChannelAdd`; Released -> `DTAP_ChannelDel`.
- `DTAP_LogicLinkStateChangeCbk` (dtap_channel.c:476-497): On link connected, flushes cached basic-mode frames via `DTAP_RecvBasicFrameContinue`.

**Key design: channels are not hard-coded; they are dynamically created/destroyed per-connection by the CM layer.** This is fundamentally different from our static single-channel approach.

### 1.3 dtap_frame — Frame Encapsulation/Decapsulation

Frame handling uses a vtable pattern (`DTAP_FrameCtx_S`, dtap_frame.h:72-78) with `parseFrame`/`checkFrameHeader`/`buildFrame` methods. Frame contexts are registered at init and dispatched by the 4-bit `frameType` field in the basic header.

- `DTAP_ParseFrame()` (dtap_frame.c:51-63): Dispatches to the registered frame context's `parseFrame`.
- `DTAP_SendFrame()` (dtap_frame.c:65-74): Calls `DTAP_DataSendWithPriority` (the scheduler).
- `DTAP_ParseExtension()` (dtap_frame.c:128-166): Parses TLV extension headers (up to 16 extensions).

### 1.4 dtap_scheduler — Priority Scheduling and Credit Gating

**dtap_scheduler.c** implements a 4-level priority queue with per-LCID fairness and ACB buffer credit gating. See Section 5 for full details.

### 1.5 dtap_trans — Transport Mode Vtable

**dtap_trans.h** defines `DTAP_TransMode_S` (dtap_trans.h:41-51): a vtable with `checkFrame`, `recvFrame`, `sendFrame`, `transFrame` (transparent passthrough), and `setTransChannelStatus` methods. Four modes are registered:

| Mode | Type Enum | Frame Type Used | CRC | Seq/ACK | Purpose |
|------|-----------|-----------------|-----|---------|---------|
| Basic | 0x00 | BASIC (0b0000) | No | No | Simple data, RX caches up to 64 frames |
| Transparent | 0x01 | None (raw) | No | No | Raw passthrough, no header |
| Stream | 0x02 | SIMPLEX_FRAG (0b0010) | Yes | txSeq only | Unidirectional with reorder timer |
| Reliable | 0x03 | DUPLEX_FRAG (0b0100) | Yes | txSeq + reqSeq + ACK/NACK | Full ARQ with retransmission |

---

## 2. TCID Channel System

Defined in `dtap_tcid.h` (dtap_tcid.h:31-57):

```
Fixed channels (0x01-0x1D):
  0x01  TCID_SLB_CMTC   — SLB Common Management
  0x02  TCID_SLE_CMTC   — SLE Common Management (CM signaling)
  0x09  TCID_SLB_SMTC   — SLB Service Management
  0x0A  TCID_SLE_SMTC   — SLE Service Management (SSAP rides here)
  0x10  TCID_SLB_RSMTC  — SLB Relay Service Management
  0x11  TCID_MDCMTC     — Multi-Domain Coordination
  0x12  TCID_5GITC      — 5G Interworking
  0x1D  TCID_FTC_RFU_END — Fixed channel range ends

Dynamic channels (0x1E+):
  0x1E  TCID_SLB_CUTC   — SLB default unicast
  0x1F  TCID_SLE_CUTC   — SLE default unicast
  0x50-0x7F  Broadcast/Multicast dynamic range
  0x80-0xDF  Unicast dynamic range
  0xFF  TCID_MAX
```

### 2.1 Priority Assignment

From `DTAP_ChannelCreate` (dtap_channel.c:335-343):

| TCID Range | Priority Level | Enum Value |
|------------|---------------|------------|
| 0x02 (CMTC) | CMD | DTAP_PRIORITY_CMD = 1 |
| 0x01-0x1F (<=CUTC) | HIGH | DTAP_PRIORITY_HIGH = 2 |
| 0x50-0xDF (BC/UC) | NORMAL | DTAP_PRIORITY_NORMAL = 3 |
| Fragment channel (special) | FRAGMENT | DTAP_PRIORITY_FRAGMENT = 0 (highest) |

**Important:** `DTAP_PRIORITY_FRAGMENT = 0` is the highest priority, used for DLI-layer fragment retransmission. It uses a synthetic `DTAP_FRAGMENT_TCID = TCID_MAX (0xFF)` to distinguish fragment packets.

### 2.2 PI (Protocol Indicator)

For tcid <= 0x1D (fixed channels), the PI byte is omitted from the frame header (dtap_frame_basic.c:50-53). For tcid > 0x1D (dynamic channels), a 1-byte PI follows the basic header (dtap_frame_basic.c:55-62). PI values (dtap.h:37-47):

| PI | Protocol |
|----|----------|
| 0x00 | None (reserved) |
| 0x01 | IPv4 |
| 0x02 | IPv6 |
| 0x03 | LWCLTP (connectionless transport) |
| 0x04 | LWCTP (connection-oriented transport) |
| 0x05 | WNP (Wireless Network Protocol) |
| 0x06 | WAP (Wireless Adjacency Protocol) |

This means SSAP PDUs on tcid 0x0A have **no PI byte** — the channel itself identifies the protocol.

---

## 3. Frame Format — Byte-Level Analysis

### 3.1 Basic Frame

Defined in `dtap_frame.h:108-138` and built in `dtap_frame_basic.c:81-107`.

**Layout (4 bytes minimum):**

```
Byte 0:     tcid (8 bits)
Byte 1:     frameType:4 | optionBit:1 | crcBit:1 | pBit:1 | fBit:1
Bytes 2-3:  length (16-bit LE) — counts bytes AFTER this field
[Optional]  pi (1 byte) — only when tcid > 0x1D
[Optional]  extension (variable) — only when optionBit=1
[Variable]  payload
[Optional]  crc (2 bytes) — only when crcBit=1
```

**Basic frame constraints** (dtap_frame_basic.c:67-79):
- `optionBit` must be 0 (no extensions in basic frame)
- `crcBit` must be 0 (no CRC in basic frame)
- `pBit` and `fBit` must be 0

**Our ACB wire format comparison:**

On the WS73 `/dev/hwsle` wire, an ACB frame has a 5-byte DLI header:
```
[0xA3][handle_lo][handle_hi_prio_ts][len_lo][len_hi][DTAP payload...]
```
The DTAP payload is the DTAP basic frame starting with `tcid`. So our `hwsle_transport_send_acb()` (hwsle_transport.c:51-71) writes `[0xA3][tcid u16][len u16][payload]` — but this is the **WS73 kernel DLI framing**, not the OHOS DTAP framing. The OHOS DLI header is different (handle+priority+timestamp encoded differently). The DTAP basic frame sits inside the ACB payload on the OHOS stack, but on WS73, our SSAP PDU rides directly inside the 0xA3 payload without a DTAP header.

**This is the critical difference:** We skip the entire DTAP frame layer. Our SSAP PDU is the raw payload of the WS73 ACB frame. The OHOS stack wraps each PDU in a DTAP header (tcid + frameType + length) before handing it to DLI. Since our WS73 kernel driver does its own tcid-based routing (we verified tcid is at offset 5 in the DLI header), the DTAP-level tcid in the OHOS stack is redundant for our case.

### 3.2 Simplex Fragment Frame (Stream mode)

Layout (dtap_frame.h:162-172):

```
Byte 0:     tcid (8 bits)
Byte 1:     0b0010 | o | c | p | r
Bytes 2-3:  length (16-bit LE)
Byte 4:     pi (always present in enhanced frames)
Bytes 5-6:  txSeq:14 | sar:2
[Optional]  extension (variable)
[Variable]  payload
Bytes N-1,N: crc16 (2 bytes, mandatory)
```

Total header: 7 bytes (dtap_frame.h:310).

SAR values (dtap_frame.h:50-55):
- 0b00 = UNSEG (unfragmented, complete SDU)
- 0b01 = FIRST (first fragment)
- 0b10 = MID (middle fragment)
- 0b11 = LAST (last fragment)

### 3.3 Duplex Fragment Frame (Reliable mode)

Layout (dtap_frame.h:214-226):

```
Byte 0:     tcid
Byte 1:     0b0100 | o | c | p | f
Bytes 2-3:  length
Byte 4:     pi
Bytes 5-6:  txSeq:14 | sar:2
Bytes 7-8:  reqSeq:14 | rfu:2
[Optional]  extension
[Variable]  payload
Bytes N-1,N: crc16
```

Total header: 9 bytes (dtap_frame.h:312).

The `reqSeq` field carries the receiver's expected next sequence number (piggybacked ACK in data frames). The `fBit` in the basic header indicates this is an ACK response to a poll.

### 3.4 ACK Frame

Layout (dtap_frame.h:268-274):

```
Byte 0:     tcid
Byte 1:     0b0101 | o | c | r | f
Bytes 2-3:  length
Bytes 4-5:  reqSeq:14 | sBit:1 | rfu:1
[Optional]  extension
[Variable]  (empty payload)
Bytes N-1,N: crc16
```

`sBit = 0`: normal ACK; `sBit = 1`: negative ACK (NACK).

### 3.5 Aggregation Frames

Simplex aggregate (frameType = 0b0001) and duplex aggregate (frameType = 0b0011) frames carry multiple SDUs in one frame. Each SDU is prefixed with a 2-byte length (dtap_frame.h:279-282):

```
DTAP_AggregateSdu_S: [length u16 LE][data[0..length-1]]
```

**Current implementation status:** `DTAP_BuildSimplexAggrFrame` and `DTAP_BuildDuplexAggrFrame` both return `DTAP_ENHANCED_FRAME_NOT_SUPPORT_ERR` (dtap_frame_enhance.c:327, 424). Only fragmentation is implemented; aggregation is parsed but not built.

---

## 4. Transport Mode Details

### 4.1 Basic Mode

- No CRC, no sequence numbers, no ACK.
- RX side caches up to 64 frames in a linked list (`DTAP_Basic_Channel_S.cacheRxBuffs`) waiting for the upper layer to register a recv callback (dtap_channel.c:215-225).
- `isReady` is initially `false` for SMTC (tcid 0x0A) — the basic channel waits for upper-layer readiness before delivering cached frames (dtap_channel.c:222).

### 4.2 Transparent Mode

- No frame header at all. `transFrame` method is set; data passes through raw.
- Channel is 1:1 mapped with logic link — no tcid lookup on receive (dtap_channel.c:193-195).

### 4.3 Stream Mode

- Uses simplex fragment frames with txSeq (14-bit) and SAR.
- CRC16 mandatory.
- Reorder timer (default 1000ms) and flush timer (default 1000ms).
- `expectedSeq` / `maxExpectedSeq` tracking for reordering.
- No retransmission or ACK — fire-and-forget with ordering.

### 4.4 Reliable Mode

- Full windowed ARQ using duplex fragment frames.
- **Tx/Rx windows** (dtap_channel.h:49-62): default size = `CM_CAP_WND` (200 PDUs).
- **Three timers** (dtap_channel.c:295-305):
  - `reorderTimeout` = 10000ms (10s) — if incomplete sequence not received, flush
  - `retransTimeout` = 1000ms — retransmit unacknowledged frames
  - `rspTimeout` = 1000ms — poll response timeout (periodic, started at channel creation)
- **Polling** (`pBit`): `sendPollingCounter` increments per window/4; when reached, a poll is sent requesting the peer to ACK.
- **NACK teardown**: `nackCnt` tracks consecutive NACKs; excessive NACKs trigger channel teardown.
- **Cache buffer** (`cacheBuff`): when txWindow is full, the most recent PDU is cached and retried when window space opens (dtap.c:442-476).
- **CRC16** with configurable `crcInit` seed per channel.

---

## 5. Scheduler — Priority Queues, Credit Gating, Fairness

### 5.1 Architecture

The scheduler (dtap_scheduler.c) manages 4 priority queues (dtap_scheduler.c:71):

```c
static DTAP_PriorityQueue g_dtapScheduler[DTAP_PRIORITY_MAX] = {0};
// [0]=FRAGMENT, [1]=CMD, [2]=HIGH, [3]=NORMAL
```

Each `DTAP_PriorityQueue` contains:
- `lcidList`: linked list of `DTAP_LcidNode`, each representing one logical connection
- `pktCnt`: total pending packets in this priority level

Each `DTAP_LcidNode` contains:
- `channelList`: linked list of `DTAP_Channel_S` nodes belonging to this LCID
- `pktCnt`: pending packets for this LCID
- `hasScheduled`: fairness flag

### 5.2 Send Path

`DTAP_DataSendWithPriority()` (dtap_scheduler.c:765-812):
1. Validates scheduler is initialized and parameters.
2. Rejects if queue is full (`pktCnt >= DTAP_PACKET_MAX_SIZE = 10000`, except CMD queue).
3. Creates a `DTAP_PendingPacket` with a monotonically increasing sequence number.
4. Pushes to the appropriate priority queue.
5. Calls `DTAP_SchedulerRun()` to attempt immediate send.

### 5.3 Scheduling Algorithm

`DTAP_SchedulerRun()` (dtap_scheduler.c:695-713):
1. Iterates priorities from FRAGMENT (0) to NORMAL (3) — **highest priority first**.
2. If global `g_sendNotAckPktCnt >= g_apBufferNum` (chip buffer full), stops entirely.
3. For each priority queue, calls `DTAP_SchedulePriority()`.

`DTAP_SchedulePriority()` (dtap_scheduler.c:667-693):
1. Iterates through all `DTAP_LcidNode`s in the queue.
2. For each LCID, checks `DTAP_CanSend()` (credit gate).
3. Calls `DTAP_ScheduleLcid()` to send packets from that channel.
4. **Fairness:** After all LCIDs have been serviced, previously-scheduled LCIDs are moved to the tail of the list (dtap_scheduler.c:684-691). This ensures round-robin fairness within each priority level.

`DTAP_ScheduleLcid()` (dtap_scheduler.c:610-665):
1. Peeks at the first channel with pending packets.
2. For each packet: if already split (DLI fragment), sends directly; otherwise splits via `DLI_SplitData` and sends fragments one by one.
3. Respects the per-LCID credit gate during fragment sending.
4. If credit exhausted mid-fragment-send, saves unsent fragments to a special fragment channel (`DTAP_PRIORITY_FRAGMENT`).
5. After sending, moves the channel to the tail of its LCID's channel list for fairness (dtap_scheduler.c:498-502).

### 5.4 Credit Mechanism (ACB Buffer Gating)

This is the flow control backbone. From `dtap_scheduler.c`:

**Global state:**
- `g_apBufferNum` (dtap_scheduler.c:76): Total ACB buffer slots in the chip, default 1 (DTAP_MIN_BUFFER_NUM). Updated dynamically via `DTAP_DLIDataNumChangecbk`.
- `g_sendNotAckPktCnt` (dtap_scheduler.c:73): Count of packets sent but not yet acknowledged by the chip.

**Per-LCID state** (`DTAP_LcidBufferNode`, dtap_scheduler.c:64-69):
- `quota`: Send quota for this LCID, recalculated on connect/disconnect.
- `sendNotAckPktCnt`: Per-LCID unacknowledged count.

**Credit gate logic** — `DTAP_CanSend()` (dtap_scheduler.c:505-508):
```c
return node == NULL ? false
    : g_sendNotAckPktCnt >= g_apBufferNum ? false   // global: chip buffer full
    : node->sendNotAckPktCnt < node->quota;          // per-LCID: quota exceeded
```

**Quota calculation** — `DTAP_RecalcLcidQuota()` (dtap_scheduler.c:140-157):
```c
quota = g_apBufferNum / lcidNums;   // equal share
remainder = g_apBufferNum - (quota * lcidNums);
// first `remainder` LCIDs get quota+1, rest get quota
```
Quotas are **equal-share** across all connected LCIDs, with remainder distributed to the first N connections.

**Credit release** — `DTAP_SendCompleteCbk()` (dtap_scheduler.c:814-826):
- Called by DLI when the chip acknowledges sent packets (`numCompletedPackets`).
- Decrements both global and per-LCID counters.
- Calls `DTAP_SchedulerRun()` to resume sending if credits freed up.

**Credit on disconnect** — `DTAP_DLIDisconnectCbk()` (dtap_scheduler.c:176-194):
- Subtracts the disconnected LCID's unacknowledged count from the global counter.
- Removes the LCID buffer node and recalculates quotas.

### 5.5 DLI Fragmentation

When a DTAP packet is larger than the DLI fragment size (`DLI_GetFragmentMaxLen()`), the scheduler:
1. Calls `DLI_GetDataFragmentNums()` to compute fragment count.
2. Splits via `DLI_SplitData()` (dtap_scheduler.c:510-534).
3. Sends fragments sequentially, respecting credit.
4. If credit exhausted mid-send, saves unsent fragments to a special `DTAP_PRIORITY_FRAGMENT` queue channel (dtap_scheduler.c:569-608).

---

## 6. Gap Assessment — Our hwsle_transport vs. OHOS DTAP

### 6.1 What We Do

Our `hwsle_transport.c` (hwsle_transport.c:1-142):

| Feature | Our Implementation | OHOS DTAP |
|---------|-------------------|-----------|
| **Channel count** | Single (tcid 0x0A = SMTC) | Multi-channel (CMTC, SMTC, CUTC, dynamic) |
| **Channel lifecycle** | Static, open at init | Dynamic, created/destroyed per-connection by CM |
| **Frame format** | Raw: `[0xA3][tcid u16][len u16][payload]` | DTAP basic/enhanced frame with 4-bit type, PI, extensions, CRC |
| **Transport modes** | None (raw ACB) | 4 modes (Basic/Transparent/Stream/Reliable) |
| **Scheduling** | None (fire-and-forget) | 4-level priority queue with round-robin fairness |
| **Credit/flow control** | None | Global + per-LCID ACB buffer credit gating |
| **Fragmentation** | None (caller must fit in one write) | DLI-level fragmentation + DTAP-level SAR |
| **Sequence numbers** | None | 14-bit txSeq/reqSeq for ordering and retransmission |
| **CRC** | None (kernel may add) | CRC16 with per-channel init seed |
| **Aggregation** | Not implemented | Parsed but build returns NOT_SUPPORT |
| **Reliable ARQ** | None | Full windowed ARQ (window=200), retransmit + reorder timers |
| **Multicast/Broadcast** | None | Cast mode (unicast/multicast/broadcast) per channel |

### 6.2 What We Skip and Why

We intentionally skip DTAP framing because:

1. **WS73 kernel DLI has its own framing**: The `/dev/hwsle` driver on WS73 adds its own 5-byte header `[0xA3][handle+fields][len]` with tcid routing baked in. The OHOS DTAP frame header is an additional layer that exists because the OHOS stack uses a different DLI implementation.

2. **SSAP on fixed tcid 0x0A is sufficient**: For our TV-box use case (SSAP service management), tcid 0x0A carries all SSAP PDUs. We do not need dynamic channels, multi-protocol routing (PI), or per-connection channel management.

3. **Single LCID**: We maintain one connection at a time to the dongle. No multi-LCID scheduling is needed.

### 6.3 What We Are Missing (and Should Consider)

| Gap | Risk | Severity | Recommendation |
|-----|------|----------|----------------|
| **No credit/flow control** | Sending faster than chip can consume causes packet loss | HIGH | Implement ACB credit gating (see Section 7) |
| **No send queuing** | Burst writes may overwhelm kernel buffers | MEDIUM | Add a bounded TX queue with backpressure |
| **No fragmentation** | Large SSAP PDUs may exceed kernel buffer | MEDIUM | Ensure all SSAP PDUs fit in one ACB frame, or add fragmentation |
| **No sequence numbers** | Cannot detect out-of-order or duplicate delivery | LOW | SSAP has its own request/response semantics; ordering likely maintained by kernel |
| **No CRC at our layer** | Relies on kernel/link-layer CRC | LOW | Acceptable — kernel handles CRC |
| **No multi-channel** | Cannot separate control + data + low-latency paths | LOW (for now) | Add multi-channel if TV-box needs separate HID + SSAP paths |

---

## 7. Borrowable Design Patterns

### 7.1 ACB Credit Gating (Highest Priority)

**Source:** `dtap_scheduler.c` credit mechanism (dtap_scheduler.c:505-508, 814-826).

**Pattern:** Two-level credit gate:
1. Query chip for available ACB buffer slots: `DLI_DataNumGet(ACB_DATA_TYPE)`.
2. Before each send, check: `g_sendNotAckPktCnt < g_apBufferNum` (global) AND `node->sendNotAckPktCnt < node->quota` (per-LCID).
3. After chip acknowledges (`numCompletedPackets`), decrement counters and retry pending sends.

**Corroborating source:** `sle_measure_sdk` uses `sle_flow_ctrl_flag() > 0` before each notify (COMMUNITY-PROJECTS.md, Section B). This is the same pattern at a higher abstraction.

**Implementation sketch for our transport:**
```c
// In hwsle_transport.c:
static uint32_t g_tx_in_flight = 0;
static uint32_t g_tx_max = 1;  // start conservative, update from chip

int hwsle_transport_send_ssap(const uint8_t *pdu, size_t len) {
    if (g_tx_in_flight >= g_tx_max) return -EAGAIN;  // backpressure
    int ret = hwsle_transport_send_acb(TCID_SLE_SMTC, pdu, len);
    if (ret > 0) g_tx_in_flight++;
    return ret;
}
// On recv: when we get a "num completed" indication from DLI, decrement.
```

### 7.2 Priority Queuing (Medium Priority)

**Source:** `dtap_scheduler.c` 4-level priority queues (dtap_scheduler.c:71, 695-713).

**Pattern:** Separate queues for CMD > HIGH > NORMAL traffic. Within each queue, round-robin fairness across LCIDs. CMD queue has unlimited size; others cap at 10000.

**Applicability to us:** If we need separate control (CMTC on 0x02) and data (SMTC on 0x0A) channels, a simple 2-level priority scheme would suffice. For a single SSAP channel, this is overkill.

### 7.3 DLI Fragmentation (Medium Priority)

**Source:** `dtap_scheduler.c:510-534` + `dtap_frame_enhance.c:185-254`.

**Pattern:** When a packet exceeds `DLI_GetFragmentMaxLen()`, split it into fragments with SAR markers. Fragment channel gets highest scheduler priority (FRAGMENT=0) to ensure fragments are sent promptly.

**Applicability to us:** The WS73 kernel DLI may handle fragmentation for us (we observed `DLI_SplitData` is a kernel-side operation). We should verify whether large writes to `/dev/hwsle` are automatically fragmented or rejected. If rejected, we need to implement pre-fragmentation.

### 7.4 Reliable ARQ (Low Priority — Future)

**Source:** `dtap_channel.h:78-99` + dtap_trans.h reliable mode.

**Pattern:** Windowed ARQ with txSeq/reqSeq piggybacked ACK, retransmission timer (1s), reorder timer (10s), poll-based ACK solicitation (every window/4), NACK teardown.

**Applicability to us:** SSAP itself has request/response semantics at the application layer. True reliable transport is only needed if we want to guarantee in-order delivery of multiple outstanding requests. For a simple SSAP client, the current request-then-response model is sufficient.

### 7.5 Frame Aggregation (Low Priority — Not Yet Implemented in OHOS Either)

The aggregation build functions return `NOT_SUPPORT_ERR`. This feature is aspirational in the OHOS stack. Not applicable to us.

---

## 8. Wire Format Reconciliation

Our hardware-verified WS73 wire format for ACB data:

```
DLI header:  [0xA3][handle_lo][handle_hi_prio_ts][len_lo][len_hi]
             ^-- 1 byte      ^-- 2 bytes (handle|priority|timestamp)  ^-- 2 bytes (payload length)
DTAP frame:  [tcid][frameType|bits][length_lo][length_hi][...payload...][crc?]
```

**On OHOS:** The DLI header's "handle" field encodes `(handle << 4) | (priority << 2) | timestamp`, and the payload is a full DTAP frame.

**On WS73 (our dongle):** We verified the wire directly — the DLI header's 2 bytes at offset 1-2 are the **tcid as u16 LE** (not the OHOS handle encoding). Our `hwsle_transport_send_acb()` writes `tcid` as u16 LE, which matches the WS73 kernel driver's expectation.

This means:
- The WS73 kernel driver's DLI header format **differs from OHOS DLI**.
- Our 5-byte header `[0xA3][tcid u16 LE][len u16 LE]` is correct for WS73.
- The DTAP frame layer (tcid + frameType + length + optional PI/CRC) is an OHOS-specific addition that sits between SSAP and DLI on that stack. On WS73, SSAP PDUs ride directly as the ACB payload without DTAP framing.

---

## 9. Open Questions

1. **WS73 kernel DLI fragmentation:** Does the WS73 `/dev/hwsle` kernel driver automatically fragment large writes? Or does it reject writes exceeding the DLI fragment size? We need to test with payloads > 128B and > 256B to determine if we need application-level fragmentation.

2. **ACB credit availability on WS73:** Does the WS73 kernel driver expose an ACB buffer count equivalent to `DLI_DataNumGet(ACB_DATA_TYPE)`? If not, how do we know when to back-pressure? Options: (a) rely on `write()` returning `EAGAIN`/`EWOULDBLOCK`, (b) fixed conservative limit, (c) kernel signals via some ioctl.

3. **Multi-channel on WS73:** Can the WS73 kernel driver handle multiple tcids simultaneously? If we need tcid 0x02 (CMTC for control) + 0x0A (SMTC for SSAP) + dynamic tcids (for future data), does the kernel route them correctly, or do we need one file descriptor per tcid?

4. **OHOS DTAP aggregation:** The aggregation frame builders return `NOT_SUPPORT_ERR`. Is this a TODO or a permanent design decision? If aggregation were implemented, it could improve throughput by batching small SSAP PDUs.

5. **Reliable mode window sizing:** The default `CM_CAP_WND = 200` seems large for a USB dongle scenario. What is the actual chip-side buffer capacity? The `sle_measure_sdk` uses a simpler `sle_flow_ctrl_flag()` mechanism without explicit window management, suggesting the chip handles its own buffering.

6. **PI-based routing vs. tcid-based routing:** The OHOS stack uses PI for protocol multiplexing on dynamic channels (tcid > 0x1D). On WS73 with fixed tcid 0x0A, we don't need PI. But if a future TV-box scenario requires IPv4/IPv6 over SLE (PI=0x01/0x02), would we need to add PI support?

---

## 10. Summary

The OHOS DTAP layer is a full-featured data-plane implementation with multi-channel multiplexing, 4 transport modes, priority scheduling, and ACB credit gating. Our `hwsle_transport.c` is a minimal single-channel raw transport that bypasses all of this.

The gap is acceptable for our current SSAP-over-WS73 use case. The most critical borrowable element is **ACB credit gating** (Section 7.1) — without it, we risk overflowing the chip's receive buffers. The multi-channel and reliable-ARQ features are architecturally interesting but not needed until we expand beyond single-connection SSAP.

The WS73 kernel DLI header format differs from the OHOS DLI header format (our tcid u16 LE vs. their handle+priority+timestamp encoding), confirming that the DTAP frame layer is an OHOS-specific construct that does not exist on the WS73 wire. Our `hwsle_transport.c` correctly implements the WS73 wire format.
