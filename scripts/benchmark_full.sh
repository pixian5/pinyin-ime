#!/bin/bash
# 综合性能基准 — 测量启动/查询/吞吐/准确率/内存
# 用法: bash scripts/benchmark_full.sh
cd "$(dirname "$0")/.."
source ./setup_env.sh

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

echo "=== pinyin-ime 综合性能基准 ==="
echo "版本: $(cat VERSION)"
echo "日期: $(date '+%Y-%m-%d %H:%M:%S')"
echo ""

# 1. 启动时间
echo "--- 1. 启动时间 ---"
for i in 1 2 3; do
    start=$(date +%s%N)
    echo "" | $CLI --quiet --top1 "$LM" "$DICT" > /dev/null
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 1000000 ))
    printf "  第%d次: %dms\n" "$i" "$elapsed"
done

# 2. 批量吞吐
echo ""
echo "--- 2. 批量吞吐 ---"
cat > /tmp/bench_input.txt << 'EOF'
nihao
zhongguo
beijingdaxue
shanghaishi
woxiangdingyigefangjian
mingtianjian
woxihuanbiancheng
zhongguoshiyigedaguo
woaini
tamenbuzhidaozhejianshi
EOF

# Run 100 queries
start=$(date +%s%N)
for i in $(seq 1 10); do
    while IFS= read -r line; do
        echo "$line" | $CLI --quiet --top1 "$LM" "$DICT" > /dev/null
    done < /tmp/bench_input.txt
done
end=$(date +%s%N)
total_ms=$(( (end - start) / 1000000 ))
avg_ms=$(( total_ms / 100 ))
tps=$(( 100000 / total_ms ))
printf "  100条总时间: %dms\n" "$total_ms"
printf "  平均: %dms/条\n" "$avg_ms"
printf "  吞吐: %d 条/秒\n" "$tps"

# 3. 不同输入长度延迟
echo ""
echo "--- 3. 不同输入长度延迟 ---"
for input in ni nihao nihaozhongguo nihaozhongguorenmin beijingdaxueqinghuadaxue woxiangqubeijinglvyoukanyikantiananmen; do
    start=$(date +%s%N)
    for i in 1 2 3; do
        echo "$input" | $CLI --quiet --top1 "$LM" "$DICT" > /dev/null
    done
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 3 / 1000000 ))
    len=${#input}
    result=$(echo "$input" | $CLI --quiet --top1 "$LM" "$DICT")
    printf "  %2d字符: %3dms → %s\n" "$len" "$elapsed" "$result"
done

# 4. 准确率（使用 boost 测试集）
echo ""
echo "--- 4. 准确率 ---"
PASS=0; TOTAL=0
while IFS='|' read -r input expected; do
    [ -z "$input" ] && continue
    result=$(echo "$input" | $CLI --quiet --top1 "$LM" "$DICT")
    TOTAL=$((TOTAL + 1))
    if [ "$result" = "$expected" ]; then
        PASS=$((PASS + 1))
    fi
done << 'EOF'
nihao|你好
zhongguo|中国
shanghaishi|上海市
woxiangdingyigefangjian|我想订一个房间
mingtianjian|明天见
woaini|我爱你
woshizhongguoren|我是中国人
zhongguoshiyigedaguo|中国是一个大国
beijingshishoudu|北京是首都
dongdeqi|懂得起
EOF
printf "  %d/%d (%.1f%%)\n" "$PASS" "$TOTAL" "$(echo "scale=1; $PASS * 100 / $TOTAL" | bc)"

# 5. 功能延迟对比
echo ""
echo "--- 5. 功能延迟对比 ---"
measure() {
    local label="$1"; shift
    local start end elapsed
    start=$(date +%s%N)
    for i in 1 2 3; do
        echo "nihao" | $CLI --quiet --top1 "$@" "$LM" "$DICT" > /dev/null
    done
    end=$(date +%s%N)
    elapsed=$(( (end - start) / 3 / 1000000 ))
    printf "  %-4s: %dms\n" "$label" "$elapsed"
}
measure "普通"
measure "模糊" --fuzzy
measure "纠错" --correct

echo ""
echo "=== 基准完成 ==="
