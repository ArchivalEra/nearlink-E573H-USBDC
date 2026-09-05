Type: research
Status: open
Blocked by:

## Question

对照 sdk/ws73 USB 内核态（host：Linux 4.9，arm-himix100；device：riscv31 rv32imc，wifi_soc/hcc/plf 等）与 fbb_ws63 的 WiFi 协议/框架，审计「WiFi 功能差距」——哪些可被电视盒三模复用的 WiFi 能力（如 STA/AP、wow、csa、twt、btcoex、ALG），在当前仓库（stack/ssap、sdk、docs、.scratch 历史）中缺哪些？以 feature 粒度列出 feature_mgr 应新增的 FEAT_WIFI_STA/SOFTAP/TWT/BTCOEX 成本与最小可移植子集。只做研究，不落代码。
