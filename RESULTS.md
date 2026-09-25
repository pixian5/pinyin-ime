# Route A 实验结果

> 用 LCCC 口语语料 + SkyPile-150B(2020) 网页文本训练 libime 3-gram LM，替换官方三元 LM。
> 目标：打赢官方 `zh_CN.lm`（基线 news.txt 句级 39.55% / 字级 88.82%）。**已达成。**

## 结论

**4 个干净 hold-out 域全面打赢官方基线，无一例外。**

| 域 | 性质 | 官方句级 | 新模型句级 | Δ句级 | 官方字级 | 新模型字级 | Δ字级 |
|---|---|---|---|---|---|---|---|
| news.txt（新闻） | 干净 | 39.55% | **47.60%** | +8.05 | 88.82% | **92.11%** | +3.29 |
| recent.txt（近期新闻） | 干净 | 31.82% | **39.62%** | +7.80 | 85.94% | **88.44%** | +2.50 |
| baike.txt（百科） | 干净 | 34.37% | **37.85%** | +3.48 | 87.99% | **89.07%** | +1.08 |
| wiki.txt（维基） | 干净 | 21.35% | **24.50%** | +3.15 | 77.59% | **80.60%** | +3.01 |
| lccc_eval.txt（口语） | ⚠️ 泄漏 | 49.24% | 82.80% | (+33.56) | 87.78% | 96.95% | (+9.17) |

- 干净域句级平均 **+5.62 pp**（区间 +3.15 ～ +8.05），字级平均 **+2.47 pp**（区间 +1.08 ～ +3.29）。
- ⚠️ `lccc_eval.txt` 由 `make_lccc_eval.py` 从训练集 `lccc_full.txt` 按 md5 抽样抽出，与训练同源；
  82.80% 属记忆效应，**不作为泛化成绩**。严谨对话域成绩需剔除该 1570 句后重训。

## 模型规格

| | 官方 `zh_CN.lm` | `zh_CN_modern.lm` |
|---|---|---|
| n-gram 规模 | ~7M | 17 万词 / 5490 万 bigram / 2.03 亿 trigram |
| 体量 | 34.7 MB | 765 MB（22×） |
| 训练语料 | 官方未公开 | LCCC full（3055 万句）+ SkyPile-150B 2 分片 ≈ 5.59 亿词 |
| 延迟 P50 | 49–58 ms | 62–78 ms |
| 延迟 P99 | 95–103 ms | 121–126 ms |

量化参数：`slm_build_binary -s -a 22 -q 4 trie`（与官方 `data/CMakeLists.txt` 同参）。

## 剪枝对照（news.txt）

| 模型 | 剪枝 | trigram | 体量 | 句级 | 字级 | P50 | P99 |
|---|---|---|---|---|---|---|---|
| 官方 | — | ~7M 总 | 34.7 MB | 39.55% | 88.82% | 55.8 ms | 103 ms |
| 无剪枝 | 0 | 2.03 亿 | 765 MB | 47.60% | 92.11% | 69.8 ms | 126 ms |
| p1 | 0 0 1 | 5187 万 | 374 MB | 47.55% | 91.87% | 69.8 ms | 127 ms |
| p2 | 0 0 2 | 2687 万 | 299 MB | 47.00% | 91.63% | 70.2 ms | 127 ms |

- 剪单例 trigram（p1）**近乎无损**：句级 -0.05pp、字级 -0.24pp，体量砍半（765→374MB）。
- p2 再剪 doubleton，句级 -0.60pp，体量 299MB。
- 延迟不随剪枝下降：瓶颈在未剪的 5490 万 bigram（及 libime 解码本身），非 trigram。
- 结论：压缩体积靠剪 trigram（几乎免费）；压缩延迟需剪 bigram（精度风险更大）。

## 复现要点

1. 构建工具链：`scripts/setup_build_env.sh` → `scripts/build_conda_forge.sh`（fcitx5 5.1.23 + libime 1.1.17 + kenlm，含 4-gram 补丁）。
2. 语料：LCCC `fetch_lccc.py` 清洗；SkyPile `lm/prepare_skypile.py` 抽句清洗 → `lm/scdict_segment.py` 用 `sc.dict` 词表分词 → 合并 `combined_seg.txt`（5.59 亿词）。
3. 训练：`lmplz -o 3 -S 8G --discount_fallback` → `libime_slm_build_binary -s -a 22 -q 4 trie`（无剪枝）。
4. 评测：`bench/run_bench.py --corpus <域> --cli cpp/build/libime_bench --cli-arg <lm> --cli-arg <sc.dict> --cli-arg 5`。

## 下一步（待选）

- [x] 熵剪枝（`--prune 0 0 1` / `0 0 2`）已对照：p1（374MB）为体积/精度甜点
- [ ] 剪 bigram（`--prune 0 1 1`）压延迟到官方水平
- [ ] 4-gram（已备 `KENLM_MAX_ORDER=4` 补丁）
- [ ] 严谨口语 hold-out：剔除 lccc_eval 1570 句后重训，拿干净对话域成绩