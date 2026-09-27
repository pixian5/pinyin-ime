# pinyin-ime

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
> 与 `python3 scripts/test_interactive.py` 做回归护栏；下述第 1 项（图形 addon）仍需
> 显示服务器，本 headless 沙箱无法验收，留待带图形环境再落地。

1. **Fcitx5 addon / 图形落地**：✅ 已落地首版并补齐中英切换 / 中文标点 / 引号配对 / 光标移动（`fcitx5-addon/`，见「Fcitx5 addon」一节）——引擎解码 / 候选 / 上屏已接入，沙箱内编译与符号导出验证通过；剩余：实机验收、（可选）`EngineProc` 进程外骨架（40ms 防抖 / 异步 IO / 崩溃重启）。
2. **用户词库 / 自学习**：会话内 `learn()` + 用户词自动造词已实现，且已落到**独立用户路径**
   （`--user-data`，`user.history` + `user.dict`，不覆盖系统 `zh_CN.lm`）✅。剩余：误上屏撤回、
   崩溃时未保存增量丢损、跨设备同步（PLAN-A §4②）。
3. **容错纠错集成**：把已完成但未集成的 `correction/`（CHIME 信道 + 音节二元转移）接入
   libime 解码管线（需引擎重建，PLAN-A §4①）。
4. **输入体验**：双拼✅、候选分页✅、中英切换✅、中文标点✅、引号智能配对✅（CLI 与 Fcitx5 addon 双侧均已实现）；剩余：表情、候选部首/偏旁检索、单引号配对（`'` 与拼音隔音符冲突，需另设触发规则）。
5. **光标移动 / 逐词分段**：✅ 已实现（`pinyinBeforeCursor/AfterCursor` 按词边界步进，方向键移动候选光标；CLI + addon 双侧，见 M1「光标移动 / 逐词分段」）；✅ `Home/End` 跳句首句尾（2026-09-28 落地，CLI + addon 双侧）；剩余可选项：在候选窗内水平移动选字。
6. **换更好的语言模型**（Route A 训练/剪枝已有产物，见 `lm/` 与 git log）：用现代大语料
   （维基 + 字幕 / 对话，避开新闻类敏感语料）再训 LM，抬整句准确率。

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
