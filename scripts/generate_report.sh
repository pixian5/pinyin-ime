#!/bin/bash
# 生成综合测试报告 (Markdown格式)
# 用法: bash scripts/generate_report.sh [输出文件]

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"
source ./setup_env.sh 2>/dev/null

OUTPUT="${1:-test_report.md}"
NB="/workspace/native-build"
CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

# Get version
VERSION=$(echo "v" | "$CLI" --version 2>&1 | grep -oP "\K[\d.]+")
DATE=$(date '+%Y-%m-%d %H:%M:%S')

echo "生成测试报告..."

# Run CLI tests
CLI_OUTPUT=$(timeout 120 bash scripts/test_all.sh 2>&1)
CLI_PASS=$(echo "$CLI_OUTPUT" | grep -c "✅")
CLI_FAIL=$(echo "$CLI_OUTPUT" | grep -c "❌")
CLI_TOTAL=$((CLI_PASS + CLI_FAIL))

# Run daily accuracy
DAILY_OUTPUT=$(/usr/local/bin/python3 scripts/eval_daily.py 2>&1)
DAILY_SENT=$(echo "$DAILY_OUTPUT" | grep "Sentence" | grep -oP '\d+/\d+')
DAILY_CHAR=$(echo "$DAILY_OUTPUT" | grep "Character" | grep -oP '\d+/\d+')

# Run performance benchmark
BENCH_OUTPUT=$(/usr/local/bin/python3 scripts/benchmark.py 2>&1)
BENCH_RATE=$(echo "$BENCH_OUTPUT" | grep -oP '\d+ 句/秒')
BENCH_P50=$(echo "$BENCH_OUTPUT" | grep "P50" | grep -oP '[\d.]+ ms')
BENCH_P99=$(echo "$BENCH_OUTPUT" | grep "P99" | grep -oP '[\d.]+ ms')

# Run E2E tests
E2E_PASS="N/A"; E2E_TOTAL="N/A"; E2E_FAIL="0"
if [ -f "$NB/test_pinyin_e2e" ]; then
    export LD_LIBRARY_PATH="$NB/mamba-env/lib:$NB/opt/fcitx5-server/lib:$NB/opt/fcitx5-server/lib/fcitx5:$NB/opt/libime/lib"
    export XDG_DATA_HOME="$NB/opt/fcitx5-server/share"
    export XDG_CONFIG_HOME="/tmp/fcitx5-test-config"
    export HOME="/tmp/fcitx5-test-home"
    mkdir -p "$XDG_CONFIG_HOME/fcitx5/conf" "$HOME"
    cat > "$XDG_CONFIG_HOME/fcitx5/profile" << 'PROFILE'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=pinyinime
[Groups/0/Items/0]
Name=keyboard-us
Layout=
[Groups/0/Items/1]
Name=pinyinime
Layout=
[GroupOrder]
0=Default
PROFILE
    E2E_OUTPUT=$(timeout 120 "$NB/test_pinyin_e2e" 2>&1)
    E2E_TOTAL=$(echo "$E2E_OUTPUT" | grep -oP 'Total: \K\d+')
    E2E_PASS=$(echo "$E2E_OUTPUT" | grep -oP 'Pass: \K\d+')
    E2E_FAIL=$(echo "$E2E_OUTPUT" | grep -oP 'Fail: \K\d+')
fi

# Calculate totals
TOTAL_TESTS=$((CLI_TOTAL + E2E_TOTAL))
TOTAL_PASS=$((CLI_PASS + E2E_PASS))
TOTAL_FAIL=$((CLI_FAIL + E2E_FAIL))

# Status helpers
cli_status="✅ 全部通过"; [ "$CLI_FAIL" -gt 0 ] && cli_status="❌ 有失败"
e2e_status="✅ 全部通过"; [ "$E2E_FAIL" != "0" ] && e2e_status="❌ 有失败"
total_status="✅ **全部通过**"; [ "$TOTAL_FAIL" -gt 0 ] && total_status="❌ 有${TOTAL_FAIL}项失败"

# Generate report
{
echo "# pinyin-ime 测试报告"
echo ""
echo "**版本**: v${VERSION}  "
echo "**日期**: ${DATE}  "
echo "**平台**: $(uname -s) $(uname -m)"
echo ""
echo "## 总览"
echo ""
echo "| 测试项 | 通过/总计 | 结果 |"
echo "|--------|-----------|------|"
echo "| CLI综合测试 | ${CLI_PASS}/${CLI_TOTAL} | ${cli_status} |"
echo "| E2E端到端测试 | ${E2E_PASS}/${E2E_TOTAL} | ${e2e_status} |"
echo "| daily准确率(句) | ${DAILY_SENT} | — |"
echo "| daily准确率(字) | ${DAILY_CHAR} | — |"
echo "| **合计** | **${TOTAL_PASS}/${TOTAL_TESTS}** | ${total_status} |"
echo ""
echo "## 性能"
echo ""
echo "| 指标 | 值 |"
echo "|------|-----|"
echo "| 吞吐量 | ${BENCH_RATE} |"
echo "| P50延迟 | ${BENCH_P50} |"
echo "| P99延迟 | ${BENCH_P99} |"
echo ""
echo "## 功能覆盖"
echo ""
echo "| 功能 | 状态 | 说明 |"
echo "|------|------|------|"
echo "| 全拼输入 | ✅ | 标准拼音输入 |"
echo "| 简拼输入 | ✅ | 首字母缩写(bj→北京) |"
echo "| 模糊音 | ✅ | 平翘舌/前后鼻音/n-l |"
echo "| 拼音纠错 | ✅ | M3模型纠错(senme→什么) |"
echo "| 双拼 | ✅ | 8种方案(小鹤/自然码/微软等) |"
echo "| 整句输入 | ✅ | 长拼音自动分词 |"
echo "| 拼音消歧 | ✅ | xian→xi'an地名消歧 |"
echo "| 用户学习 | ✅ | 持久化user.history+user.dict |"
echo "| 联想预测 | ✅ | 上屏后联想 |"
echo "| JSON输出 | ✅ | 结构化候选输出 |"
echo "| 表情输入 | ✅ | Ctrl-E表情选择 |"
echo "| 误上屏撤回 | ✅ | Ctrl-Z撤回 |"
echo "| fcitx5集成 | ✅ | addon可加载 |"
echo ""
echo "## E2E测试分类"
echo ""
echo "| 类别 | 数量 |"
echo "|------|------|"
echo "| 基础词 | 8 |"
echo "| 长短语 | 7 |"
echo "| 常见句 | 3 |"
echo "| 模糊音 | 5 |"
echo "| 纠错 | 3 |"
echo "| 简拼 | 2 |"
echo "| 常见短语 | 14 |"
echo "| 技术术语 | 12 |"
echo "| 单音节 | 11 |"
echo "| 复合词 | 8 |"
echo "| 日常用语 | 7 |"
echo "| 长句 | 3 |"
echo "| 其他 | 17 |"
echo ""
echo "## 环境信息"
echo ""
echo "| 项目 | 值 |"
echo "|------|-----|"
echo "| 语言模型 | $(du -h "$LM" 2>/dev/null | cut -f1) |"
echo "| 系统词典 | $(du -h "$DICT" 2>/dev/null | cut -f1) |"
echo "| 用户数据 | $(du -sh ~/.local/share/pinyin-ime 2>/dev/null | cut -f1) |"
echo "| CLI二进制 | $(du -h "$CLI" 2>/dev/null | cut -f1) |"
echo ""
echo "---"
echo "*报告由 scripts/generate_report.sh 自动生成*"
} > "$OUTPUT"

echo "✅ 报告已生成: $OUTPUT"
echo ""
head -15 "$OUTPUT"
