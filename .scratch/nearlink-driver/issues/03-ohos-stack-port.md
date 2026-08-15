# 03 — 开源栈 Linux 移植面清单

Type: research
Status: open

## Question

OpenHarmony `communication_nearlink_service` 移植到通用 Linux 的**完整工程量面**：哪些依赖必须 stub/替换、HAL 缝怎么接、可移植边界在哪。这是 Phase 3 的可行性依据。

调研项（源：`/mnt/hdd/nearlink-stuff/communication_nearlink_service`）：
1. `bundle.json` 的 ~50 个 OHOS 依赖逐项分类：纯 stub（hilog/hisysevent/parameter/c_utils…）/ 需替换（huks→openssl?、ipc→本地调用、samgr/safwk→去 SA 化）/ 可保留（libuv/openssl）
2. 构建：GN + `ohos_shared_library` 模板 → 需要重写 GN/CMake 目标；列清所有 `ohos_*` 模板依赖点
3. HAL 缝：`services/hardware/src/SleDliLayerAdapter.cpp` 的 6 函数 ABI（SleHalInit/SleSendDliPacket/SleReset/SleHalClose/GetDliVersion/回调）——哪些要重写、HDI proxy 怎么替成本地函数指针
4. C/C++ 混编、`services/service` 的 SA IPC 层能否整体剥掉（纯库化）
5. 栈的进程模型：SA 单进程 vs 库内嵌；`kill(getpid(), SIGKILL)` 等强耦合点
6. 最小可编译子集：只要 cp/dp/dli/sdf + sle 核心，最小需要哪些文件

产出：依赖分类表 + 「剥离成本评估」（小/中/大）+ 最小移植清单，附证据路径。

## Why it blocks

Phase 3 的可行性与工作量估算；也是 map 目的地「可执行方案」的关键输入。
