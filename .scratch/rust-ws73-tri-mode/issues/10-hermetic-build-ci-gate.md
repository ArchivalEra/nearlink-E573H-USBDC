Type: task
Status: open
Blocked by: 03, 07

## Question

实现可重现构建与回滚门：锁 riscv32 gcc7.3 / rustc 1.96 / kconfiglib / cmake 版本，Cargo.lock 入库，hermetic 构建脚本，LTO 开关一键回滚（WSCFG_EXTRA_CFLAGS 与 Cargo profile 双切），Map 产物归档可追溯，wait-for-idle.sh 参与 CI。
