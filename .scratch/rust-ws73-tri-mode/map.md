# Map: rust-ws73-tri-mode — WS73 WiFi/蓝牙补全 + Rust-ws73 + 极致性能

> Wayfinder map. Tracker: local-markdown, children under `issues/NN-*.md`, `Blocked by: NN`.

## Destination

**单 WS73 dongle 三模全速（WiFi + 蓝牙 + 星闪）且 host 侧 WiFi/蓝牙缺口已补、全量 LTO（含极致档 PGO）、Rust 在 `rust-ws73/` 以深模块形态落地并通过性能基线。**

18W ccache + wait-for-idle(10%) 已就位 (ecbb6d4)。

## Notes

- 域：WS73 / SLE / WiFi / BLE / HCC-over-USB / Rust / LTO
- 技能：`/research` (AFK)、`/grilling`、`/domain-modeling`、`/prototype`、`/setup-ts-deep-modules`
- 规范：`docs/` 英文-only，`.gitignore` 白名单（需 `!*.toml !Cargo.* !rust-ws73/**`），双语 README 6/6，`scripts/check-docs.sh`
- 编译门限：所有编译走 `scripts/wait-for-idle.sh 1.0` + `ccache`（`loadavg 10` 高时等待）
- Rust 基线：`rustc 1.96.1` 本机仅 `x86_64+wasm32`，riscv 需 `rustup target add riscv32imc-unknown-none-elf`

## Decisions so far

<!-- 一票一 gist，详情在票内 -->

## Not yet specified

- 三模 PM 仲裁 `pm_svc_state[WLAN/BLE/SLE]` + `WIFI_TCM_OPTIMIZE` 组合策略
- `feature_mgr TINY..FULL` 在 Rust 侧位域模型
- Rust `no_std alloc/core` 与 `libble_host.a` FFI 粘合细节
- PGO 训练负载与 `codegen-units=1` 代价

## Out of scope

- 音频完整支持（LE Audio / GA 已明确放弃）
- `ws63-liteos_rom.bin 304K` 芯片 ROM 改动与烧录
- `hi3798` 电视盒 BSP 本体
