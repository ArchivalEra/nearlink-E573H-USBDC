# 04 — 传输形态拍板：内核 usb_driver vs userspace

Type: grilling
Status: open
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
