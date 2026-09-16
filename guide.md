# guide.md — NearLink 猎收（Harvest）方法 Handoff

> 本文档是**方法**的交接，写给任何接手猎收的 agent 或人。
> 只写不随时间变化的东西：流程、格式、命令、红线。
> **所有会变的数字（报告数、概念数、同步次数、章节号）一律不写在这里** —— 动态状态去指定的状态文件里现场读取，不要相信任何记忆或摘要里的数字。

---

## 1. 猎收是什么

一句话：**把素材（本地代码库 / 远端仓库）消化成 OKF 合规的知识报告，每次推送一组报告 = 一次"同步"（sync）**。

- 知识库在 `knowledge/`（OKF v0.2 bundle），报告放 `knowledge/harvest/`，命名 `NEW-<主题>.md`。
- 一次同步的最小闭环：新报告 + 双语 README 计数递增 + `knowledge/intel/RESEARCH-DIRECTIONS.md` 追加章节 + 三道门禁全绿 + 推送成功（远端 SHA == 本地 HEAD）。
- 报告的"数量/概念数"等状态只存在于这些地方，需要时现场查：
  - `README.md` / `README.en.md` 的情报库标题行与正文行
  - `knowledge/index.md`、`knowledge/log.md`（由脚本再生，**永远不要手编**）
  - `knowledge/intel/RESEARCH-DIRECTIONS.md` 的最后一个章节
  - `git log --oneline`（commit message 里含当时的计数迁移）
  - `.scratch/nearlink-driver/issues/11-harvest-unfinished.md`（未完成队列）

## 2. 素材来源

| 来源 | 位置 / 用法 |
| --- | --- |
| 本地仓库库 | `/mnt/hdd/nearlink-stuff/`（41+ 个已克隆仓库 + `2026_embedded_competition/` 竞赛语料，IOT/AIOT 两赛道） |
| GitHub | `gh api` CLI 已装好可直接用（search repos / read files / clone） |
| GitCode | API v5，请求头 `private-token: $(cat ~/.local/share/gitcode-token)` |
| Gitee | API v5，query 参数 `access_token=$(cat ~/.local/share/gitee-token)`（注意其 search 索引很弱，命中 0 ≠ 不存在，需要组织遍历兜底） |

**Token 永远不进仓库**。它们存放在 `~/.local/share/gitcode-token` 与 `~/.local/share/gitee-token`（仓库外），脚本从那里读。

## 3. 方法循环（每轮）

1. **找方向**：没有指定方向时，自主找最新热点 —— GitHub `search/repositories`（`nearlink OR sparklink`，按 `pushed:>最近日期` / `sort=updated`）、GitCode 搜索 + 官方组织枚举（`hinearlink`、`HiSpark` 等）、竞赛语料未消化项、本地库未深挖仓。**不要停下来问用户。**
2. **查陈旧**（对每个候选仓）：`gh api repos/<owner>/<repo> --jq '.pushed_at'` 对比本地克隆 HEAD 日期；本地落后先 `git pull`；确认素材没过时再消化。
3. **去重**：先查 `RESEARCH-DIRECTIONS.md` 和 `knowledge/harvest/` 里该主题/该仓是否已有报告；有则只做增量（delta），没有才写全量。
4. **深挖**：读代码拿**文件:行号级锚点**（`file.c:123`），不要只复述 README。找"可复用"的机制（协议、状态机、参数、坑），而不是产品宣传。
5. **写报告**（格式见 §4）。
6. **归档链**（命令见 §5）。
7. **推送并验证**（见 §5 末尾）。

## 4. 报告格式（硬标准）

文件：`knowledge/harvest/NEW-<大写主题>.md`。

Frontmatter 必须是完整字段集、顺序如下（缺一个都算不规范）：

```yaml
---
type: harvest
title: "一句话标题（英文，双引号包裹）"
language: en
created: YYYY-MM-DD
tags: [harvest, 三到六个小写关键词]
sources:
  - "/绝对/路径/或/repo/URL"
trust: A          # A=代码级逐行验证, B=README/结构级, C=二手转述
stale_after: YYYY-MM-DD   # created + 6 个月
---
```

正文必须含四节：

- **Executive findings** —— 核心发现，每条带 `file:line` 锚点，讲机制不讲愿景；
- **Boundaries** —— 边界：没读什么、什么是自我声明未验证的、许可限制；
- **Reusable** —— 对本项目（WS73 USB dongle / SSAP 栈）可复用的点，逐条可执行；
- **Comparison anchors** —— 与知识库已有报告的对照（引用此前报告名或 sync 主题）。

**红线（会被门禁直接拒绝 + 自查项）：**

- `language: en` 的文件**一个汉字都不能有**（含引号里转述的中文、中文文件名、中文标点）。写完自查：引用中文注释时用 "(translated: ...)" 转写。
- 竞赛/开源项目的 PCB、3D 模型、原理图目录一律不读不写（硬件设计材料禁触）。
- 固件二进制 / fwpkg / blob 只记名字与版本，不读取、不入库。
- DashScope 等 API key 一类敏感串不写进报告。
- 旧 `language: zh` 报告按原样保留（正文不动，frontmatter 补全到同一标准）。
- **同名≠同物**：判断一个 "SparkLink/NearLink" 命名的仓是不是真的 NearLink，必须有代码级证据（include 头文件、API 调用）；否则在报告里写 verdict 行。
- README / 报告里**永远不写硬编码计数**，一律动态读改（见 §5）。

## 5. 归档链（每次同步的精确命令序列）

```bash
# 0) 写好 knowledge/harvest/NEW-XXX.md 之后：

# 1) 再生索引与 frontmatter（不手编 index/log）
python3 scripts/okf_frontmatter.py

# 2) 双语 README 计数递增：动态读取当前数字 +1，绝不硬编码
python3 - <<'EOF'
import re
def bump(path, hp, bp):
    t = open(path, encoding="utf-8").read()
    m = re.search(hp, t); docs = int(m.group(1)) + 1
    t = re.sub(hp, lambda mm: mm.group(0).replace(mm.group(1), str(docs)), t, count=1)
    m2 = re.search(bp, t); c, h = int(m2.group(1))+1, int(m2.group(2))+1
    t = re.sub(bp, lambda mm: mm.group(0).replace(mm.group(1), str(c)).replace(mm.group(2), str(h)), t, count=1)
    open(path, "w", encoding="utf-8").write(t)
# 标题行模式（若 README 格式变了，先 grep 再改这里）
bump("README.md",    r"（(\d+) 份研究文档", r"(\d+) 份概念 = (\d+) 份猎收报告")
bump("README.en.md", r"\((\d+) research docs", r"(\d+) concepts = (\d+) harvest reports")
EOF

# 3) RESEARCH-DIRECTIONS 追加新章节：章节号 = 现有最后一节 + 1（用中文数字），
#    内容 = 一行报告名 + 本批最有价值的发现（含加粗关键机制）。

# 4) 提交（message 惯例：harvest: <主题> - <最亮点> (旧N->新M concepts)）
git add -A && git commit -m "harvest: ..." && NEW_SHA=$(git rev-parse HEAD)

# 5) 三门禁。注意：check-okf 必须直跑并检查退出/✗，不要用管道 tail 掩盖退出码
python3 scripts/check-okf.py > /tmp/okf.log 2>&1
grep -q "✗" /tmp/okf.log && { grep "✘" /tmp/okf.log; exit 1; } || {
  git push -q origin main
}

# 6) 推送验证：必须确认远端真的到了新提交（防止假同步）
[ "$(git rev-parse origin/main)" = "$NEW_SHA" ] && echo "SYNC OK"
```

**门禁语义**（pre-push 会自动再跑一遍，失败即拒推）：

- `scripts/check-okf.py`：OKF 合规（frontmatter 完整性、`language: en` 无 CJK、索引一致）。报错行以 `✘` 开头。
- `scripts/check-docs.sh`：6/6（README 双语互链 / 索引完整 / docs/ 纯英文 / gitignore 白名单 / 身份 / **双语 README 必须同变**）。
- `scripts/check-harvest-archive.sh`：outgoing 中每出现新增 `NEW-*.md`，就要求同一推送里 `README.md` 和 `README.en.md` 都被修改（单边更新 = 拒）。

**两个常见坑（已踩过，勿重蹈）：**

- `check-okf.py 2>&1 | tail -1 && 下一步` —— tail 永远成功，门禁被掩盖导致计数漂移。必须用上面 `grep -q "✗"` 的写法。
- 推送偶发 `Recv failure: 连接被对方重置` —— 网络瞬断，`sleep` 几秒重试即可，不要改配置。

## 6. 项目红线（超出报告格式的约束）

- **不派子代理**：所有猎收在主会话直接做（所有者偏好，非技术限制）。
- **不打断**：无人值守模式下持续推进到目标同步数；有疑问也不要停下来问，把判断写进报告的 Boundaries。
- **USB 边界**：只操作星闪 USB 口，其它 USB 设备不碰。
- 素材仓的**许可证要记**：GPL 案例集/代码只记事实不复制；"禁止竞赛/教学复用"的声明要在报告里转述。
- 报告里的结论要能被下一个 agent 独立验证：锚点、路径、版本号写全。

## 7. 接手时的第一步

```bash
git -C <repo> pull && git log --oneline -5        # 看最近同步主题
tail -30 knowledge/intel/RESEARCH-DIRECTIONS.md   # 看最后一批挖了什么
ls /mnt/hdd/nearlink-stuff/                       # 盘点本地素材
```

然后从 §3 第 1 步开始。方法不变，状态现场读。
