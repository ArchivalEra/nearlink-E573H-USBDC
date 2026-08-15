# 08 — sle_soc.ko 编译 + 固件自动下载打通 kernel 态通道

Type: task
Status: open
Blocked by:

## Question

编译 `sle_soc.ko`（SLE/NearLink host 内核模块）并触发固件自动下载，把 kernel 态通道打通到「双设备进 5EP + SLE HCI 可达」。这是票 06 之后的下一个自然步骤。

## 背景（票 06 已做到的）

- `plat_soc.ko` 已在 x86 编译 + 加载成功（wireless_usb 注册、双设备 boot probe 成功）——见票 06 Answer + commit 4339095
- 固件自动下载**未触发**：PM 层 `pm_sle_enable`（driver/bsle/sle_driver/sle_host_register.c:64 `pm_sle_open`）在用户态 open /dev/hwsle 时触发，而 `/dev/hwsle` 由 `sle_soc.ko` 注册
- 设备现状：1-4/1-5 均 boot 态（重启清空 RAM 固件），`/etc/ws73_cfg.ini` + `/etc/ws73/*.bin` 已就位

## 执行要点（含安全指南，上次黑屏教训）

1. **编译安全（必须遵守）**:
   - **禁止**顶层 `make sle`（递归 `-j$(CPU_NUM)` 无限制并行 → 黑屏根源）
   - 直接单线程: `make -C <sdk>/driver/bsle/sle_driver -j1`（5 个源文件，峰值 <600M）
   - 编前 `free -h` 检查：可用 <1G 立即 abort
   - 先给 `sle_driver/Makefile` 做与 platform 相同的适配（`EXTRA_CFLAGS`→`ccflags-y`、`-isystem` clang 头、`-mcmodel`、`.config` 已含 LLVM 配置）
2. **加载**: `insmod sle_soc.ko`（依赖 plat_soc 已在）
3. **触发**: open `/dev/hwsle`（`sle_dev.c:90` 的 `pm_sle_enable`）→ 固件自动下载 → 设备重枚举 5EP
4. **验证**:
   - 设备 bcdDevice 0100→0318、speed 12→480、5EP
   - dmesg: `hwsle` misc device + 固件下载日志
   - `/dev/hwsle` 存在
5. 若需用户态 SLE 栈：sparklinkd 系 ARM 二进制不可用 → 下一步是 OHOS 栈移植（票 03 GREEN 结论）或自写 HCI 客户端

## 阻塞

（无——票 06/07 已 resolve，基础设施全就位；仅受「用户批准编译」与「内存余量」约束）
