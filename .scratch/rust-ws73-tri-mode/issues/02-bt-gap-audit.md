Type: research
Status: open
Blocked by:

## Question

对照 fbb_ws63 的 BLE 侧（ble_soc.ko + libble_host.a + 7 个 ble_* sample：ble_gatt_client/server 等）与 SDK 的 ble_host 预编译库，审计「蓝牙功能差距」——TCID 0x1F CUTC / 0xA4 ICB 多通道、SM 配对、低时延等在 stack/ssap（hwsle_transport 仅 0x0A、ssap_link 仅 0x14xx/0x18xx、feature_mgr 的 ADV/SCAN/ICB/LOW_LAT/RANGING 纯 bitmask 无驱动调用）中缺哪些？给出 hwsle_transport 扩展点与 libble_host.a FFI 粘合方案。
