# pinyin-ime

> **项目开发规约见 [PROJECT_RULES.md](PROJECT_RULES.md)**（版本号递增 / docs 文档命名 / CI/CD / 重构原则 / 协作语言与注释 / 主动优化建议，每轮开发前必读）。

用 libime 做「好用的拼音输入法」。**M0 评测框架、Route A 语言模型实验、M3 纠错模块均已完成**，
当前推进 **M1：最小可运行输入法**（`cpp/pinyin_ime_cli.cpp`，复用 libime 引擎的交互式命令行输入法，
已打通「打字 → 候选 → 选词上屏」，headless 沙箱即可验证「能打字」）。路线决策与技术选型详见
[PLAN-A.md](PLAN-A.md)，评测框架用法/口径/踩坑详见 [bench/README.md](bench/README.md)。

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

## 引擎解锁 + 口语语料 + 词库线（2026-09-25，本沙箱）

### Route 1 引擎解锁（openEuler 22.03 **aarch64** · conda-forge gcc 15，含 4-gram）

本沙箱与 §14 那台「只读 /usr」沙箱不同（`/usr` 可写、dnf 可用），但 dnf 里
**只有 gcc 10.3.1、无 gcc-toolset-13、无 fcitx5 包**，故「原生 gcc-toolset」路走不通，
落到 §14 的 conda-forge 路线。从零搭环境全套步骤固化在 `scripts/setup_build_env.sh`，
编译复用 `scripts/build_conda_forge.sh`：

- micromamba 2.9.0 + conda-forge **gcc 15**（aarch64 工具链 + boost/fmt/zstd/libuv/ECM/gettext/libuuid）
- fcitx5 5.1.23（GitHub archive）+ libime 1.1.17（git clone）+ kenlm（**codeload 归档**，
  因 github 的 git 智能协议在本沙箱偶发「HTTP2 framing / connection timed out」）
- **开了 `KENLM_MAX_ORDER=4`**（为 4-gram 预留，免二次重建）；同步放大
  `languagemodel.h` 的 `StateSize`（20→32），否则
  `sizeof(void*)+sizeof(lm::ngram::State)<=StateSize` 静态断言编译失败
- 数据 tarball 从 `download.fcitx-im.org/data/` 下载；系统/conda 双份 zstd 解包

结果：**基线逐字复现** —— 句级 39.55% / 字级 88.82% / 码长 3.099；
`zh_CN.lm` 34,736,327 B、`sc.dict` 4,489,669 B（gcc 15 复现值，与 §14 一致）。
KenLM 训练工具 `lmplz` 一并产出；冒烟 `nihao→你好 / zhongguo→中国 / woshiyige→我是一个` 过。

### 口语语料（LCCC-large，modelscope 源）

`bench/fetch_lccc.py`：从 modelscope `OmniData/LCCC` 下载 LCCC-large（611MB /
解压 1.5GB，`lccd.json`），流式清洗为「句子/行、无空格纯文本」（分词交给
`scdict_segment.py` 保证栅格与 sc.dict 兼容）。**全量清洗产物 30,548,768 句（1GB）**，
落 `native-build/corpus/`（不入库）。许可为 LCCC 自定义（研究用途），分发需另评估。

### 口语留出评测集 + 口语域基线

`bench/make_lccc_eval.py`：从清洗产物按句子 md5 确定性抽 ~2000 句，产出
`bench/corpus/lccc_eval.txt`（1866 句，已入库）。口语域 LM 基线（当前 sc.dict）：

| 域 | 句级 | 字级 | 码长 |
|---|---|---|---|
| 新闻（news.txt，原基线） | 39.55% | 88.82% | 3.099 |
| 口语（lccc_eval.txt） | **49.24%** | 87.78% | 2.841 |

口语域句级反比新闻高（句子更短），字级低约 1 点（口语词汇/新表达是当前短板）。
这是 Phase B「换口语 LM」训练时的对照基线。

### 词库线（进行中）

补现代汉语词典词表 + 搜狗/微软词库。**注意 §13 已证「换大词典无增益」**，此线价值
在**真实词频校准**（搜狗/微软反映用户实际输入，最直接能抬首词命中率）与
**专业领域覆盖**（THUOCL 的 IT/医学/金融/法律/成语/地名），不在词库规模。来源盘点：

| 来源 | 许可 | 规模 | 状态 |
|---|---|---|---|
| THUOCL（清华词库） | MIT | 11.2 万词 + 词频（11 分类） | 已下载，`native-build/corpus/dict/` |
| 现代汉语词典词表（gboard_dict_3） | 仓库标 MIT，源为版权词典**词条表** | 12MB Gboard 二进制 | 待反解，或另找纯文本词条源 |
| 搜狗词库（.scel 细胞词库） | 专有（自由下载/研究用途） | — | 待写转换器 |
| 微软拼音词库 | 专有、内嵌 Windows | — | 最难取，暂缓 |

> 许可红线：真正的《现代汉语词典》含释义、受版权保护，扫描版是盗版不能用；
> 可公开用的是它的**纯词条表**（词条近乎客观事实，风险远低于释义）。

## M1：最小可运行输入法（能打字，2026-09-26）

按 PLAN-A §5 的 M1「能打字、评测可跑」，落地一个**命令行的可运行输入法**
`cpp/pinyin_ime_cli.cpp`（复用 libime 引擎；Fcitx5 图形 addon 是 M1 的剩余项）。
它把 `libime_bench` 的 `PinyinIME + PinyinContext` API 升级成真正的交互前端：

- **行模式**（stdin 非 TTY）：每行一段连续全拼 → 输出 `\t` 分隔候选（第一项整句首选），
  供脚本 / 评测回归，协议与 `libime_bench` 一致。
- **交互模式**（stdin 为 TTY）：raw mode 逐键输入；`a-z / '` 输入拼音、退格删除、
  `1-9` 选词上屏、空格 / 回车首选上屏、Esc 清空、Ctrl-C 退出。preedit 显示拼音音节切分，
  候选实时刷新，提交走 `selectCandidatesToCursor()` + `learn()`（会话内自学习）。
- **简拼（默认）+ 模糊音（`--fuzzy`）**：首字母输入如 `bjdx→北京大学` 由 libime 原生支持
  （无需额外开关）；`--fuzzy` 启用平翘舌（z/zh、c/ch、s/sh）、前后鼻音（an/ang、en/eng、
  in/ing、ian/iang、uan/uang）、n/l、f/h、l/r 及键盘邻近纠错（`PinyinFuzzyFlag`），
  常见打错也能命中首选。另有 `--help` 查看用法。
- **用户词库自学习持久化（`--user-data DIR`）**：选词上屏时 `learn()` 把选中句子写入用户
  语言模型历史、并把系统词典没有的词自动加入用户词典；退出时落盘 `user.history` + `user.dict`，
  下次启动自动加载——让用户常用词/新词在简拼（歧义大）下更靠前。默认目录
  `~/.local/share/pinyin-ime`。

### 构建与运行

```sh
# 1) 引擎构建（一次性，见 scripts/setup_build_env.sh + build_conda_forge.sh）
bash scripts/setup_build_env.sh && bash scripts/build_conda_forge.sh
#    产出 libime 1.1.17 + zh_CN.lm(34,736,327 B) + sc.dict(4,489,669 B)，逐字节复现基线

# 2) 编译 CLI + 行协议适配器（conda 工具链）
ENV=/workspace/native-build/mamba-env; OPT=/workspace/native-build/opt
export PATH="$ENV/bin:$PATH"
cmake -S cpp -B cpp/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$OPT/fcitx5;$OPT/libime;$ENV"
cmake --build cpp/build

# 3) 行模式冒烟
LD_LIBRARY_PATH="$OPT/libime/lib:$OPT/fcitx5/lib:$ENV/lib" \
  bash -c 'printf "nihao\nzhongguo\n" | cpp/build/pinyin_ime_cli "$0" "$1" 5' \
  "$OPT/libime/lib/libime/zh_CN.lm" "$OPT/libime/share/libime/sc.dict"

# 4) 交互模式（真人打字，需 TTY）
LD_LIBRARY_PATH="$OPT/libime/lib:$OPT/fcitx5/lib:$ENV/lib" \
  cpp/build/pinyin_ime_cli "$OPT/libime/lib/libime/zh_CN.lm" "$OPT/libime/share/libime/sc.dict"
```

### 一键复现 + 自动回归（2026-09-26，本沙箱）

构建 / 运行 / 自检均改为一键脚本（构建目录可用 `NB` 环境变量覆盖，默认
`/workspace/native-build`，三个构建脚本统一走该变量）：

```sh
bash scripts/setup_build_env.sh && bash scripts/build_conda_forge.sh && bash scripts/build_cli.sh  # 一次性构建 + 编译
echo "nihao" | bash scripts/run_cli.sh   # 行模式；去掉管道直接跑即交互模式
bash scripts/smoke_test.sh               # 「能打字」自动回归（top1 断言）
```

本沙箱 2026-09-26 从零复现：conda-forge 工具链当前检出 gcc **16.2.0**
（脚本按 gcc 15 设计、conda-forge 已前移，编译/运行仍正常），libime 1.1.17 +
`zh_CN.lm`（34.7MB）+ `sc.dict`（4.5MB）逐项产出；`smoke_test.sh` 20/20 通过
（全拼 / 简拼 / 模糊音 / 双拼），交互模式（伪终端逐键 `nihao` 空格上屏「你好」）通过。

### 双拼 + 候选分页（2026-09-26）

- **双拼**（`--shuangpin` / `--sp-scheme NAME`）：复用 libime 原生 `ShuangpinProfile`，
  内置 8 套方案（小鹤/自然码/微软/紫光/智能ABC/中文之星/拼音加加/国标），默认小鹤；
  行模式 / 交互模式通用。小鹤冒烟 8 例全命中：`nihc→你好`、`vsgo→中国`、
  `bwjkdaxt→北京大学`、`ufme→什么`、`veli→这里`、`iifj→吃饭`、`jiuu→技术`、
  `wouiyige→我是一个`。
- **候选分页**：交互模式从「只展示 top-n」升级为每页 9 个候选，`,`/`[` 上一页、
  `.`/`]` 下一页，页尾显示 `[当前/总页]`，选字键 1-9 作用于当前页。整句输入会多拉
  n-best（交互态 `setNBest(45)`）供翻页；短输入（单声母）则把该声母下全部单字候选
  呈现出来可翻页。

### 中英切换 + 中文标点（2026-09-27，本沙箱从零复现构建）

让命令行输入法从「能打字」向「能真正日用」再走一步，补上两个此前「待实现」的体验项：

- **中英切换（`Tab`）**：交互模式下按 `Tab` 在中文/英文态之间切换。英文态下输入
  `a-z / 0-9 / 空格 / 半角标点` 原样进入一行缓冲（状态行显示 `英文: <缓冲>`），
  回车整行上屏、退格删末字符、`Esc` 清缓冲并切回中文、`Tab` 仅切回中文。解决
  「打字半途想敲英文只能清空重来」的根本不顺手。
- **中文标点（半角 → 全角）**：`,`→`，`、`.`→`。`、`;`→`；`、`:`→`：`、`?`→`？`、
  `!`→`！`、`(`→`（`、`)`→`）`、`[`→`【`、`]`→`】`、`\`→`、`、`"`→`“`/`”`（引号智能配对，见下节）。
  空闲时按标点**直接上屏**；打拼音中按标点会**先上屏当前首选**
  再出标点，复用已有 `select_and_commit`，不新增解码逻辑（纯前端）。
- **回归护栏**：新增 `scripts/test_interactive.py`（pty 驱动，headless 可跑），
  断言四条链路——整句上屏、中英切换、空闲标点、打拼音中先上屏再标点；行模式
  `smoke_test.sh` 仍 20/20 通过（基线与改动均在本沙箱逐字节复现）。

> 实现说明：中英切换与中文标点都是**交互前端**（`termios` raw 循环）内的状态机改动，
> 不触碰 libime 解码管线，因此无需重建引擎、也不影响行模式评测口径。

### 引号智能配对（2026-09-27）

把中文标点的 `"` 从「朴素单边开引号（恒为 `“`）」升级为**开闭配对**：会话内维护一个
`quoteOpen` 状态，`"` 第一次按出 `“`、第二次出 `”`、如此交替。上屏语义沿用中文标点的
「空闲直接上屏 / 打拼音中先上屏首选再出标点」，仅引号字符随状态翻转为开/闭。

- **实现**：`chinese_punct()` 移除 `"` 的固定映射，主循环新增 `ch == '"'` 专有分支——
  空闲时按 `quoteOpen` 翻转输出 `“`/`”`；打拼音中先 `select_and_commit` 首选再出引号。
  纯前端，不触碰 libime 解码管线，无需重建引擎。
- **回归护栏**：`test_interactive.py` 新增两例——空闲三次 `"` 断言 `“→”→“` 交替、
  打拼音中 `nihao`+`"` 断言先上屏「你好」再出 `“`。交互回归 4 → 6 例，本次 **6/6 通过**；
  行模式 `smoke_test.sh` 仍 **20/20 通过**（无引擎改动，基线不变）。
- **边界**：单引号 `'` 仍是拼音隔音符（`xi'an`），未纳入引号配对；`'`→`‘`/`’` 需另设
  触发键或上下文规则，留待后续。

### 光标移动 / 逐词分段（2026-09-27，CLI + addon 双侧）

让「整句输入 → 逐词纠错」成为可能：方向键按**词边界**移动候选光标，把候选焦点收窄到
某个词，逐个确认/重选某一段，而非「要么整句要么重打」。纯前端，复用 libime 既有 API
（`pinyinBeforeCursor()` / `pinyinAfterCursor()` 沿最优路径的段边界步进——实测
`woshiyige` 边界 0→2→5→7→9，左右箭头各跳一段），不触碰解码管线、无需重建引擎。

- **命令行（`pinyin_ime_cli`）**：交互循环从「逐字节读」升级为逻辑键解析 `read_key()`，
  识别 `\x1b[D`（←）/ `\x1b[C`（→）等转义序列（50ms 探测区分单独 Esc 与方向键）；左右箭头
  调 `setCursor()` 按词边界移动候选光标，预编辑用 `preeditWithCursor()` 渲染、在光标处插 `▏`。
- **Fcitx5 addon**：`keyEvent` 增加 `FcitxKey_Left/Right` 分支，同样按词边界移动光标；
  预编辑改用 `preeditWithCursor()` 并用 `Text::setCursor()` 把候选光标传给候选窗渲染
  （无拼音时方向键不消费，交给应用移动文本光标）。
- **回归护栏**：`test_interactive.py` 新增 2 例——多词往返（`woshiyige` + ←→ + 空格断言
  「我是一个」）、逐词分段到句首（`woshiyige` + ←×4 + 空格断言「我是一个」），交互回归
  **6 → 8 例全过**；`smoke_test.sh` 仍 **20/20**（无引擎改动）。

### 验证结果

- 冒烟：`nihao→你好`、`zhongguo→中国`、`woshiyige→我是一个`、`beijingdaxue→北京大学` ✅
- 交互（伪终端实测）：逐键 `ni hao` 候选实时刷新、空格上屏「你好」 ✅
- 简拼（首字母输入，默认开启）：`bjdx→北京大学`、`zg→中国`、`zggh→中国国航`、`tx→体系`、`js→技术` ✅
- 模糊音（`--fuzzy`）：`cifan→吃饭`、`senme→什么`、`zeli→这里`、`lanfang→南方` 四例
  从「打错」全部命中首选（无 `--fuzzy` 时依次是 此番 / 森么 / 啫喱 / 兰芳）✅
- 评测回归（M0 框架）：oracle 100%/100% 自检通过；libime 引擎 news.txt 2000 句
  **句级 39.65% / 字级 88.83% / 码长 3.099**，复现基线（39.55% / 88.82% / 3.099，差异为浮点噪声）✅

### 简拼准确度评测（2026-09-26）

新增 `bench/jianpin_eval.py`（整句简拼）与 `first_word.py --jianpin`（词级简拼），
输入从全拼换成首字母简拼（每音节取首字母，复用 `pinyinbench.core.hanzi_to_jianpin`），
量化「只打首字母」的还原率。

| 口径 | 简拼 | 全拼（同口径对照） |
|---|---|---|
| 整句·句级 | 1.75% | 39.65% |
| 整句·字级 | 27.97% | 88.83% |
| 整句·平均码长 | **1.000 键/字** | 3.099 键/字 |
| 词级 top1 / top5 | 14.15% / 30.92% | 77.30% / 91.74% |

按词长分档（jieba 对 news.txt 分词 6649 个 2–6 字词，简拼 top1）:

| 词长 | 词数 | top1 | top5 |
|---|---|---|---|
| 2 字 | 5028 | 6.1% | 23.2% |
| 3 字 | 984 | 26.7% | 45.2% |
| 4 字 | 611 | 57.0% | 69.6% |
| 5 字 | 18 | 88.9% | 88.9% |
| 6 字 | 8 | 87.5% | 87.5% |

**结论**：简拼把平均码长从 3.099 压到 1 键/字（省 68% 击键），但纯首字母信息量有限，
命中率随**词长**急剧变化——2 字词简拼歧义极大（top1 仅 6%），4 字词过半命中，
5 字及以上（成语/短语/专名）接近无歧义（top1 ≈88%）。`bjdx→北京大学` 之所以稳定，
正因其是 4 字且首字母组合唯一。**简拼适合长词/短语；短词歧义大，宜走全拼或候选选词**。

> 说明：headless 沙箱无显示服务器，故「能打字」以终端 CLI 落地；Fcitx5 Qt addon 需图形环境，
> 是 M1 的下一步（见「未来优化方向」）。

## 数据与产物存放位置（三处）

> git 只放「可复现源码 + 小体积评测语料」；大体积语料走 S3；编译产物留在各构建机
> `native-build/`（不入库、可重跑）。

| 位置 | 存什么 |
|---|---|
| **Git 仓库** | 全部脚本/文档 + 小评测语料（news/recent/baike/wiki/daily/dialogue/lccc_eval） |
| **S3（CST Cloud）** | 大体积语料（LCCC 原始 611MB + 清洗 1.06GB）+ THUOCL 词库 |
| **构建机 `native-build/`** | libime/fcitx5 编译产物、`zh_CN.lm`/`sc.dict`、`lmplz`、清洗中间产物（可重跑） |

### S3 明细（CST Cloud 科技云 · 桶 `xbz` · 前缀 `pinyin-ime/`）

endpoint `https://s3.cstcloud.cn`，ACL private（需自有凭证才能访问，本仓库不含任何密钥）。

| S3 路径（相对 `s3.cstcloud.cn`） | 内容 | 大小 | 谁生成 |
|---|---|---|---|
| `xbz/pinyin-ime/corpus/LCCC-large.zip` | LCCC 原始数据（modelscope `OmniData/LCCC`） | 611MB | `bench/fetch_lccc.py` 的输入源 |
| `xbz/pinyin-ime/corpus/lccc_full.txt` | 全量清洗口语语料（30,548,768 句） | 1.06GB | `bench/fetch_lccc.py` 清洗产物 |
| `xbz/pinyin-ime/dict/thuocl/` | THUOCL 词库（MIT，11 分类，`词\t词频`） | ~2.7MB | 手工下载 |

拉取（用你自己的 rclone/S3 凭证，`<remote>` 指向 `s3.cstcloud.cn`）：

    rclone copy <remote>:xbz/pinyin-ime/corpus/ ./corpus/

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

# 10) 引擎原生构建（micromamba+conda-forge gcc 15，含 4-gram；跑完见 3) 基线复现）
bash scripts/setup_build_env.sh && bash scripts/build_conda_forge.sh

# 11) LCCC 口语语料：下载+清洗（先小样本 5000 句验证，再全量）
cd bench && python3 fetch_lccc.py --data-dir /workspace/native-build/corpus/lccc \
  --out /tmp/lccc_sample.txt --max-utterances 5000

# 12) 口语留出评测集 + 口语域基线
python3 make_lccc_eval.py --src /workspace/native-build/corpus/lccc_full.txt --out corpus/lccc_eval.txt
LD_LIBRARY_PATH=/workspace/native-build/opt/libime/lib:/workspace/native-build/opt/fcitx5/lib:\$LD_LIBRARY_PATH \
  python3 run_bench.py --corpus corpus/lccc_eval.txt --cli ../cpp/build/libime_bench \
  --cli-arg /workspace/native-build/opt/libime/lib/libime/zh_CN.lm \
  --cli-arg /workspace/native-build/opt/libime/share/libime/sc.dict --cli-arg 5
```

评测产物 `bench/results/` 不入库（gitignore），随时按上述命令重跑。

## Fcitx5 addon（系统级输入法，2026-09-27）

把命令行输入法的引擎逻辑真正接进 Fcitx5 框架，注册一个叫 `pinyinime` 的输入法。
新增目录 `fcitx5-addon/`（外部 addon：`libpinyinime.so` + `pinyinime.conf`）。

**结构**（对照 fcitx5 官方 `keyboard` addon 的骨架，两类分工）：

- `PinyinIMEEngine : InputMethodEngine` —— 每 addon 一个，持共享的
  `libime::PinyinIME`（词典 `sc.dict` + 语言模型 `zh_CN.lm`，跨输入上下文共享）。
- `PinyinIMEState : InputContextProperty` —— 每输入上下文一个，持独立的
  `libime::PinyinContext`（候选 / 预编辑 / 选中状态），经 `FactoryFor<>` 按需创建。

**按键处理**（`keyEvent`）：`a-z / '` 进拼音、退格删、空格/回车整句上屏、`1-9`
选字（全局索引 `page_*pageSize + (N-1)`）、`-`/`,`/`=`/`.` 翻页、`Esc` 清空；
上屏走 `selectCandidatesToCursor()` + `learn()`；预编辑 `preedit()`、候选
`candidatesToCursor()` 经 `CommonCandidateList`（`DisplayOnlyCandidateWord`）渲染，
数字选字在引擎里自行处理（与官方 pinyin 一致）。用户词库/自学习历史落
`~/.local/share/pinyin-ime/{user.history,user.dict}`。配置：`NBest`/`PageSize`/
`DataDir`/`UserDataDir`/`EnableFuzzy`。

**验证状态（本沙箱）**：对 fcitx5 5.1.23 + libime 1.1.17 编译通过、链接与工厂符号
（`fcitx_addon_factory_instance_pinyinime`）导出正常、`pinyinime.conf` 生成正确
（`Type=SharedLibrary` / `Category=InputMethod` / 依赖 `core:5.1.23`）。**实机运行仍需
显示服务器 + Fcitx5 守护进程**，本 headless 沙箱无法验收；装到有桌面的 Linux 机即可实测。

### 实机安装（Ubuntu 26.04 示例）

> 前置判断：Ubuntu **GNOME** 默认是 IBus，接 Fcitx5 要手动切换（`im-config` 选
> fcitx5，注销重登）；KDE（Kubuntu/Plasma）自带 fcitx 生态、更顺。二者选一，别让
> IBus 和 Fcitx5 同时抢会话。

```sh
# 1) 依赖：fcitx5 框架 + libime/fcitx5 开发头 + 构建工具
sudo apt update
sudo apt install -y fcitx5 fcitx5-frontend-gtk3 fcitx5-frontend-gtk4   fcitx5-frontend-qt5 fcitx5-configtool im-config   libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev   libimecore-dev libimepinyin-dev libime-data libime-data-language-model   cmake ninja-build git build-essential

# 2) 克隆 + 编译 addon（不用 docker，直接用发行版 libime/fcitx5）
git clone https://gitcode.com/laotongtian/pinyin-ime.git
cd pinyin-ime/fcitx5-addon
cmake -S . -B build -G Ninja -DCMAKE_BUILD_TYPE=Release
cmake --build build
sudo cmake --install build   # 装到 lib/<arch>/fcitx5/ 与 share/fcitx5/addon/

# 3) 确认数据文件位置（找不到就在 conf 里设 DataDir）
dpkg -L libime-data-language-model | grep 'zh_CN.lm'
dpkg -L libime-data | grep 'sc.dict' || dpkg -L libimepinyin0 | grep 'sc.dict'

# 4) 切到 Fcitx5（GNOME 必做，KDE 可跳过）
im-config    # 选 fcitx5 → 确定 → 注销重新登录

# 5) 启用 pinyinime：fcitx5-configtool →「输入法」→ 添加 → 搜 “拼音”/“pinyinime”
#    注销重登后 Ctrl+Space / Super+Space 切换中英，即可打拼音。
```

> Ubuntu 仓库自带 `fcitx5-chinese-addons`（内置 pinyin，装上即用）；本项目这个
> addon 是「自己造输入法」的落地——解码/候选/上屏全由 `pinyinime.cpp` 控制，
> 方便继续改词库 / LM / 纠错。

### 中英切换 + 中文标点（addon 迁移，2026-09-27）

把 CLI 里已实现的「中英切换 / 中文标点 / 引号智能配对」迁到 Fcitx5 addon 的
`keyEvent` 前端（纯状态机，不触碰 libime 解码管线），让系统级输入法对齐命令行输入法体验：

- **中英切换（`Tab`）**：每个输入上下文在中文 / 英文态间切换。英文态下 `a-z / 0-9 /
  空格 / 半角标点` 原样进预编辑缓冲（状态行显示 `英文: <缓冲>`），回车整行上屏、
  退格删末字符、`Esc` 清缓冲并切回中文、`Tab` 仅切回中文（缓冲保留）。切走时拼音
  context 不清空，切回即恢复。
- **中文标点（半角 → 全角）**：`,`→`，`、`.`→`。`、`;`→`；`、`:`→`：`、`?`→`？`、
  `!`→`！`、`(`→`（`、`)`→`）`、`[`→`【`、`]`→`】`、`\`→`、`。空闲按标点直接上屏；
  打拼音中按标点先 `selectCandidate(0)` 上屏首选再出标点。
- **引号智能配对（`"`）**：会话状态 `quoteOpen_` 翻转输出 `“`/`”` 交替；上屏语义同中文标点。
- **键码还原**：新增 `asciiFromKey(KeySym, Shift)` 把按键还原成 ASCII 字符，兼容 fcitx5
  「符号已折叠进 keysym」与「基键 + Shift 状态」两套约定（按 US 布局映射数字排/字母
  大小写/标点），`fullwidthPunct` 负责 ASCII → 全角。`,` `.` `[` `]` 打拼音中仍是翻页键、
  空闲才是标点；`-` `=` 保持纯翻页键。

### 已知限制 / 下一步

- 首版已覆盖**整句拼音 + 数字选字 + 翻页 + 中英切换 + 中文标点（含引号配对）+ 光标移动 / 逐词分段**；
  剩**表情**等待迁（模糊音已有 `EnableFuzzy` 开关）。
- 尚未在真实桌面回归；候选窗由 fcitx5 自带 `classicui`/`kimpanel` 模块渲染，本
  addon 不自带 Qt UI。

## 从零复现 + 构建镜像加固（2026-09-27，新沙箱）

本轮在**全新 headless 沙箱**（openEuler 系 aarch64，/usr 可写）从零复跑全套构建，验证
「一键复现」口径，并顺手加固下载镜像/代理（国内直连慢，国外源码/二进制统一走镜像 + 代理）：

- **构建镜像加固**（`scripts/setup_build_env.sh`）：conda 源 `MIRROR` 改为环境变量可覆盖
  （默认清华 tuna，可按需用华为云 `mirrors.huaweicloud.com/anaconda` 等）；`GITHUB_MIRROR`
  默认留空——有 mihomo 代理（`PROXY=http://127.0.0.1:7890`）时直连走代理，无代理才用
  `ghfast.top` 等镜像前缀；micromamba 二进制下载同样吃 `PROXY`，不再硬依赖单一 GitHub 镜像。
- **逐字节复现**：conda-forge 检出 gcc **16.2.0**，libime 1.1.17 + `zh_CN.lm`
  （34,736,327 B）+ `sc.dict`（4,489,669 B）与 README 基线逐字节一致。
- **回归护栏全绿**：`smoke_test.sh` **20/20**、`test_interactive.py` **8/8**；
  命令行输入法「能打字」在所有口径（全拼 / 简拼 / 模糊音 / 双拼 + 中英切换 / 中文标点 /
  引号配对 / 光标移动）再现。

## 待开发目标（接手从这里继续）

1. **换更好的语言模型（Route A 真正的下一步）**：本轮已证明「词典更大不涨准确率」，
   杠杆在语言模型。下一步评估（a）更大词表/更高阶 n-gram 的 ARPA LM，
   或（b）本地打分模型重排序；并与（c）用户词库/自学习叠加看增益。
   语料方向：维基百科 + 字幕/对话，避开新闻类（敏感词风险 + 语料偏窄导致 LM 退化）。
2. **M1：fork libime + Fcitx5 addon**（复用 wetype.cpp 的 `EngineProc` 前端资产）。
   「能打字、评测可跑」已由 `cpp/pinyin_ime_cli.cpp`（命令行前端）达成 ✅；Fcitx5 addon
   已落地并补齐中英切换 / 中文标点 / 引号配对 / 光标移动（编译 + 符号导出通过）✅；剩余是
   **带显示服务器的实机验收**（见「未来优化方向」）。
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

## 未来优化方向（按优先级）

> 已把「构建 → 运行 → 自检」做成可复现一键脚本（`build_cli.sh` / `run_cli.sh` /
> `smoke_test.sh` / `test_interactive.py`），后续每步改动可用 `bash scripts/smoke_test.sh`
> 与 `python3 scripts/test_interactive.py` 做回归护栏；图形 addon 的 XIM 链路与
> DBus + GTK3 im module 链路均已在本 headless 沙箱用 Xvfb 实测通过（见第 8、16 轮），
> 真正缺少的只剩带物理显示器/真桌面的环境做最终视觉验收。

1. **Fcitx5 addon / 图形落地**：✅ 已落地首版并补齐中英切换 / 中文标点 / 引号配对 / 光标移动（`fcitx5-addon/`，见「Fcitx5 addon」一节）——引擎解码 / 候选 / 上屏已接入；✅ XIM 链路（第 8 轮起 Xvfb 实测）与 **DBus + fcitx5-gtk im module 链路（第 16 轮，GTK3 真实应用端到端 10/10）** 均已通过。剩余：① Wayland 真桌面（ENABLE_WAYLAND + im module）、② addon 候选焦点视觉高亮（`setCursorIndex`，headless 下候选窗由 classicui 渲染，视觉打磨不阻塞功能）、③（可选）`EngineProc` 进程外骨架（40ms 防抖 / 异步 IO / 崩溃重启）。
2. **用户词库 / 自学习**：会话内 `learn()` + 用户词自动造词已实现，且已落到**独立用户路径**
   （`--user-data`，`user.history` + `user.dict`，不覆盖系统 `zh_CN.lm`）✅；✅ 上屏即落盘
   （崩溃 / 被 kill 最多丢最后一次上屏的增量，CLI 与 addon 双侧）。剩余：误上屏撤回、跨设备同步
   （PLAN-A §4②）。
3. **容错纠错集成**：✅ 已用 C++ 原生移植（`cpp/pinyin_corrector.*` + `--correct`）替换 Python 每行
   fork 桥接，对拍逐字一致；剩余：接入交互态/addon、与 libime 引擎级联做解码后重排。
4. **输入体验**：双拼✅、候选分页✅、中英切换✅、中文标点✅、双引号智能配对✅、单引号智能配对✅
   （`'` 空闲配对开‘/闭’、打拼音中仍是隔音符 `xi'an`；CLI 与 addon 双侧）；剩余：表情、
   候选部首/偏旁检索、候选窗内水平移动选字。
5. **光标移动 / 逐词分段**：✅ 已实现（`pinyinBeforeCursor/AfterCursor` 按词边界步进，方向键移动候选光标；CLI + addon 双侧，见 M1「光标移动 / 逐词分段」）；✅ `Home/End` 跳句首句尾（2026-09-28 落地，CLI + addon 双侧）；剩余可选项：在候选窗内水平移动选字。
6. **换更好的语言模型**：✅ 已用 500 万句对话语料训出 73MB 对话域 LM，对话字级 87.78%→93.80%；
   ⚠️ 纯对话会过拟合、新闻域崩盘 → 剩余：**混合语料**（对话 + 维基/通用）按配比重训拿两域均衡。

## 目录

| 路径 | 说明 |
|---|---|
| `PLAN-A.md` | 路线决策、难度评估、全部基线数字与每步结论（主文档） |
| `bench/README.md` | 评测框架用法、指标口径、基线、踩坑、已知限制 |
| `bench/` | 评测框架 + `run_bench.py`/`first_word.py`/`latency_perkey.py`/`convert_rimeice.py`/`jianpin_eval.py`（简拼整句评测） |
| `cpp/libime_bench.cpp` | libime 行协议适配器（整句 + perkey 模式） |
| `cpp/pinyin_ime_cli.cpp` | M1 可运行输入法（命令行前端，交互 + 行模式，复用 libime 引擎） |
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
| `scripts/setup_build_env.sh` | 从零搭原生构建环境（micromamba+conda-forge，含 4-gram 补丁与源码下载） |
| `scripts/build_conda_forge.sh` | conda-forge 工具链源码编译 fcitx5 + libime（产出 libime 库 / zh_CN.lm / sc.dict / lmplz） |
| `scripts/build_cli.sh` | 编译 cpp 下的 CLI 与行协议适配器 |
| `scripts/run_cli.sh` | 一键运行输入法（自动注入运行库路径） |
| `scripts/smoke_test.sh` | 「能打字」自动回归（全拼/简拼/模糊音/双拼 top1 断言） |
| `fcitx5-addon/` | Fcitx5 系统级输入法 addon（注册 `pinyinime`，复用 libime 引擎；`pinyinime.h/.cpp` + `CMakeLists.txt`） |
| `scripts/test_interactive.py` | 交互模式回归（pty 驱动：整句上屏 / 中英切换 / 中文标点） |
| `bench/fetch_lccc.py` | LCCC 口语语料下载 + 清洗（modelscope 源） |
| `bench/make_lccc_eval.py` | 从清洗产物抽口语留出评测集 |
| `bench/corpus/lccc_eval.txt` | 口语留出评测集（1866 句，口语域基线） |

> rime-ice 转换产物与 sc.dict 之外的生成文件都在构建机 `native-build/` 下，
> 不入库；入库的是可复现的源码（`convert_rimeice.py`）与评测脚本。
## 开发进度（2026-09-28 · 接手后首轮：headless 从零复现 + 镜像/代理落地）

本轮目标「做成真正能跑起来的输入法」达成——在新沙箱从零复现全套构建并全量回归通过，全程未用 docker：

- **环境**：headless 沙箱，系统 gcc 10.3.1、dnf 无 gcc-toolset / fcitx5 包 → 走
  `scripts/setup_build_env.sh` 的 conda-forge 工具链（本机检出 **gcc 16.2.0** + cmake 4.2.3、
  ninja 4.x）。构建产物落在 `NB=/root/workspace/native-build`（本机持久工作区；脚本默认
  `/workspace/native-build`，统一用 `NB` 覆盖且与仓库脚本约定一致）。
- **镜像 / 代理**：conda-forge 走清华 tuna 镜像；micromamba / fcitx5 / libime / kenlm 等
  GitHub / codeload 源全部走 mihomo 代理 `http://127.0.0.1:7890`（无镜像前缀）。
- **逐字节复现**：`zh_CN.lm` 34,736,327 B、`sc.dict` 4,489,669 B，与 README 基线一致。
- **引擎编译**：fcitx5 5.1.23 + libime 1.1.17（含 kenlm + zh_CN.lm + sc.dict）+ `lmplz`。
- **CLI 编译**：`cpp/build/pinyin_ime_cli` + `cpp/build/libime_bench` 通过。
- **回归护栏全绿**：`smoke_test.sh` **20/20**（全拼/简拼/模糊音/双拼），
  `test_interactive.py` **8/8**（整句上屏 / 中英切换 / 中文标点 / 引号配对 / 光标移动+逐词分段）。

### 本轮复现命令（固化，可一键重跑）

```sh
cd pinyin-ime
export NB=/root/workspace/native-build
export MIRROR=https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge
export PROXY=http://127.0.0.1:7890                      # mihomo 代理，国外源走它
bash scripts/setup_build_env.sh                         # 环境 + 工具链 + 源码
bash scripts/build_conda_forge.sh                       # 引擎编译
bash scripts/build_cli.sh                               # 编译 CLI
echo nihao | bash scripts/run_cli.sh                    # 行模式
bash scripts/smoke_test.sh                              # 20 例回归
python3 scripts/test_interactive.py cpp/build/pinyin_ime_cli \
  "$NB/opt/libime/lib/libime/zh_CN.lm" "$NB/opt/libime/share/libime/sc.dict"
```

### 未来优化方向（下一轮按优先级聚焦，详见「待开发目标」）

1. **Route A 换语言模型**：用维基 + 字幕/对话语料（避开新闻类语料源）训练更高阶 n-gram /
   本地重排序，抬整句准确率（现基线句级 39.55% / 口语域 49.24%）。
2. **M3 纠错集成**：把 `correction/`（CHIME 信道 + 音节二元转移）接进 libime 解码管线
   （需引擎重建）。
3. **用户词库/自学习**：误上屏撤回、崩溃未落盘增量防丢、跨设备同步。
4. **输入体验**：表情、单引号智能配对（与 `xi'an` 隔音符冲突，需另设触发规则）、Home/End
   跳句首句尾、候选部首/偏旁检索。
5. **Fcitx5 addon 实机验收**：首版已编译 + 符号导出通过，剩余带显示服务器的桌面机实测。

### 本轮其他发现（镜像踩坑，供后续参考）

- 华为云 `mirrors.huaweicloud.com/anaconda/cloud/conda-forge` 镜像的 repodata `.zst` 分片
  与 libmamba 不兼容（报 `ZSTD decompression error: Unknown frame descriptor`），已弃用；
  阿里云 / 腾讯云的同路径 conda-forge 一返回 404。建议固化用清华 tuna（仓库默认）或教育网
  cernet 镜像。


## 开发进度（2026-09-28 · 第2轮：Home/End 跳句首句尾 + 全栈重建，回归 20/20 + 10/10）

本轮在全新 headless 沙箱从零复现全套构建（未用 docker），并在「能打字」基础上补上
一个真实输入体验项——**Home/End 跳句首句尾**（候选光标一键跳到整句拼音的首/尾），
CLI 与 Fcitx5 addon 双侧同步实现。

- **从零重建（镜像 + 代理）**：conda 源走清华 tuna 镜像；micromamba / fcitx5 / libime /
  kenlm 等 GitHub/codeload 源走 mihomo 代理 `http://127.0.0.1:7890`（本回合代理提前就绪，
  未用镜像前缀）。工具链检出 gcc 16.2.0 + cmake 4.2.3 + ninja；引擎产物 `zh_CN.lm`
  34,736,327 B、`sc.dict` 4,489,669 B 逐字节复现基线。
- **Home/End（新功能）**：`pinyin_ime_cli` 的 `read_key()` 重构为 `read_esc_byte()` 助手，
  新增识别 `\x1b[H`/`\x1b[F`（xterm）、`\x1bOH`/`\x1bOF`（应用模式）、
  `\x1b[1~/4~/7~/8~`（旧终端）四族序列；主循环 **Home→`setCursor(0)`、End→`setCursor(size())`**
  （`size()` 即输入总字符数，`PinyinContext` 继承自 `fcitx::InputBuffer`）。addon 侧在
  `keyEvent` 增加 `FcitxKey_Home/End` 分支（无拼音时不消费，交给应用移动文本光标）。
  语义：光标在 0 与结尾时 `candidatesToCursor()` 均等于全句候选（`needCandidatesToCursor()`
  返回 false），故 Home/End 只改候选光标位置、不改变整句上屏结果。
- **回归护栏**：`test_interactive.py` 新增 ⑨Home / ⑩End 两例（`←` 到词中 → Home/End 跳回 →
  空格整句上屏，断言「我是一个」；若 Home/End 未实现，`\x1b[H/F` 会被误当 Esc 清空、
  断言失败）。交互回归 **8 → 10 例全过**；`smoke_test.sh` 仍 **20/20**。
- **addon 编译 + 符号导出**：`libpinyinime.so` 编译通过，`fcitx_addon_factory_instance_pinyinime`
  导出正常，`pinyinime.conf` 生成正确（依赖 `core:5.1.23`）。

### 本轮复现命令（固化）

```sh
cd pinyin-ime
export NB=/root/workspace/native-build
export MIRROR=https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge
export PROXY=http://127.0.0.1:7890
bash scripts/setup_build_env.sh && bash scripts/build_conda_forge.sh && bash scripts/build_cli.sh
bash scripts/smoke_test.sh                                  # 20 例
python3 scripts/test_interactive.py cpp/build/pinyin_ime_cli \
  "$NB/opt/libime/lib/libime/zh_CN.lm" "$NB/opt/libime/share/libime/sc.dict"   # 10 例
cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$NB/opt/fcitx5;$NB/opt/libime;$NB/mamba-env"
cmake --build fcitx5-addon/build
```

### 未来优化方向（下一轮按优先级聚焦，与「未来优化方向（按优先级）」同步）

1. **Fcitx5 addon 实机验收**：编译 + 符号导出已通过，剩余带显示服务器的桌面机实测（headless 沙箱无显示服务器）。
2. **Route A 换语言模型**：用维基 + 字幕/对话语料（避开新闻类敏感语料）训练更高阶 n-gram / 本地重排序，抬整句准确率。
3. **M3 纠错集成**：把 `correction/`（CHIME 信道 + 音节二元转移）接进 libime 解码管线（需引擎重建）。
4. **输入体验**：表情、单引号智能配对（与 `xi'an` 隔音符冲突，需另设触发规则）、候选窗内水平移动选字、候选部首/偏旁检索。
5. **用户词库/自学习**：误上屏撤回、崩溃未落盘增量防丢、跨设备同步。


## 开发进度（2026-09-28 · 第3轮：按「未来优化方向」逐个推进 5→4→2→3）

按优先级逐项推进待开发目标，每项都落到可验证的代码 / 产物：

### item 5 — 用户词库崩溃防丢 ✅
- 之前 CLI 只在 Ctrl-C 正常退出时才 `save_user_data()`，被 `kill`/崩溃会丢整个会话的增量。
  改为 `commit()` 包装「上屏即落盘」（`select_and_commit` 成功后立即 `save_user_data()`），
  并加 `SIGTERM/SIGHUP` 信号处理（`sigaction` 不带 `SA_RESTART`，让阻塞中的 `read()` 被
  EINTR 打断 → 主循环退出时落盘）。
- 实测：pty 打字上屏「你好」「中国」后 `kill -9`（SIGKILL 无法捕获），`user.history`(76B)
  /`user.dict`(339B) 均已落盘；改动前会全丢。addon 侧此前已在 `selectCandidate()` 尾部
  `saveUserData()`，本就具备。

### item 4 — 单引号智能配对 ✅
- 解决「`'` 与拼音隔音符冲突」：**空闲时 `'` 是中文标点**（开‘/闭’交替）、**打拼音中 `'` 仍是
  隔音符**（`xi'an` → 西安 不受影响）。CLI 与 Fcitx5 addon 双侧同步实现。
- 回归：交互回归 10 → **12 例全过**（新增 ⑪单引号“/”/“交替、⑫`xi'an`→「西安」）。

### item 2 — Route A 换语言模型：训练管线端到端打通 ✅（验证管线，非最终大模型）
- 修复 `lm/scdict_segment.py`：`libime_pinyindict -d` 导出是**空格**分隔，原 `split("\t")`
  会把整行当词、分词必坏；改 `split()`。
- 全链路复现：sc.dict 导出词表（293,115 词）→ 口语语料（手写 daily+dialogue + lccc_eval，
  2047 句，全程脚本统计不读内容）→ FMM 分词 → `lmplz -o 3` → `libime_slm_build_binary`
  （QuantArrayTrie）→ `libime_prediction`，产出可加载的对话域 LM（`zh_CN_dialogue.lm` 196KB）。
- 验证：用新 LM 行模式 `nihao→你好 / zaijian→再见 / xiexie→谢谢 / woshiyige→我是一个`。
  **含义**：Route A「换更好 LM」的机制已可复现，下一轮只需换大语料（S3 的 LCCC 全量 1.06GB
  或维基+字幕）重训，无需再验证工具链。

### item 3 — M3 纠错集成：前端纠错桥 ✅（桥接，非 C++ 原生 port）
- 新增 `correction/fix_pinyin.py`：把（可能打错的）连续全拼经 `corrector.correct()` 纠正，
  输出 `'` 分隔正确音节，供解码前端送解码前先修复。
- 端到端增益实测（无 `--fuzzy`）：`senme` 直解「森么」→ 纠错后「什么」（sen→shen 修复成功）；
  `cifan` 两者皆「此番」——命中 README 已声明的「合法-合法」边界（`ci` 本是合法音节）。
- 边界（诚实声明）：这是**前端桥接**（行模式/评测可用，每行一次 Python fork），未把 CHIME
  信道 + 二元转移 port 进 libime 原生解码（需 C++ 移植 + 引擎重建），也未做常驻纠错服务。

### 回归护栏（全绿）
`smoke_test.sh` **20/20**、`test_interactive.py` **12/12**、`libpinyinime.so` 编译 + 工厂符号导出正常。

### 未来优化方向（下一轮按优先级聚焦）
1. **Fcitx5 addon 实机验收**（headless 无显示服务器，需桌面机）。
2. **Route A 真正换大模型**：管线已通，下一轮拉大语料（维基+字幕/对话）重训高阶 n-gram。
3. **M3 纠错原生集成**：把 CHIME+二元转移 port 进 C++（或常驻 Python 纠错服务 + addon 异步调用），
   替换行模式每行 fork 的桥接。
4. **输入体验**：表情、候选部首/偏旁检索、候选窗内水平移动选字、误上屏撤回。
5. **跨设备同步**（用户词库，PLAN-A §4②）。


## 开发进度（2026-09-28 · 第4轮：M3 纠错 C++ 原生移植 + 真正拉大语料重训 LM）

按上一轮列出的两个「最高杠杆」逐一推进，两项都落地并评测：

### ① M3 纠错 native 集成（C++ 无 Python 依赖）✅
- 把 `correction/corrector.py` 的 `correct()` **移植到 C++**：`cpp/pinyin_corrector.{h,cpp}`
  （音节 Trie + 编辑距离剪枝 + 加权 noisy-channel + 平翘舌/前后鼻音混淆 + 二元 Viterbi）。
- 数据固化：新增 `correction/generate_cpp_data.py` 从 `syllables.py` / `prior.py` /
  `bigram_costs.json`（全部 ASCII 拼音，无敏感文本）生成 `cpp/correction_data.h`
  （420 音节 + 先验 + 62553 二元转移），运行时零文件依赖、零 Python fork。
- CLI 新增 `--correct`：行模式解码前先经 C++ 纠正（替换上一轮 `fix_pinyin.py` 每行 fork 桥接）。
- **对拍自检**：`cpp/pinyin_corrector_test`（CMake 目标）与 `correction.fix_pinyin.py` 同协议，
  **35/35 例逐字一致**（含 `zhing→zhong`、`zongguo→zhong'guo` 等难点）。
- 端到端：`senme` 直解「森么」→ `--correct`「什么」；`zongguo`「宗国」→「中国」。

### ② 拉大语料重训 LM（Route A 真落地）✅ 带重要发现
- 语料：modelscope `OmniData/LCCC`（中文对话，611MB zip → 流式清洗 **500 万句**、**2887 万词**），
  sc.dict 最大匹配分词（唯一词 12.7 万）。
- 训练：`lmplz -o 3`（127,406 词表 / 654 万二元 / 1594 万三元，ARPA 711MB）→
  `libime_slm_build_binary -q 4` → **73MB 对话域 LM**。
- 评测（`run_bench.py` 同口径，对照基线 34MB `zh_CN.lm`）：

  | 域 | 指标 | 基线 | 新对话 LM | 变化 |
  |---|---|---|---|---|
  | 对话 lccc_eval | 句级 | 49.24% | **66.24%** | **+17.0** |
  | 对话 lccc_eval | 字级 | 87.78% | **93.80%** | **+6.02** |
  | 新闻 news（哨兵） | 句级 | 39.55% | 9.85% | −29.7 |
  | 新闻 news（哨兵） | 字级 | 88.82% | 76.16% | −12.7 |

- **结论（诚实）**：Route A「换更好 LM」**成立且放大收益**——对话域（此前最短板）被 500 万句
  对话语料直接打到字级 93.80%。**但纯对话语料过拟合**，新闻域崩盘（与 README 早前记录的
  LCCC 400K 实验同构：口语占比过高把 LM 挤成「对话专用表」）。下一步是**混合语料**
  （对话 + 维基/通用，按域配比）重训拿两域均衡抬升。

### 回归护栏（全绿）
`smoke_test.sh` 20/20、`test_interactive.py` 12/12、C++/Python 纠错对拍逐字一致、addon 编译通过。

### 未来优化方向（下一轮聚焦）
1. **换更好的语言模型（Route A 收官）**：✅ 已完成——拉 520MB 中文维基 + 500 万句对话按 33%/67%
   混合训出 98MB LM，对话字级 87.78%→91.17%、新闻字级 88.82%→85.37%（详见第5轮进度）。
2. **M3 纠错接入交互态 + addon**：`pinyin_corrector` 已是自包含 C++，可直接被 addon include。
3. **Fcitx5 addon 实机验收**（headless 无显示服务器）。
4. **输入体验**：表情、候选部首/偏旁检索、误上屏撤回、跨设备同步。


## 开发进度（2026-09-28 · 第5轮：混合语料重训均衡 LM——Route A 收官）

本轮拉取中文维基语料，与对话语料按不同配比混合重训，完整刻画「换 LM 的域权衡」曲线。

### 语料与管线
- **中文维基**：modelscope `AI-ModelScope/wikipedia-cn-20230720-filtered`（2023 快照、25.4 万
  高质量词条，`completion` 字段为已清洗文本；520MB JSONL，中国 CDN 直连 5MB/s 稳定）。
  新增 `lm/extract_wiki_jsonl.py` 抽取为「句子/行」纯文本（400 万句 / 282MB）。
- 另附 `lm/extract_wiki.py`（zhwiki 官方 bz2 XML 的流式抽取，处理 `{{}}`/`[[ ]]`/`<ref>` 等
  标记；本轮因 wikimedia 走代理偶发断流、改走 modelscope CDN，但脚本保留供 raw dump 用）。
- **混合训练管线**：`lm/train_mixed_lm.sh`（维基分词 → 对话+维基按行混合 → lmplz 3-gram →
  QuantArrayTrie 二值化 → predict）。

### 结果（`run_bench.py` 同口径，对照基线 34MB `zh_CN.lm`）

| 模型 | 对话句级 | 对话字级 | 新闻句级 | 新闻字级 |
|---|---|---|---|---|
| 基线 `zh_CN.lm`（34MB） | 49.24% | 87.78% | 39.55% | 88.82% |
| 纯对话 500 万句（73MB） | **66.24%** | **93.80%** | 9.85% | 76.16% |
| **33% 口语 + 67% 维基（98MB）** | 56.88% | **91.17%** | 26.35% | 85.37% |
| 15% 口语 + 85% 维基（107MB） | 51.85% | 89.40% | 26.00% | 85.45% |

### 结论（诚实）
1. **Route A 成立**：对话语料把对话域（此前最短板）从字级 87.78% 抬到 91~94%；口语占比越高
   对话增益越大。
2. **新闻域退化与口语占比基本无关**：15%↔33% 口语的新闻字级都停在 ~85.4%（−3.4），退化源是
   **训练语料里没有新闻类文本**（按敏感纪律主动避开）——维基≠新闻分布。
3. **最优「对话倾向均衡」= 33% 口语混合**：对话字级 +3.4、新闻字级 −3.5，相对 15% 更强且新闻
   不更差，是「看好对话、可接受新闻小幅回退」下的最优点。
4. 若要新闻域**不退化**：两条路（待下轮）：(a) 引入新闻类语料（与敏感纪律冲突，需审慎）；
   (b) 与官方 `zh_CN.lm` 做**域插值/集成**（LM 分数加权）而非整体替换。

### 回归护栏
本轮只新增语料抽取/训练脚本，未改解码代码；`smoke_test.sh` 20/20、`test_interactive.py` 12/12、
C++/Python 纠错对拍一致均不回归。

### 未来优化方向（下一轮聚焦）
1. **LM 域插值/集成**：把对话倾向 LM 与官方 `zh_CN.lm` 做分数插值（域自适应），不引入新闻语料
   的前提下两域兼得。
2. **M3 纠错接入交互态 + addon**（`cpp/pinyin_corrector` 已自包含）。
3. **Fcitx5 addon 实机验收**。
4. **输入体验**：表情、候选部首/偏旁检索、误上屏撤回、跨设备同步。
## 开发进度（2026-09-28 · 第6轮：从零复现 + 表情输入，回归 20/20 + 14/14）

本轮在新 headless 沙箱（openEuler 22.03 aarch64，/usr 可写、无 docker）从零复刻全套构建，
并在「能打字」基础上新增一个输入体验项——**表情输入（Ctrl-E）**，CLI 侧落地 + pty 回归。

### 从零复现（安装依赖 · 未用 docker）
- **密钥落文件**：GitCode 访问令牌写入仓库外独立文件并 `chmod 600`（不入库、不外显）；
  `git clone` 用 `https://oauth2:<token>@gitcode.com/...` 直连。
- **工具链**：`scripts/setup_build_env.sh` 走 conda-forge（清华 tuna 镜像；华为云 conda-forge
  镜像 repodata `.zst` 与 libmamba 不兼容、已弃，阿里云/腾讯云同路径 404——沿用 README 早前结论）；
  micromamba / fcitx5 / libime / kenlm 等 GitHub / codeload 源走 mihomo 代理
  `http://127.0.0.1:7890`（本回合代理就绪，未用镜像前缀）。本机检出 **gcc 16.2.0** + cmake 4.2.3 + ninja。
- **引擎产物逐字节复现**：`zh_CN.lm` 34,736,327 B、`sc.dict` 4,489,669 B，与 README 基线一致；
  fcitx5 5.1.23 + libime 1.1.17（含 kenlm 子模块 + `lmplz`）。

### 表情输入（Ctrl-E，新功能，CLI 侧）
- 库内置 **27 个「安全中性」常用表情**（分 3 页、每页 9 个），码点用 `\U/\u` 转义写死在
  `pinyin_ime_cli.cpp`（`emoji_list()`），不引入额外数据文件、不依赖运行时资源。
- 交互模式按 **Ctrl-E** 进入表情选择——状态行显示 `表情[1/3]: 1.😀大笑 2.😂笑哭 …`；
  `1-9` 选表情直接上屏并自动退回中文，`,`/`[` 上一页、`.`/`]` 下一页，`Esc`/`Ctrl-E` 退回中文
  （拼音 context 不清空，退回即恢复）。
- 纯前端（复用现有 `output_line` 上屏通道），不触碰 libime 解码管线、无需重建引擎；
  addon 侧迁移留待下一轮（与中英切换/中文标点/引号配对的「先 CLI 后 addon」路径一致）。

### 回归护栏（全绿）
`smoke_test.sh` **20/20**（全拼/简拼/模糊音/双拼）、`test_interactive.py` **14/14**
（新增 ⑬ 表情选中 😀、⑭ 表情翻页选 👍 两例）、C++ 纠错对拍自检通过（`senme→shen'me`、
`zongguo→zhong'guo`；`--correct` 端到端 `senme→什么`）、`libpinyinime.so` 编译 + 工厂符号
`fcitx_addon_factory_instance_pinyinime` 导出正常。

### 本轮复现命令（固化）
```sh
cd pinyin-ime
export NB=/root/workspace/native-build
export MIRROR=https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge
export PROXY=http://127.0.0.1:7890
bash scripts/setup_build_env.sh && bash scripts/build_conda_forge.sh && bash scripts/build_cli.sh
bash scripts/smoke_test.sh                                  # 20 例
python3 scripts/test_interactive.py cpp/build/pinyin_ime_cli \
  "$NB/opt/libime/lib/libime/zh_CN.lm" "$NB/opt/libime/share/libime/sc.dict"   # 14 例
cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$NB/opt/fcitx5;$NB/opt/libime;$NB/mamba-env"
cmake --build fcitx5-addon/build
```

### 未来优化方向（下一轮按优先级聚焦）
1. **Fcitx5 addon 实机验收** + **表情 addon 迁移**：编译 + 符号导出已通过，剩余带显示服务器的
   桌面机实测（headless 沙箱无显示服务器）；把 CLI 的表情选择同步迁到 `pinyinime.cpp` 的 `keyEvent`。
2. **LM 域插值/集成**：把对话倾向 LM 与官方 `zh_CN.lm` 做分数插值（域自适应），不引入新闻语料
   的前提下两域兼得（承接第 5 轮结论）。
3. **M3 纠错接入交互态**：`pinyin_corrector` 已自包含 C++，可接进 CLI 交互态 / addon（需处理
   与简拼的分歧——纠错只对「全拼 + 打错」生效、不误伤简拼）。
4. **输入体验**：候选部首/偏旁检索、误上屏撤回、跨设备同步。

## 开发进度（2026-09-28 · 第7轮：M3 纠错接入交互态 + 表情迁 Fcitx5 addon，回归 20/20 + 17/17）

本轮在全新 headless 沙箱又从零复现全套构建（未用 docker），并按上一轮「未来优化方向」落实两项：
**M3 纠错接入交互态**（`--correct` 现在对交互模式生效）与**表情迁到 Fcitx5 addon**（CLI/addon 共用表情表）。

### 从零构建（安装依赖 · 未用 docker）
- conda 源走清华 tuna 镜像；micromamba / fcitx5 / libime / kenlm 等 GitHub/codeload 源走 mihomo 代理
  `http://127.0.0.1:7890`。工具链检出 **gcc 16.2.0** + cmake + ninja。
- 引擎产物**逐字节复现**基线：`zh_CN.lm` 34,736,327 B、`sc.dict` 4,489,669 B；
  fcitx5 5.1.23 + libime 1.1.17（含 kenlm + `lmplz`）。

### 特性① M3 纠错接入交互态（`--correct`，CLI 侧）
- 此前 `--correct` 只对**行模式**生效；本轮在交互模式「空格/回车」分支接入同一套 C++ 纠错
  （`cpp/pinyin_corrector`，无 Python 依赖）：上屏前先取 `ctx.preedit()` 整串纠错，
  若结果不同则清空重输再 commit 首选。
- **新增 `is_full_pinyin()` 门控**（`cpp/pinyin_corrector.h/.cpp`）：只有「零编辑可完整切分」的
  全拼输入才值得纠错；简拼（`bjdx`/`zg`）、双拼、混合输入（`bjdaxue`）由门控挡下，绝不误伤。
  - 实现：`SyllableTrie::canSegmentZeroEdit()` 简单 reachability DP（420 音节表），零额外依赖。
  - 坑：交互态 `ctx.preedit()` 音节间带空格（渲染 `sen me`），必须先滤空格与隔音符 `'` 再判定，
    否则 `is_full_pinyin("sen me")` 恒 false、纠错静默失效（已修复并有回归用例覆盖）。
- **边界（诚实）**：`zhing`（想打 zhong 但 i/o 打错且结构不成音节）这类「零编辑切分失败」的重错
  全拼仍会被门控挡下、按原样解码，不做纠正——这是「绝不误伤简拼/混合输入」的安全取舍；
  常见「合法音节内打错」如 `senme→shen'me`、`zongguo→zhong'guo` 均正常命中。
- 回归：交互 14 → **17 例**（⑮`senme`→「什么」 ⑯`zongguo`→「中国」 ⑰`zg` 简拼不被误伤）。

### 特性② 表情迁到 Fcitx5 addon（+ 共享表情表）
- 抽出共享头 `cpp/emoji_list.h`（27 个安全中性常用表情 + `emoji_list()`），CLI 与 addon 双侧复用，
  消除「CLI 内置、addon 无法复用」的重复实现。
- addon 侧新增 **Ctrl-E 表情选择**：拦截点放在 `keyEvent` 的「Ctrl/Alt/Super 组合键交给框架」守卫
  **之前**（否则 Ctrl-E 会被框架吃掉）；进入后 1-9 选表情、`,`/`[` 上一页、`.`/`]` 下一页、
  `Esc`/Ctrl-E 退出；表情态渲染走 `updateUI` 的 preedit 页签 + 候选列表，选中走 `commitText`。
- 验证：`libpinyinime.so` 编译通过，工厂符号 `fcitx_addon_factory_instance_pinyinime` 导出正常，
  `pinyinime.conf` 生成正确。实机交互待桌面机验收（headless 无显示服务器）。

### 测试基建修复：隔离用户数据目录（重要）
- 发现：pty 交互回归会 `learn()` 上屏内容到**默认用户目录** `~/.local/share/pinyin-ime`；本回合
  新用例曾把错误候选（森么/笔记大）learn 进去，导致同一沙箱内第二次跑 `smoke_test.sh` 从
  **20/20 漂到 18/20**（`bjdx` top1 变「笔记大小」、模糊音 `senme` top1 变「森么」）——不是引擎回归，
  是测试交叉污染。
- 修复：`smoke_test.sh` 与 `test_interactive.py` 均改用一次性隔离 `--user-data` 目录（`mktemp -d` +
  退出清理），测试之间、测试与真实用户数据之间互不污染。这也是「能真正跑起来的输入法」的
  可重复验证保障。

### 回归护栏（全绿）
`smoke_test.sh` **20/20**、`test_interactive.py` **17/17**、C++/Python 纠错对拍 exit 0、
行模式 `--correct`（`senme→什么` / `zongguo→中国` / `bjdx`/`zg` 不被改）、
Python 纠错单测 20/20、`libpinyinime.so` 编译 + 工厂符号导出正常。

### 本轮复现命令（固化）
```sh
cd pinyin-ime
export NB=/root/workspace/native-build
export MIRROR=https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge
export PROXY=http://127.0.0.1:7890
bash scripts/setup_build_env.sh && bash scripts/build_conda_forge.sh && bash scripts/build_cli.sh
bash scripts/smoke_test.sh                                  # 20 例
python3 scripts/test_interactive.py cpp/build/pinyin_ime_cli \
  "$NB/opt/libime/lib/libime/zh_CN.lm" "$NB/opt/libime/share/libime/sc.dict"   # 17 例（含交互纠错）
./cpp/build/pinyin_corrector_test                            # C++/Python 对拍（exit 0）
cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_PREFIX_PATH="$NB/opt/fcitx5;$NB/opt/libime;$NB/mamba-env"
cmake --build fcitx5-addon/build                             # addon（含表情）
```

### 未来优化方向（下一轮按优先级聚焦）
1. **Fcitx5 addon 实机验收**（含表情/纠错）：编译 + 符号导出均通过，剩余带显示服务器的桌面机实测
   （headless 沙箱无显示服务器）；把交互态纠错也迁到 `pinyinime.cpp`（addon 侧 `--correct` 无开关，
   需配置项 `EnableCorrect`）。
2. **LM 域插值/集成**（承接第 5 轮结论）：把对话倾向 LM（33% 口语 + 67% 维基，98MB）与官方
   `zh_CN.lm` 做分数插值，不引入新闻语料前提下两域兼得。
3. **输入体验**：候选部首/偏旁检索、误上屏撤回、跨设备同步（用户词库，PLAN-A §4②）。
4. **纠错边界扩展**：`zhing` 这类「零编辑切分失败」的重错全拼，评估在门控内加「代价阈值 + 含元音」
   的二级判定（当前严格门控为安全优先，暂不放开）。


## 开发进度（2026-09-28 · 第8轮：安装虚拟桌面实际测试——Fcitx5 pinyinime addon 真机跑通，11/11）

本轮按「安装虚拟桌面实际测试」落地：headless 沙箱里用 **Xvfb 虚拟显示 + 补编 fcitx5-X11（XIM 前端）+
 自编 XIM 测试客户端**，把 pinyinime addon 真正跑起来验证——**这是 addon 首次实机运行**，
并当场发现、修复了 4 个编译期永远发现不了的真实 bug。

### 环境（dnf 安装，未用 docker）
- `xorg-x11-server-Xvfb`（虚拟显示 :99）、`xterm`、`dbus-x11`（session bus）
- 编译依赖：`libxcb-devel xcb-util-devel xcb-util-keysyms-devel xcb-util-wm-devel libX11-devel libXtst-devel libxkbcommon-devel expat-devel fontconfig-devel freetype-devel libpng-devel pixman-devel pango-devel gdk-pixbuf2-devel librsvg2-devel libxkbfile-devel iso-codes nlohmann-json(conda) xorg-libx11 xorg-kbproto expat(conda)` + `libX11` XKB 头补齐（conda gcc 不搜 /usr/include）

### 补编 fcitx5-X11（`$NB/opt/fcitx5-x11`）
- 复用 `scripts/build_conda_forge.sh` 参数，额外开 `-DENABLE_X11=On -DENABLE_KEYBOARD=On -DENABLE_SERVER=On`
  （主程序 `fcitx5` 在 `src/server/`，被 `ENABLE_SERVER=Off` 挡掉）；XIM 前端由 `ENABLE_X11` 控制，无需 DBus。
- **xcb-imdkit 1.0.3**（conda-forge 无此包）源码编入 conda env，提供 XIM 协议库；
- fcitx5 5.1.23 的 GitHub archive 缺 yoga 子模块——按 `.gitmodules` 钉的 commit `042f5013152e`
  （facebook/yoga）补齐，并注释其自带 `add_subdirectory(tests)`（gcc16 下 -Werror 不过）；
- librsvg 兼容补丁：新版（openEuler 2.50/≥2.52）移除 `rsvg_handle_get_intrinsic_size_in_pixels`，
  本地打补丁改用 `rsvg_handle_get_dimensions`（仅本地源码副本，不入库）。
- 产物：`fcitx5` 主程序 + `libxim.so`/`libclassicui.so`/`libxcb.so`/keyboard(内嵌) 等 addon。
- **数据路径适配**：addon 的 DataDir 候选不含「LM 在 lib / dict 在 share」的区分布局——
  实测用 `~/.config/fcitx5/conf/pinyinime.conf` 设 `DataDir=$NB/opt/libime/lib`
  + `/usr/lib64/libime/zh_CN.lm` 软链（addon 硬编码候选路径之一）兜底。

### 实测发现并修复的 4 个 addon 真实 bug（本轮核心价值）
> 之前 addon 只有「编译 + 符号导出」验证，这 4 个问题全部只在真机运行才暴露。

1. **`FactoryFor` 未显式注册**（崩溃）：fcitx5 的 `InputContext::propertyFor(&factory_)` 要求工厂
   已在 `InputContextManager` 注册（`registerProperty("pinyinimeState", &factory_)`），漏注册则
   `slot_=-1`、propertyFor 越界读垃圾指针（state=0x41）→ 首次按键 SIGSEGV。此前 addon 从未运行过，
   所以一直没发现。
2. **引擎数据无懒加载**（ime_ 常 NULL）：`ensureDataLoaded()` 只在 `reloadConfig()` 里调用，而
   fcitx5 启动不保证调用——补上 keyEvent 入口 `ensureDataLoaded()` + 工厂 lambda 内先加载，
   缺数据时优雅放行（不消费）。
3. **空候选列表导致 classicui abort**：`updateUI` 在无候选时仍 `setCursorIndex(0)`，classicui 渲染
   `candidateFromIndex(0)` 直接 abort（`CommonCandidateList: invalid index`）。改为仅在
   `end > begin` 时设置候选列表。
4. **Ctrl+E 匹配语义**：fcitx5 `Key::check(FcitxKey_E)` 是「精确状态匹配」（要求无修饰键），
   Ctrl+E 实际是 `sym=0x45 + states=Ctrl`（XIM 转来小写/大写都有）——必须写
   `check(FcitxKey_E, KeyState::Ctrl) || check(FcitxKey_e, KeyState::Ctrl)`。

### 测试工具（新增，已入库）
- `tools/xim_test_client.c`：XIM 客户端 + XTest 发键，`XOpenIM→XCreateIC→XSetICFocus`，
  `XFilterEvent` 把键交给 IM，`XmbLookupString` 收 commit，按脚本序列驱动（`\s`空格 `\t`Tab
  `\n`回车 `\xNN`Ctrl+字母 `\S`Ctrl+Space，`?`/`"` 等自动带 Shift）。
- `tools/keysend_target.c`：向指定窗口发键（xterm 等标准客户端对照测试）。
- `scripts/xvfb_test_env.sh`：一键起/停 dbus + Xvfb :99 + fcitx5（XIM 环境变量齐全）。
- `scripts/xvfb_ime_test.sh`：pinyinime addon 实测矩阵（11 用例断言）。

### 实测结果（Xvfb :99 · fcitx5 5.1.23 · pinyinime addon 0.5.0）
| 用例 | 上屏 | 结果 |
|---|---|---|
| nihao | 你好 | ✅ |
| zhongguo | 中国 | ✅ |
| zg（简拼） | 中国 | ✅ |
| bjdx（简拼） | 北京大学 | ✅ |
| woshiyige | 我是一个 | ✅ |
| Tab → hello ⏎ | hello（英文态） | ✅ |
| , . ? | ，。？ | ✅ |
| " " | “ ”（配对） | ✅ |
| Ctrl-E → 1 | 😀 | ✅ |
| Ctrl-E → . → 1 | 👍（翻页） | ✅ |
| zhongguo ! | 中国！ （先上屏首选再出标点） | ✅ |

**11/11 通过**；CLI 侧回归不受影响：`smoke_test.sh` 20/20、`test_interactive.py` 17/17、
C++/Python 纠错对拍 exit 0。

### 本轮复现命令（固化）
```sh
cd pinyin-ime
export NB=/root/workspace/native-build
# 1) 补编 fcitx5-X11（首次）
dnf install -y xorg-x11-server-Xvfb xterm dbus-x11 libxcb-devel xcb-util-devel \
  xcb-util-keysyms-devel xcb-util-wm-devel libX11-devel libXtst-devel libxkbcommon-devel \
  expat-devel fontconfig-devel freetype-devel libpng-devel pixman-devel pango-devel \
  gdk-pixbuf2-devel librsvg2-devel libxkbfile-devel iso-codes
# （xcb-imdkit/yoga/rsvg/头补齐等步骤见前述，具体命令较长，已固化为自定义 cmake 序列）
cc tools/xim_test_client.c -lX11 -lXtst -o tools/xim_test_client
cc tools/keysend_target.c -lX11 -lXtst -o tools/keysend_target
# 2) 一键实测
bash scripts/xvfb_test_env.sh start       # 起 dbus + Xvfb :99 + fcitx5
bash scripts/xvfb_ime_test.sh             # 11 用例断言
```

### 未来优化方向（下一轮按优先级聚焦）
1. **addon 实机验收补全**：本轮在 Xvfb 虚拟桌面验证了 XIM 前端链路；剩余带真实桌面的
   DM（GNOME/KDE）+ fcitx5-gtk/qt im module 全链路验收，以及 Wayland 前端编译。
2. **addon 交互态纠错**：第 7 轮 CLI 已实现 `--correct`；addon 侧补配置项 `EnableCorrect` 接入
   `cpp/pinyin_corrector`（全拼门控 is_full_pinyin 已有）。
3. **LM 域插值/集成**（承接第 5 轮结论）：对话倾向 LM 与官方 `zh_CN.lm` 分数插值。
4. **候选体验**：候选窗内水平移动选字、部首/偏旁检索。
5. **把 Xvfb 实测纳入 CI 门禁**：`scripts/xvfb_ime_test.sh` 可作为 addon 变更后的自动回归。


## 开发进度（2026-09-28 · 第9轮：M3 纠错接入 Fcitx5 addon（EnableCorrect），Xvfb 实测 14/14）

承接第 7 轮「CLI 已实现 `--correct`」与第 8 轮「Xvfb 虚拟桌面实测」，本轮把 **M3 纠错正式接进
Fcitx5 addon**，让系统级输入法也具备容错纠错能力，并当场修复了「无 DBus 时 addon 配置不加载」
的隐藏问题。

### 实现
- **配置项**：`PinyinIMEConfig` 新增 `EnableCorrect`（默认 false，与 CLI `--correct` 默认关一致）。
- **keyEvent 空格/回车分支**：`EnableCorrect` 开启且当前拼音是「全拼」时，解码前先经
  `pinyin_corrector::correct_to_string()` 整串纠正再上屏；`is_full_pinyin()` 门控保证
  简拼 / 双拼 / 混合输入不误伤（与 CLI 同一套 C++ 纠错，语义一致）。
- **CMake**：addon 编译源纳入 `../cpp/pinyin_corrector.cpp`（此前仅编译 CLI 侧）。

### 关键发现：无 DBus 时 addon 配置根本不会加载
- fcitx5 的 addon 用户配置（`conf/pinyinime.conf`）默认由 **DBus 模块的 `SetConfig`** 载入
  （`dbusmodule.cpp` 里 `addonInstance->setConfig(...)` 是唯一调用点）；本沙箱 fcitx5 以
  `ENABLE_DBUS=Off` 构建 → 无 DBus 模块 → `setConfig` 永不被调用 → `EnableCorrect`/`DataDir`
  等配置项全部保持代码默认值。
- **修复**：addon 构造函数里显式 `reloadConfig()`（它本就会 `readAsIni(config_, "conf/pinyinime.conf")`
  读用户配置），使配置加载独立于 DBus。这也是「无 DBus 环境」下 addon 自包含的必要步骤。
- **附带修正**：fcitx5 的 ini 配置格式是**顶层叶子 `Name=Value`**（bool 序列化为 `True/False`），
  不是 `[Name]` 块 + `value=`。此前第 8 轮写 conf 的 `[DataDir]` 块格式无效，一直靠
  `/usr/lib64/libime` 软链兜底；本轮一并更正为 `DataDir=<path>` 形式。

### 实测结果（Xvfb :99 · EnableCorrect=True，`scripts/xvfb_ime_test.sh`）
在上一轮 11 例基础上，新增 3 例纠错相关与配置回归：

| 新增用例 | 上屏 | 结果 |
|---|---|---|
| M3 纠错 senme → 什么 | 什么 | ✅ |
| M3 纠错 zongguo → 中国 | 中国 | ✅ |
| 纠错不误伤简拼 zg | 中国 | ✅ |

**Xvfb 实测 14/14**；CLI 侧回归不受影响：`smoke_test.sh` 20/20、`test_interactive.py` 17/17、
C++/Python 纠错对拍 exit 0。

### 本轮复现命令（固化）
```sh
cd pinyin-ime
export NB=/root/workspace/native-build
bash scripts/xvfb_ime_test.sh        # 起环境 + 写 conf(EnableCorrect=True) + 14 例断言
```

### 未来优化方向（下一轮按优先级聚焦）
1. **Wayland 前端 / 真桌面最终验收**：XIM（X11）链路已实测通过；补 `ENABLE_DBUS=On` +
   fcitx5-gtk/qt im module，在带 DBus 与真实桌面的环境做最终验证（含配置经 DBus 载入的路径）。
2. **LM 域插值/集成**（承接第 5 轮结论）：对话倾向 LM 与官方 `zh_CN.lm` 分数插值，两域兼得。
3. **候选体验**：候选窗内水平移动选字、部首/偏旁检索、误上屏撤回。
4. **纠错边界扩展**：`zhing` 这类「零编辑切分失败」的重错全拼（第 7 轮已声明的边界）。
5. **Xvfb 实测纳入 CI 门禁**：`scripts/xvfb_ime_test.sh` 已可一键跑，建议接入 addon 变更后的自动回归。

## 开发进度（2026-09-29 · 第10轮：候选翻页键补全/对齐 + fcitx5-X11 全流程固化为脚本）

本轮目标「继续开发 Linux 输入法」：在新沙箱从零复现全套构建并全量回归通过（未用 docker），
在「真正能跑起来的输入法」基础上补上一块真实的输入体验拼图——**候选翻页键补全与 CLI/addon 双侧对齐**，
并把第 8/9 轮只存在口述里的 fcitx5-X11 补编流程固化成可复现脚本。

### 环境与从零复现（镜像 + 代理，未用 docker）
- **工具链**：conda-forge 走清华 tuna 镜像（本机检出 gcc **16.2.0** + cmake 4.2.3 + ninja）；
  国外源码/二进制（micromamba、fcitx5、libime、kenlm、xcb-imdkit、yoga）统一走 mihomo 代理
  `http://127.0.0.1:7890`；X11/Xvfb 系统库走 dnf（openEuler 仓库）。
- **逐字节复现**：`zh_CN.lm` 34,736,327 B、`sc.dict` 4,489,669 B，与 README 基线一致。
- **回归护栏**：`smoke_test.sh` **20/20**、`test_interactive.py` **20/20**（含新增翻页 3 例）、
  `xvfb_ime_test.sh` **15/15**（14 基线 + 1 翻页）。

### 本轮功能：候选翻页键补全 / 对齐
- **CLI**：翻页键补齐 `-` / `=`（此前只有 `,`/`.`/`[`/`]`，与 addon 不一致），并新增
  `PageUp`（`\x1b[5~`）/ `PageDown`（`\x1b[6~`）。
- **addon**：新增 `FcitxKey_Page_Up` / `FcitxKey_Page_Down` 翻页；默认 `NBest` 5 → **45**
  （对齐 CLI 交互态的候选拉取量，让「每页 9 个、多页」的翻页真正有内容可翻）。
- 中文注释按要求补齐。

### fcitx5-X11 补编流程固化为脚本（本轮可复现性）
- 新增 `scripts/setup_x11_deps.sh`：dnf 装 X11/Xvfb 依赖、conda 装 `nlohmann_json`、
  源码编译 xcb-imdkit 1.0.3、补齐 yoga 子模块（facebook/yoga @042f5013152e）、
  librsvg 2.50 兼容补丁（`rsvg_handle_get_intrinsic_size_in_pixels` → `get_dimensions`）。
- 新增 `scripts/build_fcitx5_x11.sh`：用 conda gcc 16 补编 fcitx5-X11（X11/KEYBOARD/SERVER 开），
  并注入系统头库（`-idirafter /usr/include`、`LIBRARY_PATH=/usr/lib64`、pkg-config 系统路径前置）。
- 数据路径适配：libime「LM 在 `lib/libime`、dict 在 `share/libime`」的区分布局与 addon 的
  `DataDir` 候选不匹配 → `lib/libime/sc.dict` 符号链接补齐；`default/zh_CN` 默认输入法由官方
  `pinyin/rime` 覆盖为 `pinyinime`，使 Xvfb 下 Ctrl+Space 能直接切到本输入法。

### 本轮修复的小问题
- `xvfb_test_env.sh` 的 `ensure_display` 用 socket 判活：Xvfb 被 kill 后残留 `/tmp/.X11-unix/X99`
  僵尸 socket，会误判「还在运行」而不再拉起 → 改为按 `pgrep -x Xvfb` 进程判活。
- `tools/keysend_target.c` 缺 `#include <stdbool.h>`（`cc` 下 `bool` 未定义，编译不过）→ 已补。
- `tools/xim_test_client.c` 新增 `\u/\d`（PageUp/PageDown）、`\w`（等 250ms）转义，支撑翻页用例。

### 本轮重要发现（供下一轮参考）
1. **addon 与 CLI 的候选人排序并不总是一致**（⚠️ 第 11 轮曾误判、第 12 轮已彻底更正）：
   真实根因是**默认用户目录污染**（`~/.local/share/pinyin-ime` 里残留的 `user.history`/`user.dict`），
   并非 libime/addon 缺陷，addon 的候选排序本身确定且与 CLI 完全一致。详见
   `docs/2026年09月29日12时30分根因更正_候选排序问题实为默认用户目录污染.md`。
2. **XIM 单次翻页选字结果漂移**（⚠️ 第 12 轮更正：同样是默认用户目录污染所致，非键竞态、非非确定
   排序）；隔离用户数据目录后 `=` 翻页选字的 `idx=9` 恒为「亿」，可加回确定性断言。

### 本轮复现命令（固化）
```sh
cd pinyin-ime
export NB=/root/workspace/native-build
export MIRROR=https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge
export PROXY=http://127.0.0.1:7890
bash scripts/setup_build_env.sh          # 引擎工具链 + 源码（libime/zh_CN.lm/sc.dict）
bash scripts/build_conda_forge.sh        # 引擎编译（fcitx5 头部 + libime + lmplz）
bash scripts/build_cli.sh                # CLI
bash scripts/smoke_test.sh               # 20 例
python3 scripts/test_interactive.py cpp/build/pinyin_ime_cli \
  "$NB/opt/libime/lib/libime/zh_CN.lm" "$NB/opt/libime/share/libime/sc.dict"  # 20 例
bash scripts/setup_x11_deps.sh           # X11/Xvfb + xcb-imdkit + yoga + rsvg 补丁
bash scripts/build_fcitx5_x11.sh         # fcitx5-X11（XIM 前端）+ 数据路径/默认输入法适配
cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$NB/opt/fcitx5-x11" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_PREFIX_PATH="$NB/opt/fcitx5-x11;$NB/opt/libime;$NB/mamba-env"
cmake --build fcitx5-addon/build && cmake --install fcitx5-addon/build
cc tools/xim_test_client.c -lX11 -lXtst -o tools/xim_test_client
cc tools/keysend_target.c  -lX11 -lXtst -o tools/keysend_target
bash scripts/xvfb_ime_test.sh           # 15 例
```

### 未来优化方向（下一轮按优先级聚焦）
1. **定位 addon/CLI 候选排序差异**：同一 nbest/用户数据/语料下逐字节对比，确认是否 libime
   `setNBest` 对格点 cut 的影响；若是，修正 addon 或 CLI 的 NBest 口径使其一致。
2. **XIM 翻页竞态复现与根治**：确认是 xcb-imdkit 事件乱序还是 addon 状态机，必要时给 addon
   翻页/选字加「每键落定」护栏（不 Proactively 改，先复现定位）。
3. **Wayland / 真桌面验收**：承接第 9 轮，ENABLE_DBUS=On + fcitx5 im module 最终验证。
4. **候选体验**：候选窗内水平移动选字、误上屏撤回。

## 开发进度（2026-09-29 · 第11轮：根因定位 addon 单音节候选排序随进程随机旋转）

> ⚠️【本节结论已被第 12 轮推翻，请直接看第 12 轮】本节归档当时的排查证据，但「ASLR 旋转」结论错误。
> 真实根因见 `docs/2026年09月29日12时30分根因更正_候选排序问题实为默认用户目录污染.md`。

本轮对上轮遗留的「addon 与 CLI 候选排序不一致 + 翻页选字漂移」做**根因定位**。结论：两者是
同一个问题——**addon 在 Fcitx5 进程内，libime 单音节候选排序随进程（ASLR）随机旋转**；CLI/独立
进程则完全确定。

### 排查过程（逐项排除，都写在 docs 里）
- 用最小复现程序（`/tmp/repro.cpp`，精确复刻 addon 的 `PinyinIME` 初始化序列 + 逐字 `type` +
  `candidatesToCursor()`）对比：
  - **nbest**：CLI 行模式 nbest=5/20/45 → 排序全一致，与 nbest 无关；
  - **locale**：repro 分别 `LANG=C / zh_CN.utf8 / en_US.utf8` → 排序不变；
  - **symlink 路径**：repro 用真实路径 vs `/usr/lib64/libime` 软链 → 排序一致；
  - **用户数据污染**：全新 UserDataDir + 清空 `~/.local/share/pinyin-ime`（此前残留 user.dict/history）→ 依旧旋转；
  - **并发 vs 进程级**：同进程内单 IC 连续「输 yi → Esc」×3 dump 完全一致（排除并发竞态）；
    5 次 fresh 重启得 5 种不同排序，且都是正确排序 `一以已亦易宜意依矣亿` 的**循环移位** → 进程级 ASLR 依赖。
- **影响面**：仅单音节同音字第 2 名以后排名；整句首选/句子级质量不受影响（smoke 20/20、Xvfb 14/14 稳定）。
- **更正上一轮**：翻页选字漂移不是键竞态，是候选排序本身在变（`idx=9` 稳定、idx 上那个字会变）。

### 产物
- `docs/2026年09月29日11时50分addon单音节候选排序随进程随机旋转问题.md`：完整证据表 + 下一步。
- 本轮为纯根因定位 + 文档（无引擎功能改动）；按规约版本号仍递增 `0.6.2 → 0.6.3`。

### 未来优化方向（下一轮按优先级聚焦）
1. **修复 addon 候选排序不确定**：在 libime 候选比较器补稳定 tie-break（按 pinyin/UTF-8 字节序），
   或 addon 侧对 `candidatesToCursor()` 结果按 score 稳定重排兜底；修复后补回单音节翻页选字断言。
2. Wayland / 真桌面验收（ENABLE_DBUS=On + im module）。
3. 候选窗水平选字、误上屏撤回。

## 开发进度（2026-09-29 · 第12轮：候选排序问题真相大白——默认用户目录污染，非 libime/addon 缺陷）

本轮继续深挖上轮的「候选排序随进程旋转」问题，最终**推翻前两轮的两次误判**，找到真正根因。

### 真正根因
`addon` 的候选排序与 CLI 完全一致、本身确定且正确。之前的「旋转/分数错乱」全是**默认用户目录污染**：
- 早期用 CLI **行模式没带 `--user-data`** 探测候选，把学习历史写进了默认 `~/.local/share/pinyin-ime/`
  的 `user.history`/`user.dict`。
- 后来很多次 addon 实测**没写 conf**（`pn()` 只 `pkill` + `rm /tmp/...`），addon 回落到被污染的这个
  默认目录，从而对单音节候选做了错误重排，「旋转」是近距分数字在重复落盘后彼此换位。

### 决定性证据（见更正 docs）
- addon 不写 conf（落污染默认目录）→ 分数错误压缩、跨进程旋转；
- addon 不写 conf + 清空默认目录 → 分数恢复正常；
- addon 写 conf（`UserDataDir=/tmp/...` 隔离）+ 故意保留污染默认目录 → 分数仍正确（conf 正确覆盖）；
- 隔离目录 3 次 fresh 重启 → 3/3 完全一致、与独立进程逐分一致（`一=-10.5343 以=-11.2976 …`）。

### 本轮教训（已记入 docs）
- 排查「随机/不确定」前先核对**数据是否隔离**，再归因算法/并发/ASLR；否则会像本轮一样误判两轮。
- CLI 探测/测试**一律显式 `--user-data`**（测试脚本本已隔离，探测时易忘）。

### 产物
- `docs/2026年09月29日12时30分根因更正_候选排序问题实为默认用户目录污染.md`（更正主文档）。
- 旧文档 `...11时50分...随机旋转问题.md` 顶部加「已更正」标注。
- **补回 addon 单音节翻页选字确定性断言**：`xvfb_ime_test.sh` 从 15 例 → **17 例**（新增
  `=` 翻页选字「亿」、PageDown 翻页选字「其」），连跑 3 次全稳定；reset 顺带防御性清空默认目录。
- 无引擎功能改动；版本号按规约 `0.6.3 → 0.6.4`。

### 回归
- `smoke_test.sh` 20/20、`test_interactive.py` 20/20、`xvfb_ime_test.sh` **17/17**（连跑 3 次）全绿。

### 未来优化方向（下一轮按优先级聚焦）
1. Wayland / 真桌面验收（ENABLE_DBUS=On + im module）。
2. 候选窗水平选字、误上屏撤回。

## 开发进度（2026-09-29 · 第13轮：误上屏撤回（Ctrl-Z）CLI + addon 双侧落地，回归 20/20 + 22/22 + 18/18）

本轮在新 headless 沙箱从零复现全套构建（未用 docker，镜像 + 代理），在「真正能跑起来的输入法」
基础上新增一个日用体验项——**误上屏撤回（Ctrl-Z）**：打错整句上屏后，Ctrl-Z 把刚上屏的候选
「撤下来」，恢复成拼音候选态重新编辑，CLI 与 Fcitx5 addon 双侧同步实现（README「未来优化方向」
已列两轮，本轮落地）。

### 从零构建（安装依赖 · 未用 docker）
- conda-forge 走清华 tuna 镜像；micromamba / fcitx5 / libime / kenlm / xcb-imdkit / yoga 等
  国外源统一走 mihomo 代理 `http://127.0.0.1:7890`；X11/Xvfb 系统库走 dnf（openEuler 仓库）。
- 引擎产物**逐字节复现**基线：`zh_CN.lm` 34,736,327 B、`sc.dict` 4,489,669 B；
  fcitx5 5.1.23 + libime 1.1.17（含 kenlm + `lmplz`）+ fcitx5-X11（XIM 前端，`opt/fcitx5-x11`）。
- 密钥处理：GitCode 访问令牌已写入仓库外独立文件（`~/.config/gitcode/token`，chmod 600），
  不入库、不外显；克隆走 `https://oauth2:<token>@gitcode.com/...`。

### 误上屏撤回（Ctrl-Z，新功能）
- **CLI（`pinyin_ime_cli`）**：交互模式 Ctrl-Z 触发。维护「撤回栈」（最多 32 条，记录
  上屏文本 + 拼音串）；上屏成功即压栈（仅候选整句上屏进栈，英文/标点/表情不动）；
  Ctrl-Z 时若正在输入拼音则不撤（防误伤），否则清掉上屏行（`\e[1A\r\e[2K`，上屏文本独占一行）、
  恢复拼音回候选态重新编辑，状态行尾部显示「（已撤回上屏，可继续编辑拼音）」提示（下次按键消失）。
- **Fcitx5 addon（`pinyinime.cpp`）**：keyEvent 在「组合键交给框架」守卫**之前**拦截
  Ctrl+Z（与 Ctrl-E 同策略）；`undoLastCommit()` 先 `inputContext_->forwardKey(BackSpace) × UTF-8
  字符数` 通知应用删掉刚上屏文本，再把拼音 `context_.type()` 恢复成候选态；`deactivate()`（失焦）
  清空撤回栈（防误伤别的窗口）。撤回栈随输入上下文（per-IC）独立。
- 纯前端状态机，不复用/不触碰 libime 解码管线 —— 无需重建引擎，CLI 与 addon 共用同一套语义
  （与中英切换/中文标点/引号配对同路径）。

### 本轮发现并修复的真实问题（含 2 个脚本/编译期之外的坑）
1. **addon 拼音收集时机**（关键）：`selectCandidate()` 里最初在 `selectCandidatesToCursor()`
   **之后**收集恢复拼音——那时 `preedit()` 已变化/清空，撤回恢复的是空拼音、空格无法再上屏
   （Xvfb 实测暴露：`nihao\s` 上屏「你好」→ Ctrl-Z → 空格无输出）。改为**选中之前**收集
   `context_.preedit()`（滤空格/隔音符），Xvfb 实测撤回后再空格正常上屏「你好」。
2. **`setup_x11_deps.sh` heredoc 缩进 bug**：rsvg 兼容补丁的 Python 内嵌脚本里顶层
   `assert`/`open` 两行带前导空格（顶层语句不允许缩进 → IndentationError 中断脚本）；已修。
3. **编译期小坑**：fcitx5 5.1.23 的 KeyState 无修饰值叫 `KeyState::NoState`（不是 `None`）；
   `undoStack_` 初版放 private、`PinyinIMEEngine::deactivate` 越权访问报错 → 移到 public
   （与 `page_`/`englishMode_` 等状态成员一致）。

### 回归护栏（全绿）
`smoke_test.sh` **20/20**、`test_interactive.py` **22/22**（新增 ㉑上屏→撤回→恢复拼音、
㉒撤回后重新上屏两例）、C++/Python 纠错对拍 exit 0、`xvfb_ime_test.sh` **18/18**
（新增「误上屏撤回 Ctrl-Z」一例：上屏「你好」→ Ctrl-Z → 空格重新上屏「你好」，
断言用正则 `你好.*你好` 容忍 forwardKey 退格产生的中间 COMMIT）。addon 编译 + 工厂符号导出正常。

### 本轮复现命令（固化）
```sh
cd pinyin-ime
export NB=/root/workspace/native-build
export MIRROR=https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge
export PROXY=http://127.0.0.1:7890
bash scripts/setup_build_env.sh && bash scripts/build_conda_forge.sh && bash scripts/build_cli.sh
bash scripts/smoke_test.sh                                  # 20 例
python3 scripts/test_interactive.py cpp/build/pinyin_ime_cli \
  "$NB/opt/libime/lib/libime/zh_CN.lm" "$NB/opt/libime/share/libime/sc.dict"   # 22 例（含撤回）
bash scripts/setup_x11_deps.sh && bash scripts/build_fcitx5_x11.sh
cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$NB/opt/fcitx5-x11" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_PREFIX_PATH="$NB/opt/fcitx5-x11;$NB/opt/libime;$NB/mamba-env"
cmake --build fcitx5-addon/build && cmake --install fcitx5-addon/build
cc tools/xim_test_client.c -lX11 -lXtst -o tools/xim_test_client
bash scripts/xvfb_ime_test.sh                              # 18 例（含撤回）
```

### 未来优化方向（下一轮按优先级聚焦）
1. **Wayland / 真桌面最终验收**：XIM（X11）链路已实测；补 `ENABLE_DBUS=On` + fcitx5-gtk/qt
   im module，在带 DBus 与真实桌面的环境做最终验证（含配置经 DBus 载入的路径）。
2. **LM 域插值/集成**（承接第 5 轮结论）：对话倾向 LM 与官方 `zh_CN.lm` 分数插值，两域兼得。
3. **候选体验**：候选窗内水平移动选字、部首/偏旁检索（撤回已落地）。
4. **纠错边界扩展**：`zhing` 这类「零编辑切分失败」的重错全拼（第 7 轮已声明的边界）。
5. **撤回增强**：多级撤回提示可选回显被撤回文本（当前提示不含文本以便回归断言）；
   addon 撤回在 xterm 之外的编辑器（Vim/浏览器）实测覆盖更广。

## 开发进度（2026-09-29 · 第14轮：纠错边界扩展——zhing 类重错全拼放行，回归 20/20 + 23/23 + 19/19）

承接第 7 轮声明的边界（`zhing` 这类「零编辑切分失败」的重错全拼被 `is_full_pinyin` 门控挡下），
本轮在 `cpp/pinyin_corrector` 里新增 **`should_correct()` 保守门控**，把「疑似全拼打错」也放行进
纠错流程，同时保持「绝不误伤简拼 / 双拼 / 混合输入」的安全底线。

### 门控设计（`cpp/pinyin_corrector.{h,cpp}`）
新增 `bool should_correct(const std::string &pinyin)`，解码前端（CLI 行模式 / 交互态、Fcitx5 addon
`EnableCorrect`）统一从 `is_full_pinyin()` 切换到它：

- **① 零编辑可切分（真全拼）**：`is_full_pinyin` 原逻辑，直接放行（`senme`/`cifan` 等音节内打错照常）。
- **② 疑似全拼打错（重错）**，需同时满足四条保守判据：
  1. 长度 >= 3（拦 `zg` 等 2 字母简拼）；
  2. 含至少一个元音 a/e/i/o/u/v（拦 `bjdx`、`zhng` 等零元音串）；
  3. 纠错结果与输入**等长**且替换 1~2 处（纯替换型；含插入/删除的 `zhng→zheng` 本轮仍不放开）；
  4. 纠正后 **<= 2 音节**（拦 `bjdaxue` 这类「简拼前缀 + 全拼后缀」混合输入——它会被切成
     bo|da|xue 三音节，天然过滤）。

### 实测行为（行模式 `--correct`，干净用户目录）
| 输入 | 原（无纠错 / 旧门控） | 现（should_correct） | 说明 |
|---|---|---|---|
| `zhing` | 知唔 / 之嗯（分裂） | **中 种 重 终 众** | 重错全拼放行，纠正为合法单字 |
| `zongguo` | 宗国 | **中国** | 既有用例保持 |
| `senme` | 森么 | **什么** | 既有用例保持 |
| `zg` | 中国 | **中国** | 简拼不误伤 ✓ |
| `bjdx` | 北京大学 | **北京大学** | 简拼不误伤 ✓ |
| `bjdaxue` | 北京大学 | **北京大学** | 混合输入不误伤（第 4 条关键）✓ |
| `zhng` | 章嗯 / 种嗯 | 章嗯 | 删元音型（del）按设计不放开 ✓ |

### 回归护栏（全绿）
`smoke_test.sh` **20/20**、`test_interactive.py` **23/23**（新增 ㉓ zhing + 空格上屏「中」）、
C++/Python 纠错对拍 exit 0、`xvfb_ime_test.sh` **19/19**（新增「纠错边界 zhing→中」）。
CLI 与 addon 共用同一 `pinyin_corrector.cpp`，行为一致。

### 版本
`0.6.5 → 0.6.6`（VERSION + fcitx5-addon/CMakeLists.txt 同步）。

### 未来优化方向（下一轮按优先级聚焦）
1. **Wayland / 真桌面最终验收**：XIM（X11）链路已实测；补 `ENABLE_DBUS=On` + fcitx5-gtk/qt
   im module，在带 DBus 与真实桌面的环境做最终验证（含配置经 DBus 载入的路径）。
2. **LM 域插值/集成**（承接第 5 轮结论）：对话倾向 LM 与官方 `zh_CN.lm` 分数插值，两域兼得。
3. **候选体验**：候选窗内水平移动选字、部首/偏旁检索（撤回已落地）。
4. **纠错边界再扩展**：`zhng` 类删元音型（del）重错，在「音节数 + 元音分布」更强判据下评估放开；
   `should_correct` 目前按「等长替换型 + <=2 音节」保守取舍，del/ins 型与 3+ 音节替换型留待后续。
5. **撤回增强**：多级撤回提示可选回显被撤回文本；addon 撤回在 xterm 之外的编辑器（Vim/浏览器）实测。

## 开发进度（2026-09-29 · 第15轮：候选焦点选字（Ctrl+←/→）CLI + addon 双侧落地，回归 20/20 + 25/25 + 20/20）

本轮补上主流中文输入法（搜狗/微软）的标配交互——**候选窗内水平移动焦点选字**：在候选列表里
用 `Ctrl+←/Ctrl+→` 左右移动「焦点候选」，空格上屏焦点候选（不再只能数字选字或整页选字）。纯前端
状态机，不触碰 libime 解码管线。

### CLI（`pinyin_ime_cli`）
- `read_key()` 扩展识别 `\x1b[1;5D`（Ctrl+←）/ `\x1b[1;5C`（Ctrl+→）两个转义序列（xterm 修饰键
  序列，`read_esc_byte` 50ms 探测继续复用）；新增 `kKeyCtrlLeft/kKeyCtrlRight` 逻辑键。
- 新增候选焦点状态 `candFocus`（页内 0..8，默认 0 = 首选）。渲染时焦点候选加 `*` 前缀标记
  （`*2.候选`），空格/回车/标点/引号「打拼音中上屏」统一改为上屏**焦点候选**（默认 0 与旧
  「首选」语义完全一致）；数字选字后焦点跟随所选候选。
- 焦点复位：退格、新拼音、词边界 ←/→、Home/End、翻页（PageUp/Down 与 -,=[,].）、纠错重输后均把
  焦点归零（候选刷新后焦点回到首选）。

### Fcitx5 addon（`pinyinime.cpp`）
- `keyEvent` 在「组合键交给框架」守卫**之前**新增 Ctrl+←/→ 拦截，且**仅当有拼音候选时**才拦截——
  空闲时透传（编辑器里 Ctrl+←/→ 是「按词移动文本光标」，不能吃掉）。这与 Ctrl-E（表情）、Ctrl-Z
  （撤回）放同一层守卫前。
- `PinyinIMEState` 新增 `candFocus_`；`reset()`/`flipPage()`/退格/拼音输入/纠错重输统一复位为 0；
  空格/回车 `selectCandidate(page*pageSize + candFocus_)` 上屏焦点候选（默认 0 = 首选）。
- 候选窗视觉高亮（`CommonCandidateList::setCursorIndex`）留待真实桌面实机（headless 沙箱候选窗
  由 classicui 渲染，先保证上屏语义正确，视觉打磨不阻塞功能）。

### 测试工具与回归
- `tools/xim_test_client.c` 新增 `\l`（Ctrl+Left）/`\r`（Ctrl+Right）转义（XTest 发 `XK_Left/Right +
  ControlMask`），支撑 addon 焦点用例。
- **回归护栏全绿**：`smoke_test.sh` 20/20、`test_interactive.py` **25/25**（新增 ㉔ yi+Ctrl+→+空格
  →「以」、㉕ yi+Ctrl+→×2+空格→「已」）、C++/Python 纠错对拍 exit 0、`xvfb_ime_test.sh` **20/20**
  （新增「候选焦点 Ctrl+→ + 空格 (yi)→以」）。

### 本轮重要发现（再次印证第 12 轮教训）
候选焦点用例首版放在 Xvfb 基础区时，`yi` 的候选排序被**同一 fcitx5 会话里反复 `learn()` 污染**，
导致焦点功能「看似」失效（`focus=1` 却上屏「亿」、`focus=2` 上屏「一」）——实为候选排序被历史
learn 提权打乱，非焦点逻辑缺陷。诊断日志确认 `candFocus_` 0→1→2 推进、空格 `idx` 计算完全正确。
修复：焦点用例移到**独立清空用户数据块**（`rm -rf` 隔离目录 + 防御性清默认目录 + 重启 fcitx5），
与翻页用例同一策略后 PASS。**结论：addon 候选排序本身确定，出问题先查数据隔离，再归因逻辑。**

### 版本
`0.6.6 → 0.6.7`（VERSION + fcitx5-addon/CMakeLists.txt 同步）。

### 未来优化方向（下一轮按优先级聚焦）
1. **Wayland / 真桌面最终验收**：XIM（X11）链路已实测；补 `ENABLE_DBUS=On` + fcitx5-gtk/qt
   im module，在带 DBus 与真实桌面的环境做最终验证（含配置经 DBus 载入的路径）；并补 addon
   候选焦点视觉高亮（`setCursorIndex`）。
2. **LM 域插值/集成**（承接第 5 轮结论）：对话倾向 LM 与官方 `zh_CN.lm` 分数插值，两域兼得。
3. **候选体验**：部首/偏旁检索（撤回收回、候选焦点已落地）。
4. **纠错边界再扩展**：`zhng` 类删元音型（del）重错，在「音节数 + 元音分布」更强判据下评估放开。
5. **撤回增强**：多级撤回提示回显被撤回文本；addon 撤回在 xterm 之外编辑器实测。

## 开发进度（2026-09-29 · 第16轮：真桌面终验——DBus + fcitx5-gtk im module + GTK3 应用端到端，10/10）

承接连续 5 轮置顶的「真桌面最终验收」：补齐 XIM 之外的**主流真实桌面通道**——`fcitx5` 开启
DBus 前端 + `fcitx5-gtk` 的 GTK3 im module，用真实 GTK3 应用（GtkEntry）经 `GTK_IM_MODULE=fcitx`
打出中文。这是浏览器/编辑器等 GTK 应用使用输入法的同一条路，至此「真正能跑起来的输入法」闭环。

### 打通的两条现实通道（对照）

| 通道 | 客户端 | 中间环节 | 验收 |
|---|---|---|---|
| XIM（X11） | `tools/xim_test_client`（XIM 协议） | libxim + xcb-imdkit | 第 8 轮起，现 20/20 |
| **DBus + GTK im module** | `tools/gtk_ime_test_client`（真实 GTK3 应用） | im-fcitx5.so → DBus → fcitx5 | **本轮新增，10/10** |

### 实现

- **fcitx5 开 DBus**（`scripts/build_fcitx5_x11.sh` 改 `ENABLE_DBUS=On`）：增量编译出 `dbus`
  模块 + `dbusfrontend` 前端；XIM 前端不动、回归仍 20/20，双前端并存。
- **fcitx5-gtk 5.1.7 → GTK3 im module**（`scripts/build_fcitx5_gtk.sh`）：纯客户端库，用**系统
  gcc 10.3** 编译（避免 conda libstdc++ 与系统 GTK3 应用冲突）；只编 GTK3（关 GTK2/4/GIR/SNOOPER）。
- **注册 immodule**：openEuler GTK3 靠 `immodules.cache` 缓存文件发现（非运行时扫描），用
  `gtk-query-immodules-3.0-64` 生成写入 `/usr/lib64/gtk-3.0/3.0.0/`。
- **GTK3 实测客户端**（`tools/gtk_ime_test_client.c`）：带 GtkEntry 的应用，每次上屏打印
  `GTK_COMMIT:<text>`；按键经 `tools/keysend_target`（XTest）注入；每个用例先 `Ctrl+Space`
  激活输入法。
- 完整路线与踩坑见 `docs/2026年09月29日14时58分真桌面终验_DBus加fcitx5gtk打通.md`。

### 实测结果（Xvfb :99 · DBus 会话 · GTK3 3.24.30 · fcitx5 5.1.23 · addon）

全拼 `nihao→你好`、`zhongguo→中国`、整句 `woshiyige→我是一个`、简拼 `zg→中国`/`bjdx→北京大学`、
纠错 `senme→什么`/`zongguo→中国`、中英切换 `Tab+hello→hello`、中文标点 `,→，`、
先上屏再标点 `zhongguo!→中国！` —— **10/10 全绿**。

### 本轮复现命令（固化，可一键重跑）

```sh
cd pinyin-ime
export NB=/root/workspace/native-build
export MIRROR=https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge
export PROXY=http://127.0.0.1:7890
# 引擎 + CLI（不变）
bash scripts/setup_build_env.sh && bash scripts/build_conda_forge.sh && bash scripts/build_cli.sh
# fcitx5-X11（现含 DBus 前端）
bash scripts/setup_x11_deps.sh && bash scripts/build_fcitx5_x11.sh
# addon + XIM 实测客户端（不变）
cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
  -DCMAKE_INSTALL_PREFIX="$NB/opt/fcitx5-x11" -DCMAKE_INSTALL_LIBDIR=lib \
  -DCMAKE_PREFIX_PATH="$NB/opt/fcitx5-x11;$NB/opt/libime;$NB/mamba-env"
cmake --build fcitx5-addon/build && cmake --install fcitx5-addon/build
cc tools/xim_test_client.c -lX11 -lXtst -o tools/xim_test_client
# 真桌面（DBus + GTK3 im module）—— 本轮新增
bash scripts/setup_desktop_deps.sh && bash scripts/build_fcitx5_gtk.sh
# 回归护栏（四路全绿）
bash scripts/smoke_test.sh                     # 20/20
python3 scripts/test_interactive.py cpp/build/pinyin_ime_cli \
  "$NB/opt/libime/lib/libime/zh_CN.lm" "$NB/opt/libime/share/libime/sc.dict"   # 25/25
bash scripts/xvfb_ime_test.sh                  # XIM  20/20
bash scripts/xvfb_desktop_ime_test.sh          # DBus+GTK  10/10（本轮新增）
```

> 注：`build_fcitx5_gtk.sh` 需 root（写系统 `immodules.cache`）；`keysend_target` 与
> `gtk_ime_test_client` 均由对应构建脚本顺带编译。

### 版本
`0.6.7 → 0.6.8`（VERSION + fcitx5-addon/CMakeLists.txt 同步）。

### 未来优化方向（下一轮按优先级聚焦）

1. **Wayland 真桌面延展**：XIM + DBus/GTK 两条链路均已验；补 `ENABLE_WAYLAND=On` + gtk/qt
   Wayland im module，在带物理显示器/合成器的真桌面做视觉终验（含候选焦点高亮 `setCursorIndex`）。
2. **候选焦点视觉高亮**（`setCursorIndex`）：语义已对（第 15 轮），视觉打磨留待真桌面。
3. **LM 域插值/集成**（承接第 5 轮结论）：对话倾向 LM 与官方 `zh_CN.lm` 分数插值，两域兼得。
4. **候选体验**：部首/偏旁检索（撤回、候选焦点已落地）。
5. **纠错边界再扩展**：`zhng` 类删元音型（del）重错，在「音节数 + 元音分布」更强判据下评估放开。
6. **撤回增强**：多级撤回提示回显被撤回文本；addon 撤回在 Vim/浏览器等真实编辑器实测。
