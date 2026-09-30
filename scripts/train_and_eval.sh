#!/bin/bash
# 一键训练 + 评测脚本
# 用法: bash scripts/train_and_eval.sh [corpus.txt]
set -e

CORPUS="${1:-bench/corpus/news.txt}"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"

NB="/workspace/native-build"
ENV="$NB/mamba-env"
OPT="$NB/opt"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/libime/lib:$OPT/fcitx5/lib"
CLI="$PROJ_DIR/cpp/build/pinyin_ime_cli"
LM="$OPT/libime/lib/libime/zh_CN.lm"
DICT="$OPT/libime/share/libime/sc.dict"

UD=$(mktemp -d)
trap "rm -rf $UD" EXIT

echo "=== 1. 生成训练对 ==="
python3 "$SCRIPT_DIR/train_from_corpus.py" "$CORPUS" /tmp/_train_pairs.txt

echo "=== 2. 词级学习 ==="
$CLI --learn-file /tmp/_train_pairs.txt --user-data "$UD" "$LM" "$DICT"

echo "=== 3. 评测 ==="
python3 "$SCRIPT_DIR/quick_eval.py" "$CORPUS" "$UD" "$CLI" "$LM" "$DICT"

rm -f /tmp/_train_pairs.txt
