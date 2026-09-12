---
type: intel
title: OHOS NAI Layer -- Management Semantics Dissection
language: en
created: 2026-08-17
tags: []
---

# OHOS NAI Layer -- Management Semantics Dissection

Date: 2026-08-17
Author: Research Sub-Agent (NearLink Project)

## Sources

All sources under:
`/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/nai/`

| File | Role |
|---|---|
| `slem/src/slem.c` | SLE Management facade |
| `slem/src/sle_dli_layer.c` | DLI layer adapter (stub shell) |
| `slem/include/slem.h` | SLEM public API |
| `slem/include/sle_dli_layer.h` | DLI adapter API |
| `slem/include/slem_dli_callback.h` | SLEM-DLI callback definition |
| `crypto/src/sle_crypto.c` | Crypto primitives (OpenSSL) |
| `crypto/include/sle_crypto.h` | Crypto API + algorithm enums |
| `nlm/src/nlstk_init.c` | Full stack init/deinit/enable/disable |
| `nlm/include/nlstk_init_api.h` | Stack lifecycle API |
| `dft/src/nai_dft.c` | DFT (telemetry) reporting |
| `dft/include/nai_dft.h` | DFT API |
| `naifwk/include/nai_errno_base.h` | Error code taxonomy |
| `naifwk/include/nai_log.h` | NAI logging macros |
| `api/include/common/sysdep.h` | Byte-order encode/decode macros |

---

## 1. SLEM: SLE Management Facade

### Core lifecycle (slem.c)

SLEM is a thin orchestration shell -- it owns no state machine of its own, but delegates to `NLSTK_InitStack` / `NLSTK_EnableStack` / `NLSTK_DisableStack` / `NLSTK_DeinitStack`.

```c
// slem.c:33-39
int slem_initialize(void) {
    if (NAI_DftInit() != NLSTK_ERRCODE_SUCCESS) {
        NAI_LOG_ERROR("Gle init stack dft failed.");
    }
    return NLSTK_InitStack();
}
```

Key observation: DFT (telemetry) initialization is done *before* stack init. This is the only pre-step SLEM adds before the NLM bootstrap.

### Public API surface (slem.h:31-42)

| Function | Behavior |
|---|---|
| `slem_initialize()` | DFT init then `NLSTK_InitStack()` |
| `slem_close()` | `NLSTK_DeinitStack()` |
| `slem_enable()` | `NLSTK_EnableStack()` |
| `slem_disable()` | `NLSTK_DisableStack()` |
| `slem_is_enabled()` | Always returns 0 (stub) |
| `slem_registerCallbacks()` | Empty -- no-op (slem.c:23-26) |
| `slem_deregisterCallbacks()` | Empty -- no-op (slem.c:28-31) |

The callback registration/deregistration is completely stubbed out. The `SlemCallbacks` struct (slem_dli_callback.h:19-21) contains only a single `void (*dliFailure)()` pointer -- this is meant to notify SLEM when DLI fails. The stubs suggest this feature is not yet wired.

### Relationship to upper layers (SSAP/CM/DTAP)

SLEM does not directly reference SSAP, CM, or DTAP. Those are initialized *inside* `NLSTK_InitStack()` via `StackInitInner()`. SLEM is the outer shell; the real inter-layer wiring happens in `nlstk_init.c`. The upper layers are completely opaque to SLEM.

---

## 2. DLI Layer Adapter (sle_dli_layer.c)

### Architecture: stub shell

Every function in `sle_dli_layer.c` is a no-op returning `NLSTK_ERRCODE_SUCCESS`:

```c
// sle_dli_layer.c:32-35
uint32_t sle_dli_init(void) {
    return NLSTK_ERRCODE_SUCCESS;
}
```

Functions present:
- `sle_dli_init()` -- no-op (line 32)
- `sle_dli_enable()` -- no-op (line 37)
- `sle_dli_disable()` -- no-op (line 42)
- `sle_dli_cleanup()` -- no-op (line 47)
- `SleGetCallbackInstance()` -- returns NULL (line 21)
- `SleSetCallbackInstance()` -- no-op (line 26)

### What this means

The `sle_dli_layer` was designed as SLEM's abstraction over DLI, but the actual DLI initialization, command dispatch, and event routing are done *directly* in `nlstk_init.c` via `DpInit()` / `DpEnable()`. The adapter was either:

1. A planned abstraction that was bypassed in favor of direct DLI calls, or
2. Reserved for a future SLEM-managed DLI lifecycle that was never completed.

The real DLI integration lives in `nlstk_init.c` where `DliCallbackInit()` (line 134) wires up:
- `postOtherThread` -> `SchedulePostTask` (cross-thread dispatch)
- `postOtherBlockedThread` -> `SchedulePostTaskBlocked` (synchronous cross-thread)
- `dftReportKill` -> `DftReportKill` (DFT telemetry)
- `recvAcbHandler` -> `DTAP_DataRecv` (ACB data reception routed to DTAP)
- `getExtRegOpcode` -> DLI extension function registry
- `hadmProcessCsCaps` -> HADM capability processing

The adapter header (sle_dli_layer.h:20-24) declares four lifecycle functions -- all stubbed. The real DLI-into-stack binding is the `DLI_Callback` struct set via `DLI_SetCallback()` in `nlstk_init.c:143`.

---

## 3. Crypto Layer (sle_crypto.c)

### Algorithm support

The crypto layer supports two algorithm families (sle_crypto.h:28-41):

| Enum | Value | Algorithm |
|---|---|---|
| `KEY_NEGOTIATION_ALGORITHM_ABILITY_KE1` | 0x01 | SM2 (Chinese national standard) |
| `KEY_NEGOTIATION_ALGORITHM_ABILITY_KE2` | 0x02 | ECDH P-256 (OpenSSL) |
| `KEY_DERIVATION_ALGORITHM_ABILITY_HA1` | 0x01 | HMAC-SM3 (not implemented) |
| `KEY_DERIVATION_ALGORITHM_ABILITY_HA2` | 0x02 | AES-CMAC-128 (implemented) |

### Crypto API surface (sle_crypto.h:43-47)

| Function | Purpose |
|---|---|
| `Crypto_RandNumGenerate()` | `/dev/urandom` RNG (sle_crypto.c:137-161) |
| `Crypto_PubPriKeyPairGenerate()` | ECDH P-256 key pair generation with compressed pubkey constraint (line 80-92) |
| `Crypto_SecKeyGenerate()` | ECDH shared secret derivation (line 94-107) |
| `Crypto_DerivedKeyGenerate()` | AES-CMAC key derivation (line 109-122) |
| `Crypto_Sha256()` | SHA-256 hash (line 124-127) |

### Implementation details

**Key pair generation** (sle_crypto.c:168-208): Generates ECDH P-256 keys via OpenSSL's `EVP_EC_gen("P-256")`. It enforces that the compressed public key starts with `0x03` (the `PUBKEY_CONV_FORM`). If a random key pair does not yield this prefix, it retries up to 100 times (`CRYPTO_KEY_GEN_MAX_TRY_CNT`). This is because SLE protocol requires specific compressed form for wire encoding.

**Endianness** (sle_crypto.c:332-341): All key material undergoes `EndianReverseOctets()` before and after OpenSSL operations. SLE/SLE protocol uses little-endian wire format; OpenSSL expects big-endian bignum representation. Every cryptographic operation flips bytes bidirectionally.

**AES-CMAC** (sle_crypto.c:274-322): 128-bit AES-CMAC key derivation. Key and input are both byte-reversed before CMAC computation. Output MAC is also reversed. This is the key derivation function for SLE pairing.

**SM2/SM3 (KE1/HA1)**: Both KE1 and HA1 cases are empty `break` statements (sle_crypto.c:83-85, 113-115). The Chinese national cipher algorithms are not implemented in this build. Only ECDH P-256 (KE2) and AES-CMAC (HA2) are functional.

### Relationship to OHOS-SM-SECURITY

The crypto layer's API matches what the Security Management (SM) layer would call during pairing:
- SM generates local key pair -> `Crypto_PubPriKeyPairGenerate()`
- SM receives remote public key, computes shared secret -> `Crypto_SecKeyGenerate()`
- SM derives session key from shared secret -> `Crypto_DerivedKeyGenerate()`
- SM computes authentication hash -> `Crypto_Sha256()`

The SM layer itself (`nlstk_sm`) is initialized in `nlstk_init.c:246` via `SmInit()` and lives outside the NAI directory. The crypto module is a *library* called by SM, not a standalone component with its own lifecycle.

---

## 4. Stack Initialization Sequence (nlstk_init.c)

### Full init sequence: `NLSTK_InitStack()` (nlstk_init.c:501-535)

```
StackFuncInit()
  |-- dlopen("libnearlink_stack_ext.z.so")
  |-- StackOpenFuncInit()      -- calls STK_ExtInit(&g_stkCallback)
  |-- StackExtFuncInit()       -- registers DLI/COLLAB/COMMON/QOSM/Devd/BNL/HADM extension functions

SdfInit()
  |-- SDF_ThreadInit(5)       -- 5 worker threads
  |-- SDF_EvcInit()           -- event channel init

ScheduleEnable()              -- task scheduler

DpInit()
  |-- DliCallbackInit()       -- wires DLI callbacks
  |-- DLI_Init()              -- Data Link Interface
  |-- CM_Init()               -- Connection Manager
  |-- DTAP_Init()             -- Data Transport & Application Protocol
  |-- QOSM_Init()             -- QoS Manager
  |-- TRANS_Init()            -- Transport layer

ExtModuleInit()               -- plugin SO module init

StackInit() [via SchedulePostTaskBlocked]
  |-- CfgdbInit()             -- Configuration Database
  |-- DevdInit()              -- Device Discovery
  |-- SmInit()                -- Security Management
  |-- SSAP_Init()             -- SSAP protocol
  |-- HadmInit()              -- HADM (High-accuracy Distance Measurement)
  |-- BalInit()               -- DevdScan + Cdsm + Actm + MicpDev
  |-- BNL_Init()              -- BNL proxy
```

### Init dependency graph

```
SDF (threads+events) -> Scheduler -> DLI -> CM -> DTAP -> QOSM -> TRANS
                                                                    |
                                                                StackInit (async):
                                                                Cfgdb -> Devd -> Sm -> SSAP -> Hadm -> Bal -> BNL
```

### Enable sequence: `NLSTK_EnableStack()` (nlstk_init.c:668-700)

```
DpEnable()
  |-- DLI_Enable()
  |-- CM_Enable()
  |-- QOSM_Enable()

ReadDpInfos()               -- DLI_ReadCommConfigValue
CfgdbReadCommConfigValue()  -- config DB read

StackEnable() [async]
  |-- SmEnable()
  |-- DevdEnable()
  |-- McpMediaEnable()
  |-- McpVolumeEnable()
  |-- CcpEnable()
  |-- IcceClientEnable() / IcceServerEnable()
  |-- PortClientEnable() / PortServerEnable()
  |-- HidEnable()
  |-- BasEnable()
  |-- ActmEnable()
```

### Key design pattern

The init uses a **two-phase** approach:
1. **Synchronous phase**: SDF, Scheduler, DLI/CM/DTAP/QOSM/TRANS, plugin SO -- all blocking, sequential
2. **Async phase** (`SchedulePostTaskBlocked`): Cfgdb through BNL -- posted to the worker thread pool with a 1-second timeout

The async phase exists because these modules likely depend on the SDF scheduler being operational. The `StackInitInner` function (nlstk_init.c:231) runs on a worker thread, with the caller waiting up to `DEFAULT_WAIT_TIME` (1000ms) for completion.

### Deinit is strict reverse order

`NLSTK_DeinitStack()` (nlstk_init.c:537-549): `StackDeinit` -> `ExtModuleDeinit` -> `DpDeinit` -> `ScheduleDisable` -> `SdfDeinit` -> `StackFuncDeinit`. Within each phase, sub-modules are torn down in strict reverse init order.

### Plugin SO pattern

The stack loads `libnearlink_stack_ext.z.so` via `dlopen` (nlstk_init.c:439). This is a *plugin architecture*:
- `STK_ExtInit(&g_stkCallback)` -- passes callback table to plugin
- `STK_ExtDeinit()` -- cleanup
- Extension function registration per module: DLI, COLLAB, COMMON, QOSM, Devd, BNL, HADM

The callback table `g_stkCallback` (nlstk_init.c:381-391) provides:
- `postTask` / `postTaskBlocked` -- cross-thread scheduling
- `timerAdd` / `timerDel` -- timer management
- `getAddr` / `getLcid` -- LCID-to-address translation via CM
- `setRealACBSubrate` -- ACB subrate configuration
- `enableRealConnHighPower` -- high-power mode
- `setRemoteFeature` -- remote feature configuration

---

## 5. Takeaways for Our Stack

### Our current architecture

Our stack (`ssap/server/link/transport`) is flat -- codec, server, and link call transport directly with no intermediate management layer.

### What NAI provides that we lack

#### 5.1 Lifecycle management

NAI separates **init** (allocate resources, wire callbacks, start threads) from **enable** (bring up data paths) from **disable** (graceful teardown) from **deinit** (release resources). We currently have `ssap_link_init` + `hwsle_transport_open` as our only lifecycle entry points, with no separation between resource allocation and data-path activation.

**Borrowable pattern**: A `*_init()` / `*_enable()` / `*_disable()` / `*_deinit()` quadruple. This is critical for power management -- the stack can be initialized but radio-disabled to save power.

#### 5.2 Callback-based decoupling

NAI wires layers via callback structs (`DLI_Callback`, `SlemCallbacks`, `STK_Callback`) rather than direct function calls. Each layer exposes a `SetCallback()` function. This allows:
- Testing with mock callbacks
- Hot-swapping implementations
- Clear ownership boundaries

**Borrowable pattern**: Our transport layer currently has hard-coded dependencies. A callback table would allow SSAP to wire to transport without transport knowing about SSAP.

#### 5.3 Plugin architecture

The `dlopen`-based plugin SO allows extending the stack without recompiling the core. Extension functions are registered per-module (`DLI_RegisterExtFunc`, etc.).

**Borrowable pattern**: If we want to add features like encryption or multi-channel without bloating the core, a plugin slot architecture would allow feature modules to register themselves.

#### 5.4 Async initialization with timeout

`SchedulePostTaskBlocked` with `DEFAULT_WAIT_TIME` (1000ms) timeout prevents the init sequence from hanging if a module is slow. If it times out, cleanup is deferred via a separate task.

**Borrowable pattern**: Our `ssap_link_init` is synchronous and blocking. If we add more initialization steps (crypto, pairing, device discovery), async init with timeout becomes necessary.

#### 5.5 Separation of crypto as a library

Crypto is not a standalone layer with its own init -- it is a stateless library called by SM. The SM layer owns the pairing state machine; crypto just provides primitives.

**Borrowable pattern**: If we add encryption, keep crypto as a library called from the protocol state machine, not as a layer that needs its own lifecycle management. This avoids init-order complexity.

#### 5.6 DFT/telemetry as cross-cutting concern

NAI's DFT subsystem (nai_dft.c) provides structured telemetry reporting with parameterized events. It is initialized before the stack, and any module can call `NAI_DftReport()` or `NAI_DftCache()`.

**Borrowable pattern**: A lightweight telemetry/tracing layer would be useful for debugging our stack, especially for crypto pairing flows where visibility into intermediate states matters.

### What NAI does NOT provide

- **No multi-channel management**: NAI does not contain channel multiplexing logic. Each CM logic link is a single channel.
- **No connection state machine in NAI**: SLEM is just a facade. The actual connection state machine lives in CM (Connection Manager), which is outside the NAI directory.
- **No adaptive retransmission**: No retry logic in the DLI adapter (all stubs).

---

## Open Questions

1. **What is the full `SlemCallbacks` lifecycle?** The struct has only `dliFailure` but was likely designed for more callbacks (link up/down, pairing state changes, etc.). Is there a more complete version in a different build or branch?

2. **Where is the CM state machine?** `nlstk_init.c` calls `CM_Init()`, `CM_Enable()`, `CM_Disable()`, but CM logic is outside NAI. The CM module manages the actual connection state machine and would be the next layer to examine.

3. **Where does SM call crypto?** `SmInit()` is called in `StackInitInner()` but `sle_crypto.h` includes `nlstk_sm_api.h`. This suggests SM defines the `NLSTK_SmKeyPair_S` / `NLSTK_SmDerivedMac_S` types, and crypto implements them. The actual SM state machine code would show the full pairing flow.

4. **Why is `sle_dli_layer.c` entirely stubbed?** The four lifecycle functions and both callback getters/setters are no-ops. Was this module superseded by direct DLI calls in `nlstk_init.c`, or is it a placeholder for future work?

5. **What triggers `DpEnable` vs `DpInit`?** DLI/CM/QOSM are initialized in `DpInit` but enabled in `DpEnable`. This suggests the data path is initialized once at stack init, then enabled/disabled dynamically (e.g., for power saving or error recovery). What triggers re-enable?

6. **Extension function pattern**: `StackExtFuncInit` registers extension functions for 7 modules. What is the signature and dispatch mechanism for these extensions? How does the plugin SO expose them?
