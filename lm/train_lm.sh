#!/usr/bin/env bash
# train_lm.sh — 训练替代中文三元语言模型（目标：换更好的 LM）。
#
# 流水线：
#   1. prepare_thucnews.py  从 THUCNews.zip 抽取清洗后的纯汉字句子
#   2. scdict_segment.py    用 sc.dict 词表最大匹配分词（保证 LM 词表 ⊆ sc.dict，栅格兼容）
#   3. lmplz                训练 Modified Kneser-Ney 平滑的 order-gram ARPA
#   4. libime_slm_build_binary  转成 QuantArrayTrie（与 fcitx5 完全相同的参数）
#   5. libime_prediction    生成 .predict 文件（与官方 zh_CN.lm.predict 同构）
#
# 用法：train_lm.sh [--order 3] [--max-chars 120000000] [--zip THUCNews.zip]
# 产物：$OUT/zh_CN_news.{arpa,lm,lm.predict}
set -euo pipefail

NB="${NB:-/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"
LM="$OPT/libime/bin"                  # libime_slm_build_binary / libime_prediction
KENLM_LMPLZ="$NB/build/kenlm/bin/lmplz"
REPO="$(cd "$(dirname "$0")/.." && pwd)"   # pinyin-ime 仓库根

ORDER=3
MAX_CHARS=100000000
ZIP="$NB/data/THUCNews.zip"
DICT="$NB/data/dict_sc.txt"
OUT="$NB/lm"

while [[ $# -gt 0 ]]; do
  case "$1" in
    --order)      ORDER="$2"; shift 2 ;;
    --max-chars)  MAX_CHARS="$2"; shift 2 ;;
    --zip)        ZIP="$2"; shift 2 ;;
    --dict)       DICT="$2"; shift 2 ;;
    --out)        OUT="$2"; shift 2 ;;
    *) echo "未知参数: $1" >&2; exit 2 ;;
  esac
done

mkdir -p "$OUT/tmp"
TMP="$OUT/tmp"
RAW="$OUT/raw.txt"
SEG="$OUT/seg.txt"
ARPA="$OUT/zh_CN_news.arpa"
LMBIN="$OUT/zh_CN_news.lm"
PRED="$OUT/zh_CN_news.lm.predict"

echo "== [1/5] 抽取 + 清洗语料 (≤ $MAX_CHARS 字) =="
"$ENV/bin/python" "$REPO/lm/prepare_thucnews.py" --zip "$ZIP" --out "$RAW" --max-chars "$MAX_CHARS"

echo "== [2/5] sc.dict 分词 =="
"$ENV/bin/python" "$REPO/lm/scdict_segment.py" --dict "$DICT" --input "$RAW" --output "$SEG"
wc -lc "$SEG"

echo "== [3/5] lmplz 训练 $ORDER-gram ARPA =="
[ -x "$KENLM_LMPLZ" ] || { echo "找不到 lmplz: $KENLM_LMPLZ" >&2; exit 3; }
"$KENLM_LMPLZ" -o "$ORDER" -S 5G -T "$TMP" --discount_fallback \
  --text "$SEG" --arpa "$ARPA"
ls -l "$ARPA"

echo "== [4/5] 转 QuantArrayTrie =="
"$LM/libime_slm_build_binary" -s -a 22 -q 4 trie "$ARPA" "$LMBIN"
ls -l "$LMBIN"

echo "== [5/5] 生成 .predict =="
"$LM/libime_prediction" "$LMBIN" "$ARPA" "$PRED"
ls -l "$PRED"

echo "== 完成 =="
echo "ARPA:  $ARPA"
echo "LM:    $LMBIN"
echo "PRED:  $PRED"
echo "评测：python3 bench/run_bench.py --corpus corpus/news.txt --cli cpp/build/libime_bench --cli-arg $LMBIN --cli-arg $OPT/libime/share/libime/sc.dict --cli-arg 5"