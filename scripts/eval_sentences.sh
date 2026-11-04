#!/bin/bash
# 句级评测脚本
CLI="./cpp/build/pinyin_ime_cli"
LM="./pinyin-ime-s3/v0.9.139/share/libime/libime/zh_CN.lm"
DICT="./pinyin-ime-s3/v0.9.139/share/libime/libime/sc.dict"
export LD_LIBRARY_PATH="./pinyin-ime-s3/v0.9.139/lib:$LD_LIBRARY_PATH"

CORPUS="cpp/eval_sentences.txt"
USERDATA="/tmp/eval_sent_$$"
mkdir -p "$USERDATA"

correct=0
total=0
top5_correct=0
fail_list=""

while IFS=$'\t' read -r input expected; do
    # Skip comments and empty lines
    [[ "$input" =~ ^# ]] && continue
    [[ -z "$input" ]] && continue
    
    result=$(echo "$input" | $CLI "$LM" "$DICT" --user-data "$USERDATA" 2>/dev/null | head -5)
    first=$(echo "$result" | head -1 | cut -f1)
    
    total=$((total + 1))
    
    if [ "$first" = "$expected" ]; then
        correct=$((correct + 1))
    else
        fail_list="$fail_list\n  ❌ [$total] $input → '$first' (期望: $expected)"
    fi
    
    # Top5 check
    if echo "$result" | grep -qF "$expected"; then
        top5_correct=$((top5_correct + 1))
    fi
done < "$CORPUS"

echo "=========================================="
echo "句级评测结果"
echo "=========================================="
echo "总条目: $total"
echo "首选命中: $correct/$total ($(( correct * 100 / total ))%)"
echo "Top5命中: $top5_correct/$total ($(( top5_correct * 100 / total ))%)"
echo "=========================================="
if [ -n "$fail_list" ]; then
    echo -e "失败列表:$fail_list"
fi

rm -rf "$USERDATA"
