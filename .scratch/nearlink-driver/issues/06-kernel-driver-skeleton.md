# 06 — 内核 usb_driver 骨架设计（ws73usb）

Type: prototype
Status: open
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
