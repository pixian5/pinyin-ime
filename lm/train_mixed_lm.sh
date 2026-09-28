#!/usr/bin/env bash
# train_mixed_lm.sh — 对话 + 维基混合语料重训均衡 LM。
#
# 目的：上一轮纯对话 500 万句把对话域打到字级 93.80%，但新闻域崩到 9.85% 句级
# （纯对话过拟合）。本轮把对话与维基通用语料按比例混合，训练两域均衡的 LM。
#
# 依赖（已由前序步骤准备）：
#   $NB/corpus/lccc_train_seg.txt          500 万句对话（已 sc.dict 分词）
#   $NB/corpus/wiki_train.txt              维基正文（extract_wiki.py 产物）
# 产物：$NB/lm/zh_CN_mixed.{arpa,lm,lm.predict}
#
# 用法：DIALOG_LINES=2000000 WIKI_LINES=4000000 bash lm/train_mixed_lm.sh
set -euo pipefail

NB="${NB:-/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"
DIALOG_LINES="${DIALOG_LINES:-2000000}"
WIKI_LINES="${WIKI_LINES:-4000000}"

WIKI_RAW="$NB/corpus/wiki_train.txt"
WIKI_SEG="$NB/corpus/wiki_train_seg.txt"
DIALOG_SEG="$NB/corpus/lccc_train_seg.txt"
MIX="$NB/corpus/mixed_seg.txt"
ARPA="$NB/lm/zh_CN_mixed.arpa"
LMBIN="$NB/lm/zh_CN_mixed.lm"
PRED="$NB/lm/zh_CN_mixed.lm.predict"

[ -f "$NB/data/dict_sc.txt" ] || { echo "缺 $NB/data/dict_sc.txt（先 libime_pinyindict -d 导出）"; exit 1; }

# 1) 维基正文分词
echo "== [1] 维基正文分词 =="
"$ENV/bin/python" "$(dirname "$0")/scdict_segment.py" \
  --dict "$NB/data/dict_sc.txt" --input "$WIKI_RAW" --output "$WIKI_SEG"
wc -l "$WIKI_SEG"

# 2) 混合（对话 N 句 + 维基 N 句）
echo "== [2] 混合：对话 $DIALOG_LINES + 维基 $WIKI_LINES =="
head -n "$DIALOG_LINES" "$DIALOG_SEG" > "$MIX.part1"
head -n "$WIKI_LINES" "$WIKI_SEG" > "$MIX.part2"
cat "$MIX.part1" "$MIX.part2" > "$MIX"
rm -f "$MIX.part1" "$MIX.part2"
wc -l -w "$MIX"

# 3) lmplz 训练 3-gram
echo "== [3] lmplz 训练 3-gram =="
mkdir -p "$NB/lm/tmp"
"$NB/build/kenlm/bin/lmplz" -o 3 -S 5G -T "$NB/lm/tmp" --discount_fallback \
  --text "$MIX" --arpa "$ARPA"
ls -l "$ARPA"

# 4) 二值化 + predict
echo "== [4] 二值化 + predict =="
export LD_LIBRARY_PATH="$OPT/libime/lib:$OPT/fcitx5/lib:$ENV/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
"$OPT/libime/bin/libime_slm_build_binary" -s -a 22 -q 4 trie "$ARPA" "$LMBIN"
"$OPT/libime/bin/libime_prediction" "$LMBIN" "$ARPA" "$PRED"
ls -l "$LMBIN"

echo "== 完成 =="
echo "LM: $LMBIN"
echo "评测：cd bench && python3 run_bench.py --corpus corpus/lccc_eval.txt --cli ../cpp/build/libime_bench --cli-arg $LMBIN --cli-arg $OPT/libime/share/libime/sc.dict --cli-arg 5"