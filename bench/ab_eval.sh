#!/bin/sh
# 三方对照：基线（在库转移表） vs A(+LCCC 50K) vs B(+LCCC 1M)
#
# 只换 correction/data/bigram_costs.json 这一个变量，评测协议固定
# （--per-domain 40 --seed 0），因此差异可归因到语料。
# 域跑 dialogue（留出集，看是否补上口语短板）+ news（回归哨兵，防「补口语把新闻讲坏」）。
#
# 用法: sh bench/ab_eval.sh
set -eu
cd "$(dirname "$0")/.."
PY=bench/.venv/bin/python
DATA=correction/data/bigram_costs.json
OUT=${OUT:-/root/workspace/native-build/logs}
DOMAINS="${DOMAINS:-dialogue news}"

# 关键：中断/超时也必须把在库产物还原，否则会留下未提交变体污染工作区
restore() { git checkout -- "$DATA" 2>/dev/null || true; }
trap restore EXIT INT TERM

run_case() {
    _label="$1"
    _file="$2"
    _tag="$3"
    if [ ! -f "$_file" ]; then
        echo "!! $_label 数据缺失，跳过: $_file"
        return
    fi
    cp "$_file" "$DATA"
    # 原始输出整段落盘（不要再用 sed 截区间，容易把表体切掉）
    $PY -m correction.sentence_eval --domains $DOMAINS \
        --per-domain 40 --seed 0 > "$OUT/ab_${_tag}.txt" 2>&1
    echo "=== $_label -> $OUT/ab_${_tag}.txt ==="
}

run_case "基线（无口语语料）" /tmp/bigram_costs_orig.json base
run_case "A：默认语料 + LCCC 50K" /tmp/bigram_A_50k.json a50k
run_case "B：默认语料 + LCCC 1M" /tmp/bigram_B_1m.json b1m

echo "=== 完成（在库产物已由 trap 还原）==="
