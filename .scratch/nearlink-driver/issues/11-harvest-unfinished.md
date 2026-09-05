# 11 — 仓库猎收（harvest）未完成事项

Type: task
Status: open
Blocked by:

## Question

token 驱动的持续猎收（2026-08-19 会话）中断后，哪些仓已克隆未消化、哪些方向还没搜？

## 已完成（会话内）

- 本地仓 39 个 / 9.4G（/mnt/hdd/nearlink-stuff/），含 hispark-rs 24 子仓全家桶
- NEW-* 报告 6 份归队（1804 行）：BearPi-NLChat / BS21-WTSL / HiSilicon-Assessment / WS63FLASH-GHIDRA / XF-BURN / XFUSION

## 未消化（已克隆，未派/未归队）

| 仓 | 价值点 | 建议 |
|---|---|---|
| nearLinKernel (29K, Julia) | 名字像 NearLink 实为 Julia，需一句话甄别 | 快速甄别即可 |
| NLChat_Web (636K) | NLChat 网页端 | 低价值，README 级 |
| nearlink-toolbox-website (71 files) | 星闪工具箱网站（Next.js） | 工具箱固件下载方法论 |
| xinghongpai-nearlink-dev-board (178 files) | 华秋开源 WS63V100 板（KiCad 原理图+BOM） | 引脚 vs HHD-01、外设布线 |
| sle_uart + Hi3863-SLE-2025 (小) | SLE 透传变体、传感器-over-SLE 数据帧 | 与 NearLinkSLE-SAMPLES 对比 UUID/MTU |
| hispark-rs/* 24 子仓 (771M) | **重磅**：WS63/BS2X Rust 全家桶（QEMU/rt/pac/crypto/nvs/rtos/rf-facade/fwpkg/flash） | rust-ws73 ticket 04 option B 的现成地基，最优先 |
| sle_mesh v4.4.9 (19M) | 2026-06 推送，比 Mesh-ePaper 新 | HELLO-DV/AIMD/dedup 增量 |
| fbb_bs2x_rust (659M) | BS2x Rust fork——芯片内 Rust 先例 | Rust+LiteOS 共存模式 |

## 未搜方向

- 厂商官方 org（hisilicon/HiHope-Official 在 GitHub 无 nearlink 仓，gitee API 需登录）
- SLE 配对/加密实现（P0-12 SM 层参考）——只搜到应用层调用，无 host 栈实现
- openharmony/communication_dsoftbus（软总线，46M，与 nearlink 竞合）未评估
- openharmony/device_soc_hisilicon（gh code search 出现过 ws63v100/sdk 路径）未克隆
- gitee 侧需 token，GitHub API 匿名限流（10 次/分钟搜索）

## Answer

（待接手会话填写）
