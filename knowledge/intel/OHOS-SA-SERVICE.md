---
type: intel
title: "OHOS NearLink SA 1190 + Services Layer — Structure, Stack Integration, and TV-box Porting Assessment"
language: zh
created: 2026-08-17
tags: [intel, ohos, nearlink, services]
sources:
  - "/mnt/hdd/nearlink-stuff/communication_nearlink_service"
trust: B
stale_after: 2027-02-17
---

# OHOS NearLink SA 1190 + Services Layer — Structure, Stack Integration, and TV-box Porting Assessment

Date: 2026-08-17
Author: research subagent (context: WS73 USB dongle `ffff:3733` Linux driver + OHOS TV-box evaluation)

## Sources

Primary (read-only, local) — OHOS `communication_nearlink_service`, under
`/mnt/hdd/nearlink-stuff/communication_nearlink_service/`:

- `sa_profile/1190.json`, `services/etc/init/nearlink_service.cfg`, `services/etc/param/nearlink.para`, `services/etc/param/nearlink.para.dac`
- `services/server/` — `nearlink_host_server.{h,cpp}`, `nearlink_ssap_client_server.cpp`, `BUILD.gn`
- `services/ipc/` — `nearlink_host_stub.h`, `nearlink_ssap_client_stub.{h,cpp}`, `nearlink_sle_datatransfer_callback_proxy.cpp`
- `services/service/` — `src/common/{SleServiceManager.cpp,ProfileServiceManager.{h,cpp},ProfileInfo.{h,cpp},ClassCreator.h,profile_list.h,UnloadSa.cpp}`, `src/ssap/{ssap_client_service.h,ssap_client_stack_adapter.{h,cpp}}`, `src/scan/ScanStackAdapter.cpp`, `src/datatransfer/nearlink_sle_datatransfer_service.{h,cpp}`, `src/adapter/SleAdapter.cpp`, `include/{SleInterfaceProfile.h,SleInterfaceManager.h,SleInterfaceAdapterSub.h,interface_profile_ssap_client.h}`, `BUILD.gn`
- `services/common/` — `include/nearlink_permission_manager.h`, `src/nearlink_system_ability_subscriber.cpp`
- `services/hardware/` — `include/SleDliLayerAdapter.h`, `src/SleDliLayerAdapter.cpp`, `BUILD.gn`
- `services/stack/` — `src/dli/sapi/src/dli_sapi.c`, `src/dli/layer/src/dli_layer.c`, `src/nai/slem/src/slem.c`, `src/nai/nlm/src/nlstk_init.c` (via slem)
- `services/device_manager/` — `include/nearlink_device_manager.h`

Ours (local):
- `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/OHOS-FRAMEWORK-LAYER.md` — framework↔SA binder contract, `GetProfile`/proxy model, app-facing API
- `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/OHOS-HDI-DRIVER.md` — HDI/HDF seam (transport-only, packet types, H4/fd, no USB)

Networking/build/hardware: none used. All line citations are relative paths under
`/mnt/hdd/nearlink-stuff/communication_nearlink_service/` (`file:line`).

---

## 1. Services layer structure

Three cooperating libraries under `services/`, plus supporting dirs. Build graph:
`server/BUILD.gn:48` `nearlink_server` (the SA payload loaded by `sa_main`) links
`ipc:nl_service_ipc`, `service:nearlink_service_impl`, `common:nearlink_service_common`,
`device_manager`, `dft`, and `utils` (`server/BUILD.gn:113-124`).

### 1.1 `server/` — SA 1190 process side (binder dispatch + capability gates)

- 15 thin "server" classes (`server/include/nearlink_*_server.h`), each `class XxxServer : public NearlinkXxxStub` (a binder stub) that performs the caller-identity/permission check and forwards to the matching profile service in `service/`. Files: `nearlink_host_server`, `nearlink_sle_central_manager_server`, `nearlink_sle_advertiser_server`, `nearlink_ssap_client_server`, `nearlink_ssap_server_server`, `nearlink_sle_datatransfer_server`, `nearlink_hadm_client_server`, `nearlink_hid_host_server`, `nearlink_sle_controller_server`, `nearlink_cloud_pair_server`, plus audio `nearlink_asc_server`, `nearlink_vcp_client_server`, `nearlink_tws_client_server`, `nearlink_cdsm_client_server`, `nearlink_utils_server`.
- `NearlinkHostServer` is the SystemAbility itself (`nearlink_host_server.h:31`, `: SystemAbility(NEARLINK_HOST_SYS_ABILITY_ID, true)` `nearlink_host_server.cpp:748`) and the hub that publishes every profile server.
- Each server is a pimpl class; e.g. SSAP client server keeps per-caller `{pid,uid,appId}` remote containers and a callback list with per-caller token (`nearlink_ssap_client_server.cpp:69-74, 144-361`).

### 1.2 `service/` — the real implementation (`libnearlink_service_impl` + `libnearlink_mcp_service`)

Subdirectory map (all under `service/src/`):

| Dir | Contents | Role |
|---|---|---|
| `adapter/` | `SleAdapter`, `SleAdapterWrapper`, `SleRemoteDeviceAdapter`, `SleProfileConnectManager`, `SleReconnectManager`, `SleSecurity`, `SleFeature`, `SleProperties`, `SleCoexist`, `SleASC` | The SLE adapter: drives the stack (cm/sm/cfgdb), reconnect, coexistence |
| `common/` | `SleServiceManager` (enable/disable state machine), `ProfileServiceManager`, `SysStateMachine`, `AdapterStateMachine`, `UnloadSa`, `SleEdmManager`, `SleCommonEventSubscriber`, `SleHiviewUe`, `DeviceBatteryManager`, `SleCollaborationManager`, `SleSwitchDependency` | Lifecycle + orchestration |
| `scan/` | `ScanService` + `ScanStackAdapter` | scan impl, wraps `NLSTK_Devd*` |
| `advertiser/` | `SleAdvertiserImpl` + `SleAdvertiserAdapter` | advertising impl |
| `ssap/` | `ssap_client_service` + `ssap_client_stack_adapter`, `ssap_server_service` + `ssap_server_stack_adapter`, `ssap_based_services_manager` | SSAP client/server impl, wraps `NLSTK_Ssap*` |
| `datatransfer/` | `nearlink_sle_datatransfer_service` + `_cache` | port-based data transfer (socketpair fd model) |
| `hadm/` | `nearlink_hadm_client_service` + `hadm_stack_adapter` | ranging (HADM) client |
| `cloudpair/`, `controller/`, `dialog/`, `datashare/`, `audio/` | cloud pairing, SLE controller, pairing dialogs, name-change persistence, audio framework adapter | support |
| profiles | `asc/ bas/ ccp/ cdsm/ dis/ hid/ icce/ lis/ mcp/ mic/ port/ tws/ vas/ vcp/` | standardized SLE profiles; `hid/HidHostUhid` is the kernel uhid bridge |
| `ServiceManagerPluginLoader/` | dlopen of `libnearlink_server_ext_plugin.z.so` | vendor extension servers + svc cmd |

### 1.3 `ipc/` — binder contract implementation

- `ipc/include/*_stub.h` + `ipc/src/*_stub.cpp` — server-side `IRemoteStub` dispatch. Pattern: `std::map<uint32_t, pair<handler, shared_ptr<NearLinkPermissionItem>>> memberFuncMap_`, each IPC code mapped to a static handler + permission set (`nearlink_ssap_client_stub.h:34-36`, `nearlink_ssap_client_stub.cpp:37-58`).
- `ipc/include/*_callback_proxy.h` + `ipc/src/*_callback_proxy.cpp` — proxies the service uses to push events back at the framework's registered callback stubs (e.g. `nearlink_sle_datatransfer_callback_proxy.cpp:46` `WriteFileDescriptor` — fd handoff over binder).
- `nearlink_hicollie_adapter`, `nearlink_host_observer_proxy`, etc. — host observers/battery/rssi.

### 1.4 Supporting dirs

- `common/` — `NearLinkPermissionManager`, `parameter_manager`, `SleKiaManager`, `nearlink_verification_manager`, `state_machine`, `ThreadUtil`, `nearlink_timer`, `nearlink_system_ability_subscriber` (SA add/remove hooks), `DynamicLibraryLoader`, `BaseObserverList`, `message`.
- `device_manager/` — `NearlinkDeviceManager` (real↔random address conversion, retained device info recovery), `SleRemoteDeviceManager`, `SleConfig`, `SleCoexistManager`, `SleHuksTool` (secure storage), `ManufacturerAbilityLoader`, `xml_parse`, `AdapterDeviceConfig`.
- `dft/` — HiSysEvent DFX (UE/exception/database), with `C` API wrapper for the stack.
- `etc/` — init cfg + parameters (see §2).
- `hibox/` — HiBox C services (audio/dts/hitws/pairconn/local/process/service).
- `hardware/` — the HAL bridge, `nearlink_dli_adapter` shared lib (§3.2).

---

## 2. SA 1190 lifecycle

### 2.1 Registration and process model

- SA id **1190**, process `nearlink_service`, host binary `sa_main`, payload `libnearlink_server.z.so` (`sa_profile/1190.json:3-4`). `run-on-create:false`, `auto-restart:true`, `ondemand` with triggers `persist.nearlink.switch_enable` ∈ {1,2}, `persist.nearlink.reload_sa=1`, `persist.bluetooth.collaboration_service=1` — all gated on `const.nearlink.enable=true` (`1190.json:12-44`).
- Registration is a file-scope static: `SystemAbility::MakeAndRegisterAbility(NearlinkHostServer::GetInstance().GetRefPtr())` (`nearlink_host_server.cpp:561`).
- The service process config: uid `nearlink`, gid `nearlink,shell,wakelock,uhid,radio`, `ondemand:true`, caps `CAP_NET_ADMIN,CAP_SYS_NICE,CAP_WAKE_ALARM,CAP_BLOCK_SUSPEND`, secon `u:r:nearlink_service:s0`, plus a 26-item `permission` list (`nearlink_service.cfg`, services section). The cfg also creates `/data/log/nearlink`, `/data/service/el1/public/nearlink`, `/data/vendor/nearlink` (`nearlink_service.cfg` jobs section) — note `el1` and vendor dirs are the only "user" hint (single-user, user 0).
- On-demand: `persist.nearlink.switch_enable=1` is the boot-on switch (`nearlink.para:15`); `persist.nearlink.reload_sa` and `reset_service` are service-restart knobs (`nearlink.para:17-19`, `nearlink.para.dac` grants rw to uid `nearlink`).

### 2.2 Start/Stop sequence

- `OnStart()` (`nearlink_host_server.cpp:771-795`): resets `reload_sa=0`, `DftManagerStart`, `Init()` → `pimpl->Init()` (`:581-612`): concurrent-task auth, register system/adapter/peripheral/battery/rssi observers, `SleInterfaceManager::Start()`, `NearlinkDeviceManager::RecoverRetainedDeviceInfo()`, `createServers()`; then `Publish(self)` (`:814-818`).
- `SleServiceManager::StartTask()` (`SleServiceManager.cpp:307-341`): `slem_initialize()` (stack init, `slem.c:38` → `NLSTK_InitStack`), plugin loader + verification strategies, collaboration init, `CreateAdapters()`, `ProfileServiceManager::Initialize()`, DLI-reset callback registration, sys-state machine, `WaitForAllSwitchDependency()`.
- `createServers()` (`nearlink_host_server.cpp:647-697`) instantiates all profile servers into `sleServers_` (name→`IRemoteObject`): advertiser, central-manager, cloud-pair, ssap-client, ssap-server, hadm-client, hid-host, datatransfer, sle-controller; then `createPluginServer()` (dlopen ext plugin, `:699-723`) and, if `NearlinkSystemConfig::IsAudioSupported()`, `createAudioServers()` adds ASC/VCP/TWS/CDSM servers (`:725-746`).
- `OnStop()` (`:827-836`): `CheckAndReloadSa` (writes `reload_sa=1` to re-arm for half-state), `DftManagerStop`, `pimpl->Clear()` deregisters all observers.
- Unload: `UnloadSa` starts a 15 s timer when state reaches OFF, then `samgr->UnloadSystemAbility(NEARLINK_HOST_SYS_ABILITY_ID)` (`UnloadSa.cpp:51-59`).
- Driver death: `SleDliDeathRecipient::OnRemoteDied` → `kill(getpid(), SIGKILL)`; init auto-restarts the SA (`SleDliLayerAdapter.cpp:40-46`). Stack-level chip reset detected in-band (`ff ff 01 00 c7`) also SIGKILLs (`dli_layer.c:626-632`).

### 2.3 Multi-profile organization (host + sub-objects)

- Hub/spoke over binder: framework calls `GetProfile(name, remote)` on the host (`nearlink_host_server.cpp:838-847`), which looks up `sleServers_`. Each profile server object is itself a binder stub, so per-profile calls go direct.
- Profile-name constants in `SleInterfaceProfile.h:40-56`; support table (id/uuid) in `ProfileInfo.cpp:28-64`. Framework caches per-profile proxies in `profileRemoteMap_` (see FRAMEWORK-LAYER §4).
- Server-side profile lifecycle is independent of binder: `ProfileServiceManager::Start()` reads the config profile list and instantiates each via `ClassCreator` (factory registration, `ClassCreator.h:54-59`), then `Enable()/Disable()` drive per-profile `Context::Enable()/Disable()` state machines (`ProfileServiceManager.cpp:110-147,181-278`). System-supported profile UUIDs (HID, BAS, DIS, PORT, optional ICCE, and audio-only ASC/CDSM/VCP/MIC) are gated in `SetSystemSupportProfileServices` (`ProfileServiceManager.cpp:536-558`).

---

## 3. Stack integration and the HAL bridge

### 3.1 Service → stack (direct C API)

The profile services link `services/stack:nearlink_stack` (`service/BUILD.gn:260`) and call the stack's C API through per-feature "stack adapters":

- SSAP client: `NLSTK_SsapClientRegApp / DeregApp / DiscoverServices / DiscoverServicesByUuid / GetServices / GetServicesByUuid / ReadProperty / ReadPropertiesByUuid / CallMethod / WriteProperty / ReadDescriptor / WriteDescriptor / ExchangeMtu / SetPropertyNotification / SetPropertyIndication / Connect / Disconnect` plus async callbacks (`ssap_client_stack_adapter.cpp:393-620`; headers `nlstk_ssap_app_client.h`, `nlstk_ssap_app_link.h`). Timeouts `REQ_TIMEOUT 30000`, `CMD_TIMEOUT 15000` (`ssap_client_stack_adapter.h:29-30`).
- Scan: `NLSTK_DevdRegScanModule / AllocScannerId / StartScan / StopScan / RemoveScannerId / DeregScanModule` (`ScanStackAdapter.cpp:56-130`).
- Data transfer: `TRANS_Addr_S`/`SLE_Addr_S` receive + `QOSM_TransChannelRspParams_S` callbacks (`nearlink_sle_datatransfer_service.h:96-99`); port connect creates a kernel `socketpair`, returns the fd to the app over binder (`nearlink_sle_datatransfer_service.cpp:964-968`, `nearlink_sle_datatransfer_callback_proxy.cpp:46`).
- HADM ranging: `nearlink_hadm_stack_adapter`.
- Connection/security/storage used by `SleAdapter`: `cm_api.h`, `nlstk_sm_api.h`, `nlstk_cfgdb_api.h`, `nlstk_public_define_ext.h`, `slem.h` (`SleAdapter.cpp:28-33, 47, 72-76`).

Stack internals (`services/stack/src/`): `cp/` (SLE controller: `bsl/sle/{cm,devd,hadm,qosm,servm/ssap,sm}`), `dli/`, `dp/` (data plane: `dtap` + `transport`), `nai/` (`slem` init, `nlm/nlstk_init`), `sdf/` (portable framework: oal/memm/thread/timer), `utils/`. Note: `stack/src/adapter/` holds the *ext wrapper* glue for chip-vendor extensions (`*_ext_func_wrapper.c`), not the service adapter.

### 3.2 Stack → DLI → hardware/ (HAL bridge) → HDI

- `services/hardware/` builds `nearlink_dli_adapter` (`hardware/BUILD.gn:22`), exposing a tiny C seam `SleDliLayerAdapter.h:61-70`: `SleHalInit(callbacks)`, `SleReset()`, `SleSendDliPacket()`, `SleHalClose()`, `GetDliVersion()`.
- Call chain down: `dli_layer.c:121` → `DLI_SapiInit(DLI_PacketReceived)` (`dli_sapi.c:43`) → `SleHalInit(&callbacks)` (`dli_sapi.c:63`) → `ISleHciInterface::Get()` + `SleHalInit(g_sleDliCallbacks)` on the HDI proxy (`SleDliLayerAdapter.cpp:86-121`). Send path: `SleSendDliPacket` → `g_iSleDli->SleSendHciPacket(data)` (`SleDliLayerAdapter.cpp:129-149`). HDI v1.0/v1.1 feature-detect via `CastFrom` (`:165-178`).
- So the hardware/ lib sits **between** the stack's DLI SAPI and the HDF driver; it is the only chip-facing code in this repo. The HDF driver + HDI IDL live in `drivers_interface`/`drivers_peripheral` (not present locally; see OHOS-HDI-DRIVER.md).

Full service→chip path:
`framework app → binder → server/ stub (perm check) → service/ profile → stack adapter → C stack (NLSTK_*) → dli layer → dli_sapi → hardware/ nearlink_dli_adapter → HDI ISleHciInterface → HDF driver → vendor fd/UART`.

---

## 4. Permissions and multi-user

### 4.1 Permission model

- Four nearlink permissions (`nearlink_permission_manager.h:74-77`): `GET_NEARLINK_LOCAL_MAC`, `GET_NEARLINK_PEER_MAC`, `ACCESS_NEARLINK`, `MANAGE_NEARLINK`.
- Two enforcement points:
  1. **Binder dispatch**: `CHECK_PERMISSION_AND_EXECUTE` macro verifies interface token + per-code permission item before running the handler (`nearlink_permission_manager.h:43-68`). Per-code map, e.g. SSAP client stub (`nearlink_ssap_client_stub.cpp:37-58`): most ops need `ACCESS_NEARLINK`; `DiscoverServiceByUuid`/`GetServicesByUuid` need `MANAGE_NEARLINK`; `CallMethod` is system-API + `ACCESS_NEARLINK`; `RequestIndication` is system-API + both.
  2. **Event fan-out re-check**: every pushed callback re-verifies against the per-observer stored `tokenId_` (e.g. `SsapClientCallbackImpl::OnConnectionStateChanged` → `VerifyPermission(ACCESS_NEARLINK, tokenId_)`, `nearlink_ssap_client_server.cpp:149-153`; host observers `nearlink_host_server.cpp:427,440,472`).
- App identity: `RegisterApplication` stores `{pid, uid, appId}` per callback (`nearlink_ssap_client_server.cpp:69-74,455-459`); every subsequent SSAP op is validated by `CheckSsapClientApp(appId)` comparing `IPCSkeleton::GetCallingPid/Uid` (`:103-122`, applied at `:505, 517, 529, ...`). Remote death triggers `Disconnect(appId)` + `DeregisterApplication(appId)` (`:373-407`).
- Capability helpers via access_token: `IsNativeCaller/IsHapCaller/IsSystemHap/CheckSystemPermission/GetNearlinkApiVersion/GetHapBundleName/GetCallingName/IsUseRealAddr` (`nearlink_permission_manager.h:87-220`).
- MAC privacy: real vs random address per caller — observers store `isRealMac` in the remote container; `NearlinkDeviceManager::ConvertToRandomAddress` maps on fan-out (`nearlink_host_server.cpp:161-171, 405, 429-431`).

### 4.2 Multi-user

- **No real multi-user isolation in the service layer.** The service is a single system-wide SA; state dirs are `el1` (`/data/service/el1/public/nearlink`) and `/data/vendor/nearlink` (`nearlink_service.cfg`). The only `userId` API is `GetHapAppIdentifier(userId, bundleName)` (`nearlink_permission_manager.h:218`) with no per-user logic; the sole other `userId` hit in `service/src` is an unrelated VAS log. For a TV box (single primary user) this is fine; a multi-profile/child-profile requirement would be net-new.

---

## 5. OHOS TV-box landing change list

**Verdict: SA 1190 + services layer is drop-in usable on OpenHarmony Standard.** If the vendor image ships the `nearlink_service` part (`communication_nearlink_service`), the entire binder/service/stack above the HDI seam is present and apps get manager/scan/advertising/remoteDevice/ssap/datatransfer/ranging for free (FRAMEWORK-LAYER §6.1). The single material gap is the **driver** (below the `nearlink_dli_adapter` seam). Everything else is config/pruning.

### "OHOS 电视盒星闪落地改动清单" (concrete checklist)

1. **Ship the part**: include `communication_nearlink_service` in the product image so `nearlink_service` SA (1190) + `libnearlink_server.z.so` + `nearlink_service.cfg` + `nearlink.para`/`.dac` + `sa_profile/1190.json` are installed.
2. **Product params**: `const.nearlink.enable=true`, `persist.nearlink.switch_enable=1` (auto-on), chip-type param (`const.nearlink.slechiptype`), and ensure `IsNearLinkSupported()` capability check passes (`nearlink_host_server.cpp` / framework). The 1190 on-demand triggers depend on `const.nearlink.enable=true` (`1190.json:31-47`).
3. **WS73 USB driver (the real work)**: the HDF driver (`drivers_peripheral/nearlink`) only knows a plain fd (UART/vendor) with H4 framing — **no USB VID/PID handling exists anywhere** (OHOS-HDI-DRIVER.md). Need either:
   - a USB→fd HCI shim exposing the WS73 `ffff:3733` bulk/CDC pipes as the H4-framed fd the vendor interface expects, or
   - a bespoke HDF driver instance binding the WS73 USB part and implementing `ISleHciInterface` v1.0/v1.1 (`SleHalInit`/`SleSendHciPacket`/`Close`) directly.
   Either must keep the `[type byte][payload]` packet stream (0xA1 CMD / 0xA2 EVENT / 0xA3 ACB / 0xA4 ICB) that `SleDliLayerAdapter.h:36-47` and the DLI stack expect.
4. **HDI proxies**: ship `libnearlink_hci_proxy_1.0.z.so` (+ 1.1 if feature-detect wanted); `1190.json:8-10` pins `min_hdi_proxy_version`.
5. **MAC persistence**: provide `/data/vendor/nearlink/slemac.txt` (or vendor equivalent) — HDF responsibility per OHOS-HDI-DRIVER.md.
6. **Prune profiles for the box**: default `ProfileInfo.cpp:47-64` enables SSAP client/server, HID, DIS, LIS, ICCE, PORT, CDSM, MCP, ASC, TWS, VCP, CCP, VAS, BAS, MIC. For a remote-control/SSAP-only box, gate audio profiles off (`NearlinkSystemConfig::IsAudioSupported()` false already skips ASC/TWS/VCP/CDSM/MCP/CCP/VAS/MIC in `ProfileServiceManager.cpp:128-135`); keep SSAP/scan/datatransfer/ranging/HID. `HidHostUhid` needs kernel uhid support.
7. **No service-layer code changes expected** for the standard remote-control use case; the SA self-manages on-demand start, 15 s unload, SIGKILL-on-driver-death auto-restart under init.
8. **Optional**: drop the closed `libnearlink_server_ext_plugin.z.so` (plugin servers, `nearlink_host_server.cpp:703`) and `libnearlink_measure.z.so` (ranging algorithm) unless the vendor provides them; ranging APIs degrade gracefully if absent.
9. **Test seam**: the stack's fake-HAL (`dli_data_stub.c`, `-DNEARLINK_SERVICE_STACK_LOCAL_TEST`) is already in-tree for host-side testing before hardware lands.

---

## 6. Conclusion: Path A vs Path B service-layer effort

- **Path A (OHOS TV box) — service layer effort ≈ zero, driver effort ≈ 100% of the work.**
  SA 1190 + `server/`+`service/`+`ipc/`+`stack/` are a finished, self-contained system-service implementation with lifecycle, permissioning, privacy (real/random MAC), multi-profile gating, and on-demand unload already handled. If OpenHarmony Standard + the part ships in the vendor image, the only buildable-new component is the WS73 USB HDI/HDF shim (§5.3). All framework/service/stack code above it is reusable unchanged.
- **Path B (plain Linux) — must reimplement the whole service layer, but with a complete blueprint.**
  Our C stack already covers the protocol half (`stack/ssap/` codec/server). The OHOS `service/` layer gives an exact spec for what we lack: adapter state machine (`SleServiceManager`/`SysStateMachine`), profile factory + lifecycle (`ProfileServiceManager`/`ClassCreator`), per-peer appId/pid/uid ownership and death cleanup (`SsapClientServerRemoteContainer`), callback fan-out with per-caller permission re-check, real/random address privacy (`NearlinkDeviceManager`), port data transfer via socketpair+fd handoff, and the `server/` thin-stub + permission-map pattern that we can emulate with a Unix socket + SCM_RIGHTS.
- Recommended: mirror the OHOS *object model and verbs* (host hub + named profiles, appId lifecycle, callback classes) on Linux rather than porting the binder/SA machinery (FRAMEWORK-LAYER §6.2/7). If a future Path A is desired, keeping the C API parameter conventions byte-compatible makes the later migration a transport swap.

---

## Open questions

1. Does HiSilicon ship an OHOS HDF nearlink driver for the WS73 USB part (`ffff:3733`), or only UART? The whole Path A cost collapses into this one answer. Not verifiable locally (`drivers_peripheral`/`drivers_interface` absent).
2. Audio-profile gating: `NearlinkSystemConfig::IsAudioSupported()` is product-config driven — what is the actual box config? This decides whether ASC/TWS/VCP/CDSM servers build.
3. Multi-user: the OHOS service is single-user-wide (`el1`, no userId logic). If the TV box needs per-profile/multi-user isolation, that is net-new in both paths.
4. Closed binaries: availability of `libnearlink_server_ext_plugin.z.so` and `libnearlink_measure.z.so` from the vendor affects plugin servers and ranging accuracy.
5. Data-plane fd handoff works via binder `WriteFileDescriptor` on OHOS (`nearlink_sle_datatransfer_callback_proxy.cpp:46`); on plain Linux the equivalent is SCM_RIGHTS over the socket — confirm our `/dev/hwsle` ACB path can serve the same port semantics (CUTC 0x1F).
6. DLI/HDI protocol drift: `min_hdi_proxy_version` pins v1.0; is there a v1.1-only feature the box needs (e.g. `CheckOnBoardState`)? Requires the HDI IDL checkout to answer.
