# WS63 / WS63E "from zero to flash" build manual (HiHope_NearLink_DK_WS63E_V03 / BearPi-Pico_H3863)

Date: 2026-08-17

## Sources

All line references are against the local SDK dump (read-only) at `/mnt/hdd/nearlink-stuff/fbb_ws63/` (this is a GitHub mirror `x-eks-fusion/fbb_ws63`; upstream is `https://gitee.com/HiSpark/fbb_ws63`):

- `tools/README.md` — Windows env guide (HiSpark Studio, CH340 driver, serial program-load)
- `src/build.py` + `src/build/script/cmake_builder.py` + `src/build/script/enviroment.py` + `src/build/script/usr_config.py` — build system entry, target matching, menuconfig
- `src/build/config/target_config/ws63/config.py` + `target_config.py` — targets (`ws63-liteos-app`, `ws63-flashboot`), template (chip/core/arch/toolchain)
- `src/build/toolchains/riscv32_musl_100*.cmake` — cross-compiler wiring
- `src/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_app.config` — default menuconfig
- `src/CMakeLists.txt`, `src/build/cmake/build_function.cmake`, `build_component.cmake` — Kconfig → CMake wiring
- `vendor/build_sample.py` + `vendor/HiHope_NearLink_DK_WS63E_V03/build_config.json` + `vendor/BearPi-Pico_H3863/build_config.json` — board/demo registry and CI sample-build flow
- `src/tools/pkg/chip_packet/ws63/packet.py` — fwpkg (flash image) contents and output path
- `src/build/config/target_config/ws63/script/entry.py` — build hooks (auto-build flashboot)
- `src/application/samples/bt/sle/` (CMakeLists.txt, Kconfig, sle_uuid_server/) — the target demo
- `docs/board/WS63V100 SDK开发环境搭建 用户指南.pdf` — official Linux env + build + flash doc (extracted text)
- `docs/hardware/HiHope_NearLink_DK_WS63E_V03/HIHOPE_NEARLINK_DK_3863E_V03.pdf` — board schematic (CH340K USB-UART)
- `vendor/BearPi-Pico_H3863/README.md`, `docs/board/IO复用关系.md`

## TL;DR (30-min path)

1. Install host prereqs: `cmake` (>=3.14.1), `python3`, `pip3 install kconfiglib pycparser`. Toolchain is **already bundled** — no compiler download needed.
2. Enable the in-tree demo: append 4 lines to `src/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_app.config` (or run `python3 build.py ws63-liteos-app menuconfig`):
   `CONFIG_SAMPLE_ENABLE=y`, `CONFIG_ENABLE_BT_SAMPLE=y`, `CONFIG_SAMPLE_SUPPORT_SLE_SAMPLE=y`, `CONFIG_SAMPLE_SUPPORT_SLE_UUID_SERVER_SAMPLE=y`
3. `cd src && python3 build.py -c ws63-liteos-app`
4. Image: `src/output/ws63/fwpkg/ws63-liteos-app/ws63-liteos-app_all.fwpkg`
5. Connect board via USB-C (CH340 on UART0), flash `_all.fwpkg` with a serial loader ("Connecting, please reset device..." → press RESET). **Flasher is the biggest gap**: the SDK ships NO Linux flasher and no BurnTool binary; official path is Windows HiSpark Studio / BurnTool (URL below). UART0 (115200) is hardwired for burn/log (env PDF §2.2.7).
6. Open serial console on UART0 @115200 → SLE server announces; scan/connect with a client.

---

## 1. Build environment requirements

Host (from `docs/board/WS63V100 SDK开发环境搭建 用户指南.pdf` §1.2 "搭建 Linux 开发环境"):

- Linux **Ubuntu 20.04+**, bash shell; `sudo dpkg-reconfigure dash` → bash (§1.2)
- **cmake**: `sudo apt install cmake` (§1.2.2). SDK requires `cmake_minimum_required(VERSION 3.14.1)` (`src/CMakeLists.txt:5`).
- **python3** (recommended 3.8.0+) + setuptools/pip (§1.2.3)
- **Kconfiglib 14.1.0+**: `sudo pip3 install kconfiglib` — hard requirement, `usr_config.py:7` does `from kconfiglib import Kconfig` (§1.2.3). Not bundled in SDK.
- **pycparser**: needed by upgrade/sign tooling (§1.2.3 step 5).
- make or ninja: `src/build.py:47-51` checks `find_executable("cmake")` and `make`/`ninja`. Default generator is **Unix Makefiles** (`enviroment.py:337-341`); `-ninja` selects Ninja.

Note: `tools/README.md:3` states the **Windows** HiSpark Studio flow only supports the **LiteOS** build (not OpenHarmony).

### Cross-compilation — NOT host build

`ws63-liteos-app` targets **riscv32** (RISC-V, hard-float), not arm, not host:

- `target_config.py:69-71`: `'arch': 'riscv31'`, `'tool_chain': 'riscv32_musl_100_fp'`, `'board': 'evb'`
- `target_config.py:62-63`: `-mabi=ilp32f -march=rv32imfc` (hard FP). Boot template uses non-fp `riscv32_musl_100` (`target_config.py:149`).
- Toolchain binaries are `riscv32-linux-musl-gcc ... 7.3.0` (verified executable, x86-64 host ELF) at `src/tools/bin/compiler/riscv/cc_riscv32_musl_100/{cc_riscv32_musl,cc_riscv32_musl_fp}/bin/` (plus `_win` variants).
- Chip is Hi3863/WS63E — "RISC-V 高性能 32bit CPU" (`vendor/BearPi-Pico_H3863/README.md:28`).

So: cross-compile on x86_64 Linux → riscv32 firmware. The x86-64 → riscv32 compiler is bundled; the host needs only cmake/python/kconfiglib.

## 2. Build command sequence

Work dir = SDK root **`src/`** (the `build.py` entry; `root_dir` = its own dir, `build.py:41`).

```
# 0. Host prereqs
sudo apt install cmake            # >= 3.14.1
pip3 install kconfiglib pycparser # kconfiglib >= 14.1.0

# 1. (optional, interactive) enable the demo via menuconfig
python3 build.py ws63-liteos-app menuconfig
#   Application -> Enable Sample -> ... -> SLE -> SLE UUID Server Sample
#   (or hand-edit the .config, see checklist)

# 2. clean build the app target (auto-builds flashboot first)
python3 build.py -c ws63-liteos-app

# 3. incremental rebuild after code change
python3 build.py ws63-liteos-app
```

Key flags (`build.py:8-34`, env PDF §2.2.2): `-c` clean; `-j<num>` threads (default = cpu_count, `enviroment.py:328`); `-def=A,B,C=1` extra defines; `-component=X` build only component X; `-ninja`; `-[debug|release|normal]`; `-dump` print target params; `-nhso` skip HSO DB update.

How it works: `build.py` → `CMakeBuilder.build()` (`cmake_builder.py:57-102`). The positional arg is matched as a **target name** or **keyword substring** against registered targets (`enviroment.py:410-463`). `ws63-liteos-app` is defined in `config.py:7-121` (os liteos, ram_component incl. `samples` and `bth_gle`/`bth_sdk`/`bt_host`). `cmake_builder.py:219` runs `cmake -G ... -DCMAKE_TOOLCHAIN_FILE=... ` then `make -jN` (`cmake_builder.py:48-55`). Output build dir: `output/ws63/acore/ws63-liteos-app/` (`enviroment.py:250-254`). First build auto-compiles flashboot because `entry.py:24-27` (build_pre hook) builds `ws63-flashboot` if `output/ws63/acore/boot_bin/flashboot.bin` is missing.

## 3. Board selection (HiHope vs BearPi) and demo selection

**Key finding: the firmware target is board-agnostic.** There is no separate "board" build target — `target_config.py:71,150,170` hardcode `board: 'evb'`, chip `ws63`, core `acore`. Both boards run the same `ws63-liteos-app` firmware. Board identity exists only in the **sample/demo** layer:

- `vendor/HiHope_NearLink_DK_WS63E_V03/demo/` — 27 HiHope demos (incl. `sle_led`, `sle_throughput`, `sle_distribute_network`, `sle_wifi_coexist`)
- `vendor/BearPi-Pico_H3863/{peripheral,products,wifi}/` — BearPi demos (incl. `products/sle_uart`, `products/sle_gateway`, `products/ble_uart`)
- Each vendor dir has `build_config.json` listing `{buildTarget, relativePath, chip, buildDef}` pairs — e.g. HiHope `build_config.json:94-99` (`sle_throughput` server: `CONFIG_SAMPLE_SUPPORT_SLE_SERVER_SPEED=y`), BearPi `build_config.json:128-141` (`sle_uart` server/client).
- `vendor/build_sample.py` (CI) is how vendor demos become buildable: it copies a demo dir into `src/application/samples/`, injects `buildDef` `...=y` lines into `src/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_app.config` (build_sample.py:14, 308-357), and runs `python3 build.py -c <buildTarget>`; output copied from `output/ws63/fwpkg/ws63-liteos-app/ws63-liteos-app_all.fwpkg` (build_sample.py:21).

So "choosing a board" = choosing which sample source is compiled + its `buildDef` pins.

### Choosing the SLE demo (sle_uuid_server)

`sle_uuid_server` is **already an in-tree sample** at `src/application/samples/bt/sle/sle_uuid_server/` — no copying needed. Enablement is a Kconfig chain:

1. `src/application/Kconfig:9-13`: `CONFIG_SAMPLE_ENABLE=y`
2. `src/application/samples/Kconfig:9-12`: `CONFIG_ENABLE_BT_SAMPLE=y`
3. `src/application/samples/bt/Kconfig`: `CONFIG_SAMPLE_SUPPORT_SLE_SAMPLE=y` (choice)
4. `src/application/samples/bt/sle/Kconfig:9-10`: `CONFIG_SAMPLE_SUPPORT_SLE_UUID_SERVER_SAMPLE=y` (choice)

CMake gates: `src/CMakeLists.txt:80-84` (USE_KCONFIG) + `build_function.cmake:35-52` (`KCONFIG_GET_PARAMS` reads the `.config`); `src/application/samples/CMakeLists.txt:9-10` (bt) → `samples/bt/CMakeLists.txt:9-10` (sle) → `samples/bt/sle/CMakeLists.txt:5-7` (`add_subdirectory_if_exist(sle_uuid_server)`). Sample entry point auto-registers via `app_run(sle_uuid_server_entry)` (`sle_uuid_server/src/sle_uuid_server.c:297`); it prints via `test_suite_uart_sendf` on UART0.

Important: enabling via `-def=CONFIG_SAMPLE_SUPPORT_SLE_UUID_SERVER_SAMPLE=y` on the command line does **NOT** work — `-def` injects a C macro into the `defines` list (`enviroment.py:346-354`, `merge_defines`), not a CMake `CONFIG_*` variable. The CMake gates (`if(DEFINED CONFIG_...)`) only see Kconfig variables from the `.config` file. The `.config` must be edited (or menuconfig used).

## 4. Flashing

### Connection
- HiHope DK V03: USB **Type-C** to PC; on-board **CH340K** USB-UART on **UART0** (schematic `docs/hardware/HiHope_NearLink_DK_WS63E_V03/HIHOPE_NEARLINK_DK_3863E_V03.pdf`; tools/README.md:91-95). CH340 serial chip → `/dev/ttyUSB0` on Linux (ch341/CH340 kernel driver).
- BearPi-Pico H3863: USB Type-C, "具备调试烧录功能" (`vendor/BearPi-Pico_H3863/README.md:90`).
- UART0 @ 115200 is **fixed** for burn + debug/AT logging; UART1 @ 921600 is the debugkits port; UART2 free (`docs/board/WS63V100 SDK开发环境搭建 用户指南.pdf` §2.2.7; `docs/board/IO复用关系.md:22-23` → UART0 = GPIO17/18).

### Flash image
`output/ws63/fwpkg/ws63-liteos-app/ws63-liteos-app_all.fwpkg` — "empty-chip" image containing loaderboot, params, ssb, flashboot(+backup), nv(+backup), app bin (packet.py:42-60, 94, 200-255). Incremental after first flash: `ws63-liteos-app_load_only.fwpkg` (packet.py:256-260; env PDF §2.2.1).

### Flash procedure (official, Windows)
`tools/README.md:89-113`: HiSpark Studio → 工程配置 → 程序加载 → 传输方式 **serial**, COM port → click 程序加载 → on "Connecting, please reset device..." press RESET → wait → reboot → serial prints appear (UART0).

**Gap:** the SDK dump contains **no Linux flasher and no BurnTool binary** (searched: no jlink_tool/derived_key_tool dirs, no pyusb/pyserial flashing scripts; `src/tools/bin/` has only `compiler/`, `lzma_tool/`, `sign_tool/`). The env PDF §3.3 defers to a separate "WS63V100 BurnTool 工具 使用指南" that is **not present** in `docs/board/` (17 PDFs, none BurnTool). Practical Linux options to confirm tomorrow: (a) HiSpark Studio on a Windows box, (b) download the BurnTool GUI, (c) a community serial flasher for the WS63 loaderboot protocol. This is flagged as an open question.

## 5. Toolchain inventory — what you have, what you must download

### Already in the SDK (`src/tools/bin/...`) — no download:
| Item | Path |
|---|---|
| RISC-V GCC 7.3.0 (Linux, fp) for app | `compiler/riscv/cc_riscv32_musl_100/cc_riscv32_musl_fp/bin/` |
| RISC-V GCC 7.3.0 (Linux, non-fp) for boot | `compiler/riscv/cc_riscv32_musl_100/cc_riscv32_musl/bin/` |
| Same compilers for Windows | `cc_riscv32_musl_fp_win/`, `cc_riscv32_musl_win/` |
| Image sign tool | `bin/sign_tool/sign_tool_pltuni` (+`.exe`) |
| LZMA pack tool | `bin/lzma_tool/lzma_tool` (+`.exe`) |
| Prebuilt loader boot / ssb | `src/interim_binary/ws63/bin/boot_bin/{loaderboot.bin,ssb.bin}` |
| ROM symbols for link | `src/drivers/chips/ws63/rom_config/acore/acore.sym` |

### Missing — must download/install (URLs from local files only):
1. **cmake** (>=3.14.1) — not installed on this machine (verified). `sudo apt install cmake` (env PDF §1.2.2).
2. **kconfiglib** (>=14.1.0) + **pycparser** — pip. URL: `https://pypi.org/project/kconfiglib` (env PDF §1.2.3). **Not installed** on this machine (verified).
3. **HiSpark Studio** (Windows IDE incl. serial program-load): `https://hispark-obs.obs.cn-east-3.myhuaweicloud.com/HiSparkStudio.exe` (tools/README.md:7).
4. **CH340G driver** (Windows only): `https://www.wch.cn/downloads/CH341SER_EXE.html` (tools/README.md:95). On Linux the ch341 driver is in-kernel (not a download).
5. SDK itself (if you want pristine upstream): `https://gitee.com/HiSpark/fbb_ws63.git` (tools/README.md:49).
6. **BurnTool GUI** (per env PDF §3.3 "WS63V100 BurnTool 工具 使用指南") — referenced but **no URL present in the SDK dump**; must be sourced. This is the critical unknown for Linux flashing.

### Local host state (verified today): python3 3.14.6 present; **cmake MISSING**; make present; ninja MISSING (optional); kconfiglib MISSING; pycparser MISSING; distutils usable.

Note: `src/libs_url/ws63/cmake/ohos.cmake` is an OpenHarmony component list, not a download manifest; no firmware/toolchain download URLs are listed anywhere in `libs_url/`.

## 6. Minimal-change checklist (flash an SLE demo, target = demo only)

Assume working copy of fbb_ws63 with `src/` as SDK root; no changes to core SDK:

1. [ ] Host prereqs: `sudo apt install cmake`; `pip3 install kconfiglib pycparser`
2. [ ] Edit `src/build/config/target_config/ws63/menuconfig/acore/ws63_liteos_app.config` — change line 24 `# CONFIG_SAMPLE_ENABLE is not set` → `CONFIG_SAMPLE_ENABLE=y` and add:
   ```
   CONFIG_ENABLE_BT_SAMPLE=y
   CONFIG_SAMPLE_SUPPORT_SLE_SAMPLE=y
   CONFIG_SAMPLE_SUPPORT_SLE_UUID_SERVER_SAMPLE=y
   ```
   (equivalent: `python3 build.py ws63-liteos-app menuconfig`)
3. [ ] `cd src && python3 build.py -c ws63-liteos-app` (first build ~includes flashboot automatically, entry.py:24-27)
4. [ ] Expect image at `src/output/ws63/fwpkg/ws63-liteos-app/ws63-liteos-app_all.fwpkg`
5. [ ] USB-C to board; verify `ls /dev/ttyUSB*` (CH340). If HiHope: core board sits on the dock; UART0 = CH340K.
6. [ ] Flash `_all.fwpkg` via serial loader ("Connecting, please reset device..." → press RESET) — tooling = open question (section 4/5)
7. [ ] Reboot board; serial monitor on `/dev/ttyUSB0` @115200 shows SLE announce logs
8. [ ] From a second WS63 board (or our WS73 host later): SLE seek → find announce (name "WS63_SLE_UUID_Server" etc.) → connect → read/write UUID 0xABCD properties (`sle_uuid_server.h:18-21`)

For a BearPi-only demo (`sle_uart`, `sle_gateway`) you'd additionally copy the demo dir into `src/application/samples/products/` and set its `buildDef` lines in the `.config` — exactly what `vendor/build_sample.py:308-357` automates for CI.

## 7. Conclusion — 30-minute feasible path tomorrow

Feasible, with one known risk (flasher). Compilation path is fully offline-capable: the riscv32 GCC 7.3.0 toolchain is bundled in the SDK (both fp/non-fp, Linux binaries verified runnable), and the `sle_uuid_server` demo is in-tree so only a 4-line Kconfig edit + `build.py -c ws63-liteos-app` is needed. The host machine needs exactly three apt/pip packages (cmake, kconfiglib, pycparser). The image (`_all.fwpkg`) is a standard multi-part serial image (loaderboot → ssb → flashboot → nv → app). The only blocker is the **flasher on Linux**: no BurnTool/flasher ships in the SDK dump and no URL for the BurnTool GUI exists in local docs; plan A is HiSpark Studio (Windows, serial load, URL in section 5), plan B is finding a Linux serial flasher implementing the WS63 loaderboot protocol. Everything else (board power via USB-C, CH340 UART0 115200, RESET-to-sync) is documented and cheap to verify.

## Open questions

1. **Linux flasher**: none bundled; BurnTool GUI referenced (env PDF §3.3) but not shipped and no URL found. Which tool will we use tomorrow (HiSpark Studio/Win, BurnTool download, or a community WS63 serial flasher)?
2. **ROM bins**: `target_config.py:137-142` reference `interim_binary/ws63/bin/rom_bin/{ws63-liteos_rom.bin,pke_rom.bin}` which are **absent** from this checkout (only `boot_bin/loaderboot.bin` + `ssb.bin` are tracked). The build appears to tolerate this (`rom_check` short-circuits because `rom_sym_path` exists, `cmake_builder.py:398-400`; `rom_in_one.sh` exits 0 if romboot missing; `pke_rom.sh` skipped since `rom_lib.c` absent), but the resulting fwpkg may lack ROM/rompack parts. Verify the produced `_all.fwpkg` flashes and boots — otherwise fetch the missing ROM artifacts from upstream gitee.
3. **Python 3.14 vs SDK**: SDK docs recommend 3.8.0+; this host has 3.14.6. `build.py` uses `distutils.spawn` (build.py:38), which works here, but menuconfig TUI and `pycparser` on 3.14 should be smoke-tested on first run.
4. **`CONFIG_PORTING_XF_ENABLE=y`** is default-on (ws63_liteos_app.config:25) — the XF abstraction is always built; if the UUID server's prints don't appear, confirm the UART0 pinmux on the specific board.
5. Board choice affects only sample + pin/buildDef, not firmware target; confirm which board we'll actually receive (HiHope V03 vs BearPi) to pick the matching demo (both run `sle_uuid_server` identically).
