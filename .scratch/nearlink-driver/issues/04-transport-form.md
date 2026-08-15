# 04 — 传输形态拍板：内核 usb_driver vs userspace

Type: grilling
Status: resolved
Blocked by: 01

## Question

HCC 传输层做成**内核 usb_driver** 还是 **userspace (libusb)**？这是 Phase 2 的形态决策，直接决定代码骨架与调试方式。

考量维度：
- 01 还原出的握手协议形态（命令流短交互 vs 大块 bulk 流）
- 内核：URB 管理、固件下载（需读文件系统 / 装载固件 blob）、/dev 字符设备暴露、suspend/resume 责任
- userspace：开发速度、调试便利（gdb）、丢 USB 复位/权限管理、性能上限（Full Speed 12Mbps 下够不够）
- 开源栈接入：03 的 HAL 缝若走字符设备，userspace 传输反而更顺
- 长期形态：星闪低时延（SLE 连接间隔 6.25ms）对传输延迟的敏感度

产出：拍板「内核 / userspace / 混合」，并给 Phase 2 骨架的粗描述（文件布局、接口面）。

## Blocked by

01 — 需要先知道握手协议形态才能定实现载体。

## Answer

拍板（2026-08-15 会话，用户决定）：**内核 usb_driver 为最终形态**，原因是需要 SLE 12Mbps 极限线速（USB Full Speed 也是 12Mbps，userspace 拷贝/syscall 开销会卡吞吐）；**节奏走混合**——时间充裕、一步步来：Phase 1 先用 userspace (libusb) 工具验证握手（快、低风险），Phase 2 再写内核驱动定稿（boot 下载 + kernel 通道，暴露 /dev/ws73hci，开源栈 HAL 缝接字符设备）。

Phase 2 骨架要点（基于 01/05 已定规范）：
- boot 态: 2 EP 按 01 的 ASCII 握手灌固件（剥 64B 头, FILES 分块 ≤32KB, QUIT → 重枚举）
- kernel 态: 5 EP（BULK_IN/OUT + INT_IN + RW_REG×2）跑 HCC 数据通道（92B 散射头）
- 暴露 misc 字符设备 /dev/ws73hci（HCI 帧字节流 + type 字节，对齐 OHOS 栈 HAL 缝——02 的传输适配器）
