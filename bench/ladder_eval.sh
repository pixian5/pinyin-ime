#!/bin/sh
# 口语语料配比阶梯实验：找「对话收益 vs 新闻回退」的拐点。
#
# 背景：在库训练语料仅约 30 万字，加 1M 句 LCCC（1061 万字）会让转移表
# 变成对话专用模型，新闻域从 51.9% 崩到 33.8%。故需在配比轴上定位拐点。
#
# 对话域用**全部 112 句**留出集（此前只抽 40 句，统计功效不足）。
# 用法: sh bench/ladder_eval.sh
set -eu
cd "$(dirname "$0")/.."
PY=bench/.venv/bin/python
DATA=correction/data/bigram_costs.json
POOL=/root/workspace/native-build/corpus/lccc_raw.txt
WORK=/root/workspace/native-build/ladder
OUT=$WORK/logs
D=bench/corpus

mkdir -p "$WORK" "$OUT"

restore() { git checkout -- "$DATA" 2>/dev/null || true; }
trap restore EXIT INT TERM

# 阶梯：口语句子数
for n in 5000 20000 50000 150000 400000; do
    sub="$WORK/lccc_${n}.txt"
    [ -s "$sub" ] || head -n "$n" "$POOL" > "$sub"
    table="$WORK/bigram_${n}.json"
    if [ ! -s "$table" ]; then
        echo "### 标定 $n 句 -> $table"
        $PY -m correction.scripts.calibrate_transition \
            --corpus $D/wiki.txt $D/news.txt $D/daily.txt $D/recent.txt \
                     $D/baike.txt "$sub" \
            --out "$table" > "$OUT/calib_${n}.log" 2>&1
    fi
    echo "### 评测 $n 句"
    cp "$table" "$DATA"
    $PY -m correction.sentence_eval --domains dialogue news \
        --per-domain 112 --seed 0 > "$OUT/eval_${n}.txt" 2>&1
    restore
done

echo "### 基线（纯在库语料，无口语）"
cp /tmp/bigram_costs_orig.json "$DATA"
$PY -m correction.sentence_eval --domains dialogue news \
    --per-domain 112 --seed 0 > "$OUT/eval_0.txt" 2>&1
restore

echo "=== 阶梯完成，日志在 $OUT ==="
