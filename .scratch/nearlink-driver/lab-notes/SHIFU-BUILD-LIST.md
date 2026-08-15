# 师傅编译资料清单 — WS73 星闪驱动 (hi3798 老盒, aarch64)

> 生成: 2026-08-15 · 用途: 给电视盒子大神编译 WS73 星闪三模驱动的完整资料清单
> 宿主: 海思 WS73 芯片 (蓝牙+WiFi6+星闪SLE 三模), USB ffff:3733 dongle
> 盒子: **hi3798 系列**（用户侧确认, 海思 STB SoC）

## ⭐ 关键情报: SDK 原生支持 hi3798

SDK 板级代码**原生支持 `CONFIG_SUPPORT_Hi3798MV320`**（不是移植，是官方板级！）:
- `driver/platform/pm/plat_pm_board.c:438` — UART 复用寄存器 `0xf8a2106c`
- `driver/platform/hcc/host/hcc_uart_host.h:37` — UART 固件波特率 4M
- 板级宏只有俩: `CONFIG_SUPPORT_Hi3518V300` / `CONFIG_SUPPORT_Hi3798MV320`

师傅编译时只需在 config 开 `CONFIG_SUPPORT_Hi3798MV320` 即可（其余 7.x 适配见下）。

---

## 一、源码/资料绝对路径清单

### 1. SDK 根
```
/home/archivalera/plum/zcode-projects/nearlink/sdk/ws73_sdk_linux_WS73_1.10.110/
```

### 2. 内核模块源码（要编 4 个 ko）

| 模块 | 源码目录 | 产物 |
|---|---|---|
| 平台/HCC | `.../driver/platform/` | `plat_soc.ko`（USB 传输 + 固件下载 + PM） |
| SLE/星闪 | `.../driver/bsle/sle_driver/` | `sle_soc.ko`（/dev/hwsle + SLE 通道） |
| BLE/蓝牙 | `.../driver/bsle/ble_driver/linux/` | `ble_soc.ko`（/dev/hwble） |
| WiFi | `.../driver/wifi/` | `wifi_soc.ko`（802.11ax） |

### 3. 编译系统
```
.../Makefile                          # 顶层（make platform/sle/ble/wifi）
.../build/scripts/hconfig.py          # Kconfig 处理器（make prepare 用）
.../build/config/ws73_usb_light.config    # USB 配置模板（基准）
.../build/config/ws73_default.config     # SDIO 默认
.../build/config/ws73_cfg_default.ini    # 设备配置（拷到盒上 /etc/ws73_cfg.ini）
```

### 4. Hi3798MV320 板级（原生支持）
```
.../driver/platform/pm/plat_pm_board.c        # 行 438: UART_MUX_REG 0xf8a2106c
.../driver/platform/hcc/host/hcc_uart_host.h  # 行 37: 固件波特率 4M
```

### 5. 固件（拷到盒上 /etc/ws73/）
```
.../firmware/us/ws73.bin          # 主固件 143,956 B (SHA256 头+明文, 无加密)
.../firmware/us/wifi_cali.bin     # WiFi 校准
.../firmware/us/btc_cali.bin      # BT/SLE 校准
.../firmware/us/wow.bin           # WoWLAN
```

### 6. 用户态（可选, 需同架构）
```
.../application/bin/rk3568/sle/    # aarch64/glibc2.33 预编译 (sparklinkd/ctrl/chba)
.../application/bin/3516V610/      # 32位 musl-arm (仅参考)
.../application/lib/rk3568/sle/libsle_host.a   # aarch64 静态库 (SLE API)
.../application/lib/rk3568/ble/libble_host.a   # aarch64 静态库 (BLE API)
.../include/bsle/sle/              # SLE 用户态 API 头
.../application/sample/sle/        # SLE 示例 (sle_uuid server/client)
```

### 7. 我们已做的 7.x 内核适配（师傅在 7.2 上大概率复用!）

| 文件 | 改动 |
|---|---|
| `.../driver/platform/Makefile` | `EXTRA_CFLAGS→ccflags-y`（7.x kbuild 废弃旧名）; clang 分支 `-mcmodel=large→kernel`; `-isystem <clang 内建头>`; 去 -Werror |
| `.../driver/platform/inc/oal/linux/arch/oal_kernel_file.h` | set_fs 全家 → no-op（7.x 移除） |
| `.../driver/platform/osal/linux/osal_fileops.c` | set_fs 脚手架删除 + kernel_read/write |
| `.../driver/platform/osal/linux/osal_timer.c` | `del_timer→timer_delete` |
| `.../driver/platform/osal/linux/osal_addr.c` | virt_to_phys 强转 |
| `.../driver/platform/cfg/ini.h` | `i_ctime→i_ctime_sec` |
| `.../driver/platform/drv/mac_addr/mac_addr.c` | `random_ether_addr→eth_random_addr` |
| `.../driver/platform/cfg/customize_wifi.c` | 原型 `(void)` |
| `.../driver/platform/diag/zdiag_adapt/zdiag_stub.c` | 新文件: 摘除 7.x 损坏诊断通道的符号 stub |
| `.../driver/bsle/sle_driver/Makefile` | 同上 ccflags-y 适配 |
| `.../driver/bsle/sle_driver/sle_host_register.c` | 删无用 `asm/unaligned.h` |
| `.../driver/platform/Makefile` | 摘除 zdiag_local_log/uart/socket + plat_pm_dfr（7.x VFS 漂移） |

> 我们的适配已在 **7.1.5 x64** 上编译+加载+真机验证成功（plat_soc/sle_soc），师傅的 **7.2 aarch64** 应可直接复用这些改动。

### 8. 编译命令参考（我们验证过的, x86 侧）

```bash
# 顶层平台模块 (含 hcc+固件下载)
cd /home/archivalera/plum/zcode-projects/nearlink/sdk/ws73_sdk_linux_WS73_1.10.110
make platform -j1          # ⚠️ 必须 -j1! 顶层 make 无限制并行会 OOM 黑屏

# 单模块 (sle 示例)
cd .../driver/bsle/sle_driver
make -j1 WSCFG_KCONFIG_CONFIG=<sdk根>/.config DIR_MAP_CONFIG_FILE=release.mk \
     WSCFG_AUTOCONFIG_H=<sdk根>/output/bin/autoconfig.h modules
```

## 二、师傅侧需要准备的

1. **工具链**: aarch64-linux-gnu-gcc (或 clang, 若盒内核是 LTO/clang 构建则必须 clang+LLVM)
2. **内核头**: 盒内核版本 (7.2) 的 `/lib/modules/<ver>/build`（含 .config）——闭源 SDK 需师傅从盒上拷
3. **config 配置**: 基于 `ws73_usb_light.config` 改:
   - `WSCFG_CROSS_COMPILE="aarch64-linux-gnu-"`
   - `WSCFG_KERNEL_DIR=/lib/modules/7.2.../build`
   - `WSCFG_ARCH_NAME="arm64"`
   - `WSCFG_BUS_USB=y`
   - `CONFIG_SUPPORT_Hi3798MV320=y`（板级!）
   - 若盒内核是 clang 构建: `WSCFG_USING_LLVM_CLANG=y` + clang 路径
4. **运行时**: 固件拷到盒 `/etc/ws73/` + `ws73_cfg.ini` → `/etc/ws73_cfg.ini`

## 三、验证清单（编完在盒上跑）

```bash
insmod plat_soc.ko && insmod sle_soc.ko     # 顺序固定
ls /dev/hwsle                                # SLE 字符设备应出现
# 插上 dongle (ffff:3733) → dmesg 应见 firmware_download_etc::succ
# → 设备重枚举 5EP (bcdDevice 0318, 480Mbps)
cat /dev/hwsle &                             # 触发 SLE_OPEN 握手 (sle state 1)
```

## 四、参考素材（已就位）

- rk3568 aarch64 预编译: `.../application/bin/rk3568/`（需 glibc≥2.33, 老盒大概率不满足, 仅参考）
- 32位 musl 版: `.../application/bin/3516V610/`（老盒若是 32 位内核可参考）
- 实验场工具 (libusb 握手验证): `/mnt/hdd/laboratory/ws73-probe/`
- 探索日志: `/home/archivalera/plum/zcode-projects/nearlink/.scratch/nearlink-driver/lab-notes/EXPLORE-20260815.md`
