#!/bin/bash
# Comprehensive test report for pinyin-ime
# Runs all test suites and generates a summary

set -e
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
NB=${NB:-/root/workspace/native-build}

echo "================================================"
echo "  pinyin-ime 综合测试报告"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================"
echo ""

# 1. CLI Comprehensive Tests
echo "【1】CLI 综合测试"
echo "------------------------------------------------"
cd "$PROJECT_ROOT"
source ./setup_env.sh 2>/dev/null
CLI_RESULTS=$(bash scripts/test_all.sh 2>&1)
CLI_PASS=$(echo "$CLI_RESULTS" | grep -c "✅")
CLI_FAIL=$(echo "$CLI_RESULTS" | grep -c "❌")
echo "  通过: $CLI_PASS / $((CLI_PASS + CLI_FAIL))"
if [ "$CLI_FAIL" -gt 0 ]; then
    echo "$CLI_RESULTS" | grep "❌"
fi
echo ""

# 2. Daily 40-sentence Accuracy
echo "【2】daily 40句准确率评测"
echo "------------------------------------------------"
DAILY_RESULTS=$(/usr/local/bin/python3 scripts/eval_daily.py 2>&1)
echo "$DAILY_RESULTS" | grep -E "Total|accuracy"
echo ""

# 3. Performance Benchmark
echo "【3】性能基准测试"
echo "------------------------------------------------"
BENCH_RESULTS=$(/usr/local/bin/python3 scripts/benchmark.py 2>&1)
echo "$BENCH_RESULTS" | grep -E "句/秒|延迟|吞吐"
echo ""

# 4. E2E End-to-End Test
echo "【4】E2E 端到端测试 (fcitx5 testfrontend)"
echo "------------------------------------------------"
if [ -f "$NB/test_pinyin_e2e" ]; then
    export PATH="$NB/mamba-env/bin:$PATH"
    export LD_LIBRARY_PATH="$NB/mamba-env/lib:$NB/opt/fcitx5-server/lib:$NB/opt/fcitx5-server/lib/fcitx5:$NB/opt/libime/lib"
    export XDG_DATA_HOME="$NB/opt/fcitx5-server/share"
    export XDG_CONFIG_HOME="/tmp/fcitx5-test-config"
    export HOME="/tmp/fcitx5-test-home"
    mkdir -p "$XDG_CONFIG_HOME/fcitx5/conf" "$HOME"
    cat > "$XDG_CONFIG_HOME/fcitx5/profile" << 'EOF'
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
EOF
    E2E_RESULTS=$(timeout 60 "$NB/test_pinyin_e2e" 2>&1 | grep -v "^W\|^I")
    echo "$E2E_RESULTS" | grep -E "Summary|Total|Accuracy|Failed"
else
    echo "  (E2E test binary not found, run scripts/build_e2e_test.sh first)"
fi
echo ""

# Summary
echo "================================================"
echo "  测试报告完成"
echo "================================================"
