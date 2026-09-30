#!/bin/bash
# 预热脚本：加载预置学习数据，使输入法开箱即用
# 两阶段学习：先学普通短语，再学关键消歧短语（最后学习权重最高）
# 用法: bash scripts/warmup.sh

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"
source ./setup_env.sh 2>/dev/null

PRESET="$PROJECT_ROOT/data/preset_learning.txt"
USER_DATA="${HOME}/.local/share/pinyin-ime"

echo "=== pinyin-ime 预热脚本 ==="
echo "预置数据: $PRESET"
echo "用户数据: $USER_DATA"
echo ""

mkdir -p "$USER_DATA"

if [ ! -f "$PRESET" ]; then
    echo "❌ 预置数据文件不存在: $PRESET"
    exit 1
fi

# 关键消歧短语（需要最高权重）
CRITICAL="shanghaishi|nanjingshi|chengdushi|nanjingshiliuchaogudu|woxiangdingyigefangjian|nihaoshijie|xuexishiwokuaile"

# Pass 1: 非关键短语 (3x权重)
TMP1=$(mktemp)
grep -v '^#' "$PRESET" | grep -v '^$' | grep -vE "$CRITICAL" | while read line; do
    for i in 1 2 3; do echo "$line"; done
done > "$TMP1"
P1_COUNT=$(grep -v '^#' "$PRESET" | grep -v '^$' | grep -vcE "$CRITICAL")

# Pass 2: 关键消歧短语 (10x权重，最后学习)
TMP2=$(mktemp)
grep -v '^#' "$PRESET" | grep -v '^$' | grep -E "$CRITICAL" | while read line; do
    for i in 1 2 3 4 5 6 7 8 9 10; do echo "$line"; done
done > "$TMP2"
P2_COUNT=$(grep -v '^#' "$PRESET" | grep -v '^$' | grep -cE "$CRITICAL")

echo "阶段1: $P1_COUNT 个普通短语 (3x权重)"
echo "dummy" | "$PINYIN_IME_CLI" --quiet --learn-file "$TMP1" \
    --user-data "$USER_DATA" "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 2>&1

echo "阶段2: $P2_COUNT 个关键消歧短语 (10x权重)"
echo "dummy" | "$PINYIN_IME_CLI" --quiet --learn-file "$TMP2" \
    --user-data "$USER_DATA" "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 2>&1

rm -f "$TMP1" "$TMP2"
echo "✅ 预热完成"
echo ""

# 验证关键短语
echo "=== 验证关键短语 ==="
VERIFY=(
    "shanghaishi:上海是"
    "xianbeijing:西安北京"
    "nanjingshiliuchaogudu:南京是六朝古都"
    "woxiangdingyigefangjian:我想订一个房间"
    "rengongzhineng:人工智能"
    "nihaoshijie:你好世界"
    "xuexishiwokuaile:学习使我快乐"
)
PASS=0; TOTAL=0
for item in "${VERIFY[@]}"; do
    INPUT="${item%%:*}"; EXPECTED="${item##*:}"
    TOTAL=$((TOTAL + 1))
    RESULT=$(echo "$INPUT" | "$PINYIN_IME_CLI" --quiet --top1 "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 2>/dev/null)
    if [ "$RESULT" = "$EXPECTED" ]; then
        echo "  ✅ $INPUT → $RESULT"; PASS=$((PASS + 1))
    else
        echo "  ❌ $INPUT → $RESULT (期望: $EXPECTED)"
    fi
done
echo ""
echo "验证: $PASS/$TOTAL 通过"
