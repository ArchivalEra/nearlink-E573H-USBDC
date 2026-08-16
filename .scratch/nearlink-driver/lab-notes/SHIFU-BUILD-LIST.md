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

## 增补：7.x 内核适配清单（x86 已验证，师傅 7.2 aarch64 大概率复用）

### wifi_soc（253 文件，e79ad8a）
| 文件 | 改动 |
|---|---|
| driver/wifi/Makefile | ccflags-y + isystem clang 头 + Wno-error=incompatible-function-pointer-types |
| driver/wifi/hmac/hmac_vap.c | memcpy_s(dev_addr)→dev_addr_set()（7.x dev_addr const） |
| driver/wifi/inc/oal/linux/oal_kernel_file.h | set_fs 全家→no-op |
| driver/wifi/inc/oal/oal_net.h | netif_rx_ni→netif_rx（6.8+ 移除）；wiphy_to_rdev 死代码 stub；cfg80211_roam_info→links[0]；cfg80211_new_sta/del_sta 首参→wireless_dev*；ch_switch_notify→+link_id；preset_chandef→u.ap；+genetlink.h |
| driver/wifi/oal/linux/oal_cfg80211.c | 同上 cfg80211 API 适配 |
| driver/wifi/wal/release/linux/wal_linux_cfg80211.c | cfg80211 结构字段适配 |
| driver/wifi/wal/release/linux/wal_linux_netdev.c | dev_addr const×4 → dev_addr_set + mac_buf；wlan_get_mac/set_mac 传 buf |
| driver/wifi/wal/release/linux/wal_linux_util.c | 去 VFS namespace MODULE_IMPORT_NS |
| driver/wifi/wal/release/linux/wal_linux_vap_proc.c | 去 ../fs/proc/internal.h（内核私有头） |

### ble_soc（7676d6a）
| 文件 | 改动 |
|---|---|
| driver/bsle/ble_driver/linux/Makefile | ccflags-y + isystem；modules 目标需显式传 CC=clang（无 LLVM 分支） |
| driver/bsle/ble_driver/linux/ble_host_hcc.c | asm/unaligned.h→linux/unaligned.h（7.x 移位）；去 dev_type/HCI_PRIMARY（7.x 移除） |

### 编译命令（x86 验证，-j1 必须）
```
wifi: cd driver/wifi && make -j1 WSCFG_KCONFIG_CONFIG=<根>/.config DIR_MAP_CONFIG_FILE=release.mk \
      WSCFG_AUTOCONFIG_H=<根>/output/bin/autoconfig.h default
ble:  cd driver/bsle/ble_driver/linux && make -j1 ... CC=/usr/bin/clang NM=llvm-nm AR=llvm-ar \
      LD=ld.lld OBJCOPY=llvm-objcopy modules   # 无 clang 分支需显式传
```

## 增补 2：hi3798mv310 + SDIO 3.0 变体（2026-08-16）

**背景**: 用户有带 SDIO 3.0 的 hi3798mv310 板子。SDK **原生支持 SDIO 总线**
（`build/config/ws73_default.config` = `WSCFG_BUS_SDIO=y` + `_PRE_PLAT_HCC_SDIO=y`），
板级原生支持 hi3798（`CONFIG_SUPPORT_Hi3798MV320`，MV310 同族大概率兼容或小改寄存器）。

**SDIO 配置要点**（对比 USB 变体）:
- 基准配置: `build/config/ws73_default.config`（SDIO 默认）
- `WSCFG_BUS_SDIO=y`、`_PRE_PLAT_HCC_SDIO=y`（默认已开）
- `WSCFG_CROSS_COMPILE="aarch64-linux-gnu-"`（师傅侧）
- `WSCFG_KERNEL_DIR=/lib/modules/7.2.../build`
- `WSCFG_ARCH_NAME="arm64"`
- 板级: `CONFIG_SUPPORT_Hi3798MV320=y`（MV310 若寄存器不同需对照
  `driver/platform/pm/plat_pm_board.c:438` 的 UART_MUX_REG——注意该宏是 UART 传输专用，
  SDIO 场景可能不需要，需师傅确认）
- 固件: 用 `firmware/us/` 或 `firmware/e/`（两套均 SHA256 头+明文；SDIO 具体用哪套
  需实测确认——SDK 配置里 CONFIG_FIRMWARE_BIN_PATH 指向 /etc/ws73/ws73.bin）

**注意**: WS73 无 SLB（SparkLink Basic 高速）能力——SDK 全树无 SLB 栈/固件，
芯片定位 = WiFi6 + BLE + SLE 三模。SDIO 跑的是这**三模**（尤其 WiFi6 受益于
SDIO 直连 > USB 480M）。
