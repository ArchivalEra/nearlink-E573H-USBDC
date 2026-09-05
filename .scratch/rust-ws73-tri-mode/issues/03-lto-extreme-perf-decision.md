Type: research
Status: open
Blocked by: 01, 02

## Question

决策「LTO + 极致性能」构建链：Host C（stack/ssap）CFLAGS+=-flto -ffunction-sections -fdata-sections -O2 与 LDFLAGS+=-Wl,--gc-sections -flto；Host Rust（Cargo [profile.release] lto="thin"→"fat" codegen-units=1 panic=abort strip=true）；Device riscv rv32imc 的 riscv32-linux-musl-gcc -flto 与 linker.prelds 的 --gc-section/--cjal-relax/rom_ram_check 及 interim_binary/ws63-liteos_rom.bin 固定 ROM 流程是否冲突，ws73.bin 135K→LTO 后 text/data/bss fwpkg 实测。追加 PGO（--pgo-generate/use）评估，度量 size/nm/Map。
