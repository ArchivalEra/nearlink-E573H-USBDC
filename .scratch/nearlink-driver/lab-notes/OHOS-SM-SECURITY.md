# OHOS NearLink SM + Security Events — Deep Dive

Sources: `/mnt/hdd/nearlink-stuff/communication_nearlink_service/services/stack/src/cp/bsl/sle/sm/`, `dli/event/src/dli_secu_event.c`, `dli/interface/` opcodes/structs.

## 1. Pairing flow (NEGO → AUTH → ENCP)

State machine in sm_stm.c (7 states: INIT/NEGO/AUTH/ENCP/MISS/FULL/REMV). Role G/T bound from CM logic link; G starts encryption.

### Negotiation (control-plane controller data, opcodes 0x0133–0x0137)
- T sends PairingStart (0x0133) → G sends PairingRequest (0x0134) → T sends PairingResponse (0x0135) → G sends PairingConfirm (0x0136, includes fresh G pubkey) → T sends PairingInitInfo (0x0137, includes T pubkey).
- Auth method: PSK if both pskFlag; OOB if both oobDataFlag; NO_ENTRY if no MITM; else IO-capability table.
- Crypto negotiation: ANDs peer+local caps, picks highest bit (AES/SM per bit), defaults keyNegoAlgo=KE2 (ECDH-P256), marks link OLD when peer reports 0.

### Authentication (packets 0x0138–0x0142)
- DH key via ECDH/SM2; confirm numbers = CMAC(derivedKeyAlgo, key=randomR, Gpub||Tpub); 6-digit compare code CMAC truncated; DHKey auth codes = CMAC over randomR||salt||ioG||ioT||authMethod||codeAlgoCap||pskG||pskT||Gaddr||Taddr keyed by dk||Ra||Rb||Gaddr||Taddr; passcode enters DHKey-salt.

### Encryption (ENCP)
- G node: DLI_EnableEncryption (0x1C03) with DLI_EnableEncryptParam {connHandle, linkKey[16], cryptoAlgo, keyDerivAlgo, integrChkInd}.
- T node on ENCRYPTION_PARAMETER_REQUEST_EVT (0x000E): DLI_EncryptionParamReqReply (0x1C05) or negative (0x1C06).
- Key-missing → negative reply + SM_STATE_MISS (DLI_PIN_OR_KEY_MISSING=0x05).

## 2. Crypto algorithms (nlstk_sm_api.h)
- ENC: AC1=SM4-CCM, AC2=AES-CCM, EA1=ZUC, EA2=AES-CTR. INTG: AC1/AC2/IA1=ZUC/IA2=AES-CMAC. KEY_DERIV: HA1=HMAC-SM3, HA2=AES-CMAC-128. KEY_NEGO: KE1=SM2, KE2=ECDH P-256.
- All crypto behind external function pointers (sm_algos.c) — nothing in-stack.
- DLI algo encoding 0-indexed: 0x00=HA1, 0x01=HA2; IMG: 0x00=AC1...0x03=EA2/IA2.

## 3. Security events (dli_secu_event.c)
- 0x000E ENCRYPTION_PARAMETER_REQUEST_EVT → DLI_EncryptParamReqCbk → DLI_CBK_ENCRYPT_PARAM_REQ
- 0x0011 ENCRYPTION_CHANGE_EVT {status, connHandle, encryptChange} → DLI_EncryptChangeCbk
- 0x0014 CONTROLLER_DATA_EVT {connHandle, ctrlDataIndex(=opcode), len, data} → feeds pairing packets

## 4. Link key (16 bytes)
- Derived in-stack: CMAC/DerivedKey over `lk(0x6C6B)||Ra||Rb||Gaddr||Taddr` keyed by first 16B of DHKey. NEVER transmitted — handed to controller in 0x1C03/0x1C05 params only.

## 5. SM API surface
StartPairing, RemovePairing, SetConfirm, SetPassCode, SetPassWord, SetLocalPsk, RecoverKey, SetSecurityParams, RegExternalCbks, RegAlgoFuncs, SendImgSecuConfig, EnableImgEncp, RegImgCbks. Async via SchedulePostTask.

## 6. Gaps
- No explicit legacy handshake (implicit: peer keyNegoAlgo==0 → ECDH-P256 + CM_DEVTYPE_OLD).
- DLI_RANDOM (0x1C02), DLI_DISABLE_ENCRYPTION (0x1C04), DLI_Encrypt (0x1C01) are dead opcode-only in this tree.
- Key confirmation implicit via ENCRYPTION_CHANGE status + mutual DHKey auth codes.
- Chip internals (controller use of linkKey) outside this tree.

## Implication for WS73
Our hardware verified 0x1C01-0x1C07 all respond; pairing would follow this NEGO→AUTH→ENCP flow with external crypto (openssl SM2/AES/CMAC available). For a slim host stack, security can be deferred until connection + SSAP work.
