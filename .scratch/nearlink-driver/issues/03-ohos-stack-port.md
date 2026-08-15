# 03 — 开源栈 Linux 移植面清单

Type: research
Status: resolved

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

## Answer

**结论：可行（GREEN）**。核心栈（services/stack 的 cp/dp/dli/sdf/nai，约 7.3 万行纯 C，POSIX 线程化）可独立编成 Linux 库，剥离成本 2-4 周/1 人。bundle.json 实际列 43 个组件依赖：栈路径只需 6 个 OHOS 依赖且全部可 stub——hilog/hisysevent/securec/parameter/init 为纯 stub（stack 日志已有 `SDF_LogHook`→printf 的本地钩子开关 `NEARLINK_SERVICE_STACK_LOCAL_TEST`），openssl 直接可保留（sle_crypto.c 本就是 OpenSSL）；huks 仅 C++ 层用、栈不用。HAL 缝是 5 函数 C ABI（SleHalInit/SleSendDliPacket/SleReset/SleHalClose/GetDliVersion+2 回调），HDI proxy（ISleHciInterface::Get）可整体换成函数指针表——仓库自带假 HAL（dli_data_stub.c）即此模式。SA/IPC/SAMGR 层（services/server、services/ipc、frameworks）可整体剥掉，C/C++ 边界正好在 SleDliLayerAdapter.h 的 extern "C" ABI 处。硬耦合仅 4 处 `kill(getpid(),SIGKILL)`（含 dli_layer.c:631 芯片复位路径，须改回调）、/data/log 路径、ffrt（仅 snoop 线程）、securec。构建重定向成本小：复用 BUILD.gn 里的 sources 清单 + include_dirs，换成普通 GN/CMake 即可（24 个 ohos_shared_library + 101 unittest + 97 fuzztest 等模板全可丢）。最小子集 = services/stack/{sdf,dli,nai,cp,dp,utils,adapter} + 重写的 services/hardware（USB backend），services/service、frameworks、socket、ipc_parcel 全可弃。

完整报告（依赖分类表/剥离成本/最小清单/引用路径）：`.scratch/nearlink-driver/assets/03-ohos-stack-port.md`
