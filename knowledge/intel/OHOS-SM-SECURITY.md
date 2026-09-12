---
type: intel
title: OHOS NearLink Security Manager (SM) -- Deep Dive
language: en
created: 2026-08-16
tags: []
---

# OHOS NearLink Security Manager (SM) -- Deep Dive

**Date:** 2026-08-17
**Author:** research subagent (nearlink-driver)
**Status:** first pass -- single-read extraction from OHOS NearLink service stack

## Sources

All paths relative to `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/`.

### Primary SM source files (host-side, `cp/bsl/sle/sm/`)

| File | Lines | Role |
|------|-------|------|
| `src/sm.c` | 710 | Init/deinit, UAPI dispatch, DLI callbacks, state machine glue |
| `src/sm.h` | 52 | Internal API: `SmStartPairing`, `SmPkgDispatcher`, `SmSendMessage` |
| `src/sm_struct.h` | 309 | Message opcodes (0x0133-0x0147), PDU structures, state enum, callback events |
| `include/nlstk_sm_api.h` | 570 | **Public API**: IO abilities, auth methods, algo enums, crypto function registration, callback types |
| `src/sm_nego.c` | 436 | Negotiation phase: IO capability matrix, algorithm selection, pairing request/response/confirm/init-info exchange |
| `src/sm_auth.c` | 102 | Auth phase dispatcher: 6 auth methods via function-pointer tables |
| `src/sm_dhkey.c` | 333 | DHKey verification code generation, link key derivation (`LK = CMAC(dk \|\| Ra \|\| Rb \|\| GAddr \|\| TAddr)`) |
| `src/sm_algos.c` | 93 | Crypto algo wrapper (registered via `NLSTK_SmRegAlgoFuncs`) |
| `src/sm_encp.c` | 99 | Encryption enable: DLI_EnableEncryption with negotiated link key + algo |
| `src/sm_stm.c` | 653 | 7-state FSM: INIT -> NEGO -> AUTH -> ENCP -> FULL, with MISS and REMV |
| `src/sm_slink.h` | 74 | Security link structure: dhKey, linkKey, priKey, psk, gNode/tNode params |
| `src/sm_numcmp.c` | 302 | Numeric Comparison auth method |
| `src/sm_noentry.c` | 237 | JustWorks / No-Entry auth method |
| `src/sm_passcode.c` | 444 | Passcode entry auth method |
| `src/sm_password.c` | 316 | Password verification auth method |
| `src/sm_psk.c` | 314 | Pre-Shared Key (PSK) auth method |
| `src/sm_oob.c` | (exists) | Out-of-Band auth method |

### Crypto registration layer (`nai/crypto/`)

| File | Lines | Role |
|------|-------|------|
| `include/sle_crypto.h` | 53 | `Crypto_RandNumGenerate`, `Crypto_PubPriKeyPairGenerate`, `Crypto_SecKeyGenerate`, `Crypto_DerivedKeyGenerate`, `Crypto_Sha256` |
| `src/sle_crypto.c` | (impl) | Concrete implementations registered into SM via `NLSTK_SmRegAlgoFuncs` |

### SSAP permission enforcement (`cp/bsl/sle/servm/ssap/`)

| File | Lines | Key evidence |
|------|-------|-------------|
| `include/ssap_type.h:140-157` | Permission struct + enum: AUTHENTICATION_NEED(0x01), ENCRYPTION_NEED(0x02), AUTHORIZATION_NEED(0x04) |
| `include/inner/ssap_pkt.h:197-199` | PDU error codes: UNAUTHENTICATED, UNAUTHORIZED, UNENCRYPTED |
| `src/ssaps_server.c:339-351` | `SSAPS_ReadControlCheck`: calls `SmIsSLinkAuthComplete` / `SmIsSLinkEncryptComplete` |
| `src/ssaps_server.c:644-654` | `SSAPS_MethodCallCheck`: same auth/encrypt gate for method calls |
| `src/ssaps_server.c:632-634` | AUTHORIZATION_NEED: pushes operation to pending vector for app-level approval |

### Reference lab notes

- `OSPL-PHY-RANGING.md`: OpenSparklink SM section (non-standard, BLE-derived)
- `WS63-HADM-LL.md`: Chip-side SM confirmation via DLI

---

## 1. SM Layer Location: Host-side, 100%

The SM module lives entirely in the **host-side stack** at `cp/bsl/sle/sm/`. The chip (GLLE/WS73) provides:

1. **Crypto primitives** via registered function pointers (`NLSTK_SmCryptoAlgoFuncs_S` at `nlstk_sm_api.h:387-393`):
   - Random number generation
   - Key pair generation (SM2 or ECDH P-256)
   - Shared secret computation
   - HMAC-SM3 / AES-CMAC-128 for key derivation
   - SHA-256
2. **Link encryption enable/disable** via DLI commands (`DLI_EnableEncryption`, `DLI_EncryptionParamReqReply`)

The chip does **NOT** implement the pairing protocol, auth method selection, or key scheduling. The host stack runs a 7-state FSM (`sm_stm.c:54-62`) that orchestrates the entire pairing lifecycle.

**Evidence:** `sm.c:90-128` -- `SmInit()` registers DLI callbacks and link listeners; `sm_algos.c:29` -- `g_algoFuncs` is a host-side global holding registered chip crypto callbacks.

---

## 2. Pairing Flow: Three-Phase Protocol

The pairing protocol has three sequential phases, explicitly documented in `sm.c:20-28`:

### Phase 1: Negotiation (`sm_nego.c`)

**Opcodes:** 0x0133 (Pairing Start) through 0x0137 (Pairing Init Info), defined at `sm_struct.h:67-82`.

**Message flow** (T-node initiates):
```
T -> G:  SM_NEGO_PAIRING_START (0x0133)   -- contains authReq
G -> T:  SM_NEGO_PAIRING_REQUEST (0x0134)  -- contains ioAbility, oobFlag, authReq, keyMaxLen, secInfoDis, algoCap[4], pskFlag
T -> G:  SM_NEGO_PAIRING_RESPONSE (0x0135) -- same structure as request
G -> T:  SM_NEGO_PAIRING_CONFIRM (0x0136)  -- secKeyLen, authMethod, algoCap[4], gNodePubKey[64]
T -> G:  SM_NEGO_PAIRING_INIT_INFO (0x0137) -- tNodePubKey[64]
```

**Role model:** `sm_nego.c:83-90` -- T-node sends Pairing Start; G-node sends Pairing Request. G = "Group owner" (like BLE Central), T = "Target" (like BLE Peripheral).

**IO capability matrix** (`sm_nego.c:328-343`):
```
                 G: DisplayOnly  DisplayYN  Keyboard  NoIO  KeybDisp
T: DisplayOnly    JustWorks      JustWorks  Passcode  JustWorks  Passcode
T: DisplayYN      JustWorks      NumCmp     Passcode  JustWorks  NumCmp
T: Keyboard       Passcode       Passcode   Password  JustWorks  Password
T: NoIO           JustWorks      JustWorks  JustWorks JustWorks  JustWorks
T: KeybDisp       Passcode       NumCmp     Password  JustWorks  NumCmp
```

**Algorithm negotiation** (`sm_nego.c:346-388`): Bitwise AND of local and remote capabilities. Preference order: AES over SM (dominance of bit index 2 over 1). ECDH P-256 preferred over SM2 for key negotiation (`sm_nego.c:377-383`).

**Auth method priority** (`sm_nego.c:399-408`):
1. If both PSK flags set -> PSK
2. If both OOB flags set -> OOB
3. If neither needs MITM -> JustWorks (NoEntry)
4. Otherwise -> IO capability matrix lookup

### Phase 2: Authentication (`sm_auth.c`)

Six auth methods, each with dedicated packet dispatcher and start function:

| Method | Enum | File | MITM-resistant |
|--------|------|------|----------------|
| Numeric Compare | 0x00 | `sm_numcmp.c` | Yes |
| JustWorks (NoEntry) | 0x01 | `sm_noentry.c` | No |
| Passcode Entry | 0x02 | `sm_passcode.c` | Yes |
| Password Verification | 0x03 | `sm_password.c` | Yes |
| Out-of-Band | 0x04 | `sm_oob.c` | Yes |
| PSK | 0x05 | `sm_psk.c` | Yes (if PSK is strong) |

**Numeric Compare flow** (`sm_numcmp.c:103-161`):
```
T -> G:  T_NODE_CFM_WITH_RB (0x013C) -- confirm[16] + Rb[16]
G -> T:  RAND_NUM_RA (0x0139)         -- Ra[16]
Both:    Generate 6-digit code from CMAC, display to user
User:    Confirm via SmSetConfirm()
G:       Compute DHKey -> LINK KEY
T:       Compute DHKey -> LINK KEY
G -> T:  G_NODE_DHKEY (0x013F)        -- authCode[16]
T -> G:  T_NODE_DHKEY (0x0142)        -- authCode[16]
```

**JustWorks flow** (`sm_noentry.c:111-180`):
```
T -> G:  T_NODE_CFM (0x0138)     -- confirm[16]
G -> T:  RAND_NUM_RA (0x0139)    -- Ra[16]
T -> G:  RAND_NUM_RB (0x013A)    -- Rb[16]
Both:    User confirm (no input needed)
Both:    DHKey exchange + Link Key derivation
```

**Passcode flow** (`sm_passcode.c:237-350`): Two sub-patterns based on IO capabilities:
- G displays, T inputs (`PASSCODE_G_DISPLAY_T_INPUT`)
- T displays, G inputs (`PASSCODE_T_DISPLAY_G_INPUT`)
Uses mixed passcode (`GenMixPassCode`) as CMAC input to prevent passcode prediction.

### Phase 3: Encryption (`sm_encp.c`)

After auth succeeds, the G-node initiates encryption:
```
G:  SmEncpEnable(slink)           -- calls DLI_EnableEncryption
    with linkKey[16], cryptoAlgo, keyDerivAlgo, integrChkInd
T:  SM_EncryptParamReqCbk         -- DLI_EncryptionParamReqReply
G:  SM_EncryptChangeCbk           -- success -> FULL state
```

The encryption is **offloaded to the chip** via DLI. The host provides the negotiated link key and algorithm parameters; the chip applies them to the link-layer.

---

## 3. Encryption: ECDH + Link Key Derivation

### Key Exchange

Two algorithms supported (`nlstk_sm_api.h:121-125`):
- **SM2** (KE1 = 0x01): Chinese national standard
- **ECDH P-256** (KE2 = 0x02): NIST standard

Default: ECDH P-256 (`sm.c:141` -- `param.keyNegoAlgo | SM_KEY_NEGOTIATION_ALGORITHM_ABILITY_KE2`). If chip reports no support, P-256 is forced.

**Key sizes** (`nlstk_sm_api.h:27-32`):
- Private key: 32 bytes
- Public key: 64 bytes (X || Y)
- DH shared secret: 32 bytes (truncated to 16 for key derivation)

### Link Key Derivation (`sm_dhkey.c:154-192`)

```
linkKey = CMAC(
    key = dhKey[0:16],    // truncated ECDH shared secret
    data = "lk" || Ra[16] || Rb[16] || GAddr[6] || TAddr[6]
)
```

Where "lk" = 0x6C6B (ASCII "lk" as big-endian uint16).

### Encryption Algorithms

Four categories negotiated in `codeAlgoCap[4]` (`nlstk_sm_api.h:112-125`):

| Category | Index | Options |
|----------|-------|---------|
| Encryption | 0 | SM4-CCM (AC1), AES-CCM (AC2), ZUC (EA1), AES-CTR (EA2) |
| Integrity | 1 | SM4-CCM (AC1), AES-CCM (AC2), ZUC (IA1), AES-CMAC (IA2) |
| Key Derivation | 2 | HMAC-SM3 (HA1), AES-CMAC-128 (HA2) |
| Key Negotiation | 3 | SM2 (KE1), ECDH P-256 (KE2) |

**Preference rule** (`sm_nego.c:367-375`): AES preferred over SM. Encryption and integrity algorithms must match (both AC1 or both AC2 or both EA1/IA1 or both EA2/IA2).

### Binding Mode

`NLSTK_SmSecurityAttribute_E` (`nlstk_sm_api.h:130-134`):
- `SM_PAIRING_MODE_NO_BINDING` (0): No bonding
- `SM_PAIRING_MODE_BINDING` (1): Bonding enabled (keys stored for reconnection)

Key recovery for bonded devices (`sm.c:322-346`): `NLSTK_SmRecoverKey()` restores link key + crypto params from persistent storage.

---

## 4. Permission Gating: SSAP AUTH/ENCRYPT/AUTHZ

SSAP properties, methods, and descriptors carry a 3-bit permission field (`ssap_type.h:140-157`):

| Bit | Flag | Meaning |
|-----|------|---------|
| 0 | AUTHENTICATION_NEED (0x01) | Client must be authenticated (SM pairing complete) |
| 1 | ENCRYPTION_NEED (0x02) | Link must be encrypted |
| 2 | AUTHORIZATION_NEED (0x04) | Application must approve access |

**Enforcement points:**

1. **Read requests** (`ssaps_server.c:339-351`): `SSAPS_ReadControlCheck()` calls `SmIsSLinkAuthComplete(lcid)` and `SmIsSLinkEncryptComplete(lcid)`. Returns `SSAP_ERRCODE_UNAUTHENTICATED` or `SSAP_ERRCODE_UNENCRYPTED` if unmet.

2. **Method calls** (`ssaps_server.c:644-654`): `SSAPS_MethodCallCheck()` identical check.

3. **Read-by-UUID** (`ssaps_server.c:876-896`): `SSAP_ReadByUuidCheckError()` same pattern.

4. **Authorization** (`ssaps_server.c:632-634`): When AUTHORIZATION_NEED is set, the operation is pushed to a pending vector and delivered to the application via callback. The app must explicitly approve or deny via `SSAPS_SendReadReqRsp()`.

**Query functions** (`nlstk_sm.h:30-31`):
```c
bool SmIsSLinkAuthComplete(uint16_t lcid);
bool SmIsSLinkEncryptComplete(uint16_t lcid);
```

These query the SM state machine per-link: auth complete when state >= ENCP, encrypt complete when state == FULL.

---

## 5. vs. OpenSparklink SM: OHOS Is Spec-Compliant, OpenSparklink Is Not

### OpenSparklink SM (from `OSPL-PHY-RANGING.md`)

OpenSparklink's `sle_security.rs` is a **BLE-derived, incomplete** implementation:
- Uses BLE-style LTK/IRK/CSRK key hierarchy
- Does not implement the SLE-specific G-node/T-node pairing model
- No IO capability negotiation per SLE spec
- No SLE-specific opcodes (0x0133-0x0147)
- Cannot interoperate with a spec-compliant SLE device

### OHOS SM

The OHOS SM is **fully SLE-specification-compliant**:
- Implements the exact 3-phase protocol (Negotiate -> Authenticate -> Encrypt)
- Uses correct SLE opcodes (0x0133-0x0147)
- Implements the full IO capability matrix from the spec
- Supports all 6 auth methods including SLE-specific Password and PSK
- Uses SLE-specific key derivation: `LK = CMAC("lk" || Ra || Rb || GAddr || TAddr)`
- SLE-specific DHKey verification protocol (G->T->T->G with authCode)
- Algorithm negotiation with SM2/ECDH P-256 and SM4/AES-CCM/ZUC options
- G-node/T-node role model (not Central/Peripheral)

**Key difference:** OpenSparklink treats SLE as "BLE with different radio". OHOS treats SLE as its own protocol with its own security layer. Our driver stack must follow the OHOS model.

---

## 6. Implications for Our Driver Stack

### What the TV box (G-node role) needs

For a TV box pairing a remote control (T-node), our stack needs:

1. **SM module** -- a full 3-phase pairing engine, host-side. The OHOS implementation is the authoritative reference. Key files to port/adapt:
   - State machine (`sm_stm.c`)
   - Negotiation (`sm_nego.c`)
   - At minimum JustWorks + Numeric Compare auth methods
   - Link key derivation (`sm_dhkey.c:SmGenLinkKey`)
   - Encryption enable (`sm_encp.c:SmEncpEnable`)

2. **Crypto registration** -- register chip-side crypto functions:
   - `Crypto_RandNumGenerate` -> chip RNG
   - `Crypto_PubPriKeyPairGenerate` -> ECDH P-256 (minimum)
   - `Crypto_SecKeyGenerate` -> ECDH shared secret
   - `Crypto_DerivedKeyGenerate` -> AES-CMAC-128 or HMAC-SM3
   - `Crypto_Sha256` -> SHA-256

3. **DLI commands** for encryption:
   - `DLI_EnableEncryption` -- enable link encryption with negotiated key
   - `DLI_EncryptionParamReqReply` -- respond to chip's encryption parameter request

4. **SSAP permission enforcement** -- when registering SSAP services/properties, set permission bits and gate access via `SmIsSLinkAuthComplete` / `SmIsSLinkEncryptComplete` queries.

5. **Bonding/persistent key storage** -- `NLSTK_SmRecoverKey()` for reconnection without re-pairing.

### Minimum viable SM for remote pairing

For a TV box + remote scenario, the remote (T-node) typically has:
- Display: No (or small LED)
- Keyboard: No (or simple buttons)
- Most likely IO: `SM_IO_NO_INPUT_AND_OUTPUT` or `SM_IO_KEYBOARD_ONLY`

Looking at the IO matrix:
- T=NoIO, G=KeybDisp -> **JustWorks** (no MITM protection)
- T=Keyboard, G=KeybDisp -> **Password** (MITM protected, T displays 6-digit code)

For remote pairing, **JustWorks** is likely the spec default. For higher security, the TV box could display a passcode that the remote's buttons input.

### What we must NOT do

- Do NOT reuse BLE-style LTK/IRK/CSRK. SLE uses its own link key derivation.
- Do NOT implement SM on the chip side. The chip only provides crypto primitives and link encryption enable.
- Do NOT skip the DHKey verification exchange after auth. It is mandatory per spec.

---

## Open Questions

1. **Key persistence format**: How does `NLSTK_SmRecoverKey` store/retrieve bonded device keys? Is there a cfgdb (config database) schema? (`sm.c:134` references `CfgdbReadAlgoCaps`)

2. **Multicast (IMG) security**: `sm_struct.h:226-235` defines `SmImgSecuConfigMsg_S` with group keys and GIV. This is for multicast groups -- likely not needed for TV box + remote, but should be understood for future group remote scenarios.

3. **IRK/Address resolution**: The `NLSTK_SmSecurityInformationDistribution_IRK` flag and IRK distribution in `secInfoDis` suggest RPA (Resolvable Private Address) support. Is this implemented or just plumbed?

4. **Chip crypto capabilities**: What does `CfgdbReadAlgoCaps` actually return for WS73? Does it support SM2 or only ECDH P-256? Does it support SM4-CCM or only AES-CCM?

5. **Timeout values**: `sm_slink.h:26-28` shows 10s for protocol timeouts, 30s for user confirm timeouts. Are these configurable?

6. **Authorization callback flow**: How does the application-level authorization (AUTHORIZATION_NEED) actually work in OHOS? The pending operation vector (`SSAPS_PushOperationPenddingVector`) suggests an async approval model -- we need to understand the app-side API.
