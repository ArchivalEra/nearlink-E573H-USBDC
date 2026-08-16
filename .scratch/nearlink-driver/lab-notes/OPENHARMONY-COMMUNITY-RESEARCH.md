# OpenHarmony NearLink/星闪 Community Intelligence Dossier

Compiled 2026-08-16 from live source mining (gitcode + GitHub mirrors + Gitee API + CSDN/Bing). All URLs verified reachable at compile time.

## (a) Upstream repos — what each provides

### OpenHarmony NearLink service (the big one)
- **Gitcode (primary, accepts PRs):** https://gitcode.com/openharmony/communication_nearlink_service — Apache-2.0, `master`, 278 commits, 2 branches, 43 stars / 75 forks. **Very active**: PR #159 merged within the last 2 days (RemoteObserverList fix), #125 (auto-stop adv event), #131 (build deps), #114/#137 (单音算法适配, off_all log). Renamed "开源星闪孵化" (NearLink open-source incubation).
- **GitHub mirror (clone this):** https://github.com/openharmony/communication_nearlink_service — full recursive tree available (2883 paths).
- **SIG redirect:** https://gitcode.com/openharmony-sig/communication_nearlink_service → 301 redirects to the main-org repo.

**What it provides (layered architecture: application → framework → system service → driver):**
- `frameworks/` — JS NAPI (`frameworks/js/napi/src/ssap/napi_nearlink_ssap*.cpp`), native (`frameworks/native/nearlink_ssap_{client,server,service,property,method,event,descriptor}.cpp`), IPC (`frameworks/ipc/nearlink_ssap_*proxy/stub`).
- `services/service/src/` — profile services: `ssap/` (client+server services, stack adapters), `advertiser/`, `scan/`, `controller/` (SleControllerService), `asc/` (audio stream), `bas/`, `ccp/`, `cdsm/`, `cloudpair/`, `datatransfer/`, `dis/`, `hadm/`, `hid/`, `icce/`, `lis/`, `mcp/`, `mic/`, `port/`, `tws/`, `vas/`, `vcp/`, `audio/`, `datashare/`, `dialog/`.
- **Full protocol stack (C, in `services/stack/src/`):**
  - `dli/` — DLI layer: `dli_opcode.h` (complete command/event/sub-event opcode tables), `dli_cmd.c`, `dli_event.c`, `dli.c`, `interface/` (dli.h, dli_cmd.h, dli_cmd_struct.h, dli_layer.h, dli_event_struct.h, dli_def.h, dli_errno.h, dli_opcode.h), `layer/` (dli_layer.c + callbacks), `sapi/` (dli_sapi.c, dli_data_stub.c), `thread/`, `cmd/src/dli_cmd.c`, `event/src/dli_{connect,dev_discovery,factory,hadm,nbc,secu}_event.c`, `libnearlink_stack_base.versionscript`.
  - `cp/bsl/sle/` — SLE base stack: `cm/` (connection manager: `link/` sle_access_dli.c, sle_logic_link_mgr.c, cm_trans_channel_mgr.c, cm_event_core.c, cm_api.c; `signaling/` cm_signaling_*.c; `dyntrans/` cm_dyn_tcid.c, cm_dyn_trans_channel_mgr.c — **dynamic TCID allocator**; `icb/` cm_icb_mgr.c; `interface/cm_api.h`, `cm_def.h`), `devd/` (device discovery), `servm/ssap/` (full SSAP stack: ssap_manager.c, ssap_link.c, ssap_handle.c, ssapc_app.c, ssaps_server.c, ssapc_client.c, ssap_link_state.c, nlstk_ssap_app_*.c, `include/inner/ssap_pkt.h`), `hadm/`, `bal/profile/{bas,hid}/`, `bal/audioctl/actm/`.
  - `dp/transport/` — transport.c, transport_cltp.c, transport_proto.h (CLTP transport for large data).
  - `adapter/` — dli_ext_func_wrapper.c, dli_reg_ext_func.c.
- **`services/hardware/` — the DLI↔HDI HAL bridge:** SleDliLayerAdapter.h/.cpp (SleHalInit / SleSendDliPacket / SleReset / GetDliVersion, uses `OHOS::HDI::Nearlink::Hci::V1_0/V1_1::ISleHciInterface`), SleDliCallbacks.cpp, SleDliSnoop.cpp (btsnoop-style capture → /data/log/nearlink/, type byte 0xA1 CMD / 0xA2 EVENT / 0xA3 ACB / 0xA4 ICB), SleDliThreadUtil.cpp.
- `sa_profile/`, `socket/`, `ipc_parcel/`, `utils/`, `tools/`, `test/fuzztest/` (55+ fuzzers incl. ssapclient/server, slecontroller, datatransfer).
- Docs: README.md + README_zh.md; config flags `nearlink_config.gni`, `const.nearlink.enable=1`.

### HDI interface repo (kernel-driver API contract)
- **Gitcode:** https://gitcode.com/openharmony/drivers_interface → subdir `nearlink/`
- **GitHub mirror:** https://github.com/openharmony/drivers_peripheral + drivers_interface tree; HDI IDL at `nearlink/hci/v1_0/` and `nearlink/hci/v1_1/`:
  - `ISleHciInterface.idl` (v1_0, since OH 4.1): `SleHalInit([in] ISleHciCallback)`, `SleSendHciPacket([in] unsigned char[] data)`, `Close()`.
  - `ISleHciCallback.idl`: `initializationComplete([in] enum SleStatus)`, `hciPacketReceived([in] unsigned int type, [in] unsigned char[] data)`.
  - `SleHciTypes.idl`: `enum SleStatus {SUCCESS=0, TRANSPORT_ERROR, UNKNOWN, INITIALIZATION_ERROR}`, `enum SleType {HCI_CMD=0xA1, HCI_EVENT=0xA2, ACB_DATA=0xA3, ICB_DATA=0xA4}`.
  - v1_1 adds `CheckOnBoardState([out] boolean)`.
  - Also `nearlink/off_find/v1_0/` — OffFind (离线查找) HDI.

### HDF driver implementation repo
- **Gitcode:** https://gitcode.com/openharmony/drivers_peripheral → subdir `nearlink/` (README_zh.md "星闪驱动组件", C++, "基于DLI框架，仅支持标准系统").
- **GitHub mirror paths:**
  - DLI service: `nearlink/dli/dli_service/` → `sle_hci_interface_impl.cpp` (implements ISleHciInterface), `sle_dli_interface_driver.cpp` (HDF driver), `implement/`: `vendor_interface.cpp` (dlopen vendor lib), `h4_protocol.cpp` (H4 framing over a single fd), `dli_watcher.cpp`, `dli_protocol.h`, `ohos_sle_vendor_lib.h`, `sle_address.cpp` (MAC from /data/vendor/nearlink/slemac.txt), `sle_hal_constant.h`, `thread_util.cpp`.
  - OffFind service: `nearlink/off_find/hdi_service/implement/` — mirror structure with `liboff_find_vendor`.

### SDK / board repos
- **HiHope WS63 (fbb_ws63):** https://gitee.com/HiSpark/fbb_ws63 — WS63/WS63E solution code repo, LiteOS. Contains `src/drivers`, `src/protocol/{bt,radar,wifi}`, `src/interim_binary/ws63` (closed binaries), `src/kernel`, `src/middleware`. Mirror: https://github.com/Hny0305Lin/Hihope_WS63_NearLink_SDK.
- **HopeRunORG/NearLink:** https://github.com/HopeRunORG/NearLink (HopeRun/HiHope OpenHarmony NearLink project, last push 2025-04).
- **LZ3863 (小智派):** https://gitee.com/Lockzhiner-Electronics/lz3863 (apps/e1_sle_connect_server — SSAP server example).
- **NearLink SDR:** https://github.com/OpenSparklink/nearlink_sdr_sim; NearLink-ePaper mesh: https://github.com/NearLink-ePaper/NearLink-Mesh-ePaper (H3863 SLE mesh w/ AODV, AIMD).

## (b) HDI / kernel-driver contract details

- **Host service → HDF path:** nearlink_service's SleDliLayerAdapter.cpp does ISleHciInterface::Get() (V1_0), registers callbacks, sends via SleSendHciPacket(data); V1_1 detected via CastFrom → GetDliVersion() = DLI_VERSION_1_1. On driver death the host SIGKILLs itself.
- **Packet-type byte** is data[0] = 0xA1 CMD / 0xA2 EVENT / 0xA3 ACB / 0xA4 ICB (matches our reverse-engineered HCI and SleHciTypes.idl).
- **HDF driver transport:** VendorInterface dlopens libnearlink_sle_vendor (symbol NEARLINK_VENDOR_LIB_INTERFACE), calls op SLE_OP_DLI_CHANNEL_OPEN to get fds (int channel[DLI_MAX_CHANNEL] = {CMD, EVT, ACB_OUT, ACB_IN}), then SLE_OP_POWER_ON, SLE_OP_INIT (async initCb), LPM ops. On most devices channelCount==1 → H4Protocol. **The transport is a plain fd (UART or vendor-provided); there is NO USB VID/PID handling anywhere in the OpenHarmony HDF code** — the gap our WS73 USB driver fills.
- **H4 framing (h4_protocol.cpp):** reads type byte (masked &0x0F for header table index), then PacketHeader {headerSize, dataLengthOffset, dataLengthSize} per type from header_[DLI_PACKET_TYPE_MAX]. Length little-endian. No per-packet checksum visible. RT scheduling: SCHED_FIFO prio 1 + SetVipPrio(10) via libnearlink_mac.z.so dlopen + PERF_CTRL_SET_VIP_PRIO ioctl (magic 'x').
- **Command framing (vendor_interface.cpp):** DLI_SET_SLE_ADDR = 0x0405; DLI command = [type 0xA1][opcode LE u16][length u16 LE] then payload (6-byte MAC reversed). **Preamble is 4 bytes (opcode u16 + len u16)** — matches our finding.
- **Address management:** MAC persisted at /data/vendor/nearlink/slemac.txt; const.nearlink.slechiptype selects vendor lib; const.nearlink.enable=1 gates the service.
- **No kernel_linux nearlink driver in upstream** (0 hits). drivers_peripheral HDF talks to vendor lib, not kernel. `/dev/hwsle` is HiSilicon/WS73 SDK-specific, NOT an OpenHarmony upstream artifact.

## (c) Protocol-stack knowledge gaps now fillable

1. **DLI opcode tables — complete** (dli_opcode.h):
   - Sub-events: DLI_SUBEVENT_RSSI=0x01, ACB_SUBRATE_CHANGE=0x02, ACB_SUBRATE_REQ=0x03, POWER_LEVEL=0x13.
   - Commands: controller 0x0401-0x040D; adv 0x0C02-0x0C08; scan 0x1001-0x1003; link 0x1401-0x1403; 0x1801-0x1812; security 0x1C01-0x1C07, 0x1C28; measurement 0x2001-0x2005; sync links 0x2801-0x280E; test 0xFFFF.
   - Events: 0x0001 CMD_STATUS, 0x0002 CMD_COMPLETE, 0x0004 ADV_TERMINATED, 0x0005 DISCONN_COMPLETE, 0x0011 ENC_CHANGE, 0x0013 ACB_LOW_LATENCY_EN, 0x0014 CONTROLLER_DATA, 0x0015 CONN_COMPLETE, 0x0028 MEASURE_IQ_REPORT, 0x0029 MEASURE_STATE_CHANGE, 0x0038-0x0074 IOB/IOG/IMB/IMG, 0xFC06 CHIP_RESET_NOTIFY, 0xFC07 VENDOR_EVENT, 0x00EE CMD_ERROR.
2. **SSAP PDU layer — fully specified** (ssap_pkt.h):
   - Msg codes: 0x01 ERROR_RSP, 0x02/03 EXCHANGE_INFO REQ/RSP, 0x04/05 FIND_STRUCTURE, 0x06/07 FIND_BY_UUID, 0x08/09 READ, 0x0A/0B READ_BY_UUID, 0x0C WRITE_CMD, 0x0D/0E WRITE REQ/RSP, 0x0F VALUE_NTF, 0x10 VALUE_IND, 0x11 VALUE_ACK, 0x12/13/14 CALL_METHOD.
   - PDU = msgCode(u8) + msgCtrl(u8) + …; ctrl bitfields (fragment 2b, multi, error, oper 2b). EXCHANGE_INFO carries MTU u16 + version u16; versions 1.0-1.3 (SSAP_VERSION_1_3=0x0301); default MTU 251, max 1024.
   - Item handles u16, service-change event handle 0x000E, hash handle 0x000F. Item types: std/vendor × {primary/secondary service, property, method, event, service-reference, descriptor}. Operation bitmap: read=1, writeNoRsp=2, writeRsp=4, notify=8, indicate=0x10, broadcast=0x20.
   - SSAP_TRANS types: CMD 0x01, REQ 0x82, RSP 0x03, NOTI 0x04, IND 0x85, ACK 0x06 (REPLY_MASK 0x80).
3. **DLI adv/scan/conn parameter structures** (dli_def.h): DLI_AdvParam full layout (3B LE interval 125µs units, channel map 76/77/78, GT role, frame format 1 vs 4 (m-seq), second PHY 1/2/4M, pilot, MCS, SID); scan params type(1)+interval(2)+window(2) per PHY; defaults: conn interval 0x64 (1.25ms units), supervision 0x1FC, scan interval/window 0x20.
4. **TCID / channel scheme** (cm_dyn_tcid.h): dynamic TCID allocator per LCID, transport modes, ICB manager, signaling channels — the ACB/ICB data-path + signaling separation our project needs.
5. **Ranging**: DLI measure 0x2001-0x2005 + IQ reports + DLI_MEASURE_CONFIG_DIR; BS21E TOF+phase-difference to sub-meter; 星闪2.0 centimetric phase ranging.
6. **Low-latency ACB/IOB/IMB**: full sync-link opcode surface (0x2801-0x280E) + events.

## (d) Community news / timeline

- **2020-09:** 星闪联盟 (SparkLink Alliance, now International SparkLink Alliance) founded by ~80 units led by Huawei. https://www.isla.org.cn/. Standards: SLB (SparkLink Basic, high-speed), SLE (Low Energy), SLP. SLE in 2400-2483.5 MHz (same band as BLE).
- **NearLink 1.0/2.0:** 1.0 specs public (SLB/SLE); 2.0 standardization in progress (G/T-node non-symmetric, T-node low-power broadcast, centimetric phase ranging).
- **2023:** Mate 60 launch popularized 星闪.
- **OpenHarmony integration:** NearLink service upstream (OH 4.1-era IDL, now master with daily merges, 278 commits). Active hardening for OH 6.x/7.x.
- **HiSilicon open-source:** WS63/WS63E SDK open on Gitee (fbb_ws63); HiSilicon dev forum https://developers.hisilicon.com/.
- **WS73 Linux bring-up (directly relevant):**
  - USB transport on i.MX6ULL/Linux 5.10: WSCFG_BUS_USB=y, plat_soc.ko → ble_soc/wifi_soc, firmware to /etc/tr5330/, BLE via BlueZ 5.64, no SLE test shown (https://blog.csdn.net/eayayaya/article/details/156680047; Ebyte https://www.ebyte.com/product/2451.html; Ai-Thinker WS1 https://docs.ai-thinker.com/ai-ws1/).
  - SDIO on Hi3516CV610: plat_soc/wifi_soc/ble_soc/**sle_soc.ko** (SLE untested), official docs 《WS73V100 Linux平台驱动移植 用户指南_01》etc (https://blog.csdn.net/iikat/article/details/147385864).
  - ws73v100-wifi repo: https://github.com/gtxaspec/ws73v100-wifi.
- **Community products:** nearlink-firmwares toolbox (https://github.com/MiraHikari/nearlink-firmwares), H3863 SLE mesh (https://github.com/NearLink-ePaper/NearLink-Mesh-ePaper), HopeRun NearLink (https://github.com/HopeRunORG/NearLink), HiHope WS63 SDK mirror (https://github.com/Hny0305Lin/Hihope_WS63_NearLink_SDK).

## (e) Recommended next research directions

1. **Diff our reverse-engineered DLI HCI against dli_opcode.h + dli_def.h byte-for-byte** — our WS73 HCI is almost certainly the same DLI command set.
2. **Mine dli_cmd_struct.h + dli_layer.h + h4_protocol.cpp header_[] table** — exact DLI packet framing + command-complete dispatch contract.
3. **Study ssap_pkt.h + ssap_manager.c + ssaps_server.c/ssapc_client.c + ssap_link_state.c** — lock down SSAP connection handshake (EXCHANGE_INFO → FIND_STRUCTURE → READ → WRITE/VALUE_NTF), then implement SSAP server-side parser.
4. **Implement/validate the DLI→HDF shim as a userspace model** — replicate SleDliLayerAdapter semantics bound to our USB transport.
5. **Track upstream** for kernel/transport story + DLI 1.1 features.
6. **Reach out to CSDN WS73 porting authors** (eayayaya USB, iikat SDIO) for sle_soc.ko internals; obtain WS73V100 Linux driver 用户指南 PDFs.
7. **Query 星闪联盟 (isla.org.cn)** for SLE 1.0/2.0 spec public summaries.
8. **Harvest dp/transport (CLTP) + cm/dyntrans** for the dongle's data-path abstraction.
