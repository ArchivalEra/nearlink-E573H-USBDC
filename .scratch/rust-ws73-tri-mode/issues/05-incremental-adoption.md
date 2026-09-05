Type: grilling
Status: open
Blocked by: 04

## Question

决策「增量引入与构建集成」：阶段0 Cargo+ .gitignore 补 !*.toml !Cargo.* !rust-ws73/**（否则被白名单吞）→ 阶段1 ssap-codec 纯函数先行 FFI 对拍 → 阶段2 transport/feature-mgr→server/link，每阶段 make test 绿再切；stack/ssap/Makefile 与 cargo build --release 共存（make rust 目标 vs 独立 cargo），ccache + wait-for-idle.sh 门限。
