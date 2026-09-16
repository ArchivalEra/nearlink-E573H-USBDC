---
type: harvest
title: "Ghidra RISCV31 decoder: HiSilicon custom RV32 instruction set fully documented (push/pop, L.LI, ldst-bh, MULIADD, preshifted-ALU, long-jump, imm-condbr)"
language: en
created: 2026-09-15
tags: [harvest, ghidra, riscv, reverse-engineering, isa, ws63]
sources:
  - "/mnt/hdd/nearlink-stuff/Ghidra_RISCV31"
trust: A
stale_after: 2027-03-15
---

# Ghidra RISCV31 processor module

## Executive findings

The repo is a Ghidra processor-module extension adding HiSilicon's **custom RV32 (RISCV31) instruction set** — used by Hi3863V100 (WS63), Hi2821, and other HiSilicon RISC-V chips — to Ghidra's disassembly/decompiler. `data/languages/` carries the full spec set: `riscv.ilp31f.slaspec` (32-bit ILP32F), `riscv.rv31.sinc`, CSR/privileged/register semantics files, DWARF mapping, cspec. The README documents seven custom instruction families with bit-exact encoding tables (self-acknowledged: no official manual; translation reconstructed by guesswork, unverified for translation accuracy but the encodings are concrete):

1. **riscv_push_pop_extension** (16-bit): C.POP/C.POPRET/C.PUSH — `100 | sp16imm | rcount | func` — HiSilicon's hardware multi-register push/pop (stack-frame prologue/epilogue), explaining the non-standard function prologues we see in Ghidra teardowns of fwpkg binaries.
2. **riscv_LLI_extension** (48-bit): L.LI — 32-bit immediate load with `imm[31:0] | 0000 | rd | 0011111` — a 6-byte long-immediate, which is why linear disassembly mis-aligns around constants.
3. **riscv_ldst_bh_extension** (16-bit): C.SB/C.LBU/C.SH/C.LHU placed in the C.FSD/C.FLD/C.FSDSP/C.FLDSP slots — byte/half load-store compressed into FP-slot encodings.
4. **riscv_muladd_extension**: MULIADD (opcode 1011011, custom) — multiply-add with immediate (uimm[7:1]+sign), the DSP-style fused op.
5. **riscv_preshifted_arithmetic** (custom-0 group, opcode 0011011): ADDSHF/SUBSHF/ORSHF/XORSHF/ANDSHF — ALU ops with integrated shift (shtype+shamt), the code-density ops the compiler emits constantly.
6. **riscv_longjump_extension** (opcode 1111011): JAL16/J16 with 25-bit immediate — long-range branches across >1MB firmware images.
7. **riscv_condbr_imm_extension** (custom-0, opcode 0111011): BEQI/BNEI/BLTI/BGEI — compare-immediate-and-branch fused single instructions.

Practical consequence: **stock RISC-V disassemblers (including Ghidra's built-in RV32) produce garbage on WS63/Hi2821 ROM/RAM images** — these seven families are the delta. With the module loaded, function boundaries, stack frames, and control flow decode correctly.

## Boundaries

- Author self-declares reconstruction-by-guesswork (no official ISA manual); encodings concrete, semantics inferred — validate against compiler output if precision matters.
- Supports Hi3863V100/Hi2821 named; WS73/HiFi classes unconfirmed (same vendor ISA family expected).
- Only the module; no loader for fwpkg-encrypted images (extract-flat first, as our fwpkg teardown already covers).

## Reusable

- The seven encoding tables as the authoritative public reference for HiSilicon RV32 custom ops — cite in any future Ghidra/objdump teardown of WS63 firmware.
- The Ghidra module itself: drop into Ghidra extensions, then re-run our earlier WS63FLASH/HiDiTing teardowns for cleaner pseudocode.
- L.LI (48-bit) and JAL16 awareness explains mis-disassembly artifacts in past notes.

## Comparison anchors

- vs. NEW-WS63FLASH-GHIDRA (C teardown): that report worked around missing custom-op decoding; with this module the same images decode natively — worth a re-pass if we return to loaderboot reversing.
- vs. riscv64 toolchains in assets/sdk: the vendor toolchain emits these ops; the decoder documents what the compiler was allowed to emit.
- vs. Ghidra_RISCV31 in the 41-repo library: last unexplored tooling repo — the local library's tooling shelf is now fully mapped.
