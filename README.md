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
改为从 wiki+news 73,579 字语料统计 420 音节频度，缩放 alpha=0.035，
先验范围 0.12~0.42（跨度 0.30 < 信道最小差 0.35），只做 tie-break。
校准脚本：`python3 -m correction.scripts.calibrate_prior`

**修复 2：默认替换代价 1.5→1.2。**
原 COST_SUB_DEFAULT=1.5 与键盘邻接 0.6 差 0.9，先验（跨度 0.30）永远翻不了盘。
降至 1.2 后差缩到 0.6，substitute top3 提升 +11.8 pct，其他指标不受影响。

纠错命中率（`python3 -m correction.metric`，15 项单元测试全通过）：

| 指标 | 修复前 | 修复后 | 提升 |
|---|---|---|---|
| 未命中·delete top1 / top3 | 8.97% / 16.62% | 11.35% / 45.38% | +2.4 / +28.8 |
| 未命中·insert top1 / top3 | 23.57% / 44.05% | 78.10% / 98.81% | +54.5 / +54.8 |
| 未命中·substitute top1 / top3 | 6.90% / 20.24% | 28.10% / 59.76% | +21.2 / +39.5 |
| 混淆排序 top2 | 23.26% | 87.21% | +64.0 |

> 边界：音节级纠错无法区分「打错成另一个合法音节」和「本来就该是它」
> （如 zh→z 后 "zong" 仍合法）。这类「合法-合法」歧义要词级语言模型解，
> 不在本模块职责内。delete/substitute top1 偏低是此限制的体现。

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

# 5) 先验校准（修改语料后重生成代价表）
python3 -m correction.scripts.calibrate_prior --alpha 0.035
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
4. **M3 进展**：容错纠错模块已建（音节级 CHIME，先验已校准，未集成到 libime）。
   下一步：(a) 集成到 libime 解码管线（需引擎重建）；(b) 词级消歧提升 delete/substitute top1；
   (c) 用户自学习/个性化（Linux 普遍薄弱），见 PLAN-A §4。
5. **语料扩展**：当前主测试集仅人民网新闻（平均 19.1 字/句，句级偏难）；
   应补日常对话（LCCC）、口语、技术类，并重估句长分布对句级准确率的影响。
6. **可选（低优先级）**：rime-ice `tencent.dict.yaml`（981k 条）**无拼音字段**，
   需 pypinyin 补拼音才能加入；但鉴于本轮结论（词库规模非瓶颈），收益有限。

## 目录

| 路径 | 说明 |
|---|---|
| `PLAN-A.md` | 路线决策、难度评估、全部基线数字与每步结论（主文档） |
| `bench/README.md` | 评测框架用法、指标口径、基线、踩坑、已知限制 |
| `bench/` | 评测框架 + `run_bench.py`/`first_word.py`/`latency_perkey.py`/`convert_rimeice.py` |
| `cpp/libime_bench.cpp` | libime 行协议适配器（整句 + perkey 模式） |
| `correction/` | M3 音节级纠错模块（CHIME 噪声通道，纯 Python，不依赖 libime） |
| `correction/scripts/calibrate_prior.py` | 先验校准脚本（从语料统计音节频度生成代价表） |
| `bench/corpus/news.txt` | 人民网新闻 2000 句，主测试集 |
| `ANALYSIS.md` | 竞品 / 技术调研 |

> rime-ice 转换产物与 sc.dict 之外的生成文件都在构建机 `native-build/` 下，
> 不入库；入库的是可复现的源码（`convert_rimeice.py`）与评测脚本。
