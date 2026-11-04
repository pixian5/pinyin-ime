#!/bin/bash
# run_eval.sh — 评测 CLI 候选准确率
# 用法: bash scripts/run_eval.sh
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

export LD_LIBRARY_PATH="${PROJECT_ROOT}/pinyin-ime-s3/v0.9.139/lib:${LD_LIBRARY_PATH:-}"
LM="./pinyin-ime-s3/v0.9.139/share/libime/libime/zh_CN.lm"
DICT="./pinyin-ime-s3/v0.9.139/share/libime/libime/sc.dict"
CLI="./cpp/build/pinyin_ime_cli"
CORPUS="cpp/eval_corpus.txt"

total=0
passed=0
failed=0
first_choice_correct=0
in_top5=0

echo "========== 拼音输入法评测 =========="
echo "语料: $CORPUS"
echo "时间: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""

while IFS=$'\t' read -r input expected; do
    # Skip comments and empty lines
    [[ "$input" =~ ^# ]] && continue
    [[ -z "$input" ]] && continue
    
    total=$((total + 1))
    
    # Get candidates from CLI
    output=$(echo "$input" | $CLI $LM $DICT 2>/dev/null | head -1)
    
    # Extract first candidate (before tab)
    first=$(echo "$output" | cut -f1)
    
    # Check if first choice matches
    if [[ "$first" == "$expected" ]]; then
        passed=$((passed + 1))
        first_choice_correct=$((first_choice_correct + 1))
        in_top5=$((in_top5 + 1))
    else
        # Check if expected is in top 5
        found_in_top5=false
        IFS=$'\t' read -ra cands <<< "$output"
        for i in "${!cands[@]}"; do
            if [[ $i -lt 5 ]] && [[ "${cands[$i]}" == "$expected" ]]; then
                found_in_top5=true
                break
            fi
        done
        
        if $found_in_top5; then
            in_top5=$((in_top5 + 1))
            echo "  ⚠️  $input → 首选: $first (期望: $expected) [在Top5中]"
        else
            failed=$((failed + 1))
            echo "  ❌ $input → 首选: $first (期望: $expected)"
        fi
    fi
done < "$CORPUS"

echo ""
echo "========== 评测结果 =========="
echo "总条目: $total"
echo "首选命中: $first_choice_correct ($(( first_choice_correct * 100 / total ))%)"
echo "Top5命中: $in_top5 ($(( in_top5 * 100 / total ))%)"
echo "完全未命中: $failed ($(( failed * 100 / total ))%)"
