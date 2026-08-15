# 06 — 内核 usb_driver 骨架设计（ws73usb）

Type: prototype
Status: resolved
Blocked by:

## Question

按已定决策（04：内核形态 + 混合节奏；01：握手规范；05：固件处理；02：传输适配器），Phase 2 内核驱动 `ws73usb` 的骨架长什么样？用最小可编译的原型把关键接口钉死，供实机联调前定稿。

需要定/原型化的设计点：
1. **usb_driver 骨架**：probe 按端点数分派 boot(2EP)/kernel(5EP)（参照 `sdk/.../hcc_usb_host.c` 的 `oal_usb_probe`，但代码自写、不搬海思 osal/td 层）
2. **固件下载状态机**（boot 态）：按 01 规范实现 READM/WRITEM/FILES/QUIT，固件路径可配（module param / udev），剥 64B 头、FILES 分块 ≤32KB、超时表、QUIT 后等待重枚举 5EP
3. **kernel 态数据通道**：5 EP 的 URB 管理（RX 批量池、TX 提交、INT_IN 通知、RW_REG 寄存器读写接口），92B 散射头怎么剥/组
4. **字符设备接口**：misc 设备 `/dev/ws73hci` 的读写语义（HCI 帧字节流 + type 字节分离——对齐 02 的传输适配器结论与 OHOS 栈 HAL 缝）；open 是否触发 SLE enable 消息（对齐 `/dev/hwsle` 的 `H2D_MSG_SLE_OPEN`）
5. **与厂商无线驱动共存**：`wireless_usb`/`wifi_soc` 若同时加载会抢设备——命名/id 表策略、是否独占 ffff:3733
6. **构建**：内核树外模块（Kbuild Makefile）、x86 + 目标内核头

产出：`driver/ws73usb/` 原型源码（可编译骨架 + 关键函数签名 + 状态机注释），附设计说明。

## Blocked by

（无——01/04/05/02 均已 resolve，可直接开）

## Answer

x86 移植可行性评估 + 首轮成果（2026-08-15）:

**评估（assets/06-x86-build-assessment.md）**: SDK `driver/platform` → `plat_soc.ko` 在 x86 编译可行。
hcc include 面薄（4 头 + osal 19 .c）；内核 7.1.5 需 clang（LTO 内核）——SDK 自带
`WSCFG_USING_LLVM_CLANG` 分支。

**移植改动（sdk/ 树内，已应用）**:
1. `.config`: `WSCFG_CROSS_COMPILE=""` + `WSCFG_KERNEL_DIR=/lib/modules/7.1.5.../build` +
   `WSCFG_ARCH_NAME=x86` + `WSCFG_USING_LLVM_CLANG=y` + `WSCFG_CLANG_PATH=/usr/bin`（关 GCC）
2. `driver/platform/Makefile`: `EXTRA_CFLAGS`→`ccflags-y`（7.x kbuild 废弃旧名，导致 SDK
   include 丢失）；修被全量替换误伤的 `HCC_EXTRA_CFLAGS`（-DCONFIG_HCC_SUPPORT_* 的 -D 载体）；
   clang 分支 `-mcmodel=large`→`kernel`（与内核 `-mfunction-return=thunk-extern` 冲突）；
   `-isystem <clang 内建头>`（stdarg.h）；去 `-Werror`
3. `osal/linux/osal_fileops.c` + `inc/oal/linux/arch/oal_kernel_file.h`: set_fs 全家（7.x 移除）
   → no-op；`osal_timer.c` `del_timer`→`timer_delete`；`osal_addr.c` virt_to_phys 强转；
   `cfg/ini.h` `i_ctime`→`i_ctime_sec`；`cfg/customize_wifi.c` 原型 `(void)`；
   `drv/mac_addr` `random_ether_addr`→`eth_random_addr`
4. 摘除诊断通道（7.x VFS API 漂移，非核心路径）: `zdiag_local_log.c`/`zdiag_linux_uart.c`/
   `zdiag_linux_socket.c`/`exce/plat_pm_dfr.c`；新增 `diag/zdiag_adapt/zdiag_stub.c` 补齐
   全部被引用导出符号（zdiag_* + plat_dfr_* + exception + open_file/recv）

**真机验证**:
- `plat_soc.ko` 编译通过（clang21, vermagic 匹配 7.1.5-x64v3-xanmod1）
- insmod 成功（GPIO/ini 告警非致命；`/etc/ws73_cfg.ini` + `/etc/ws73/*.bin` 就位）
- `wireless_usb` 驱动注册成功（dmesg: "registered new interface driver wireless_usb"）
- **双设备（1-4/1-5）均被 wireless_usb 绑定、boot probe 成功**
- 固件自动下载未触发（PM 层需要 sle_soc/用户态 pm_sle_enable——下一步）

**结论**: SDK hcc 内核框架在 x86 跑通 = 票 04 内核形态可行性实锤；kernel 态通道
（固件下载→SLE_OPEN）需继续编 `sle_soc.ko` + 用户态，列为票 06 续或新票。
