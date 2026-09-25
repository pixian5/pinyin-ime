# 口语语料配比阶梯实验报告（LCCC → 二元转移模型）

日期：2026-09-25　环境：Debian 12 / x86_64 / 无 docker
上游：Route 1 探路成功（`route1-probe-report.md`）后，转 Route A 语料地基。

## 1. 实验目的

纯 Python 信道式纠错模块（`correction/`）此前已证明到天花板：
**对话域整句还原率仅 26.8%**（四类错误均值），远低于新闻/维基。
假设：短板不是算法，而是**二元转移模型的训练语料几乎全是书面语**，
缺少口语搭配。本实验验证「补口语料能否救起对话域」，并定位最优配比。

## 2. 数据准备

- 来源：HuggingFace `silver/lccc` → LCCC-base train（352 MB gz）
- 清洗：`bench/fetch_lccc_corpus.py`（流式去重 + `--exclude-corpus` 排除留出集）
- 产物池：`/workspace/native-build/corpus/lccc_raw.txt`
  10 066 068 句 / 112 045 616 字（平均 11.1 字/句）
- 阶梯子集：`head -n N`（嵌套子集，保证各档只有"量"这一个变量）
  N ∈ {5K, 20K, 50K, 150K, 400K}

### 语料规模不对称（本实验最重要的背景）

在库训练语料合计仅约 **30 万字**：

| 文件 | 句数 | 字数 |
|---|---|---|
| baike.txt | 6129 | 150 032 |
| recent.txt | 4000 | 76 534 |
| news.txt | 2000 | 38 220 |
| wiki.txt | 2000 | 35 359 |
| daily.txt | 69 | 287 |

因此 400K 句 LCCC（≈430 万字）会让口语占混合语料 **93%**，
转移表被挤成"对话专用表"——这是新闻域回退的直接原因。

## 3. 实验设计

只改变一个变量：`correction/data/bigram_costs.json`（转移表）。
评测协议固定：`correction.sentence_eval --per-domain 112 --seed 0`。

- **对话域 = 留出测试集**（112 句，从未参与任何训练）
- **新闻域 = 回归哨兵**（防止"补口语把书面语讲坏"）
- 脚本：`bench/ladder_eval.sh`（trap 保证中断也还原在库产物）

各档「基础」行（LAMBDA=0，不查转移表）在六档之间**逐字节相同**，
证明唯一变量确实是转移表，实验设计成立。

### 泄漏防护验证

```
dialogue.txt 112 句  在 lccc_5k/20k/400k 及全量 raw 池中命中  0 句
news.txt     2000 句 在 lccc_raw.txt 中命中                   0 句
```

## 4. 结果

「有上下文」（LAMBDA=0.15）四类错误整句还原率均值：

| 口语语料 | 对话（留出集） | Δ对话 | 新闻（哨兵） | Δ新闻 | 两域等权总分 |
|---|---|---|---|---|---|
| 基线 0 | 26.8% | — | 45.3% | — | 36.1% |
| +5K 句 | 41.9% | +15.1 | 45.8% | +0.5 | 43.9% |
| **+20K 句** | **46.0%** | **+19.2** | 43.1% | −2.2 | **44.6%** |
| +50K 句 | 47.1% | +20.3 | 37.3% | −8.0 | 42.2% |
| +150K 句 | 47.5% | +20.7 | 30.6% | −14.7 | 39.1% |
| +400K 句 | 48.2% | +21.4 | 26.6% | −18.7 | 37.4% |

### 拐点 = 20K 句

- 对话收益**极度前载**：5K 句就拿到 +15.1pt，20K 后每档边际增益 < 1.5pt
  （5K→20K +4.1pt；20K→50K +1.1；50K→150K +0.4；150K→400K +0.7）
- 新闻回退**近似线性**：每档 −2 ~ −7pt，50K 起明显越出噪声带
- 两域等权总分在 20K 处取极大值 → **20K 句是单表配比的最优点**

### 分错误类型看（20K 档 vs 基线）

| 错误类型 | 对话 基线 | 对话 20K | 新闻 基线 | 新闻 20K |
|---|---|---|---|---|
| delete | 6.3 | 15.3 (+9.0) | 8.0 | 8.9 (+0.9) |
| substitute | 17.1 | 33.3 (+16.2) | 35.7 | 33.0 (−2.7) |
| insert | 50.5 | 73.0 (+22.5) | 70.5 | 67.0 (−3.5) |
| adjacent | 33.3 | 62.2 (+28.9) | 67.0 | 63.4 (−3.6) |

对话域四类全面上涨（adjacent +28.9pt 最猛），新闻域回退集中在 −2 ~ −4pt。

## 5. 结论与判断

1. **假设成立**：对话域短板确实由语料领域错配造成，补 20K 句口语料把留出集
   整句还原率从 26.8% 拉到 46.0%（+19.2pt，约 6σ，非噪声）。
   这是本项目迄今纠错模块**最大的一次单步增益**。
2. **不能无限堆料**：口语量与书面语表现存在明确的 trade-off 曲线，
   400K 句时新闻从 45.3% 崩到 26.6%，转移表退化成对话专用表。
3. **单表配比已到局部最优**：继续在"一个全局转移表"里调混合比例，
   收益上限就是 +19.2pt/−2.2pt。要把两条曲线同时做高，
   必须换成**按域分表 + 运行时域适配**（识别输入是口语/书面语再选表），
   而不是继续加大语料。
4. **真正的杠杆仍在 LM/引擎（Route 1/M1）**：本模块只是信道式纠错；
   libime 已可原生构建，`zh_CN.lm` 是 5-gram 级模型，天花板远高于二元转移。

## 6. 采用决定

把 **默认训练配比改为「在库 5 份书面语料 + LCCC 20K 句」**，
重生成在库产物 `correction/data/bigram_costs.json`，并跑全量单测确认不破坏既有行为。

## 7. 复现

```bash
# 1) 取数 + 清洗（排除留出集）
bench/.venv/bin/python bench/fetch_lccc_corpus.py \
  --in /workspace/native-build/corpus/lccc_base_train.jsonl.gz \
  --out /workspace/native-build/corpus/lccc_raw.txt \
  --exclude-corpus bench/corpus/dialogue.txt \
  --exclude-corpus bench/corpus/news.txt

# 2) 阶梯实验（5K/20K/50K/150K/400K + 同样本基线）
sh bench/ladder_eval.sh

# 3) 采用拐点表
head -n 20000 /workspace/native-build/corpus/lccc_raw.txt > /workspace/native-build/ladder/lccc_20000.txt
bench/.venv/bin/python -m correction.scripts.calibrate_transition \
  --corpus bench/corpus/{wiki,news,daily,recent,baike}.txt \
             /workspace/native-build/ladder/lccc_20000.txt \
  --out correction/data/bigram_costs.json
```

### 本次修复的评测 bug

`correction/sentence_eval.py` 的 `--domains` 帮助文本写英文
（dialogue/news/wiki/baike），但过滤键是中文列名，英文参数被
`if n in by_name` **静默丢弃** → 零个域 → 输出空表且不报错。
已改为中英双语别名 + 未知域名直接 `SystemExit`（commit 07463ad）。
若未修此 bug，本实验会被误读为"加口语料毫无增益"。
