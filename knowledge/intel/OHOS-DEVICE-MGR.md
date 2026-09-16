---
type: intel
title: "OHOS NearLink Device Manager -- Interface & Data Structure Analysis"
language: en
created: 2026-08-17
tags: [intel, ohos, nearlink, device]
sources:
  - "/mnt/hdd/nearlink-stuff/communication_nearlink_service"
trust: B
stale_after: 2027-02-17
---

# OHOS NearLink Device Manager -- Interface & Data Structure Analysis

**Date:** 2026-08-17
**Analyst:** ZCode research sub-agent (ArchivalEra)
**Status:** Research notes -- read-only analysis

---

## Sources

All paths under:
`/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/device_manager/`

| File | Role |
|------|------|
| `include/nearlink_device_manager.h` | Real/Random MAC address mapping singleton |
| `src/nearlink_device_manager.cpp` | MAC randomization, retention, timer-based cleanup |
| `include/SleRemoteDeviceManager.h` | Per-device runtime state (pair, connect, encrypt, ability) |
| `src/SleRemoteDeviceManager.cpp` | Full implementation of all 60+ per-device accessors |
| `include/SleConfig.h` | Persistent config facade (XML-backed get/set for every device field) |
| `src/SleConfig.cpp` | 1600-line implementation of config persistence layer |
| `include/SleHuksTool.h` | Link key encrypt/decrypt via HKS (SM2/RSA) |
| `src/SleHuksTool.cpp` | HKS-based key management (SM2 256-bit, OAEP padding) |
| `include/IManufacturerAbilityManager.h` | Abstract ability bitmap interface |
| `include/ManufacturerAbilityLoader.h` | Dynamic-library loader for vendor ability module |
| `src/ManufacturerAbilityLoader.cpp` | `.so` loading with 5-min delayed unload |
| `include/SleCoexistManager.h` | Multi-connection coexistence parameter manager |
| `src/SleCoexistManager.cpp` | Per-LCID connection parameter tracking |
| `include/SleCoexistData.h` | `CoexistConnInfo` data structure |
| `include/AdapterDeviceConfig.h` | Abstract XML config backend interface |
| `src/AdapterDeviceConfig.cpp` | XML file I/O (libxml2), file: `sle_device_config.xml` |
| `include/xml_parse.h` | Low-level XML DOM parser (libxml2 wrapper) |
| `src/xml_parse.cpp` | Section/subSection/property hierarchy over XML |
| `BUILD.gn` | Build target + dependencies |

**Supporting types:**
- `utils/include/sle_service_data.h` -- `SlePeripheralDevice` class (full runtime device model)
- `interfaces/inner_api/include/nearlink_device_model.h` -- `DeviceModel` (modelId/subModelId/iconId/devType)
- `interfaces/def/nearlink_def.h` -- `SlePairState`, `DeviceClassForService`, `SlePairDirect`
- `interfaces/def/nearlink_def_types.h` -- `SleConnState`, `SleConnDirect`, `SLE_ADDR_TYPE`, `SLE_MANU_ABILITY_LEN`

---

## 1. Device Management Interfaces

The device manager layer consists of **three cooperating singletons** plus a config persistence layer.

### 1.1 NearlinkDeviceManager -- MAC Address Randomization

**File:** `include/nearlink_device_manager.h:34`

```
NearlinkDeviceManager::GetInstance()   // singleton
  AddDeviceInfo(realAddr, randomAddr, isRetention)
  GetDeviceRandomAddr(realAddr, randomAddr)      -> int32_t (RET_SUCCESS / RET_NO_EXIST)
  GetDeviceRealAddr(addrToReal, realAddr)         // 3 overloads
  ConvertToRandomAddress(realAddr, randomAddr, isRetention)
  UpdateRandomAddressMap(device, status)          // on pair/unpair events
  RecoverRetainedDeviceInfo()                     // startup recovery
  ClearDevicesInfo()
  ScheduleCleanDeviceInfo()                       // timer callback
```

**Data structure** (`nearlink_device_manager.h:36-48`):
```cpp
struct NearlinkDeviceInfo {
    std::string realMacAddr;      // real device MAC
    std::string randomMacAddr;    // privacy-randomized MAC
    int64_t updateTime;           // millisecond timestamp
    bool isRetention;             // true = paired device, kept permanently
};
```

Storage: `NearlinkSafeHashMap<std::string, NearlinkDeviceInfo>` keyed by real MAC address.

**Lifecycle:**
- Non-retained entries are cleaned after **10 minutes** (`TIME_MINUTES_10 = 600000`, line 32) via a periodic timer (`timerMs_ = 60000`, line 72).
- Paired devices are "retained" -- their random address is persisted to XML and never expires.
- On startup, `RecoverRetainedDeviceInfo()` loads all paired devices from XML and re-registers them.

### 1.2 SleRemoteDeviceManager -- Runtime Device State

**File:** `include/SleRemoteDeviceManager.h:31`

This is the **central device registry** for all known remote devices. It maintains a thread-safe map:

```cpp
NearlinkSafeMap<std::string, std::shared_ptr<SlePeripheralDevice>> peerConnDeviceSafeList_;
```

**Key API categories:**

**Device lifecycle (add/remove/query):**
- `AddPeripheralDevice(address, peerDevice)` -- line 35
- `RemovePeripheralDevice(address)` -- line 36
- `RemoveAllPeripheralDevices()` -- line 37
- `GetRemoteDevice(device)` -- line 38, returns deep copy via `make_shared`

**Device listing:**
- `GetPairedDevices()` -- line 40, filters by `SLE_PAIR_PAIRED`
- `GetConnectedDevices()` -- line 41
- `GetConnectingDevices()` -- line 42
- `GetAcbConnectedDevices()` -- line 43, filters CONNECTED or ENCRYPTED
- `GetDirectConnDevices()` -- line 98, devices eligible for direct reconnection
- `GetConnectedCnt()` -- line 78

**Device identity:**
- `GetDeviceName(device)` / `SetDeviceName(device, name)` -- lines 44-46
- `GetAliasName(device)` / `SetAliasName(device, name)` -- lines 46
- `SetAppearance(device, appearance)` / `GetDeviceAppearance(device)` -- lines 48-49
- `GetDeviceUuids(device)` -- line 50

**Pairing state machine:**
- `GetPairState(device)` -- line 63, returns `SlePairState` enum
- `SetPairStatus(device, pairStatus)` -- line 64
- `SetPrePairStatus(device, pairStatus)` -- line 65
- `IsBondedFromLocal(device)` -- line 51
- `SetPairDirection(device, pairDirection)` -- line 90 (ACTIVE=0x01 / PASSIVE=0x02)

**Connection state:**
- `GetAcbState(address)` / `SetAcbState(address, state)` -- lines 57, 59
- `IsAcbConnected(device)` / `IsAcbEncrypted(device)` -- lines 52-53
- `SetConnectionInfo(device, lcid, role, addrType)` -- line 100
- `SetLcid(address, lcid)` -- line 62
- `GetLcidByAddress(device)` / `GetAddressByLcid(lcid)` -- lines 55-56

**Crypto & pairing algorithms:**
- `GetPairAlgoInfo(addr, cryptoAlgo, keyDerivAlgo, integrChkInd)` -- line 68
- `SetPairAlgoInfo(addr, ...)` -- line 69
- `GetGroupAndGiv(addr, encryptGroupKeyStr, giv)` -- line 72
- `SetGroupAndGiv(addr, encryptGroupKeyStr, giv)` -- line 73

**BT/SLE dual-address mapping:**
- `SetBtAddrBySleAddr(sleAddr, btAddr)` -- line 80
- `GetBtAddrBySleAddr(sleAddr, btAddr)` -- line 81
- `GetSleAddrByBtAddr(btAddr, sleAddr)` -- line 82

**Device model (HiLink):**
- `SaveDeviceModelInfo(address, model, newModelId)` -- line 85
- `GetDeviceModelInfo(device, model, newModelId)` -- line 86
- `SaveDeviceModelInfoToConf(device, value)` -- line 102
- `SavePeerDeviceInfoToConf()` -- line 103

**Manufacturer ability:**
- `SetManufacturerAbility(device, manufacturerAbility)` -- line 87
- `GetManufacturerAbility(device)` -- line 88

**Persistence:**
- `SavePeerDeviceInfoToConf()` -- line 103, iterates all paired devices and writes to XML

### 1.3 SleConfig -- Persistent Configuration

**File:** `include/SleConfig.h:31`

XML-backed configuration store. File: `/data/service/el1/public/nearlink/sle_device_config.xml` (`AdapterDeviceConfig.cpp:28-29`).

**XML structure** (from `SleConfig.cpp` constants, lines 28-92):

```
Root
  +-- Host
  |     Address, DeviceName, CreateConfigFile, IOCapability, LocalAddrType
  +-- Sle Paired Device List
  |     +-- <MAC_ADDRESS> (subSection per device)
  |           PeerLk, CryptoAlgo, KeyAlgo, IntegrChk, GroupKey, Giv,
  |           RandomAddr, PeerAddrType, PeerAppearance, PeerName, AliasName,
  |           PairDirect, MusicVolume, CallVolume, BtAddress,
  |           CdsmAddressType, IsAudioDevice, ModelId, NewModelId,
  |           SubModelId, IconId, DevType, SleBussinessType,
  |           ManufacturerAbility, SleConnectControl, UserDisconnected,
  |           WearDetectionState, AutoConnectSwitch
  +-- Sle Cooperation Device List
  |     +-- <REPORT_ADDRESS>
  |           CdsmMemberList (semicolon-delimited), IsPrivateDevice
  +-- Sle Cloud Paired Cooperation Device List
  |     +-- <ADDRESS>
  |           BluetoothAddr, DeviceName, Token, ReportAddr,
  |           MembersAddrList, Model, SubModelId, IconId, CloudPairState
  +-- Last ASC Active Device
  +-- Last ASC Connected Device
  +-- Sle Background Reconnection Queue
```

**Key persistence APIs** (per device, `SleConfig.h`):

Per-device properties (section = "Sle Paired Device List", subSection = device address):
- Link key: `GetLinkKey(subSection)` / `SetLinkKey(section, lk)` -- lines 53-54, 94
- Crypto algo: `GetCryptoAlgo(subSection)` / `SetCryptoAlgo(section, algo)` -- lines 55-56, 95
- Key deriv algo: `GetKeyDerivAlgo(subSection)` / `SetKeyDerivAlgo(section, algo)` -- lines 57, 96
- Integrity check: `GetIntegrChk(subSection)` / `SetIntegrChk(section, check)` -- lines 58, 97
- Group key + GIV: `GetGroupKey(subSection)`, `GetGiv(subSection)` -- lines 59-60
- Random address: `GetPeerRandomAddress(subSection)` / `SetPeerRandomAddress(subSection, addr)` -- lines 67-68
- Paired device list: `GetPairedAddrList()` -- line 101
- Remove: `RemovePairedDevice(subSection)` / `RemoveAllPairedDevices()` -- lines 78-79

**Link key encryption at rest** (`SleConfig.cpp:129-181`):
- On config load, all unencrypted 16-byte link keys are encrypted via HKS SM2 (OAEP) to 128-byte blobs.
- A `CreateConfigFile` flag prevents re-encryption on subsequent boots.

### 1.4 SleHuksTool -- Key Encryption

**File:** `include/SleHuksTool.h:40`

Uses **HKS (Huawei Key Store)** with SM2 256-bit algorithm (Chinese national standard):
- Key alias: `"NearlinkKeyAlias"` (`SleHuksTool.cpp:25`)
- Algorithm: `HKS_ALG_SM2`, `HKS_SM2_KEY_SIZE_256`
- Padding: `HKS_PADDING_OAEP`, Digest: `HKS_DIGEST_SM3`
- Link key: 16 bytes -> 128 bytes encrypted (`LinkKey` -> `EncryptedLinkKey`)
- Cloud device token: 32 bytes -> 160 bytes encrypted (`CloudDeviceToken` -> `EncryptedCloudDeviceToken`)

### 1.5 SleCoexistManager -- Multi-Connection Parameters

**File:** `include/SleCoexistManager.h:32`

Tracks per-connection coexistence parameters for concurrent multi-device operation.

**Data structure** (`SleCoexistData.h:31-43`):
```cpp
struct CoexistConnInfo {
    uint16_t lcid;        // link channel ID
    uint16_t interval;    // link scheduling interval, in slots
    uint16_t latency;     // latency period, in event group cycles
    uint16_t timeout;     // timeout, in 10ms units
    SLE_Addr_S addr;      // device address
};
```

API:
- `UpdateConnectionInfo(info)` -- register/update connection
- `OnConnectionRemoved(lcid)` -- cleanup on disconnect
- `GetConnectionParam(addr, timeout, maxLatency)` -- query per-device timing
- `HasMultipleConnections()` -- true when >1 active connection
- `IterateConnInfo(callback)` -- enumerate all connections

### 1.6 ManufacturerAbilityLoader -- Dynamic Ability System

**File:** `include/ManufacturerAbilityLoader.h:33`

Loads vendor-specific capability bitmaps from a dynamically-loaded shared library:
- Library: `libnearlink_manu_ability.z.so`
- Create/Destroy functions: `CreateManufacturerAbilityManager` / `DestroyManufacturerAbilityManager`
- Delayed unload: 5 minutes (`DEFAULT_UNLOAD_TIMER_MS = 300000`, line 39)
- Ability bitmap: 16 bytes (`SLE_MANU_ABILITY_LEN = 16`)
- Named abilities include: `FENCE_5G`, `ADAPTIVE_SWITCH_5G`, `QOSID_CONFIG`, `ASC_START_PLAYING_MERGE`, `DUAL_EAR_HIGH_QUALITY_RECORDING`, `DUAL_EAR_KARAOKE`, `VOICE_CALL_AUTORATE`, `VOICE_CALL_FRAME_FOUR` (`IManufacturerAbilityManager.h:33-40`)

---

## 2. Stack Integration

### 2.1 How device_manager calls the stack

The device_manager itself does **not** call the SLE stack directly. Instead, it serves as a **data and state store** consumed by the service/adapter layer:

- `NearlinkHostServer` (the service daemon) calls `NearlinkDeviceManager::GetInstance()` extensively for address translation (`nearlink_host_server.cpp:405-607`).
- On pairing status changes, the server calls `UpdateRandomAddressMap(device, status)` which triggers retain/unretain of random addresses.
- The server calls `ManufacturerAbilityLoader::GetInstance().Load()` at init (`nearlink_host_server.cpp:719`).

The **SleRemoteDeviceAdapter** and **SleAdapter** (in `services/service/src/adapter/`) consume `SleRemoteDeviceManager` for:
- Connection count queries (`GetConnectedCnt`)
- Per-device state reads/writes
- Pairing flow orchestration

The stack integration boundary is:
```
Framework (deviceManager IPC)
    |
NearlinkHostServer (services/server/)
    |                    |
SleRemoteDeviceAdapter  NearlinkDeviceManager (address randomization)
    |
SleAdapter (services/service/)
    |                    |
Nearlink stack (HDI)    SleRemoteDeviceManager + SleConfig (persistence)
    |                    |
    +--> stack pairing/encryption layer (via SLE UAPI)
```

### 2.2 How framework consumes it

The framework's `deviceManager` communicates via IPC (OHOS Binder):
- `NearlinkHostServer` exposes standard pairing/unpairing/query APIs.
- The server uses `NearlinkDeviceManager::GetDeviceRealAddr()` on **every IPC call** to translate random addresses back to real addresses (permission-gated via `NearLinkPermissionManager::IsUseRealAddr()`).
- Paired device list is retrieved via `SleRemoteDeviceManager::GetPairedDevices()` which iterates the safe map and filters by `SLE_PAIR_PAIRED`.
- Persistence round-trips through `SleConfig::Save()` (XML fsync) after every state mutation.

### 2.3 Connection state machine

The connection lifecycle as tracked by the device manager:

```
DISCONNECTED -> CONNECTING -> CONNECTED -> ENCRYPTED
                                      \-> DISCONNECTING -> DISCONNECTED
```

(`SleConnState` in `nearlink_def_types.h:78-84`)

---

## 3. Multi-Device Support

### 3.1 No hard-coded pairing slot limit

The `SleRemoteDeviceManager` stores devices in a `NearlinkSafeMap<std::string, shared_ptr<SlePeripheralDevice>>` -- a **hash map, not a fixed-size array**. There is no hard-coded pairing slot limit in the device manager layer itself. The limit is imposed lower in the stack/adapter:

- `BAS_MAX_CONNECTION_NUM = 8` (Battery Service, `BasClientStackAdapter.cpp:29`)
- `HID_MAX_CONNECTION_NUM = 6` (HID profile, `HidHostService.cpp:35`)

The `GetConnectedCnt()` method (`SleRemoteDeviceManager.cpp:429-439`) counts devices with `IsAcbConnected()` state, used by the adapter to reject connections beyond the limit.

### 3.2 Per-device data richness

Each `SlePeripheralDevice` object (`sle_service_data.h:422-1244`) carries:
- **Address**: real, current, collaborate (3 address fields)
- **Connection**: lcid, localIndex, linkRole, acbConnectState, acbConnectDirect, addrType
- **Pairing**: pairState, prePairState, pairDirection, ioCapability, bondFlag
- **Identity**: name, aliasName, appearance, manufacturerBusiness, modelId, newModelId, subModelId, iconId, devType
- **Crypto**: cryptoAlgo, keyDerivAlgo, integrChkInd, encryptGroupKeyStr, giv
- **Class**: isAudioDevice, isDeviceAvailable, isDeviceDisplay, isUserDisconnected, cdsmAddrType, btAddr
- **Advertising**: serviceUUIDs, serviceData, manufacturerData, adFlag, txPower, rssi, payload

### 3.3 Connection coexistence

`SleCoexistManager` (`SleCoexistManager.h:32`) tracks per-LCID connection timing parameters (interval, latency, timeout). `HasMultipleConnections()` enables coexistence-aware scheduling when more than one device is active.

### 3.4 Cloud-paired device support

`SleConfig` maintains a separate XML section (`"Sle Cloud Paired Cooperation Device List"`) for devices paired via cloud provisioning, with token-based authentication and encrypted storage.

### 3.5 CDSM (Cooperation Device Set Management)

Support for grouped device sets:
- `SleCdsmAddrType`: NONE (0), REPORT (1), MEMBER (2)
- Report devices maintain member lists (semicolon-delimited addresses in XML)
- Private vs. public device distinction

---

## 4. Implications for the TV Box Project

### 4.1 What the TV box needs

A NearLink TV box must simultaneously manage:
- **Remote control(s)** -- HID profile, likely 1-2
- **Smartphone(s)** -- general-purpose, possibly ICCE/HiLink
- **Sensor(s)** -- battery service, ranging, etc.
- Potentially **cloud-paired** devices (cooperation scenario)

### 4.2 Reference patterns from OHOS device_manager

**Pattern 1: No fixed slot limit in the registry.** The OHOS design uses a hash map, not a slot array. The limit is pushed to per-profile connection limits (BAS=8, HID=6). For a TV box, this means the device manager can handle a large discovery database while actual concurrent connections are bounded by profile-specific limits.

**Pattern 2: MAC address randomization with retention.** The 10-minute expiry for non-retained entries and permanent retention for paired devices is directly applicable. A TV box needs the same pattern: transient devices from scan results are cleaned up, paired devices persist across reboots.

**Pattern 3: XML-based persistent configuration.** Each paired device gets a complete property bag (link key, algorithms, name, appearance, model, audio flag, etc.) stored in XML. This is the reference for what data must survive reboot.

**Pattern 4: Key encryption at rest.** Link keys and cloud tokens are encrypted via HKS (SM2) before XML storage. A TV box must implement equivalent at-rest encryption for security certification.

**Pattern 5: Connection coexistence parameters.** `SleCoexistManager` tracks per-LCID timing (interval, latency, timeout) for multi-device scheduling. This is critical for a TV box managing a remote (latency-sensitive) alongside a phone (throughput-oriented).

**Pattern 6: Per-device ability negotiation.** The `ManufacturerAbilityLoader` and 16-byte ability bitmap allow feature negotiation per device. A TV box can use this to distinguish remote control capabilities (keyboard-only vs. touchpad+voice) from phone capabilities (high-quality audio, karaoke).

**Pattern 7: Active/passive pairing direction.** `SlePairDirect` (ACTIVE=0x01, PASSIVE=0x02) controls reconnection behavior -- passively paired devices are NOT auto-reconnected (`SleRemoteDeviceManager.cpp:659`). This matters for a TV box where the remote should always reconnect (active) but a visitor's phone might not (passive).

**Pattern 8: Audio device flag for scheduling.** `IsAudioDevice` flag influences direct reconnection decisions (`SleRemoteDeviceManager.cpp:666-669`). For a TV box, audio devices (remote with mic, speaker) need priority reconnection.

### 4.3 What the TV box adds beyond OHOS

The OHOS device_manager is designed for a phone/headset topology. A TV box would additionally need:
- **Display output awareness** -- which device controls what is shown
- **Input multiplexing** -- simultaneous remote + phone input handling
- **Power state management** -- TV box may sleep, devices need wake-on-NearLink
- **OTA coordination** -- firmware updates across multiple device types
- **No BT fallback** -- unlike phones, a TV box NearLink dongle may be the only radio

---

## 5. Open Questions

1. **Connection limit at the dongle level**: The WS73 firmware/driver supports how many simultaneous ACB connections? BAS=8 and HID=6 are profile-level limits in the OHOS service, but the chip may have its own lower limit.

2. **Stack-level pairing database**: The device_manager persists to XML, but does the stack (`services/stack/`) also maintain its own pairing database? What happens if XML and stack state diverge?

3. **HKS availability on non-OHOS**: The `SleHksTool` depends on `hks_api.h` / `libhukssdk`. On a non-OHOS (e.g., Linux) TV box, what replaces this for at-rest key encryption?

4. **CDSM for TV box scenarios**: Is CDSM (Cooperation Device Set Management) relevant for TV boxes, or is it only for cloud-provisioned IoT scenarios?

5. **Manufacturer ability for custom profiles**: Can the TV box project inject its own ability bits into the 16-byte bitmap, or is this controlled by Huawei's library?

6. **Coexistence timing parameters**: What are the actual interval/latency/timeout values for different device classes (remote vs. phone vs. sensor)? The `SleCoexistManager` stores them but the source of truth is in the stack callbacks.

---

*End of analysis.*
