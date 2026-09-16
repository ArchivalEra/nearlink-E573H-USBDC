---
type: harvest
title: "Sparklink Playjoy HID Keyboard Protocol (iainbrux/keyboard-cli)"
language: en
created: 2026-09-12
tags: [playjoy, hid, vendor-protocol, rust, harvest]
sources:
  - "/mnt/hdd/nearlink-stuff/keyboard-cli"
trust: B
stale_after: 2027-03-12
---

# Sparklink Playjoy HID Keyboard Protocol (iainbrux/keyboard-cli)

> Hot discovery 2026-09-12 (GitHub `sparklink` sort=updated, pushed 2026-09-08). Clone: `/mnt/hdd/nearlink-stuff/keyboard-cli/` (11M, 289 files). Scope: read-only program knowledge. Every claim cites `path:line` in the clone.

## 0. Identity and the decisive classification

`keyboard-cli` (`wh`) is a Rust CLI that reads/writes rapid-trigger and actuation-point settings on the Wallhack K-001 hall-effect keyboard. The protocol it speaks is the **Sparklink Playjoy vendor configuration protocol** — the same one `terminal.wallhack.com` uses, ported from the MIT-licensed npm TypeScript package `@sparklinkplayjoy/protocol-keyboard` (`CLAUDE.md:5-8`, `THIRD_PARTY_NOTICES.md:26-27`).

**Classification (critical for our knowledge base)**: "Sparklink Playjoy" is the vendor platform brand for SparkLink/NearLink gaming peripherals (Playjoy = the NearLink peripheral ecosystem; the family also ships `@sparklinkplayjoy/sdk-keyboard` v1.0.14 per `research/aure/README.md:109`). But this protocol runs over **raw USB HID, wired** (`crates/wh-device/src/hid.rs:4-68`, hidapi, report ID 0, 64-byte reports). It is **not** the SparkLink wireless air protocol — no NearLink/StarFlash/SLE references exist anywhere in the code (grep across `*.rs|*.md|*.toml` = only ecosystem-branding hits in vendored SDK docs). The K-001's wireless face is out of this protocol's scope.

Value for us despite that: this is the **first complete public reverse-port of a Playjoy vendor peripheral protocol**, with an engineering shape (crate layering, replay tests, write invariants) that maps 1:1 onto `rust-ws73`'s design, and a HID usage/reference for the TV-box HID-remote direction (`OHOS-HID-PROFILE`).

## 1. Vendor frame (64-byte raw HID report)

`crates/wh-proto/src/frame.rs:5-15`:

```
offset 0   HEAD      0x5C
offset 1   len       payload length ≤60
offset 2   cmd       command byte
offset 3   checksum  (0x35 + HEAD + len + cmd + last_payload_byte) mod 256
offset 4.. payload   ≤60 bytes, zero-padded to 64
```

- Reply framing: `reply cmd == request cmd | 0x80` held across all 90 captured request/reply pairs (`frame.rs:9-11`, `captures/initial-load.jsonl`) — the reply bit is the pairing key, never an echoed request byte.
- `CMD_FAIL 0xFF` marks a device-reported failure in replies (`frame.rs:7`).
- The checksum is the TS `computeCRC` port (`frame.rs:2`), deliberately weak (additive) — treat as framing integrity, not security.

## 2. Command set (keyboard profile)

`crates/wh-proto/src/cmds.rs:7-19`:

| cmd | Name | Payload semantics |
|---:|---|---|
| `0x00` | `CMD` | Identity — roundtrip boot probe |
| `0x01` | `SYNC` | Device identity; dedicated `Identity(&'static str)` decode errors naming the failed field |
| `0x23` | `KEY` | Per-key settings (actuation point, rapid trigger) |
| `0x29` | `DB` | Global travel: `[rw, tick, tick, travel(2 LE), press(2), release(2), 6×0]` 16 B fixed (`cmds.rs:60-77`) |
| `0x2B` | `DEFKEY` | Default key matrix rows |
| `0x2C` | `SOCD` | Simultaneous-opposite-cardinal pair table + priority |

R/W convention: the first payload byte is `RW_READ 0x00` / `RW_WRITE 0x01` (`cmds.rs:21-24`).

SOCD priority values are **measured-and-named**: 0 = last input, 1/2 = one key wins are corpus-measured; 3 (neutral) and 4 (depth) exist in the vendored docs but no capture reached them, so the build refuses to render or write them back (`cmds.rs:34-41`) — an exemplary distinction between measured and documented-only knowledge.

## 3. Crate layering — the direct rust-ws73 analogue

`CLAUDE.md:11-24` documents five crates with one-way dependencies:

| Their crate | Owns | Our analogue |
|---|---|---|
| `wh-proto` | Pure frame codec, command encoders/parsers, **zero I/O** | `ssap-codec` (`#![forbid(unsafe_code)]`, byte-wise LE) |
| `wh-device` | `Transport` trait, `Session`, ops, hidapi; builds snapshots from live reads | `hwsle-transport` (the only unsafe/IO crate) |
| `wh-config` | Snapshot/backup file formats, key-group store | (future) our config/persistence crate |
| `wh-cli` | clap surface, formatting | (future) daemon front end |
| `wh-tui` | Full-screen write-capable UI | — |

The layering invariants worth copying verbatim (`CLAUDE.md:26-33`):
- **Dependencies run one way**; the codec depends on nothing of ours — matches our `ssap-codec` as the pure island.
- **Every command travels** parse → resolve selector against the real key matrix **read from the device, not assumed** → build records via codec → send via `Transport` → read back to verify. Read-back verification after writes is the safety net our `hwsle_transport` write path lacks.
- **`Transport` has two implementations**: `HidTransport` (real) and `ReplayTransport` (JSONL capture, byte-exact). **Every test runs against the replay one** (`CLAUDE.md:33-38`). For us: a `ReplayTransport` for `/dev/hwsle` ACB frames would let the entire `stack/ssap` test suite run fixture-driven without a dongle — the same pain `RUST-WS73-UNSAFE-FFI.md` mock-syscall design targeted.

## 4. Session and replay discipline

- `roundtrip` (`crates/wh-device/src/session.rs:25-53`): single read timeout 250 ms, **total wall-clock budget 1500 ms** — time-based, not attempt-based, because the keyboard emits unsolicited input reports while the user types and a fixed attempt count would be exhausted by ordinary typing, surfacing as a false Timeout. Secondary runaway guard `MAX_READS 256`. For our transport: the same wall-clock-deadline pattern applies to `/dev/hwsle` reads interleaved with unsolicited events.
- Unsolicited-report edge: a `0xbe` edge frame during a `cmd 0x00` roundtrip is checked and skipped **before** reply matching (`session.rs:52-53`) — our `hwsle_transport_run` demux has the same shape (event vs reply interleave) but no equivalent documented edge-skip.
- `ReplayTransport` (`crates/wh-device/src/replay.rs:20-36`): hex-decodes untrusted JSONL with `&[u8]` indexing (a `&str` slice landing inside a multi-byte UTF-8 char would panic) and requires exactly 128 hex chars per 64-byte report; every outgoing frame must match the capture byte-for-byte and the match must never be loosened.
- Capture corpus: `captures/initial-load.jsonl` (90 request/reply pairs) doubles as the protocol's executable specification.

## 5. Write invariants — the read-modify-write contract

`CLAUDE.md:35-47` names this "the single most important invariant in the codebase":

- **Writes are read-modify-write**: a settings write reads the key's current MODE first, so changing one thing cannot silently clear another.
- `Change::ap` (`apply_touch`) **promotes touch nibble 0 to 1 and deliberately leaves 1, 2, 3, 4, and unknown nibbles alone** — clobbering a nibble silently disables a feature the user set from the vendor UI.
- `Change::rt_off` uses `0xFE` clear: resets sensitivities and clears keyset membership exactly the way the vendor clears.
- `rt_records` preserves `RtContinuous` (`crates/wh-device/src/ops.rs:120`).
- Legacy duplicate paths (`ops::ap_records` direct) are kept but are **off the write path** — one planned path, old paths demoted with a comment, not deleted (`CLAUDE.md:41-47`).

This maps directly onto our `feature_mgr` drop-order discipline and any future WS73 settings-write surface: read state first, mutate through a planned `Change`, preserve unknown bits.

## 6. Decode-error taxonomy (naming the convention that failed)

`cmds.rs:14-44` grades errors instead of one generic `Shape` failure:

- `Shape` — generic reply-shape mismatch (all non-specific decoders).
- `Identity(&'static str)` — SYNC-specific decode failure naming the failed field.
- `ProfileOutOfRange(u8)` — wire profile index outside measured 0..=3 (e.g., a misbehaving device echoing `0xFF`).
- `ProfileNumberOutOfRange(u8)` — stored one-based number outside 1..=4; kept **separate** because the two validate different conventions (live wire index vs stored number) and the error must name which.
- `SocdPriority(u8)` — priority outside measured {0,1,2}, naming that 3/4 are docs-only.

The discipline: when a reply parses but carries an impossible value, the error names the convention (wire vs stored, measured vs documented), not just "bad". Our SSAP decoders currently return a single `-1` on most failure shapes — this taxonomy is a concrete upgrade pattern.

## 7. Operational caveats (transferable ops knowledge)

- **Exclusive-access trap**: the vendor's web configurator holds the HID device exclusively while its tab is open, even in a background tab; `wh` then fails to open the device and the failure looks like a missing keyboard (`README.md:44-50`). For us: the same class of trap applies to `/dev/hwsle` and `usbmon`-based capture tools fighting over the WS73 interface.
- **WSL/cross-compile workflow**: develop on Linux, cross-compile to `x86_64-pc-windows-gnu`, drive real hardware through a shim (`bin/wh`) because the HID stack is Windows-side (`README.md:6-15`). Instructive for our host-side testing when the dongle is on a remote/VM host.
- **Reproducible release archive**: `scripts/package-release.sh` builds a byte-identical zip carrying LICENSE/NOTICE/THIRD_PARTY_* — Apache-2.0 compliance as a build artifact, "a run is its own verification" (`README.md:31-42`).

## 8. What this changes for us (concrete takeaways)

1. **Adopt ReplayTransport for `stack/ssap`/`hwsle-transport` tests** — JSONL byte-exact ACB capture replay would fixture-drive the whole suite without hardware; pattern proven here with 90 pairs.
2. **Roundtrip read-back verification after SSAP writes** — our transport currently fire-and-forgets; a verify-read is the vendor-app safety net.
3. **Wall-clock session budget with unsolicited-event tolerance** — 250 ms slice / 1500 ms deadline / 256-read runaway guard, applicable to `/dev/hwsle` event-vs-reply interleaves.
4. **Error taxonomy upgrade for SSAP decoders** — name the failed convention/field instead of one `-1`.
5. **RMW + nibble-mask write discipline** for any future WS73 feature-toggle surface (map onto `feature_mgr` conditions).
6. **Playjoy ecosystem reference**: the npm SDK family (`protocol-keyboard` 1.0.7, `hid` 1.0.16, `sdk-keyboard` 1.0.14, all MIT) is the vendor peripheral platform's public face — the same ecosystem NearLink_controller samples came from. Future Playjoy device harvests (e.g. `Eironax/Qwac`, an open-source Playjoy alternative) should consult the vendored `research/proto` tree first.

## 9. Not reusable / boundaries

- Wired-USB-HID vendor protocol only; no SparkLink wireless framing here — our DLI/HCC/SSAP air-side knowledge (`OHOS-DLI-LAYER`, `OSPL-*`) is untouched.
- K-001-only; "No other board is supported or tested" (`README.md:3-4`).
- SOCD priorities 3/4 and five-anchor-class unknowns are documented-but-unmeasured; the build treats them as write-back hazards.
- The additive checksum provides no integrity against corruption beyond framing; do not import as a security pattern.

## 10. Queue noted (not digested this round)

- hispark-rs September batch (`ws63-radio-sys` 9-09 16.9M radio-blob ABI + post-link tooling, `hisi-rf-ws63` 9-10, `hisi-rs-template` 9-09, `ws63-examples` 9-09, `hisi-fwpkg` 9-07, `ws63-pac`/`ws63-svd` 9-03) — `NEW-HISPARK-RS-ECOSYSTEM.md` is the August snapshot; incremental report owed.
- `openharmony/communication_nearlink_service` pushed 2026-09-11 (local clone pre-dates) — pull + diff owed.
- `cxl0928/hi3863-sle-1v8-vehicle` (1-client-to-8-server SLE topology) — topological SLE multi-connection reference.
- `yanlinkos/fbb_ws63` YL63 vendor fork (472M) — needs fork-diff before cloning decision.

---

*Written 2026-09-12, read-only synthesis of a cloned public repo. English-only per `AGENTS.md`. No hardware, no builds.*
