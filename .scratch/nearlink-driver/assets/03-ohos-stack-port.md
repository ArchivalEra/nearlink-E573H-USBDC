# 03 — OpenHarmony NearLink stack: Linux porting surface

> Feasibility basis for Phase 3 (user-space SLE protocol stack for the WS73 USB dongle).
> Source examined: `/mnt/hdd/nearlink-stuff/communication_nearlink_service` (OpenHarmony `communication_nearlink_service`, v7.0, commit `2026-08-15` clone).
> Date: 2026-08-15.

## Executive verdict

**The NearLink host stack is a self-contained, pure-C, POSIX-threaded blob that can be compiled into a Linux shared/static library with ~1-2 person-weeks of porting work.** The core (`services/stack`: cp + dp + dli + sdf + nai) has only 6 external OHOS dependencies, all of them trivially stubbable (`hilog`, `hisysevent`, `securec`, `libbegetutil`/parameter, `openssl` — already vendorable, and one `parameter.h` read). The HAL seam is a 5-function C ABI (`SleHalInit`/`SleSendDliPacket`/`SleReset`/`SleHalClose`/`GetDliVersion` + 2 callbacks) that already ships with a **test-local stub implementation** (`dli_data_stub.c` under `NEARLINK_SERVICE_STACK_LOCAL_TEST`) proving a function-pointer HAL replacement is the intended pattern. The HDI/IPC/SAMGR/SA layer (`services/server`, `services/ipc`, `frameworks/*`, `services/service` for the most part) is a *separate C++ façade* that can be dropped entirely for a library build. The C/C++ boundary sits exactly at the `SleDliLayerAdapter.h` `extern "C"` ABI.

The hard couplings that must be neutralized are few and mechanical:
1. `kill(getpid(), SIGKILL)` on chip-reset (4 sites) — must become an error/restart callback.
2. `/data/log/nearlink` snoop paths + OHOS parameter reads (`parameters.h`) — make path/param a config knob.
3. `ffrt` in `services/hardware/src/SleDliThreadUtil.cpp` (used only for the snoop thread) — replace with a `std::thread` or pthread.
4. `securec.h` (`memcpy_s`/`sprintf_s`/`memset_s`) — provide a tiny shim (only a handful of call sites in the stack).

Everything above the stack (SA IPC servers, NAPI, ETS/Taihe, service-manager, hibox, dft) is dropable for the "SLE host stack as a Linux library" deliverable.

---

## 1. bundle.json dependency classification

`bundle.json` lists 43 `components` deps. Classification by *where each is actually referenced* (verified via BUILD.gn `external_deps` + source includes), not by name:

### A. Stack-essential (must keep or stub for the core library build)

| Dep | Category | Approach |
|---|---|---|
| `hilog` | **pure-stub** | Stack logs funnel through `sdf_log_api.h` which, unless `NEARLINK_SERVICE_STACK_LOCAL_TEST`, calls `HILOG*`. Defining that macro switches the whole stack to a pluggable `SDF_LogHook` (default `printf`) — see `services/stack/src/sdf/dfx/log/include/sdf_log_api.h:22,121`. **Best lever in the whole repo.** |
| `hisysevent` | **pure-stub** | Only for DFT/telemetry. Used by `services/stack` deps (`nearlink_stack`, `nearlink_stack_sdf`) via `libhisysevent`. Not on the critical data path; stub as no-op. |
| `init` (`libbegetutil`) | **pure-stub** | `parameter_wrapper.c` (`PropertyGetInt32` → `GetIntParameter`), `services/hardware` (`libbegetutil`), SleDliSnoop `GetParameter`. Replace with a tiny key-value file/env-based parameter getter. |
| `bounds_checking_function` (`libsec_shared`) | **pure-stub** | `securec.h` — `memcpy_s`/`sprintf_s`/`memset_s`/`sprintf_s` used ~150× in stack sources. Provide a thin shim mapping to libc (verify src/dst sizes). |
| `openssl` (`libcrypto`) | **keepable** | `services/stack/src/nai/crypto/src/sle_crypto.c` already uses OpenSSL directly (ECDH/ECDSA/AES-CMAC/SHA256) — NOT huks. `nearlink_stack` and `nearlink_stack_static` both link `libcrypto_shared`. Vendor via system OpenSSL or bundled 1.1.1/3.x. |
| `c_utils` (`utils`) | **pure-stub / replace** | Used by `services/hardware` and `nearlink_stack_static`. Mostly string/number helpers; grep shows only a handful of headers referenced from the stack path. Replaceable by libc + a few shim functions. |
| `drivers_interface_nearlink` (`libnearlink_hci_proxy_1.0/1.1`) | **needs replacement** | The HDI proxy for the chip. Only referenced from `services/hardware/BUILD.gn`. **This is exactly what a Linux HAL (e.g. our USB backend) replaces.** See §3. |
| `hdf_core` (`libhdf_utils`) | **needs replacement** | HDI infrastructure (`hdi_objcast`, service manager lookup). Only in `services/hardware`. Drops with the HDI proxy. |
| `ipc` (`ipc_single`) | **needs replacement (drop)** | In `services/hardware` only for the HDI proxy's death-recipient machinery. Drops with HDI. |

### B. Service/framework layer (dropable for library build)

| Dep | Category | Approach |
|---|---|---|
| `samgr`, `safwk` | **drop (SA arch)** | `sa_profile/1190.json`, `services/server/nearlink_host_server.cpp` (`SystemAbility::MakeAndRegisterAbility` at line 561), `nearlink_service.cfg`. Whole SA/IPC dispatch layer strips out. |
| `ffrt` | **drop / replace** | C++ concurrency only — `services/hardware/SleDliThreadUtil.cpp`, `utils/src/nearlink_extension_manager.cpp`, `utils/src/nearlink_plugin_loader.cpp`, `services/service`. Replace with `std::thread`. |
| `ability_base`, `ability_runtime`, `access_token`, `common_event_service`, `data_share`, `preferences`, `time_service`, `os_account`, `qos_manager`, `bundle_framework`, `resource_schedule_service`, `security_guard`, `hiappevent` | **drop** | Only in `services/service`, `services/common`, `services/dft`, `services/server`, `frameworks/native` — the SA/UI/permission layer. |
| `huks` | **needs replacement → but only in C++ layer** | `services/device_manager/src/SleHuksTool.cpp`, `services/service/src/adapter/SleSecurity.cpp`, `SleRemoteDeviceAdapter.cpp`, `ASCService.cpp`. The **stack itself does NOT use huks** — it uses OpenSSL. For a stack-only library, huks is irrelevant; if pairing key storage is later needed, swap for own file-based keystore or libsodium. |
| `cJSON`, `libxml2`, `json` | **drop (service-only)** | `SleKiaManager.cpp`, `DialogPairing.cpp`, `SleEdmManager.cpp`, `xml_parse.cpp` (device_manager). Not needed by stack. |
| `device_manager` (OHOS), `input`, `audio_framework`, `av_session`, `call_manager`, `core_service`, `state_registry`, `power_manager` | **drop** | Weakly-coupled, `nearlink_feature.gni` gates them (`nearlink_deps_audio_exist`, `nearlink_deps_telephony_exist`, …). All `false` by default. |
| `napi`, `node`, `libuv`, `runtime_core`, `taihe_ffi_gen`, `resource_management` | **drop** | JS/ETS/ArkTS façade only (`frameworks/js/napi/src/*`, `frameworks/ets/taihe/*`). libuv appears only as `libuv:uv` in napi BUILD.gn files. |
| `hicollie` | **drop** | Only `services/ipc/src/nearlink_hicollie_adapter.cpp` + `#ifdef HICOLLIE_ENABLE` watchdog. |

> Note on the "~50 deps" in the ticket: `bundle.json` actually lists **43** components (45 with `hisysevent_ue.yaml`-adjacent ones) — the 3rd-party list is empty. Also note `os_account` appears twice in the list (harmless dup).

---

## 2. Build system re-targeting

### 2.1 Every `ohos_*` template in use (grep across all BUILD.gn)

```
ohos_unittest(       101×  test/unittest/**/BUILD.gn
ohos_fuzztest(        97×  test/fuzztest/**/BUILD.gn
ohos_shared_library(  24×  services + frameworks + socket + utils + ipc_parcel
ohos_static_library(   6×  nearlink_stack_static, nearlink_service_impl_static, hibox? (test-only)
ohos_prebuilt_etc(     5×  services/etc/init, services/etc/param, sa_profile
ohos_taihe(            4×  frameworks/ets/taihe/**
taihe_shared_library(  4×  frameworks/ets/taihe/**
ohos_shared_headers(   2×  interfaces, frameworks/native (ranging_alogorithm_headers)
ohos_prebuilt_para(    2×  services/etc/param
ohos_sa_profile(       1×  sa_profile
ohos_cli_executable(   2×  tools/ohos-nearlinkControl
ohos_source_set(       1×  services/ipc:nl_service_ipc
ohos_var(             52×  (declare_args plumbing, not targets)
```

### 2.2 What a Linux build needs

- **Keep the file lists, drop the templates.** The valuable asset is the source lists in each BUILD.gn (`nearlink_stack_sources`, `nearlink_base_sources`, `nearlink_sdf_sources`, `nearlink_service_sources`, …). A port just needs a plain GN `static_library`/`shared_library` or a CMake `add_library` with those exact `.c`/`.cpp` lists.
- **`config("..._config")` blocks already carry the full `include_dirs`** (e.g. `services/stack/BUILD.gn:18-91` has ~70 include paths). Port these verbatim into CMake `target_include_directories`.
- **Compile flags to carry over** (from `nearlink_stack`): `-D_POSIX_C_SOURCE=200112L`, `-D_GNU_SOURCE`, `-Os`, `-fdata-sections`, `-ffunction-sections`, `-fno-exceptions`, `-fno-rtti`, `-flto` + `-Wl,--gc-sections`. The `branch_protector_ret`/`sanitize` CFI/ubsan blocks are OHOS-specific and **droppable**.
- **Droppable build artifacts:** `ohos_sa_profile`, `ohos_prebuilt_etc/para` (init cfg + parameter files), `ohos_taihe`/`taihe_shared_library` (ETS), all `ohos_unittest`/`ohos_fuzztest`.
- **`version_script` files** (`libnearlink_stack_base.versionscript`, `libnearlink_mcp_service.versionscript`) control exported symbols — reusable as-is on Linux for a versioned `.so`.

**Re-targeting cost: SMALL for stack libs** (SDF/DLI/stack/hardware): ~pure GN→CMake file-list transplant, 0.5-1 day. **MEDIUM for the full stack incl. service layer** (still mostly drop the SA glue). **LARGE if you insist on reusing the SA/IPC C++ façade** — not recommended.

---

## 3. The HAL seam

### 3.1 The 5-function C ABI

`services/hardware/include/SleDliLayerAdapter.h` (extern "C"):

```c
int  SleHalInit(SleDliCallbackFunc *callbacks);   // callbacks struct = initializationComplete + dliPacketReceived
void SleReset();
int  SleSendDliPacket(const SlePacket *packet);   // SlePacket {uint8_t *data; uint32_t size;}
void SleHalClose(void);
int  GetDliVersion(void);                          // DLI_VERSION_1_0 / DLI_VERSION_1_1
```

Callbacks (`SleDliCallbackFunc`, same header): `void (*initializationComplete)(SleInitStatus)` and `void (*dliPacketReceived)(SlePacketType, const SlePacket*)`.

Consumers of the ABI (all in the pure-C stack):
- `services/stack/src/dli/sapi/src/dli_sapi.c` — `DLI_SapiInit` calls `SleHalInit` then blocks on an SDF semaphore for the `initializationComplete` callback (3 s timeout → `SleReset()`). `DLI_SapiSend` → `SleSendDliPacket`. `DLI_SapiDeinit` → `SleHalClose`. `DLI_GetDliVersion` → `GetDliVersion`.
- `services/stack/src/dli/layer/src/dli_layer.c` — `DLI_PacketReceived` is the ultimate sink for `dliPacketReceived`; parses DLI 5-byte header (type/handle/pbFlag/length), fragments/defragments ACB, routes events to the DLI thread and ACB data to the CP thread.

### 3.2 The HDI proxy → function-pointer table swap

`services/hardware/src/SleDliLayerAdapter.cpp` is a **66-line seam** (plus callbacks/snoop/thread files). The OHOS-specific parts are:
- `g_iSleDli = ISleHciInterface::Get();` (`OHOS::HDI::Nearlink::Hci::V1_0`) — service-locator lookup.
- `g_iSleDli->SleHalInit(callbacks)`, `g_iSleDli->SleSendHciPacket(data)`, `g_iSleDli->Close()` — HDI RPC calls.
- `hdi_objcast` + `AddDeathRecipient` (line 58, 65) — IPC death notification, does `kill(getpid(), SIGKILL)` on remote death (line 44).
- `V1_1::ISleHciInterface::CastFrom` for `GetDliVersion`.

**The replacement is exactly a function-pointer table.** The struct is already half-sketched in the header:

```c
typedef int (*SleHalInitFunc)(SleDliCallbackFunc *callbacks);
typedef int (*SleSendDliPacketFunc)(const SlePacket *packet);
typedef void (*SleHalCloseFunc)(void);
```

Pattern: delete the .cpp, provide a new `SleDliLayerAdapter.c` (or keep the 5 C names) whose impl calls a registered backend struct, e.g.:

```c
typedef struct {
    SleHalInitFunc        hal_init;
    SleSendDliPacketFunc  hal_send;   // -> write(usb_fd, ...)
    void                  (*hal_close)(void);
    int                   (*hal_get_version)(void);
} SleDliHalBackend;                    // + rx pump thread calling dliPacketReceived
```

**The repo already proves this is the intended pattern**: `services/stack/src/dli/sapi/src/dli_data_stub.c` is a **complete in-tree fake HAL** compiled under `NEARLINK_SERVICE_STACK_LOCAL_TEST` (it implements `SleHalInit`/`SleSendDliPacket`/`SleReset`/`SleHalClose`/`GetDliVersion` with a worker thread + `SleSendToDliStub` injector for tests). A Linux backend is a strict upgrade: replace the stub's `ProcessTask` with a real USB-packet pump.

### 3.3 RX direction / concurrency notes

- `SleDliCallbacks::hciPacketReceived` (`services/hardware/src/SleDliCallbacks.cpp:53`) calls back into `dliPacketReceived` and promotes the thread to `SCHED_FIFO` (priority 1) via `sched_setscheduler` — **plain POSIX, keepable**.
- `SleDliSnoop.cpp` writes PCAP-like hex logs to `/data/log/nearlink/`; gated on a `const.logsystem.versiontype` parameter. For Linux: redirect to `~/.cache/nearlink/` or disable. (Optional but easy.)
- `SleDliThreadUtil.cpp` uses **ffrt** for the snoop thread — the only ffrt dependency on the data path; replace with `std::thread` (≈30 lines).

---

## 4. Process model & the SA/IPC layer

### 4.1 The full OHOS deployment (what we are NOT porting)

```
sa_main (process "nearlink_service")
 ├─ /system/profile/nearlink_service.json  (sa_profile/1190.json)  → SAMGR loads libnearlink_server.z.so
 ├─ services/server/nearlink_host_server.cpp  (SystemAbility 1190)
 │    └─ createServers(): instantiates 8-15 Nearlink*Server (advertiser, central mgr, ssap c/s,
 │        hadm, hid, datatransfer, controller, + audio/plugin) → each an IPCObjectStub
 ├─ services/service/nearlink_service_impl.z.so  (C++ profile managers: SleInterfaceManager,
 │    SleAdapter, ScanService, ssap services, …)
 ├─ services/stack/nearlink_stack.z.so       (pure C: cp/dp/dli/sdf/nai — THE STACK)
 ├─ services/hardware/nearlink_dli_adapter.z.so  (HDI proxy → nearlink HCI driver, kernel side)
 ├─ services/device_manager (C++: xml config, huks key storage, DM)
 ├─ services/dft (hisysevent/UE reporting), services/hibox, services/ipc (stubs)
 └─ frameworks/native + frameworks/js/napi + frameworks/ets/taihe  (client libs, NAPI bindings)
```

Init wiring: `services/etc/init/nearlink_service.cfg` (`sa_main` + `nearlink_service.json` args), `services/etc/param/nearlink.para` + `.para.dac` (system parameters like `persist.nearlink.switch_enable`), `sa_profile/1190.json` (`libpath`, `run-on-create:false`, `start-on-demand` on `const.nearlink.enable=true`).

### 4.2 Can the SA/IPC layer be stripped to make the stack a library? — YES

- **Yes for the stack itself**, trivially: `services/stack` (cp/dp/dli/sdf/nai) + `services/hardware` is C + a C++ shim with a C ABI. The stack has *zero* references to IPC/SA/parcel — it talks to the chip only through the 5 HAL functions.
- **Yes for the profile layer, with effort**: `services/service/src/*` (C++) implements the SLE profile API (scan/advertise/connect/ssap/etc.) by calling the stack via `services/stack/src/adapter/src/*_ext_func_wrapper.c` (dlsym-loaded extension hooks) and `SleAdapterWrapper`. It drags in access_token, samgr, ability, common_event, etc. **For Phase 3 (dongle point-to-point), the profiles live inside the stack itself** (cp/bsl/sle/{cm,devd,ssap,sm,hadm,qosm} + dp/dtap + nai/slem), so `services/service` is *not needed* to get a working link. If you want a C++ convenience layer later, port only `SleInterfaceManager`-style logic without the IPC.

### 4.3 C/C++ boundary

- **Below the boundary (C, linkable as C lib):** `services/stack/src/**` — pure C (`.c`, extern "C" guards throughout). Entry point: `NLSTK_Init` in `services/stack/src/nai/nlm/src/nlstk_init.c` (sets up CP worker, DLI thread, SDF OAL; wires the whole stack).
- **The boundary is exactly `SleDliLayerAdapter.h`** (C ABI, `extern "C"`): the hardware shim (`services/hardware`) is C++ but exposes C symbols; the stack consumes them as C.
- **Above the boundary (C++, dropable):** `services/service`, `services/server`, `services/ipc`, `services/common`, `services/device_manager`, `services/dft`, `services/hibox`, `frameworks/*`, `socket/`, `ipc_parcel/`, `utils/` (partially — `nearlink_utils` has a few C++-only helpers).

---

## 5. Hard couplings (must neutralize)

1. **`kill(getpid(), SIGKILL)` on chip reset — 4 sites:**
   - `services/hardware/src/SleDliLayerAdapter.cpp:44` (HDI death recipient), `:125` (`SleReset`).
   - `services/stack/src/dli/layer/src/dli_layer.c:631` — `DLI_PacketReceived` sees the 5-byte reset event `{0xff,0xff,0x01,0x00,0xc7}` → `DLI_DftReportKill(DLI_CHIP_KILL); kill(getpid(), SIGKILL);`.
   - `services/service/src/common/SleServiceManager.cpp:863` (service-layer reset).
   - **Porting action:** in `dli_layer.c` and `SleReset`, replace the kill with (a) a registered reset/error callback (or returning an error and triggering a re-init), and (b) for the dongle, perform a USB reset / firmware re-handshake instead of process death. This is a genuine design change, not just a stub.
2. **`/data/log/nearlink` + `/data/service/el1/public/nearlink` paths** (`SleDliSnoop.cpp:40`, `nearlink_hadm_client_service.cpp:44`, `SleEdmManager.cpp:33`) — make base path a compile-time/macro/config knob.
3. **OHOS parameter reads** — `parameters.h` (`OHOS::system::GetParameter/SetParameter`) in `SleDliSnoop.cpp`, `parameter_wrapper.c` (`parameter.h`), and throughout `services/service`. Replace with a `getenv`/ini shim; `nearlink.para` defaults are a useful spec of the keys (`persist.nearlink.switch_enable=1`, `persist.nearlink.reload_sa=0`, …).
4. **SELinux / permissions** — `nearlink_service.cfg` carries `secon: u:r:nearlink_service:s0`, uid/gid `nearlink`, caps `CAP_NET_ADMIN CAP_SYS_NICE CAP_WAKE_ALARM CAP_BLOCK_SUSPEND`, and 25 `ohos.permission.*` grants. On Linux: run as a user daemon; `CAP_SYS_NICE` is only for the `SCHED_FIFO` promotion in `SleDliCallbacks` (can drop or keep via `cap_sys_nice` if root).
5. **`securec.h`** — `memcpy_s/strcpy_s/sprintf_s/memset_s` used throughout stack (`150×` in `services/stack/src`). Provide a shim header (`#define memcpy_s(d,dn,s,n) memcpy(d,s,n)` style, with size checks) — mechanical.
6. **`ffrt`** — only `SleDliThreadUtil.cpp` on the data path + C++ service layer; replace with pthread/`std::thread`.
7. **`hiszevent` DFT** — `DLI_DftReportKill`, `nai_dft`, `sdf_trace` — compile out / no-op stubs.

---

## 6. Minimal compilable subset for "SLE host stack as a Linux library"

### 6.1 Essential (must port)

```
services/stack/src/sdf/**          # OAL: threads/epoll/eventfd/timerfd/locks/sem, DSL (list/map/vector), STM
services/stack/src/dli/**          # DLI layer: cmd, event, thread, layer, sapi (interface/headers + impl)
services/stack/src/nai/**          # nai: nlm (NLSTK_Init), slem (sle_dli_layer.c), crypto (OpenSSL), naifwk
services/stack/src/cp/**           # CP: bsl/sle/{cm, devd, ssap, sm, hadm, qosm}, nlstkfwk (schedule/cfgdb),
                                   #      bnl/proxy, bal (profiles: dis/icce/port/bas/hid; audioctl optional)
services/stack/src/dp/**           # DP: dtap (transport), dpfwk
services/stack/src/utils/**        # time_utils, parameter_wrapper (→ stub), crc16
services/stack/src/adapter/**      # ext_func_wrapper/reg (extension hooks; optional but cheap)
services/stack/include/**          # ext/* (nlstk_*_ext.h type defs)
services/hardware/**               # rewritten: keep SleDliLayerAdapter.h ABI + SleDliCallbacks logic,
                                   #   drop HDI/ffrt/snoop → new Linux backend (USB)
```

Notes on `cp` trimming: profiles under `cp/bal/audioctl/*` (actm/cctl/mctl/micp) and `cp/bal/profile/{bas,hid,icce,port,dis}` are optional — they are *applications* over the core CM/SSAP/SM; the core link path is `cp/bsl/sle/{cm,devd,servm/ssap,sm,hadm,qosm}` + `cp/nlstkfwk/schedule` + `cp/bnl/proxy` + `cp/bal/audioctl/cdsm`. Minimal viable link = cm + devd + ssap + sm + dp/dtap + dli + sdf + nai.

### 6.2 Optional / droppable

```
services/service/**               # C++ profile managers — DROP (or later port subset w/o IPC)
services/server/**                # SA server entry — DROP
services/ipc/**                   # IPC stubs — DROP
services/common/**                # C++ helpers w/ ability/ffrt — DROP
services/device_manager/**        # huks/xml config — DROP (port config load separately)
services/dft/**                   # DFX/UE reporting — DROP or no-op stub
services/hibox/**                 # DROP
services/etc/**                   # init cfg + para — DROP (use as spec only)
sa_profile/**                     # DROP
socket/**, ipc_parcel/**          # client-side IPC helpers — DROP for stack-only; needed only for C++ framework
frameworks/**                     # native + js/napi + ets/taihe — DROP (Phase 4 maybe native)
utils/**, interfaces/**           # keep only: log.h shim, nearlink_utils.h subset; def/*.h as reference
tools/**, test/**                 # DROP (but test/unittest/stack_test is a goldmine of stubs/patterns)
```

### 6.3 Target build shape

```
libnearlink_stack.so  (or .a)
  ├─ sdf, dli, nai, dp, cp (as above)
  ├─ nearlink_utils shim + securec shim + parameter shim + log shim (hilog→printf/SDF_LogHook)
  └─ hal backend: hal_linux_usb.c  →  { SleHalInit, SleSendDliPacket, SleHalClose, GetDliVersion, SleReset }
       └─ USB RX thread → dliPacketReceived
Runtime entry: NLSTK_Init() → DLI_LayerEnable()/DLI_SapiInit() … (see nlstk_init.c / slem.c)
```

### 6.4 Estimated LOC moved

| Dir | LOC |
|---|---|
| cp | ~58,100 |
| dp | ~4,800 |
| dli | ~4,900 |
| nai | ~1,650 |
| sdf | ~2,800 |
| utils + adapter | ~1,200 |
| hardware (rewrite) | ~600 (new) |
| **Stack total** | **~73,500 C + shims** |

---

## 7. Porting cost assessment

| Layer | Cost | What |
|---|---|---|
| GN→CMake for stack libs (sdf/dli/stack/hardware) | **Small** (~1-2 d) | Transplant source lists + include dirs; drop ohos templates; shims (hilog/securec/parameter/hisysevent) |
| HAL backend for WS73 USB dongle | **Small-Medium** (~1-2 wk) | New `SleDliLayerAdapter` impl over the USB bulk pipe (or HCI-over-HCC per ticket 01); reuse `dli_data_stub.c` as template |
| Kill/reset semantics rework | **Medium** (~1-2 d + testing) | `dli_layer.c` chip-reset path must not SIGKILL the host; needs restart/UE-callback design |
| C++ profile layer (`services/service`) | **Large / optional** | Not needed for Phase 3; defer |
| NAPI/ETS/Taihe UI binding | **Not needed** | Skip |

**Feasibility verdict: GREEN.** The protocol stack (cp/dp/dli/sdf/nai, ~73 KLOC C) is cleanly separable, purely POSIX, with an already-specified HAL ABI and an existing in-tree fake HAL to model the Linux backend on. The dominant risk is not OHOS deps (all mechanical stubs) but the `kill-on-reset` behavior and the correctness of the HCI-over-USB packet framing (ticket 01 territory). A working "library + USB backend" is a realistic 2-4 week effort for one engineer.

---

## Key file citations

- Deps: `bundle.json` (`component.deps.components`, 43 entries)
- Build templates: `services/stack/BUILD.gn:377` (`nearlink_stack`), `services/stack/src/dli/BUILD.gn:55`, `services/stack/src/sdf/BUILD.gn:78`, `services/hardware/BUILD.gn:22`, `services/service/BUILD.gn:393`, `services/server/BUILD.gn`, `sa_profile/BUILD.gn`
- HAL ABI: `services/hardware/include/SleDliLayerAdapter.h`, `services/hardware/src/SleDliLayerAdapter.cpp`, `SleDliCallbacks.cpp`, `SleDliSnoop.cpp`, `SleDliThreadUtil.cpp`
- HAL consumer: `services/stack/src/dli/sapi/src/dli_sapi.c`, `services/stack/src/dli/layer/src/dli_layer.c`
- Fake HAL (test pattern): `services/stack/src/dli/sapi/src/dli_data_stub.c` (`NEARLINK_SERVICE_STACK_LOCAL_TEST`)
- Stack entry: `services/stack/src/nai/nlm/src/nlstk_init.c`
- Stack crypto (OpenSSL): `services/stack/src/nai/crypto/src/sle_crypto.c`
- Stack logging hook: `services/stack/src/sdf/dfx/log/include/sdf_log_api.h` (`SDF_LogHook`, `g_fnLogOut`, default `printf`)
- SA/IPC: `services/server/src/nearlink_host_server.cpp:561` (SystemAbility register), `sa_profile/1190.json`, `services/etc/init/nearlink_service.cfg`, `services/etc/param/nearlink.para{,dac}`
- Kill sites: `dli_layer.c:631`, `SleDliLayerAdapter.cpp:44,125`, `services/service/src/common/SleServiceManager.cpp:863`
- Weak-coupling feature gates: `nearlink_feature.gni`
