Type: research
Status: open
Blocked by: 04, 08

## Question

审计 Unsafe 边界与 FFI 缝：stack/ssap 调 libble_host.a C 库 + hwsle_transport HAL 的 unsafe 面（ssap_codec 字节解析、hwsle_transport 共享内存、ssap_link 超时状态机 &mut 逃逸）。定策略：unsafe 仅 transport 一处，其余 safe，cargo clippy + miri 门。
