#!/bin/bash
# =============================================================================
# pinyin-ime 综合测试报告生成器
# 运行所有测试并生成 Markdown 报告
# 用法: bash scripts/generate_report.sh [output.md]
# =============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
OUTPUT="${1:-$PROJ_DIR/docs/TEST_REPORT.md}"

cd "$PROJ_DIR"
source ./setup_env.sh 2>/dev/null

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"
VERSION=$(cat VERSION 2>/dev/null || echo "unknown")
DATE=$(date '+%Y-%m-%d %H:%M:%S')

mkdir -p "$(dirname "$OUTPUT")"

{
echo "# pinyin-ime 测试报告"
echo ""
echo "**版本**: v${VERSION} | **日期**: ${DATE} | **平台**: $(uname -m) $(uname -r)"
echo ""

# --- 1. Smoke Tests ---
echo "## 1. 冒烟测试"
echo ""
echo '```'
bash scripts/smoke_test.sh 2>&1 || true
echo '```'
echo ""

# --- 2. Interactive Tests ---
echo "## 2. 交互测试"
echo ""
echo '```'
python3 scripts/test_interactive.py "$CLI" "$LM" "$DICT" 2>&1 || true
echo '```'
echo ""

# --- 3. Daily Evaluation ---
echo "## 3. Daily 评测"
echo ""
echo '```'
python3 scripts/eval_daily.py "$CLI" "$LM" "$DICT" 2>&1 || true
echo '```'
echo ""

# --- 4. Warmup ---
echo "## 4. 预热验证"
echo ""
echo '```'
bash scripts/warmup.sh 2>&1 || true
echo '```'
echo ""

# --- 5. E2E ---
echo "## 5. E2E 端到端测试"
echo ""
echo '```'
bash scripts/build_e2e_test.sh 2>&1 | tail -20 || true
echo '```'
echo ""

# --- 6. Performance ---
echo "## 6. 性能基准"
echo ""
echo '```'
python3 scripts/benchmark.py 2>&1 || true
echo '```'
echo ""

# --- 7. Correction ---
echo "## 7. 纠错能力"
echo ""
echo "| 输入 | 无纠错 | 有纠错 | 纠错生效 |"
echo "|---|---|---|---|"
for input in senme zongguo sengri nihou shengri; do
    with_corr=$(echo "$input" | "$CLI" --top1 --quiet --correct "$LM" "$DICT" 2>/dev/null)
    without=$(echo "$input" | "$CLI" --top1 --quiet "$LM" "$DICT" 2>/dev/null)
    if [ "$with_corr" != "$without" ]; then
        echo "| $input | $without | $with_corr | ✅ |"
    else
        echo "| $input | $without | $with_corr | - |"
    fi
done
echo ""

# --- 8. Fuzzy ---
echo "## 8. 模糊音"
echo ""
echo "| 输入 | 无模糊 | 有模糊 | 模糊生效 |"
echo "|---|---|---|---|"
for input in cifan zeli lanfang sijiao; do
    with_fuzzy=$(echo "$input" | "$CLI" --top1 --quiet --fuzzy "$LM" "$DICT" 2>/dev/null)
    without=$(echo "$input" | "$CLI" --top1 --quiet "$LM" "$DICT" 2>/dev/null)
    if [ "$with_fuzzy" != "$without" ]; then
        echo "| $input | $without | $with_fuzzy | ✅ |"
    else
        echo "| $input | $without | $with_fuzzy | - |"
    fi
done
echo ""

# --- Summary ---
echo "## 总结"
echo ""
echo "- **版本**: v${VERSION}"
echo "- **生成时间**: ${DATE}"
echo "- **平台**: $(uname -m) $(uname -r)"
echo ""

} > "$OUTPUT"

echo "测试报告已生成: $OUTPUT"
echo "报告大小: $(wc -l < "$OUTPUT") 行"
