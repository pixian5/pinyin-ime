# 语言模型训练流水线（Route A · 换更好的 LM）

本目录提供「用自备语料训练 libime 替代三元语言模型」的完整流水线，用于验证
PLAN-A §13 的下一步：**换更大/更好的语言模型**（词库规模已被 §13 证明不是瓶颈）。

## 为什么必须用 sc.dict 词表分词

libime 的解码器栅格由 `sc.dict` 的词条构成，语言模型按**词条字符串**查 KenLM
trie（见 `src/libime/core/languagemodel.cpp` 的 `index(string_view)`）。
若 LM 词表与 `sc.dict` 词粒度不一致，解码器拼出的词在 LM 里是 `<unk>`，
被 `DEFAULT_LANGUAGE_MODEL_UNKNOWN_PROBABILITY_PENALTY` 惩罚，准确率会雪崩。

因此 `scdict_segment.py` 用**前向最大匹配**把语料切成 `sc.dict` 词条（单字兜底），
保证训练出的 LM 词表 ⊆ `sc.dict` 词表，栅格完全兼容。

## 流水线

```sh
./lm/train_lm.sh                     # 缺省：THUCNews 1 亿字，3-gram
./lm/train_lm.sh --order 4 --max-chars 50000000   # 更高阶 / 更小语料
```

分五步（详见 `train_lm.sh`）：

1. `prepare_thucnews.py` — 从 `THUCNews.zip` 抽取清洗后的纯汉字句子（各分类轮转取样）
2. `scdict_segment.py` — 用 `dict_sc.txt` 词表最大匹配分词，输出空格分隔 token
3. `lmplz`（KenLM） — Modified Kneser-Ney 平滑训练 order-gram ARPA
4. `libime_slm_build_binary -s -a 22 -q 4 trie` — 转 QuantArrayTrie（与官方完全同参数）
5. `libime_prediction` — 生成 `.predict`（与官方 `zh_CN.lm.predict` 同构）

## 产物

- `zh_CN_news.arpa` — 文本 ARPA
- `zh_CN_news.lm` — 二进制 trie（评测时替换 `zh_CN.lm`）
- `zh_CN_news.lm.predict` — 输入预测文件

## 评测（同口径对比基线）

```sh
python3 bench/run_bench.py --corpus corpus/news.txt \
  --cli cpp/build/libime_bench \
  --cli-arg <LM>/zh_CN_news.lm \
  --cli-arg <prefix>/share/libime/sc.dict \
  --cli-arg 5
```

基线（2026 官方数据 `lm_sc.arpa-20260629`，`zh_CN.lm` 34,736,327 B）：
**句级 39.55%，字级 88.82%**。

## 关键参数 / 坑

- **`-S`（排序内存）**：lmplz 外部排序内存上限。14 GB 无 swap 的机器上 1 亿字
  用 `-S 5G` 可过；更大语料需相应放缩。临时目录 `-T` 指向 overlay（`$OUT/tmp`），
  不要落在 tmpfs（`/tmp` 仅几 GB，外部排序中间文件会撑爆）。
- **`--discount_fallback`**：Kneser-Ney 折扣闭合式在过小/去重不够的语料上会失败，
  加此参数回退到经验折扣（0.5 / 1 / 1.5），提高鲁棒性。
- **词典兼容**：语料分词词表不得超出 `sc.dict`；用 `--limit_vocab_file` 或直接换
  分词器都会破坏栅格兼容，慎用。
- **sentence markers**：默认 lmplz 会插入 `<s>`/`</s>`。官方 `zh_CN.lm` 不含
  markers（libime 以 `sentence_marker_missing=SILENT` 容忍），两者都能正常解码；
  含 markers 的标准模型给首词提供 `<s>` 上下文，更规范。

## 结果

| 模型 | 句级 | 字级 | 码长 | 延迟 P50/P99 | 模型体量 |
|---|---|---|---|---|---|
| 官方 `zh_CN.lm`（基线，本机复现） | **39.60%** | **88.82%** | 3.099 | 57.9 / 106.7 ms | 34.7 MB |
| 自训 `zh_CN_news.lm`（THUCNews 3-gram） | 34.20% | 88.29% | 3.099 | 51.3 / 97.4 ms | 121.4 MB |

自训模型详情：THUCNews 2005–2011 新闻，抽取 1 亿字 → 11.6M 句 → **52.9M 词次 /
125,686 词**；ARPA 1/2/3-gram = 125,689 / 10,740,012 / 27,679,453
（合计 ≈38.4M 条，是官方 7M 条的 5.5 倍）；量化 trie 121.4 MB。

**结论：首版自训 LM 比官方**差**（句级 −5.4 pct，字级 −0.53 pct），且体量更大。**
但这不是「LM 这条路走不通」，而是语料选错了——根因诊断如下。

### 根因：不是词表覆盖，而是 n-gram 概率质量

`diagnose_vocab.py` 对同一份评测语料（2000 句）按 sc.dict 分词后统计词表覆盖：

| LM 词表 | 词表规模 | 覆盖词型 | 覆盖词次 |
|---|---|---|---|
| 官方 `lm_sc.arpa` | 270,714 | 97.3%（7773/7986） | 98.57% |
| 自训 `zh_CN_news.arpa` | 125,689 | **99.4%（7936/7986）** | **99.58%** |

自训模型的**词表覆盖反而更高**（语料同为新闻，命中的新闻多字词更多）。
官方有、自训缺的高频词（`李在明[10]`、`赋能[9]`、`小度`、`制度型`…）几乎全是我
2005–2011 语料之后的**新词/新人物**；自训有、官方缺的是 `告诉记者`、`记者李`、
`最大的` 等 sc.dict 多字词（官方 LM 把它们当 `<unk>`）。

所以退化的根源是：**52.9M 词的小语料 + 15~20 年时效差 → n-gram 转移概率既噪声又过时**
（经典信号：字级只掉 0.5 点、句级掉 5.4 点——单字选择还行，长程组句乱）。

### 下一步（要真正打败官方 LM）

1. **换现代语料**（2020 年代、规模 ≥ 数亿词次），而非更大 n-gram 数；
2. 或 **4-gram**（需重编 libime 时 `KENLM_MAX_ORDER=4`，但数据不足时无益）；
3. 或转向 **用户词库 / 自学习**（PLAN-A 里体感收益更大、风险更低的方向）。

> 意义：流水线已打通并闭环验证（训练 → trie → 量化 → 评测 → 根因诊断）。
> 这轮的价值是「把尺子磨快」——现在能定量回答「换什么 LM 更好」，而不是凭感觉。