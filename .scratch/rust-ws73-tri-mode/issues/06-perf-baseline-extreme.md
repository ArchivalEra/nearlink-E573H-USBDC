Type: prototype
Status: open
Blocked by: 01, 02, 03

## Question

产出 scripts/bench-rust-ws73.sh 原型与首轮基线数：固件 output/ws63-liteos-app.elf text/data/bss + flash 2M / SRAM 500K 预算，Host SLE 12Mbps 线速 / gatts_notify 吞吐 / ssap_server_notify 时延 / libssap.a strip 前后体积；极致档以此为 PGO 回归门。当前无 output/ 需先编一次取数。
