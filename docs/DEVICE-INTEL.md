# Device Intel: `ffff:3733` ("00000000")

> 采集时间: 2026-08-15 · 采集机器: x86-64 Linux (kernel 7.1.5-x64v3-xanmod1)

## 身份

| 属性 | 值 |
|---|---|
| 总线位置 | `usb1-4`（root hub 1, port 4, `devpath=4`） |
| idVendor / idProduct | `ffff` / `3733` |
| bcdDevice | `0100` |
| 速度 | 12 Mbps (**Full Speed**) |
| bDeviceClass/SubClass/Protocol | `0/0/0`（接口级定义） |
| 配置 | 1 个 |
| 制造商 / 产品 | `00000000` / `00000000` |
| 序列号 | `>04072<1<09:=29=0;16=3:0`（乱码/混淆） |
| 电源 | self-powered, 10 mA |
| 状态 | `configured`, `authorized=1` |

## 描述符拓扑

```
Device Descriptor: 18 bytes, bcdUSB 1.10, class 0
Configuration: wTotalLength 0x20, bNumInterfaces 1, bmAttributes 0xc0, MaxPower 10mA
└── Interface 1-4:1.0 (bInterfaceNumber 0, alt 0)
      bInterfaceClass      224 (0xE0) Wireless
      bInterfaceSubClass   2    "Wireless USB Wire Adapter" (WUSB)
      bInterfaceProtocol   2    "Device Wire Adapter Control/Data Streaming"
      bNumEndpoints        2
      ├── EP 0x81 IN  Bulk, 64 bytes
      └── EP 0x01 OUT Bulk, 64 bytes
```

modalias: `usb:vFFFFp3733d0100dc00dsc00dp00icE0isc02ip02in00`

## 驱动绑定现状

- **没有任何内核驱动绑定它**（无 driver symlink，`lsusb -t` 显示 `Driver=[none]`）
- modules.alias 对 `vFFFF`/`isc02` 零匹配；内核无 `CONFIG_USB_WUSB`/`CONFIG_UWB`
- 无 /dev 节点、无网络接口（`ip link` 只有 lo/enp2s0/wlp3s0）

## 解读

这是 **boot 阶段的 WS73**：2 个 bulk 端点正好对上 SDK 的 `DEVICE_BOOT_EP_NUM == 2`。固件下载完成后设备会重枚举为 5 端点（kernel 模式：`BULK_IN/BULK_OUT/INT_IN/RW_REG_OUT/RW_REG_IN`）。厂商/产品串写死 `00000000` 是固件未加载的典型表现。
