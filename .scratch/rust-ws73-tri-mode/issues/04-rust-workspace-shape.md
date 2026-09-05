Type: grilling
Status: open
Blocked by:

## Question

拍板 rust-ws73/ 的 workspace 形态与 C 栈边界：设于仓库根与 stack/ssap/、sdk/ 并列，候选 A 纯 host 五 crate 对应 ssap/* 先 FFI 后重写、B 含 firmware/#![no_std] riscv32imc、C 单 crate vs workspace 拆分利弊；边界以 hwsle_transport HAL 5+2 为 FFI 缝，stack/ssap 保留并行。需 /grilling + /domain-modeling 明确。
