# pinyin-ime

用 libime 做「好用的拼音输入法」。当前处于 **M0（评测阶段）**：先量化「好用」、
用数据驱动技术路线，而不是先写引擎。路线决策与文化详见 [PLAN-A.md](PLAN-A.md)，
评测框架用法/口径/踩坑详见 [bench/README.md](bench/README.md)。

## 当前进度（截至 2026-09）

### 已完成：评测框架 + 两条基线

- **评测框架 `pinyinbench`**（`bench/`）：整句准确率（句级/字级）、平均码长、
  整句延迟、逐键延迟（perkey）、首词命中率（essay / jieba 两个词源）。
  `oracle`≈100% / `echo`≈0% 自检通过，口径可回归。
- **C++ 适配器 `cpp/libime_bench.cpp`**：把 libime 包成行协议子进程，两种模式
  `libime_bench <lm> <dict> <nbest>`（整句）与 `libime_bench perkey ...`（逐键计时）。
- **本机构建**（openEuler 22.03，无 docker，gcc-toolset-13，lib64 布局）：
  fcitx5 5.1.23 + libime **1.1.17**，2026 数据——语言模型 `zh_CN.lm` 34,736,327 B、
  系统词典 `sc.dict` 4,528,345 B（300,471 条）。
- **基线（sc.dict）**：句级 39.55% / 字级 88.82% / 码长 3.099；
  逐键 0.98/0.80/2.54 ms（avg/P50/P99）；首词 essay 95.80%/99.90%、jieba 80.40%/93.10%。

### Route A「换更大词典」复测 = 无增益

把系统词典换成雾凇拼音 **rime-ice**（LGPL-3.0），`base`+`ext`+`8105` 去重后
**884,332 条（sc.dict 的 2.94 倍）**，`bench/convert_rimeice.py` 转成 libime 二进制
`rimeice.dict`（18.2 MB）。语言模型 `zh_CN.lm` 保持不变，只换词典这一个变量。
同一份 `corpus/news.txt` 2000 句复测：

| 指标 | sc.dict（300,471 条） | rime-ice（884,332 条） | 变化 |
|---|---|---|---|
| 句级准确率 | 39.55% | 39.45% | −0.10 pct |
| 字级准确率 | 88.82% | 88.84% | +0.02 pct |
| 平均码长 | 3.099 | 3.099 | — |
| 逐键延迟 avg / P50 / P99（ms） | 0.98 / 0.80 / 2.54 | 0.62 / 0.55 / 1.45 | 更快 |
| 首词命中率 essay top1 / top5 | 95.80% / 99.90% | 95.65% / 99.90% | −0.15 / 0 |
| 首词命中率 jieba top1 / top5 | 80.40% / 93.10% | 80.25% / 93.00% | −0.15 / −0.10 |

**结论：3× 词库对准确率没有增益**（准确率变化 ≤0.15 个百分点，属 2000 样本的噪声）。
印证 [PLAN-A.md §12](PLAN-A.md) 末尾判断：提升来自 **34.7MB 新版语言模型**，
不是「词库越大越好」；校准瓶颈在语言模型和输入编码，不在词典覆盖。

> 转换的关键坑：词典 cost 必须按「词内读音归一」映射为 log 概率
> `cost = ln(读频/该词总频)`，与 sc.dict 同语义，全局词频交给 LM。
> 若把 rime-ice 原始整数词频直接当 cost（0~几千万），会盖过 LM 分数、输出乱码单字。
> 完整规则见 `bench/convert_rimeice.py` 文件头注释。

### M3 容错纠错模块 — 先验校准 + 信道调参

纯 Python 的音节级纠错模块（`correction/`），CHIME 噪声通道模型，不依赖 libime。

**修复 1：音节先验从手工排序改为数据驱动。**
原 `prior.py` 手工列 ~90 个常用音节，其余 330 个统一罚 2.20，淹没了信道代价（0~1.5）。
改为从 wiki+news+daily+recent+baike **300,432 字**语料统计 420 音节频度（覆盖 398/420），缩放 alpha=0.030，
先验范围 0.10~0.40（跨度 0.30 < 信道最小差 0.35），只做 tie-break。
校准脚本：`python3 -m correction.scripts.calibrate_prior`

**修复 2：默认替换代价 1.5→1.2。**
原 COST_SUB_DEFAULT=1.5 与键盘邻接 0.6 差 0.9，先验（跨度 0.30）永远翻不了盘。
降至 1.2 后差缩到 0.6，substitute top3 提升 +11.8 pct，其他指标不受影响。

纠错命中率（`python3 -m correction.metric`，15 项单元测试全通过）：

| 指标 | 修复前 | 修复后 | 提升 |
|---|---|---|---|
| 未命中·delete top1 / top3 | 8.97% / 16.62% | 12.14% / 45.65% | +3.2 / +29.0 |
| 未命中·insert top1 / top3 | 23.57% / 44.05% | 77.86% / 98.81% | +54.3 / +54.8 |
| 未命中·substitute top1 / top3 | 6.90% / 20.24% | 27.62% / 59.05% | +20.7 / +38.8 |
| 混淆排序 top2 | 23.26% | 86.05% | +62.8 |

> 边界：音节级纠错无法区分「打错成另一个合法音节」和「本来就该是它」
> （如 zh→z 后 "zong" 仍合法）。这类「合法-合法」歧义要词级语言模型解，
> 不在本模块职责内。delete/substitute top1 偏低是此限制的体现。

### M3 词级上下文消歧（音节二元转移）— 突破 delete/substitute top1 天花板

上一节的边界是「合法-合法」歧义只能靠词级模型解。本轮给 Viterbi 叠加
**音节二元转移代价**（上一音节 → 当前音节的平滑概率，数据驱动），把上一音节
的上下文纳入消歧：

- 新增 `correction/transition.py`：惰性加载 `correction/data/bigram_costs.json`，
  代价 = `LAMBDA · (-ln P(cur|prev))`，`LAMBDA = 0.15`。
- `corrector.py` 增加 `correct_syllable_in_context(query, prev)`，并把整串 `correct()`
  升级为**二元 Viterbi**（状态按 `(位置, 最后一个音节)` 展开——精确 bigram，
  不是近似 beam；仅做前沿 beam 剪枝控制规模）。
- 校准脚本 `correction/scripts/calibrate_transition.py`：默认语料
  `wiki.txt + news.txt + daily.txt + recent.txt + baike.txt`，**加一式平滑（s=0.5）**，避免 MLE 下
  「罕见音节仅出现一次就 cost≈0」的过拟合。`daily.txt` 是新增的手写日常口语
  语料（70 句），补新闻类语料缺口语短语（如「你好」）的洞——否则 `nihao`
  会被整串错切成单音节 `niao`。
- 指标 `correction/metric.py [C]`：对语料里见过的二元 `(prev, s)`，把 `s` 打成
  非合法错串，对比「无上下文 ↔ 有 prev 上下文」的 top1。

结果（`python3 -m correction.metric`，[C] 采样 5000 个二元）：

| 错字类型 | 无上下文 top1 → 有上下文 top1 | top3 | flip / broken |
|---|---|---|---|
| delete | 13.18% → **20.42%**（+7.2） | 55.38% → 55.20% | 558 / 196 |
| insert | 79.16% → 80.34%（+1.2） | 98.90% → 97.86% | 253 / 194 |
| substitute | 31.96% → **40.22%**（+8.3） | 67.26% → 72.30% | 546 / 133 |

flip ≫ broken，说明转移模型是净收益，精准命中了此前标出的两个烂点
（delete/substitute top1 偏低）。

**语料扩展 `recent.txt` + `baike.txt`**：`recent.txt` 是人民网 RSS 当前新闻
（去重后 4000 句）；`baike.txt` 是百度百科词条摘要（公开词条卡 API，6129 句 / 150,032 字）。
二者均纳入默认校准。实测诚实结论：新闻语料**近乎持平**（与 news.txt 同分布）；百科语料让
「音节二元消歧」增益**收窄**（delete +7.2 / substitute +8.3，仍为正但低于单新闻时的 +10）——
因为百科把音节二元空间填得更密，音节级上下文更难消歧。这再次印证：**音节级二元 ≠ 词级
LM**；语料多样性对纠错模块有限，真正的杠杆在 Route A 的 LM 训练（时效 + 规模敏感）。

> 边界（诚实声明）：这只是**音节级**二元转移，不是整词/整句 LM（在 libime 侧）；
> 二元表质量直接受语料覆盖影响。[C] 里 flip 集中在低频二元，说明真正的补强要靠
> LCCC/字幕等更大口语语料（Route A 方向）；`daily.txt` 只是让常见口语短语不被判成
> 「没见过」的最小种子。

**真实整句评测（`correction/sentence_eval.py`，对话域为留出测试集）**：把真实句子转
拼音、随机打错一个音节、整串恢复。下表为**当前采用配置**（在库 5 份书面语料
\+ LCCC 口语 20K 句）下、对话域**全部 112 句**留出集的「有上下文」整句还原率：

| 域 | delete | substitute | insert | adjacent | 四类均值 |
|---|---|---|---|---|---|
| 对话（留出） | 15.3% | 33.3% | 73.0% | 62.2% | **46.0%** |
| 新闻（哨兵） | 8.9% | 33.0% | 67.0% | 63.4% | 43.1% |

补口语语料前后（同一评测协议，详见 `docs/corpus-ladder-report.md`）：

| 训练配比 | 对话（留出集） | 新闻（哨兵） |
|---|---|---|
| 仅书面语料（旧基线） | 26.8% | 45.3% |
| **+LCCC 20K 句** ✅ 已采用 | **46.0%** (+19.2) | 43.1% (−2.2，落在 1σ 内) |
| +LCCC 400K 句 ✗ 弃用 | 48.2% (+21.4) | **26.6% (−18.7)** |

三点结论：(1) **对话域短板确由语料领域错配造成**——补 20K 句口语料把留出集还原率
从 26.8% 拉到 46.0%（约 6σ，非噪声），是本纠错模块迄今最大的一次单步增益；
(2) **不能无限堆料**：在库书面语料合计仅约 30 万字，口语加到 400K 句（约 430 万字，
占比 93%）会把转移表挤成「对话专用表」，新闻域从 45.3% 崩到 26.6%，
两域等权总分的极优点在 **20K 句**；(3) **delete 仍全局最弱**（15.3%），
是音节级最难消歧的错字类型。
→ 单表配比已局部最优；下一步要么**按域分表 + 运行时域适配**，
要么直接转向 libime 整句 LM（天花板远高于二元转移）。

## 复现

```sh
# 1) 评测框架（Python，venv 已备好 pypinyin 等依赖）
cd bench && .venv/bin/python -c 'import pypinyin'   # 自检依赖

# 2) C++ 适配器（openEuler 用 gcc-toolset-13，lib64 布局）
cmake -S cpp -B cpp/build && cmake --build cpp/build -j8

# 3) 基线（sc.dict）
cd bench
.venv/bin/python run_bench.py --corpus corpus/news.txt \
  --cli ../cpp/build/libime_bench \
  --cli-arg <prefix>/lib64/libime/zh_CN.lm \
  --cli-arg <prefix>/share/libime/sc.dict --cli-arg 5

# 4) 纠错模块基准（纯 Python，无需 libime）
cd ..  # 项目根目录
python3 -m correction.metric          # 纠错命中率
python3 -m correction.tests.test_core  # 单元测试

# 5) 先验校准（修改语料后原地重写 prior.py）
python3 -m correction.scripts.calibrate_prior --inplace --alpha 0.030

# 6) 二元转移校准（修改语料后重生成转移代价表，默认含 daily + recent + baike）
python3 -m correction.scripts.calibrate_transition

# 7) 重抓近期新闻语料（人民网 RSS，需网络；在 bench/ 下运行）
cd bench && .venv/bin/python fetch_news_corpus.py --target 6000 --out corpus/recent.txt

# 8) 重抓百度百科语料（公开词条卡 API，需网络；词条表由 jieba 从语料抽名词生成）
cd bench && .venv/bin/python fetch_baike_corpus.py --terms baike_terms.txt --out corpus/baike.txt

# 9) 真实整句纠错评测（需 pypinyin；对话域是留出测试集）
cd .. && python3 -m correction.sentence_eval
```

评测产物 `bench/results/` 不入库（gitignore），随时按上述命令重跑。

## 待开发目标（接手从这里继续）

1. **换更好的语言模型（Route A 真正的下一步）**：本轮已证明「词典更大不涨准确率」，
   杠杆在语言模型。下一步评估（a）更大词表/更高阶 n-gram 的 ARPA LM，
   或（b）本地打分模型重排序；并与（c）用户词库/自学习叠加看增益。
   语料方向：维基百科 + 字幕/对话，避开新闻类（敏感词风险 + 语料偏窄导致 LM 退化）。
2. **M1：fork libime + Fcitx5 addon**（复用 wetype.cpp 的 `EngineProc` 前端资产），
   先做到「能打字、评测可跑」，再进下一步调优。
3. **M2：词库与语言模型调优（评测驱动）**，目标综合（字级）准确率 > 当前基线
   （里程碑定义见 PLAN-A §5）。
4. **M3 进展**：容错纠错模块已建（音节级 CHIME + 二元转移，先验已校准，未集成到
   libime）。本轮完成 (b) 词级消歧——`transition.py` 音节二元转移模型，
   delete/substitute top1 分别 +7.2 / +8.3 pct（见上，含 recent+baike 语料）。剩余：
   (a) 集成到 libime 解码管线（需引擎重建）；(c) 用户自学习/个性化（Linux 普遍
   薄弱），见 PLAN-A §4。
5. **语料扩展**：已补 `recent.txt`（新闻）+ `baike.txt`（百科），二者对纠错无增益；
   真正要补的是日常对话（LCCC）、口语、技术类，并重估句长分布对句级准确率的影响。
6. **可选（低优先级）**：rime-ice `tencent.dict.yaml`（981k 条）**无拼音字段**，
   需 pypinyin 补拼音才能加入；但鉴于本轮结论（词库规模非瓶颈），收益有限。

## 目录

| 路径 | 说明 |
|---|---|
| `PLAN-A.md` | 路线决策、难度评估、全部基线数字与每步结论（主文档） |
| `bench/README.md` | 评测框架用法、指标口径、基线、踩坑、已知限制 |
| `bench/` | 评测框架 + `run_bench.py`/`first_word.py`/`latency_perkey.py`/`convert_rimeice.py` |
| `cpp/libime_bench.cpp` | libime 行协议适配器（整句 + perkey 模式） |
| `correction/` | M3 纠错模块（CHIME 信道 + 音节先验 + 二元转移，纯 Python，不依赖 libime） |
| `correction/scripts/calibrate_prior.py` | 先验校准脚本（从语料统计音节频度生成代价表） |
| `correction/transition.py` | 音节二元转移代价（词级上下文，数据驱动，LAMBDA=0.15） |
| `correction/scripts/calibrate_transition.py` | 二元转移校准脚本（加一式平滑，输出 data/bigram_costs.json） |
| `correction/data/` | 二元转移代价数据（校准脚本生成，入库可复现） |
| `bench/corpus/daily.txt` | 手写日常口语语料（70 句，补新闻语料欠口语短语的洞） |
| `bench/corpus/news.txt` | 人民网新闻 2000 句，主测试集 |
| `bench/corpus/recent.txt` | 近期人民网新闻（当前 RSS，去重后 4000 句），供重训语料 |
| `bench/corpus/baike.txt` | 百度百科词条摘要（6129 句 / 150,032 字），供重训语料 |
| `bench/fetch_baike_corpus.py` | 百度百科语料抓取（公开词条卡 API） |
| `bench/baike_terms.txt` | 百度百科词条名列表（jieba 从语料抽名词生成） |
| `bench/corpus/dialogue.txt` | 手写日常对话/口语/技术留出测试集（113 句，不进训练语料） |
| `correction/sentence_eval.py` | 真实整句纠错评测（打错一个音节 → 整串恢复率） |
| `ANALYSIS.md` | 竞品 / 技术调研 |

> rime-ice 转换产物与 sc.dict 之外的生成文件都在构建机 `native-build/` 下，
> 不入库；入库的是可复现的源码（`convert_rimeice.py`）与评测脚本。
