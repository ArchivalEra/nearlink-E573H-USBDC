# Device Intel: `ffff:3733` ("00000000")

> Captured: 2026-08-15 · Host: x86-64 Linux (kernel 7.1.5-x64v3-xanmod1)

## Identity

| Attribute | Value |
|---|---|
| Bus location | `usb1-4` (root hub 1, port 4, `devpath=4`) |
| idVendor / idProduct | `ffff` / `3733` |
| bcdDevice | `0100` |
| Speed | 12 Mbps (**Full Speed**) |
| bDeviceClass/SubClass/Protocol | `0/0/0` (defined at interface level) |
| Configurations | 1 |
| Manufacturer / Product | `00000000` / `00000000` |
| Serial | `>04072<1<09:=29=0;16=3:0` (garbage/obfuscated) |
| Power | self-powered, 10 mA |
| State | `configured`, `authorized=1` |

## Descriptor topology

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

## Driver binding status

- **No kernel driver claims it** (no driver symlink; `lsusb -t` shows `Driver=[none]`)
- modules.alias has zero matches for `vFFFF`/`isc02`; kernel has no `CONFIG_USB_WUSB`/`CONFIG_UWB`
- No /dev node, no network interface (`ip link` shows only lo/enp2s0/wlp3s0)

## Interpretation

This is a **boot-stage WS73**: its 2 bulk endpoints match the SDK's `DEVICE_BOOT_EP_NUM == 2`. After firmware download the device re-enumerates with 5 endpoints (kernel mode: `BULK_IN/BULK_OUT/INT_IN/RW_REG_OUT/RW_REG_IN`). The vendor/product strings hardcoded to `00000000` are typical of an un-flashed boot ROM. See [USB-PROTOCOL.md](USB-PROTOCOL.md).
