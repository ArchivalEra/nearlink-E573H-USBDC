---
type: intel
title: OHOS NearLink Framework Layer — Architecture, API Surface, and Borrowing Guide for the TV-box Stack
language: en
created: 2026-08-17
tags: []
---

# OHOS NearLink Framework Layer — Architecture, API Surface, and Borrowing Guide for the TV-box Stack

Date: 2026-08-17

## Sources

Primary (OHOS `communication_nearlink_service`), all under
`/mnt/hdd/nearlink-stuff/communication_nearlink_service/`:

- `README.md` (866 lines) — architecture + native/app usage guide.
- `interfaces/inner_api/include/*.h` — the C++ "Inner API" headers (the contract an app-facing native consumer sees).
- `frameworks/native/*.cpp/.h` — native framework impl (SystemAbility (SA) client side, pimpl pattern).
- `frameworks/ipc/` — binder `Proxy`/`CallbackStub` pairs; `frameworks/ipc/include/*_proxy.h`, `frameworks/ipc/src/*_proxy.cpp`, `frameworks/ipc/src/*_callback_stub.cpp`.
- `frameworks/js/napi/src/` — ArkTS/JS NAPI modules (`manager`, `scan`, `advertising`, `remoteDevice`, `ssap`, `datatransfer`, `cdsm`, `constant`, `common/`).
- `frameworks/ets/taihe/*/idl/ohos.nearlink.*.taihe` — Taihe IDL surface for the ArkTS-facing modules.
- `frameworks/ranging_alogorithm_adapter/` — HADM ranging algorithm `dlopen` plugin.
- `services/server/` — server-side IPC dispatch; `services/service/` — profile services; `services/stack/` — protocol stack (cp/dp/dli/nai/sdf); `services/hardware/` — DLI adapter.
- `sa_profile/1190.json` — SA registration; `services/etc/init/nearlink_service.cfg` — service init.

Ours:

- `/home/archivalera/plum/zcode-projects/nearlink/stack/ssap/include/{ssap_codec.h,ssap_server.h,hwsle_transport.h}` — our C SSAP stack.
- `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/OSPL-UAPI-CONTRACT.md` — OpenSparklink ioctl/netlink ABI reference.

Line citations below use relative paths with `file:line`, e.g. `frameworks/native/nearlink_host.cpp:367-373`.

---

## 1. Layered architecture

README states the four-layer model explicitly: application → framework → system-service → driver
(`README.md:40`). The framework layer is the "middle capability encapsulation layer"; it binds
down to the system service and up to ArkTS/callers (`README.md:55-67`).

Concretely, in this repo:

- **Native framework** (`frameworks/native/`): C++ classes with names like `NearlinkHost`,
  `SleCentralManager`, `SleAdvertiser`, `SsapClient`, `SsapServer`, `SleDataTransfer`,
  `NearlinkSleRanging`, `NearlinkRemoteDevice`, `NearlinkSleController` — the public "Inner API".
  Each is a pimpl class whose `impl` holds a binder proxy (obtained via `GetProxy<T>(profileName)`)
  and one or more callback stubs registered into the service (`frameworks/native/nearlink_host.cpp:60-118`,
  `frameworks/native/nearlink_ssap_client.cpp:59-87`).
- **JS framework** (`frameworks/js/napi/`): NAPI modules registered as
  `nearlink.manager`, `nearlink.scan`, `nearlink.advertising`, `nearlink.remoteDevice`,
  `nearlink.ssap`, `nearlink.datatransfer`, `nearlink.cdsm`, `nearlink.constant`
  (e.g. `frameworks/js/napi/src/manager/napi_nearlink_manager.cpp:286-302`). Each NAPI function
  simply calls into the native framework class (e.g. `Enable` → `NearlinkHost::GetInstance().EnableNl()`
  at `napi_nearlink_manager.cpp:67-73`).
- **ETS (ArkTS) layer** (`frameworks/ets/taihe/`): Taihe IDL files
  (`ohos.nearlink.manager.taihe`, `ohos.nearlink.remoteDevice.taihe`, `ohos.nearlink.cdsm.taihe`)
  define the app-facing namespace + `sts_inject` glue that loads the NAPI native lib
  (`frameworks/ets/taihe/nearlink_manager/idl/ohos.nearlink.manager.taihe:16-19`) and provides the
  `on('type', cb)` / `off('type', cb)` subscription façade over the per-event functions
  (`ohos.nearlink.manager.taihe:97-129`). The IDL compiles to an ANI `.so` + a static ABC
  (`frameworks/ets/taihe/nearlink_manager/BUILD.gn:36-76`).

So the stack an application touches is:
`ArkTS app → @kit.ConnectivityKit (ETS taihe IDL/ABC) → NAPI .so → native framework (inner API C++) → binder proxy/stub → service (SA 1190) → stack → DLI → chip`.

Downward, the framework talks to the system service via **OHOS IPC kit (IPC/RPC, binder-based)**,
not netlink/socket. See §4.

### Native/JS/ETS three-layer split summary

| Layer | Files | Role |
|---|---|---|
| native C++ Inner API | `interfaces/inner_api/include/*.h`, `frameworks/native/*.cpp` | complete capability API; used by system services and by NAPI |
| JS (NAPI) | `frameworks/js/napi/src/**` | thin async glue over native classes; one module per feature |
| ETS (Taihe IDL) | `frameworks/ets/taihe/**/idl/*.taihe` | app-facing ArkTS namespace + `on/off` sugar |

The README documents the framework-layer module split: local device mgmt, advertising, scanning,
remote device mgmt, SSAP data interaction, general (port) data transfer, CDSM, public constants
(`README.md:59-67`).

---

## 2. Native interface panorama (inner API)

All types in namespace `OHOS::Nearlink`, error type `NlErrCode`. Key headers under `interfaces/inner_api/include/`.

### 2.1 Host / adapter management — `nearlink_host.h` (652 lines)

Singleton `NearlinkHost::GetInstance()` (`nearlink_host.h:270`). Main ops:

- `IsNearlinkSupport()` — capability query via system property `const.nearlink.enable` (`nearlink_host.cpp:694-711`).
- `EnableNl(SleAutoConnectPolicy)` / `DisableNl()` / `EnableNlToHalf()` / `DisableNlToOff()` (`nearlink_host.h:331-352`).
- `GetSleFullState()` → `SleStateID`, `IsSleEnabled()`, `IsSleAvailableToCaller()` (`nearlink_host.h:368-403`).
- `RegisterObserver(std::shared_ptr<NearlinkHostObserver>)` — `OnStateChanged`, `OnPairConfirmed`, name/addr changed (`nearlink_host.h:51-121`).
- `RegisterRemoteDeviceObserver` — `OnAcbStateChanged`, `OnPairingRequest`, `OnPairStatusChanged`, `OnConnectionStateChanged`, `OnReadRemoteRssiEvent` (`nearlink_host.h:123-216`).
- Local identity: `GetLocalName/SetLocalName/GetLocalAddress`, `GetLocalSupportedUuids`, `GetLocalDeviceAppearance` (`nearlink_host.h:474-514`).
- Pairing: `GetPairedDevices`, `RemovePair`, `RemoveAllPairs` (`nearlink_host.h:523-547`).
- Connection: `GetAdapterConnectState`, `GetProfileConnState`, `ConnectAllowedProfiles`, `DisconnectAllowedProfiles`, `GetLinkRole` (`nearlink_host.h:453-466, 554, 598-606`).
- Misc: `NearlinkFactoryReset`, `SetFreqHopping`, `IsNearlinkSupportFrame4`, `IsFeatureSupported`, battery/RSSI observers (`nearlink_host.h:419-441, 633`).

### 2.2 Scanning — `nearlink_sle_scanner.h` (560 lines)

- `SleCentralManager::CreateSleCentralManager(callback)` factory (`nearlink_sle_scanner.h:501-502`).
- `StartScan()` / `StartFullScan(settings)` / `StartScanWithFilter(settings, filters)` / `StopScan()` (`nearlink_sle_scanner.h:517-539`).
- `SleScanSettings`: `SetScanMode`, `SetDuration` (10–60 s), `SetReportDelay`, `SetFrameType` (`nearlink_sle_scanner.h:287-361`).
- `SleScanFilter`: `SetDeviceId`, `SetName`, `SetServiceUuid(+Mask)`, `SetServiceData(+Mask)`, `SetManufacturerId/Data(+Mask)`, `SetRssiThreshold` (`nearlink_sle_scanner.h:389-455`).
- `SleScanResult`: `GetPeripheralDevice`, `GetRssi`, `GetServiceUuids`, `GetManufacturerData`, `GetServiceData`, `IsConnectable`, `GetPayload` (`nearlink_sle_scanner.h:48-127`).
- Callback `SleCentralManagerCallback`: `OnScanCallback`, `OnSleBatchScanResultsEvent`, `OnStartOrStopScanEvent` (`nearlink_sle_scanner.h:230-247`).

### 2.3 Advertising — `nearlink_sle_advertiser.h` (513 lines)

- `SleAdvertiser::CreateSleAdvertiser()` (`nearlink_sle_advertiser.h:449`).
- `StartAdvertising(settings, advData, scanResponse, duration, callback)` (duration in 10 ms units, 0 = continuous), `StopAdvertising(cb|handle)`, `EnableAdvertising/DisableAdvertising(handle)`, `SetAdvertisingData`, `GetAdvHandle` (`nearlink_sle_advertiser.h:460-492`).
- `SleAdvertiserSettings`: `SetConnectable`, `SetInterval`, `SetTxPower`, `SetPrimary/SecondaryPhy`, `SetOwnAddr(Type)`, `SetLinkRole` (`nearlink_sle_advertiser.h:220-369`).
- `SleAdvertiserData`: `AddServiceUuid`, `AddServiceData`, `AddManufacturerData`, `SetIncludeDeviceName/TxPower` (`nearlink_sle_advertiser.h:74-148`).
- Callback `SleAdvertiseCallback`: `OnStartResultEvent`, `OnStopResultEvent`, `OnSetAdvDataEvent`, `OnEnableResultEvent`, `OnDisableResultEvent`, `OnGetAdvHandleEvent` (`nearlink_sle_advertiser.h:392-427`).

### 2.4 SSAP client — `nearlink_ssap_client.h` (507 lines)

Model is one `SsapClient` per remote device, mirroring BLE-GATT-style attribute access.

- `SsapClient::CreateSsapClient(device)` factory (`nearlink_ssap_client.h:268`).
- Lifecycle: `Connect(callback)` → `Disconnect()` → `Close()` (`nearlink_ssap_client.h:278-322`).
- Discovery: `FindStructure()` / `FindStructureByUuid(uuid)`; then `GetService(uuid)` / `GetService(vector)`; `GetHandle(serviceUuid, propertyUuid, &handle)` (`nearlink_ssap_client.h:329-359`).
- Data ops: `ReadProperty`, `ReadPropertyByUuid`, `WriteProperty(+vector value)`, `CallMethod(+value)`, `ReadDescriptor`, `WriteDescriptor` (`nearlink_ssap_client.h:367-451`).
- Notify/indicate: `SetNotifyProperty(prop, enable)`, `SetIndicateProperty(prop, enable)`, `SetNotifyEvent`, `SetIndicateEvent` (`nearlink_ssap_client.h:417-471`).
- Other: `RequestSleMtuSize(mtu)`, `RequestConnectionPriority`, `ReadRemoteRssiValue` (`nearlink_ssap_client.h:408-479`).
- Callback `SsapClientCallback` — the async result channel: `OnConnectionStateChanged`, `OnServicesDiscovered`, `OnPropertyReadResult`, `OnPropertyWriteResult`, `OnMethodCallResult`, `OnDescriptorReadResult/WriteResult`, `OnMtuUpdate`, `OnPropertyChanged` (notify), `OnEventNotified`, `OnSetPropertyNotifyResult`, `OnSetPropertyIndicateResult`, `OnServicesRediscovered`, `OnServiceChanged` (`nearlink_ssap_client.h:53-251`).

### 2.5 SSAP server — `nearlink_ssap_server.h` (325 lines)

- `SsapServer::CreateSsapServer(callback)` (`nearlink_ssap_server.h:198`).
- `AddService(SsapService&)`, `RemoveSsapService`, `ClearServices`, `Close`, `GetService(uuid, isPrimary)` (`nearlink_ssap_server.h:205-232`).
- Push: `NotifyPropertyChanged(device, prop, confirm)`, `NotifyEvent(device, event, value, confirm)`, `SetPropertyValue`, `SetDescriptorValue` (`nearlink_ssap_server.h:241-269`).
- Connection: `Connect(device, secureReq, autoConnect)`, `CancelConnection(device)` (`nearlink_ssap_server.h:288-296`).
- Auth: `AuthorizeResponse(requestId, allow)` (`nearlink_ssap_server.h:278`).
- Callback `SsapServerCallback`: `OnConnectionStateUpdate`, `OnServiceAdded`, `OnPropertyReadRequest`, `OnPropertyWriteRequest`, `OnDescriptorReadRequest/WriteRequest`, `OnMtuUpdate`, `OnNotifyPropertyChanged`, `OnNotifyEventChanged`, `OnConnectionParameterChanged` (`nearlink_ssap_server.h:52-181`).

### 2.6 Service model classes

`nearlink_ssap_service.h`, `nearlink_ssap_property.h`, `nearlink_ssap_method.h`, `nearlink_ssap_event.h`, `nearlink_ssap_descriptor.h`:

- `SsapService`: uuid/handle/endHandle/type + `AddProperty/AddMethod/AddEvent/AddDescriptor` (`nearlink_ssap_service.h:60-276`).
- `SsapProperty`: `OperationIndication` enum = READ 0x01, WRITE_NO_RESPONSE 0x02, WRITE_WITH_RESPONSE 0x04, NOTIFY 0x08, INDICATION 0x10, BROADCAST 0x20, WRITE_CLIENT/SERVER_CONFIG 0x200/0x400 (`nearlink_ssap_property.h:76-85`); `PropertyWriteType` CMD/REQ/DEFAULT (`nearlink_ssap_property.h:67-71`).
- `SsapMethod`, `SsapEvent`, `SsapDescriptor` analogous (methods carry params+result, events carry params, descriptors carry config values).

### 2.7 General data transfer (port-based) — `nearlink_sle_datatransfer.h` (402 lines)

Distinct from SSAP: a raw "port" channel per app UUID.

- `SleDataTransfer::CreateSleDataTransfer()` (`nearlink_sle_datatransfer.h:319`).
- `CreatePort(uuid, callback)` / `DestroyPort(uuid)` (`nearlink_sle_datatransfer.h:327-335`).
- `Connect(ConnectionParams)` / `Disconnect(params)` / `GetConnectionState` / `WriteData(DataParams&)` (`nearlink_sle_datatransfer.h:343-372`).
- `ConnectionParams::PortTransMode`: BASIC / TRANSPARENT / STREAM / RELIABLE (`nearlink_sle_datatransfer.h:48-54`) — maps to the data-plane modes in `README.md:90-95`.
- Callback `SleDataTransferCallback`: `OnConnectionStateChanged`, `OnReceiveData` (`nearlink_sle_datatransfer.h:298-306`).
- Under the hood the port is a kernel `SocketPair`; on connect the service passes an `fd` back and the framework runs a `PortSocketManager` (rx thread + socket bridge) (`frameworks/native/nearlink_sle_datatransfer.cpp:57-66, 98-115, 152-189`).

### 2.8 Ranging — `nearlink_sle_ranging.h` (425 lines)

- `NearlinkSleRanging::CreateNearlinkSleRanging(callback)` (`nearlink_sle_ranging.h:370`).
- `StartSleRanging(device, RangingConfig)` / `StopSleRanging(device)` (`nearlink_sle_ranging.h:385-393`).
- `RangingConfig`: `SetRefreshRate`, `SetAlgoMode(RANGING_ALGO_MODE_ONE/TWO)`, `SetToneControl` (`nearlink_sle_ranging.h:66-135`).
- `RangingResult`: address, result, `distance` (float), `prob`, `rssi` (`nearlink_sle_ranging.h:143-248`).
- Callback `SleRangingCallback`: `OnSleRangingResult`, `OnSleRangingStateChange` (`nearlink_sle_ranging.h:349-357`).

### 2.9 Other inner API headers

`nearlink_remote_device.h` (pairing + device info + RSSI, `StartPair`, `SetPairingConfirmation`, `GetPairState`, `IsAcbConnected`, `UpdateConnectInterval`), `nearlink_sle_controller.h` (audio coexistence params), `nearlink_cdsm_client.h` (coordinated device set), `nearlink_hid_host.h`, `nearlink_tws_client.h`, `nearlink_vcp_client.h`, `nearlink_ASC_source.h` (profiles), `nearlink_uuid.h`.

### 2.10 Mapping to our stack (`stack/ssap/`)

Our C stack already implements the protocol half of this surface:

| OHOS inner API | Our analog | Notes |
|---|---|---|
| `SsapServer::AddService/AddProperty` | `ssap_server_add_service/add_property` (`stack/ssap/include/ssap_server.h:85-90`) | both assign handles, both model property op-bits/permission |
| `SsapServer` callback dispatch (read/write/notify requests) | `ssap_read_cb/write_cb/notify_cb` + `ssap_server_dispatch` (`ssap_server.h:44-49,93`) | our callbacks are the counterpart of `SsapServerCallback::OnPropertyReadRequest/WriteRequest` |
| `SsapServer::NotifyPropertyChanged` | `ssap_server_notify` (`ssap_server.h:96-97`) | |
| SSAP PDU opcodes/trans types | `SSAP_MSG_*` enum + `ssap_opcode_of/ssap_trans_type_of` (`stack/ssap/include/ssap_codec.h:19-52,144-146`) | our codec mirrors the OHOS `ssap_pkt.h` byte layout |
| property operation bits | `SSAP_OP_*` (`ssap_codec.h:80-85`) | identical bit values to `SsapProperty::OperationIndication` |
| — (absent) | — | we have no scanning/adv/host/port-transfer/remote-device layer yet — that is the gap the framework API shapes below can fill |

Our transport `hwsle_transport.c` maps SSAP PDUs onto `/dev/hwsle` ACB frames (tcid 0x0A)
(`stack/ssap/include/hwsle_transport.h:31-43`), which is the same role OHOS fills with its DLI →
serial/UART layer (`README.md:98-101`). OpenSparklink offers the same op set via ioctl/netlink
(`OSPL-UAPI-CONTRACT.md:35-53`), i.e. a kernel-first API; OHOS instead keeps everything in a
userspace SA and speaks binder to apps.

---

## 3. JS/ETS application interface

### 3.1 API shape

- Commands return **Promises** for async ops and plain values for sync getters; errors are thrown as
  `BusinessError`. NAPI async work is implemented with `napi_create_promise` + `napi_create_async_work`
  (`frameworks/js/napi/src/common/napi_async_work.cpp:31-38,112-139`). Callers use
  `foo().then(...).catch(...)` (`README.md:754-759`) or `try/catch` for sync calls (`README.md:622-627`).
- Events are **callback subscriptions** via `module.on('eventName', cb)` / `module.off('eventName', cb)`
  (README scan example: `scan.on('deviceFound', cb)`, `README.md:818-828`). The taihe IDL glues the
  string-based `on/off` to per-event NAPI functions (`ohos.nearlink.manager.taihe:97-129`).
- The NAPI layer pushes events by publishing on an event-subscribe map (`frameworks/js/napi/src/common/napi_event_subscribe_module.cpp`,
  consumer example `frameworks/js/napi/src/scan/napi_nearlink_scan_callback.cpp:74-82`).

### 3.2 App-visible modules (`@kit.ConnectivityKit`, namespace `@ohos.nearlink.*`)

From README usage guide + napi module registrations:

- `manager`: `isNearLinkSupported()`, `getState()`, `enableNearlink()`/`disable()`, `on('stateChange')`,
  `getLocalAddress/getLocalName/setLocalName`, `getPairedDevices()`, `factoryReset()`
  (`README.md:567-660`; `napi_nearlink_manager.cpp:43-55`).
- `scan`: `startScan(filters, options)` / `stopScan()`, `on('deviceFound', cb)` — result object
  `{address, deviceName, rssi, data, isConnectable}` (`README.md:790-863`; `napi_nearlink_scan_callback.cpp:31-51`).
- `advertising`: `startAdvertising(params)` → id, `stopAdvertising(id)`, `on('advertisingStateChange')`
  (`README.md:669-780`).
- `remoteDevice`: `createRemoteDevice(address)`, `startPairing`, `connect/disconnect`, `getPairingState`,
  `getConnectionState`, `getAcbState`, `getDeviceName/Alias/Model/Information`, `setConnectionInterval`,
  `getRssiValue`, plus pairing/conn/ACB change events (`frameworks/js/napi/src/remoteDevice/napi_nearlink_remote_device.cpp:95-130`).
- `ssap`: `createClient(address)` → client object with `connect`, `disconnect`, `close`, `getServices`,
  `getServicesByUuid`, `requestMtuSize`, `readProperty`, `readPropertyByUuid`, `callMethod`,
  `writeProperty`, `readDescriptor`, `writeDescriptor`, `setPropertyNotification/Indication`,
  `onPropertyChange`, `onConnectionStateChange`, `onMtuChange`, `onEventNotify`; and `createServer`
  with `addService`, `notifyPropertyChanged`, `sendResponse`, `onPropertyRead/Write` etc.
  (`frameworks/js/napi/src/ssap/napi_nearlink_ssap_client.cpp:103-124`, `napi_nearlink_ssap_server.cpp:81-103`).
- `datatransfer`: `createPort(uuid)`, `destroyPort`, `connect({address,uuid,transferMode})`,
  `disconnect`, `getConnectionState`, `writeData`, `onConnectionStateChanged`, `onReadData`
  (`frameworks/js/napi/src/datatransfer/napi_nearlink_datatransfer.cpp:43-52`).
- `cdsm`: `createCdsmClient(address)`, `getCdsmInfo`, `on('cdsmInfoChange')` (`ohos.nearlink.cdsm.taihe:21-43`).

### 3.3 Minimal application call sequence (TV-box remote control / peripheral scenario)

1. **Pre-check**: `manager.isNearLinkSupported()` (must be called first, `README.md:558`).
2. **Enable**: `manager.enableNearlink()`; wait for `manager.on('stateChange')` → `STATE_ON`.
3. **Scan**: `scan.on('deviceFound', cb)`; `scan.startScan([{serviceUuid: ...}], {scanMode})`;
   from `cb` take `address`.
4. **Create device + pair**: `remoteDevice.createRemoteDevice(address)`, `startPairing()`;
   handle `remoteDevice.on('pairingStateChange')` / `manager.on('pairingRequest')` and call
   `setPairingConfirmation(true)`.
5. **Connect**: `remoteDevice.connect()`; wait `on('connectionStateChange')` → connected.
6. **Discover services**: `ssap.createClient(address)`; `client.getServices()` /
   `client.getServicesByUuid(uuid)` (SSAP Find Structure).
7. **Read/write**: `client.readProperty(propUuid)` / `client.writeProperty(propUuid, value)`,
   results via Promise; method calls via `client.callMethod(...)`.
8. **Notify subscription**: `client.onPropertyChange(cb)`; then `client.setPropertyNotification(prop, true)`
   (optionally write the client-config descriptor first); receive pushed values in `onPropertyChange`.
   Server side: `ssap.createServer()`, `addService(...)`, serve `onPropertyRead/Write`, push via
   `notifyPropertyChanged`.
9. Optional high-rate path: `datatransfer.createPort(uuid)` + `connect({address, uuid, transferMode})`
   + `writeData` / `onReadData` for bulk/stream transport.
10. **Teardown**: `client.close()` / `disconnect()`, `scan.stopScan()`, `manager.off(...)`,
    `manager.disableNearlink()`.

---

## 4. IPC design

Framework ↔ service communication is **OHOS IPC/RPC (binder)** over a SystemAbility, not a message
queue or netlink.

- The service registers SA id **1190**, process `nearlink_service`, on-demand start
  (`sa_profile/1190.json:1-15`; `services/etc/init/nearlink_service.cfg`). `sa_main` is the process
  host (`nearlink_service.cfg:17-18`).
- The framework subscribes to the SA and caches the `IRemoteObject`; a single host proxy
  `INearlinkHost` is the hub, and per-feature "profile" proxies are obtained via
  `hostProxy->GetProfile(profileName, remote)` and cached in `profileRemoteMap_`
  (`frameworks/native/nearlink_sa_manager.cpp:85-103`). `GetProxy<T>(profileName)` =
  `iface_cast<T>(GetRemoteProfile(...))` (`frameworks/native/include/nearlink_sa_manager.h:124-128`).
- Every call is a `remote->SendRequest(code, data, reply, TF_SYNC)` transaction on a
  `MessageParcel` with interface token (`frameworks/ipc/src/nearlink_ssap_client_proxy.cpp:386-396`,
  and e.g. `:27-49`). Sync, one round trip per op; heavy data (SSAP property values) rides in the
  reply parcel (`nearlink_ssap_client_proxy.cpp:306-334`).
- Server side: `NearlinkSsapClientStub : IRemoteStub<INearlinkSsapClient>` dispatches `OnRemoteRequest`
  through a `code → handler` map, each handler bound to a permission check
  (`services/ipc/include/nearlink_ssap_client_stub.h:27-59`); the handler calls the profile service
  (e.g. `InterfaceProfileSsapClient`, `services/service/include/interface_profile_ssap_client.h:52-81`),
  which talks to the stack.
- **Callbacks are reverse IPC**: the framework registers a `*CallbackStub` into the service; the
  service pushes events via `remote->SendRequest` on that stub. e.g. `NearlinkSsapClientCallbackStub`
  dispatches `NL_SSAP_CLIENT_CALLBACK_*` codes (`frameworks/ipc/src/nearlink_ssap_client_callback_stub.cpp:31-80`).
  The framework wraps the incoming callback into an async fan-out to app observers
  (`frameworks/native/nearlink_host.cpp:125-151`).
- App identity: `RegisterApplication` binds a callback + (pid, uid) and returns an `appId`;
  the server validates every subsequent call with `IPCSkeleton::GetCallingPid/Uid`
  (`services/server/src/nearlink_ssap_client_server.cpp:103-120`).
- SA lifecycle: the framework's `NearlinkSaManager` subscribes `OnAdd/OnRemoveSystemAbility` and
  fires per-profile `serviceStartedFunc_/serviceStoppedFunc_`/`stateOn/stateOffFunc_` hooks so
  every feature re-arms on SA start/stop (`nearlink_sa_manager.cpp:54-68,106-136,202-241`).

**Borrowing note for a plain-Linux port**: the binder+SA machinery is not portable; but the
*splitting discipline* is — one "host" hub object + named "profile" sub-objects, and a reverse-callback
channel rather than polling. On Linux we can keep that shape with a Unix-domain socket or dbus +
fd-passing (our `/dev/hwsle` already passes `fd`s per connection in the OHOS datatransfer model,
`nearlink_sle_datatransfer.cpp:98-115`).

---

## 5. Ranging algorithm adapter (pluggable)

`frameworks/ranging_alogorithm_adapter/` — HADM (HiSilicon Auto Distance Measurement).

- Adapter is a singleton that `dlopen`s `libnearlink_measure.z.so` and `dlsym`s three symbols:
  `measure_alg_func` (compute distance), `measure_init`, `measure_set_algo_mode`
  (`frameworks/ranging_alogorithm_adapter/src/ranging_alogorithm_adapter.cpp:28-31,61-77`).
- Algorithm ABI: input `MeasureAlgPara` (IQ I/Q arrays from DUT/RTD, ToF values, RSSI, NV/ToF
  offsets, channel map, algo mode), output `DisResult` (`disSmoothed`, `disOri`, `prob`, `rssi`,
  `smoothNum`) (`ranging_alogorithm_adapter_def.h:29-107`).
- Wiring: ranging session receives a "sounding result" over IPC
  (`OnSoundingResult`, `frameworks/native/nearlink_sle_ranging.cpp:52-66`), the framework translates
  it into `MeasureAlgPara` (`ranging_alogorithm_adapter.cpp:89-130`) and pushes the computed
  `RangingResult{distance, prob, rssi}` to the app callback.
- **Pluggability model**: the algorithm is a drop-in `.so` behind three fixed symbols; the framework
  never links the algorithm statically. For us, this is a clean template: keep the measurement/PHY
  sounding on the chip, bring raw IQ/ToF/RSSI up to userspace, and load the distance algorithm as a
  `dlopen` plugin with a fixed C ABI.

---

## 6. Lessons for the TV-box

### 6.1 If the box runs OHOS

No self-developed application layer is needed: an app uses `@kit.ConnectivityKit`
(`@ohos.nearlink.*`). Requirements/caveats:

- Must be OpenHarmony Standard System; component must be in product config with
  `const.nearlink.enable = 1` (`README.md:147-149`). If the vendor (Hi3798) image ships the
  `nearlink_service` SA (SA 1190), apps get manager/scan/advertising/remoteDevice/ssap/datatransfer
  for free.
- The WS73 dongle still needs a **driver**: OHOS's driver layer (DLI) talks to "vendor chips" via
  serial/UART (`README.md:97-101`); the WS73 USB dongle is not a native OHOS DLI device, so a
  HDI/DLI shim mapping USB-CDC/hwsle onto the expected serial HCI would be required. The framework
  above it is then reusable unchanged.
- Capability checks `manager.isNearLinkSupported()` gate everything (property + SA presence),
  so a box with the service but no dongle fails gracefully.

### 6.2 If the box runs plain Linux (our stack)

The framework's API shapes are the best available **interface blueprint** for our userspace stack.
Concretely borrow:

1. **Singleton host object with state + observers.** `NearlinkHost` with `EnableNl/DisableNl`,
   `IsNearlinkSupport/IsSleEnabled`, `RegisterObserver`/`DeregisterObserver`
   (`nearlink_host.h:262-403`). Our equivalent: a `nearlink_host` singleton with
   `IsNearlinkSupport` backed by a system/device capability check and a state-change observer list.
2. **Factory-created per-peer objects.** `SleCentralManager::CreateSleCentralManager(cb)`,
   `SsapClient::CreateSsapClient(device)`, `SsapServer::CreateSsapServer(cb)` — constructor hidden
   behind `Pattern`, pimpl (`nearlink_ssap_client.h:268,493-503`). Same pattern for
   `SleDataTransfer`, `NearlinkSleRanging`. We should expose factory functions returning opaque
   handles rather than raw structs.
3. **One object per remote device for SSAP** with GATT-like verbs: `FindStructure`,
   `GetService(uuid)`, `GetHandle(service,property,&handle)`, `ReadProperty/WriteProperty`,
   `SetNotifyProperty` (`nearlink_ssap_client.h:329-451`). This maps 1:1 onto our
   `ssap_codec`/`ssap_server` C layer and is a much friendlier API than raw SSAP PDUs.
4. **Callback classes, not callback pointers only** — `virtual` observer classes with defaulted
   empty methods (so apps override only what they need) (`nearlink_ssap_client.h:53-251`,
   `nearlink_host.h:51-121`). In C we can emulate with an ops struct where every field may be NULL.
5. **Async results via per-op response callbacks.** `OnPropertyReadResult(property, ret)` /
   `OnPropertyWriteResult` carry the operation result back (`nearlink_ssap_client.h:93-129`);
   commands return `NlErrCode` synchronously only to accept/reject the request. This separates
   "call accepted" from "operation completed" — important for SSAP where the response arrives on
   the ACB channel later.
6. **Result/param classes with setters and getters** (`SleScanFilter`, `SleAdvertiserData`,
   `SleAdvertiserSettings`, `ConnectionParams`) so apps configure fields incrementally and the
   layer validates ranges (e.g. scan duration 10–60 s enforced in NAPI, `napi_nearlink_scan.cpp:40-43,118-124`).
7. **A dedicated port/stream transfer API separate from SSAP** — `SleDataTransfer` with
   `CreatePort(uuid)` + `Connect({address,uuid,transferMode})` + `WriteData` + `OnReceiveData`
   (`nearlink_sle_datatransfer.h:48-54,298-306`). We already carry ACB data frames on `/dev/hwsle`
   (tcid 0x1F CUTC, `hwsle_transport.h:39`); expose it as a port API, not raw frames.
8. **Service table + descriptors on the server side.** `SsapServer::AddService` +
   `SsapService::AddProperty/AddMethod/AddEvent/AddDescriptor` and op-bit semantics
   (`nearlink_ssap_service.h:97-121`, `nearlink_ssap_property.h:76-85`); our
   `ssap_server_add_service/add_property` already match (`ssap_server.h:85-90`).
9. **Ranging as `StartSleRanging(device, config)` + `RangingResult{distance,prob,rssi}` callback**
   with a `dlopen`-ed algorithm `.so` (`nearlink_sle_ranging.h:385-393`;
   `ranging_alogorithm_adapter.cpp:61-77`).
10. **Naming/JSON-ish parameters as the cross-language contract.** Even without ArkTS, keeping our
    C API parameterized the same way (uuid strings big-endian, address strings little-endian,
    `NlErrCode` returns) makes a future JS/Python/Go binding trivially mirror OHOS.

### 6.3 Direct structural comparison table

| Concern | OHOS framework | Our stack | OSPL (reference) |
|---|---|---|---|
| App-facing API | binder-based C++ inner API + NAPI + ArkTS | C `ssap_server/codec` (protocol only) | ioctl + netlink char-device API (`OSPL-UAPI-CONTRACT.md:35-58`) |
| Discovery/scan | `SleCentralManager` (user SA) | — (gap) | `SL_IOCTL_START_SCAN/SET_SCAN_FILTER` |
| Connection mgmt | `NearlinkHost` + `NearlinkRemoteDevice` | — (gap) | `SL_IOCTL_CONNECT/CONN_INFO` |
| SSAP | `SsapClient`/`SsapServer` (binder) | `ssap_codec` + `ssap_server` C (direct) | `SL_IOCTL_SSAP_*` |
| Data plane | `SleDataTransfer` ports | ACB on `/dev/hwsle` | `SL_IOCTL_CONN_SEND/RECV` |
| Ranging | `NearlinkSleRanging` + HADM dlopen | — (gap) | `SL_IOCTL_MEAS_*` + `MEAS_READ_CAP` |
| Notifications | reverse binder callbacks | recv callback (`hwsle_transport.h:31-34`) | per-fd event queue + poll/read (`OSPL-UAPI-CONTRACT.md:62-83`) |

The OHOS model is *everything-in-userspace-daemon + IPC*; OSPL is *kernel module + syscalls*. Our
`/dev/hwsle` userspace stack is closer to OSPL's placement but we can still adopt OHOS's *object
model and API verbs* on top.

---

## 7. Path comparison conclusion

**Path A — OHOS on the box (use official framework):** fastest application path. The entire
manager/scan/advertising/remoteDevice/ssap/datatransfer/ranging stack is provided; only a DLI/HDI
shim for the WS73 USB dongle would need to be written. Cost: must ship OpenHarmony Standard +
`nearlink_service` SA (1190) in the product image, with `const.nearlink.enable=1`. Apps are then
bound to OHOS ArkTS/`@kit.ConnectivityKit` — fine for a dedicated TV-box OS but not portable to
other Linux distros.

**Path B — plain Linux (self-developed stack):** we own the whole path but get to design the API.
Best ROI: keep our C `ssap_codec`/`ssap_server` as the engine, add the missing layers in the order
the framework suggests — (1) host singleton with state/observers, (2) scanner + advertiser,
(3) per-device SsapClient/SsapServer object API with the OHOS verbs (`FindStructure`, read/write/
notify by uuid+handle), (4) port-based data transfer, (5) ranging with a dlopen algorithm plugin —
and mirror OHOS's parameter conventions (address/uuid byte order, `NlErrCode`, callback classes) so
future bindings and any later OHOS port stay source-compatible.

If both are wanted, build Path B with the OHOS API shapes as the contract, so a future Path A
migration only swaps the transport behind identical calls.

---

## Open questions

1. WS73 dongle driver on OHOS: does HiSilicon provide an OHOS HDI/DLI for the WS73 USB part
   (`ffff:3733`), or must we write a serial-HCI→USB shim? Not answered by this repo (only UART DLI
   is mentioned, `README.md:99`).
2. The `SleDataTransfer` port mode uses kernel `SocketPair` + fd handoff
   (`nearlink_sle_datatransfer.cpp:57-66,98-115`). Is the WS73 chip capable of the same
   socket/zero-copy datapath on our stack, or do we stay on plain ACB frames (CUTC 0x1F)?
3. Ranging: the HADM `.so` is closed-source HiSilicon. For our stack, is the WS73 measurement API
   exposed over DLI (narrowband `MEAS_*` group in OSPL, `OSPL-UAPI-CONTRACT.md:52`), and can we
   obtain `libnearlink_measure` or must we implement our own distance algorithm plugin?
4. Permission model (`ACCESS_NEARLINK` etc., enforced per-stub in
   `services/ipc/include/nearlink_ssap_client_stub.h:35` and in `nearlink_service.cfg`): on plain
   Linux we should decide early whether to replicate permission checks in the daemon.
5. The `nearlink_service` build here references many OHOS platform deps (SA framework, HiLog,
   `ipc_skeleton`, `parameters`); the framework itself is not drop-in portable. How much of the
   *object model* do we want to mirror verbatim (class names) vs adapt (C handles)?
