---
type: intel
title: OpenHarmony NearLink HDI/HDF Driver Layer — Interface Shape and Relevance to Our WS73 Linux Driver
language: en
created: 2026-08-17
tags: []
---

# OpenHarmony NearLink HDI/HDF Driver Layer — Interface Shape and Relevance to Our WS73 Linux Driver

Date: 2026-08-17
Author: research subagent (context: in-house `ws73usb` Linux kernel module + userspace stack for TV box + WS73 USB dongle)

## Sources

Primary (read-only, local):
- `/mnt/hdd/nearlink-stuff/communication_nearlink_service/` — OpenHarmony `communication_nearlink_service` (github.com/openharmony mirror, master)
  - `bundle.json` (component deps)
  - `sa_profile/1190.json` (system ability profile)
  - `services/etc/init/nearlink_service.cfg` (SA service config)
  - `services/hardware/include/SleDliLayerAdapter.h`, `services/hardware/include/SleDliCallbacks.h`
  - `services/hardware/src/SleDliLayerAdapter.cpp`, `services/hardware/src/SleDliCallbacks.cpp`, `services/hardware/src/SleDliSnoop.cpp`
  - `services/hardware/BUILD.gn`
  - `services/stack/src/dli/sapi/src/dli_sapi.c`, `.../sapi/include/dli_sapi.h`, `.../sapi/src/dli_data_stub.c`
  - `services/stack/src/dli/layer/src/dli_layer.c`
  - `services/stack/src/dli/BUILD.gn`, `services/stack/BUILD.gn`
  - `services/stack/src/dli/interface/dli_opcode.h`
  - `nearlink_config.gni`, `interfaces/inner_api/include/nearlink_sle_ranging.h`
- Local siblings scanned for HDI/HDF/IDL: `fbb_bs2x`, `fbb_ws63`, `HopeRun-NearLink`, `NearLink_controller`, `NearLinkSLE`, `sparklink`, `ws73v100-wifi`, `OpenSparklink-linux`, `sle_measure_sdk` — **none contain HDI/HDF content** (only unrelated false positives like `acerhdf.c`, `hdio.rst`).
- Our notes: `.scratch/nearlink-driver/lab-notes/OPENHARMONY-COMMUNITY-RESEARCH.md`, `.scratch/nearlink-driver/issues/06-kernel-driver-skeleton.md`, `.scratch/nearlink-driver/lab-notes/OSPL-USB-TRANSPORT.md`.

Networking/build/hardware: none used. All conclusions are from local source text.

## HDI existence and location

**The `communication_nearlink_service` repo does NOT contain HDI IDL definitions or HDF driver code.** It is the *consumer* of the HDI contract; the driver-facing layer it ships is a thin C/C++ HAL bridge called `nearlink_dli_adapter` under `services/hardware/`.

Evidence:
- No `.idl`/`.hdi` files anywhere in the tree (find returned zero).
- No `.hcs` device-config files, no `HdfDriverEntry`/`DeviceResourceIface` (grep returned zero) — the HDF driver is not in this repo.
- The repo links against the HDI interface and HDF libs as external deps:
  - `bundle.json:63-65` — component deps include `drivers_interface_nearlink` and `hdf_core`.
  - `services/hardware/BUILD.gn:79-90` — `external_deps = [ "drivers_interface_nearlink:libnearlink_hci_proxy_1.0", "drivers_interface_nearlink:libnearlink_hci_proxy_1.1", "hdf_core:libhdf_utils", ... ]`.
- The C++ includes the generated HDI proxies directly:
  - `SleDliCallbacks.h:22-23` — `#include <v1_0/sle_hci_types.h>` and `#include <v1_0/isle_hci_callback.h>`; class `SleDliCallbacks : public OHOS::HDI::Nearlink::Hci::V1_0::ISleHciCallback` (line 30).
  - `SleDliLayerAdapter.cpp:20-21` — `#include <v1_0/isle_hci_interface.h>` and `#include <v1_1/isle_hci_interface.h>`.
- SA profile pins the HDI proxy version: `sa_profile/1190.json:8-10` — `"min_hdi_proxy_version": ["libnearlink_hci_proxy_1.0.z.so"]`.

Where the HDI actually lives (not local; per OPENHARMONY-COMMUNITY-RESEARCH.md:24-31):
- **IDL interface definitions**: OpenHarmony repo `drivers_interface` (gitcode.com/openharmony/drivers_interface), subdir `nearlink/hci/v1_0/` and `nearlink/hci/v1_1/` — `ISleHciInterface.idl`, `ISleHciCallback.idl`, `SleHciTypes.idl`; plus `nearlink/off_find/v1_0/` for OffFind.
- **HDF driver implementation**: OpenHarmony repo `drivers_peripheral`, subdir `nearlink/` — `dli/dli_service/` has `sle_hci_interface_impl.cpp` (implements `ISleHciInterface`), `sle_dli_interface_driver.cpp` (HDF driver entry), `implement/vendor_interface.cpp` (dlopens the vendor lib), `implement/h4_protocol.cpp` (H4 framing over a plain fd).

No other local repo under `/mnt/hdd/nearlink-stuff/` contains a `drivers_interface`/`drivers_peripheral` checkout, so the IDL/impl were not inspected locally; interface shape below is reconstructed from the consumer side in this repo, which is exact and sufficient.

## HDI interface shape (as consumed here)

### The HDI contract (OHOS::HDI::Nearlink::Hci::V1_0/V1_1::ISleHciInterface)
From `SleDliLayerAdapter.cpp`:
- Acquisition/versioning: `ISleHciInterface::Get()` returns the V1_0 proxy (`SleDliLayerAdapter.cpp:94`); V1_1 is probed by `OHOS::HDI::Nearlink::Hci::V1_1::ISleHciInterface::CastFrom(g_iSleDli)` (`SleDliLayerAdapter.cpp:171`); non-null cast ⇒ `DLI_VERSION_1_1`, else `DLI_VERSION_1_0` (`GetDliVersion`, lines 165-178). This is the "feature-detect by interface version" HDI pattern.
- Methods actually called:
  - `SleHalInit(sptr<ISleHciCallback>)` — synchronous call, async completion via callback (`SleDliLayerAdapter.cpp:106`).
  - `SleSendHciPacket(std::vector<uint8_t>)` — one raw packet down (`SleDliLayerAdapter.cpp:142`).
  - `Close()` — teardown (`SleDliLayerAdapter.cpp:160`).
  - V1_1 adds `CheckOnBoardState([out] boolean)` (per prior research; not called in this tree's adapter).
- Callback interface (`ISleHciCallback`, from `SleDliCallbacks.h:35-37` + impl):
  - `initializationComplete(SleStatus)` — `SleDliCallbacks.cpp:25-36`, maps HDI `SleStatus` to local `SleInitStatus` (SUCCESS/TRANSPORT_ERROR/INITIALIZATION_ERROR/UNKNOWN).
  - `hciPacketReceived(uint32_t type, std::vector<uint8_t> data)` — `SleDliCallbacks.cpp:53-67`; `type` is the packet-type byte, passes data straight up wrapped as `SlePacket{data,size}`.
- Death handling: `SleDliDeathRecipient::OnRemoteDied` → `kill(getpid(), SIGKILL)` (`SleDliLayerAdapter.cpp:40-46`); registered via `AddDeathRecipient` on the HDI remote object (lines 58-71). Driver death == host self-termination (supervised by init auto-restart).

### The local HAL adapter API (this repo's seam, `SleDliLayerAdapter.h`)
This is the layer we should mirror:
- `typedef struct SleDliCallbackFunc { void (*initializationComplete)(SleInitStatus); void (*dliPacketReceived)(SlePacketType, const SlePacket*); }` (`SleDliLayerAdapter.h:61-64`).
- API: `SleHalInit(callbacks)`, `SleReset()`, `SleSendDliPacket(const SlePacket*)`, `SleHalClose()`, `GetDliVersion()` (`SleDliLayerAdapter.h:66-70`).
- Packet types (`SleDliLayerAdapter.h:36-47`): `PACKET_TYPE_CMD=1, ACB=2, SCO=3, EVENT=4, ISO=5` plus the wire bytes `SLE_CMD=0xA1, SLE_EVENT=0xA2, SLE_ACB=0xA3, SLE_ICB=0xA4`.
- `SleReset()` is implemented as `kill(getpid(), SIGKILL)` (`SleDliLayerAdapter.cpp:123-126`) — i.e. "reset = force reload of the whole service", relying on init to restart it.

### Callback thread policy
`SleDliCallbacks::SetRTSchedule()` promotes the HDI callback thread to `SCHED_FIFO` priority 1 on first callback (`SleDliCallbacks.cpp:69-78`, priority at line 22). Callbacks are dispatched inline from the HDI/HDF callback context.

### Ranging / measurement — NOT in the HDI
The HDI surface is a pure packet conduit (init + raw DLI packets). Ranging rides the same 0xA1/0xA2 pipe via DLI opcodes: `DLI_CBK_*_MEASURE_*` (`dli_opcode.h:91-96`), events `DLI_MEASURE_IQ_REPORT_EVT=0x0028` / `DLI_MEASURE_STATE_CHANGE_EVT=0x0029` (`dli_opcode.h:205-207`), commands 0x2001-0x2005 (prior research). The ranging *algorithm* runs in userspace (`frameworks/ranging_alogorithm_adapter/src/ranging_alogorithm_adapter.cpp`). So: HDI = transport only; DLI = protocol; ranging math = userspace. Nothing chip-specific leaks into the HDI.

### The host-side stack consumption (DLI layer)
- `DLI_SapiInit(cb)` blocks up to `DLI_INIT_TIMEOUT 3000` ms on a semaphore posted by `initializationComplete` (`dli_sapi.c:30,63-68`); on timeout it calls `SleReset()` and deinits (`dli_sapi.c:68-73`).
- RX dispatch is by packet type: ACB→main thread data path, EVENT→DLI event thread, ICB→unsupported log (`dli_layer.c:637-652`).
- **Chip reset detection in-band**: event `ff ff 01 00 c7` (5 bytes) ⇒ `kill(getpid(), SIGKILL)` (`dli_layer.c:626-632`). The driver signals reset as an ordinary event; the stack decides to die.
- Local-test stub (`dli_data_stub.c`, compiled under `-DNEARLINK_SERVICE_STACK_LOCAL_TEST` for UT/fuzz, `dli_sapi.h:22-26`, `dli_data_stub.c:32`) provides an in-process fake HAL (`SleHalInit` posts SUCCESS immediately, `SleSendToDliStub` injects synthetic ACB/event packets, `dli_data_stub.c:99-143, 180-554`) so the whole DLI+stack can be unit-tested with no hardware. This is a deliberate seam for host-side testing.

## HDF framework mechanism (what the driver side does; from prior research — repo absent locally)

From OPENHARMONY-COMMUNITY-RESEARCH.md:33-37, 49-53:
- HDF driver (`sle_dli_interface_driver.cpp`) implements the HDI service; host resolves it via `ISleHciInterface::Get()` (IPC over HDF).
- The driver is **userspace-hosted** in OHOS (HDF host process); it `dlopen`s a vendor lib (`libnearlink_sle_vendor`, symbol `NEARLINK_VENDOR_LIB_INTERFACE`) and calls `SLE_OP_DLI_CHANNEL_OPEN` to obtain `int channel[DLI_MAX_CHANNEL]` fds, then `SLE_OP_POWER_ON` / `SLE_OP_INIT` (async initCb), LPM ops.
- Transport on most devices is a **plain fd (UART or vendor-supplied)** framed by H4 (`h4_protocol.cpp`). **No USB VID/PID handling exists anywhere in the OHOS HDF code** — the WS73 USB transport is our gap.
- MAC persisted at `/data/vendor/nearlink/slemac.txt`; `const.nearlink.slechiptype` selects the vendor lib; `const.nearlink.enable=1` gates the SA (`sa_profile/1190.json:31-47`, `nearlink_service.cfg`, `nearlink.para`).
- OHOS HDF "device matching" here is minimal: no device tree; driver instance is registered as a platform/HDF service and its match is effectively "the vendor lib is present". This is a service-registration model, not the Linux `usb_driver`/`struct usb_device_id` bind model.

## Borrowing list for the TV box (plain Linux, no HDF)

What to **copy** (interface division, not framework):

1. **HDI = transport-only; DLI/protocol = stack; ranging math = userspace.** This three-way split is the strongest takeaway. Our `ws73usb` kernel module should expose exactly the `SleDliLayerAdapter`-shaped surface (init+async callback, packet send, version query) over `/dev/ws73hci`; all SLE semantics (adv/scan/conn/measure opcodes, ACB/ICB) and ranging algorithms live in userspace. Matches our issue-06 char-device decision (open ⇒ SLE enable message, type-byte-tagged byte stream) — OHOS confirms the seam.
2. **Interface shape to mirror in `/dev/ws73hci`** (from `SleDliLayerAdapter.h:61-74`):
   - init: open + ioctl START with a poll/epoll-able fd instead of a callback pointer (kernel can't take userspace callbacks) → `poll()` replaces `initializationComplete`+`dliPacketReceived`.
   - packet stream: read/write of `[type byte][payload]` frames, type byte = 0xA1 CMD / 0xA2 EVENT / 0xA3 ACB / 0xA4 ICB (matches our HCI framing from issue 02/06 and the OSPL cross-check).
   - version: an `ioctl GET_DLI_VERSION` mirroring `GetDliVersion()` (`SleDliLayerAdapter.cpp:165-178`) so the stack can feature-detect (V1_0 vs V1_1), same intent as HDI `CastFrom`.
   - reset: `ioctl RESET` should ask the kernel module to re-init the chip rather than SIGKILL the process; the OHOS SIGKILL pattern is a luxury of a supervised userspace SA — in our architecture the char device must own reset and the chip-reset event must be surfaced in-band (see 3).
3. **In-band chip-reset event** (`dli_layer.c:626-632`): OHOS detects the chip reset as a magic DLI event `ff ff 01 00 c7` inside the normal event stream and lets the stack react (kill + init-restart). We should do the same: ws73usb surfaces reset as an event/frame on `/dev/ws73hci` (and/or a notification), and our userspace stack decides reload. This is better than a hard-ioctl-only channel and matches how the WS73 `H2D_MSG`/D2H INT notification already works.
4. **RT callback promotion** (`SleDliCallbacks.cpp:69-78`): OHOS boosts the receive callback thread to SCHED_FIFO prio 1. For TV box audio/low-latency ACB paths, our userspace RX thread (reading `/dev/ws73hci`) should similarly be SCHED_FIFO; note OHOS needs `CAP_SYS_NICE` for it (`nearlink_service.cfg:22-24`). Cheap and directly portable.
5. **Synchronous blocking init with timeout** (`dli_sapi.c:63-74`): init = blocking call, ≤3 s, semaphore on completion callback. Port as: ioctl START + poll on the event fd with a 3 s timeout + fallback teardown. Good discipline for our firmware-download → SLE-OPEN boot sequence.
6. **Host-side fake-HAL for testing** (`dli_data_stub.c`, `-DNEARLINK_SERVICE_STACK_LOCAL_TEST`): OHOS builds a userspace loopback stub of the driver seam so DLI/stack is testable with no silicon. We should ship a `ws73hci` userspace mock (loopback char device) for the same reason — unit-test the SSAP/DLI stack on x86 before the dongle is in.
7. **Versioned HDI proxy + min-version pin** (`sa_profile/1190.json:8-10`): keep a `GET_DLI_VERSION`/feature-bits ioctl and refuse to start if the module firmware/driver is older than the stack expects — mirrors OHOS's min_hdi_proxy_version gate.
8. **Death supervision**: OHOS relies on process death + auto-restart (`SleDliDeathRecipient`, `nearlink_service.cfg:18-25` "ondemand", "auto-restart"). For a TV box systemd setup: run our stack as a `Restart=always` unit and treat driver-module unload/`-EPIPE` on `/dev/ws73hci` as a restart trigger.

What **not** to copy:
- HDF framework itself (host service registration, HDI IPC, death recipients, `.hcs` config) — none of it applies to stock Linux USB; plain `usb_driver` + misc char device is strictly simpler.
- The vendor-lib `dlopen` split (`SLE_OP_DLI_CHANNEL_OPEN` over a raw fd, `vendor_interface.cpp`): OHOS keeps the transport in a closed vendor userspace lib. We keep the transport in the kernel module (correct for USB) — no equivalence needed.
- SIGKILL-as-reset (`SleReset`, `SleDliLayerAdapter.cpp:123-126`): acceptable for an OHOS SA, wrong for us (would kill the whole TV box app stack; the module should own reset instead).

## Conclusion — reference value grading for our self-developed Linux driver

- **HIGH (adopt as contract):** HDI's interface *shape* — transport-only, `[type byte][payload]` packet stream, async init + callback, version feature-detection, in-band chip-reset event, 3 s blocking init, RT RX thread, fake-HAL test seam. This is effectively the spec for `/dev/ws73hci` and matches what issue 02/06 already concluded; treat `SleDliLayerAdapter.h:29-74` as a cross-check of our char-device ABI.
- **MEDIUM (informative):** HDF's driver-side responsibilities (MAC persistence, chip-type selection, power/init sequencing, LPM) — the *duties* are ours too, but under plain Linux they map to module params + sysfs + udev, not HDF services.
- **LOW (not applicable):** HDF framework machinery itself (HDI IPC/proxy generation, host service model, device-tree-less matching) and the vendor-lib fd transport. Our `usb_driver` + misc char device + userspace stack is the right architecture for a stock-Linux TV box.
- **Bottom line:** HDI adds little to *what* goes in kernel vs userspace (we had already decided: kernel = transport + boot/firmware, userspace = stack/DLI/SSAP/ranging). Its real value is the *exact ABI shape* of the seam and a few operational patterns (reset event, RT thread, init timeout, fake-HAL, version pin) we can adopt as-is.

## Open questions

1. Does the HDI IDL (drivers_interface/nearlink) expose any method beyond `SleHalInit`/`SleSendHciPacket`/`Close`/`CheckOnBoardState` in current master (e.g. LPM or vendor-cmd passthrough)? Not verifiable locally — worth a targeted pull of `drivers_interface` and `drivers_peripheral` into `/mnt/hdd/nearlink-stuff/` for byte-level cross-check against our WS73 HCI.
2. The magic reset event `ff ff 01 00 c7` (`dli_layer.c:626`) — is it WS73 firmware specific or a general SLE DLI convention? If the WS73 emits it on chip crash, our module should watch for it before the stack does.
3. OHOS's `SleSnoop` btsnoop format (`SleDliSnoop.cpp`, types 0xA1-0xA4) is the reference trace format — should our stack emit a compatible pcap/snoop to reuse OHOS tooling? (Low priority; only if we ever interoperate with OHOS devices' logs.)
4. The DLI stack treats `PACKET_TYPE_SCO`/`PACKET_TYPE_ISO` (values 3/5) as distinct legacy types (`SleDliLayerAdapter.h:36-47`) but they never appear on the wire (wire bytes are 0xA1-0xA4). Confirm WS73 never emits 0xA3-lane SCO/ISO-style frames in our 5-EP mode, so we can drop them from `/dev/ws73hci`'s type table.
