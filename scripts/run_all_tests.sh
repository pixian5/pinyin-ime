#!/bin/bash
# pinyin-ime 全面测试汇总
# 运行所有测试脚本并汇总结果
#
# 用法: bash scripts/run_all_tests.sh

set -uo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

BUNDLE=/root/.local/pinyin-ime/opt/fcitx5-x11
DEPSLIB=/root/.local/pinyin-ime/opt/deps-lib
LIBIME=/root/workspace/native-build/opt/libime/lib64
ENV_DIR=/root/workspace/native-build/mamba-env

export PATH="$BUNDLE/bin:$ENV_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$LIBIME:$DEPSLIB:$DEPSLIB/lib64:$BUNDLE/lib:$ENV_DIR/lib"
export DISPLAY=:99
export DBUS_SESSION_BUS_ADDRESS="unix:abstract=/tmp/dbus-dVh9vTwJ2P,guid=b9840e460a5f77f64f8f57006acaf772"
export FCITX_ADDON_DIRS="$BUNDLE/lib/fcitx5"
export FCITX_DATA_DIR="$BUNDLE/share/fcitx5"
export XDG_DATA_DIRS="$BUNDLE/share:${XDG_DATA_DIRS:-}"

echo "=========================================="
echo "  pinyin-ime v$(cat VERSION) 全面测试"
echo "=========================================="
echo ""

# Service status
echo "--- 服务状态 ---"
echo "  Xvfb:    $(pgrep -x Xvfb > /dev/null && echo '✅' || echo '❌')"
echo "  dbus:    $(pgrep -x dbus-daemon > /dev/null && echo '✅' || echo '❌')"
echo "  fcitx5:  $(pgrep -x fcitx5 > /dev/null && echo '✅' || echo '❌')"
echo ""

# Run each test
declare -A results

run_test() {
    local name="$1"
    local cmd="$2"
    local result
    result=$(eval "$cmd" 2>&1)
    local score=$(echo "$result" | grep -oP '\d+/\d+' | tail -1)
    if [ -z "$score" ]; then
        score=$(echo "$result" | grep -oP '\d+ ✅' | head -1 | grep -oP '\d+')
        if [ -n "$score" ]; then
            score="${score}/?"
        else
            score="?"
        fi
    fi
    echo "  $name: $score"
}

echo "--- 测试结果 ---"
run_test "healthcheck (29项)"     "bash scripts/healthcheck.sh"
run_test "CI回归 (12项)"          "bash scripts/ci_test.sh"
run_test "模糊音 (18项)"          "python3.9 scripts/fuzzy_pinyin_test.py"
run_test "Qt端到端 (10项)"        "bash scripts/xvfb_qt_ime_test.sh"
run_test "XIM协议 (4项)"          "python3.9 scripts/xim_test.py"
run_test "用户词典持久化 (6项)"   "python3.9 scripts/user_dict_persistence_test.py"
run_test "性能测试"               "python3.9 scripts/performance_test.py"
run_test "候选词 (8项)"           "python3.9 scripts/candidate_test.py"
run_test "X11端到端 (7项)"        "$ENV_DIR/bin/python3 scripts/x11_e2e_test.py"
echo ""

# Addon status
echo "--- addon 加载 ---"
addons=$(grep "Loaded addon" /tmp/fcitx5.log 2>/dev/null | awk '{print $NF}' | tr '\n' ' ')
echo "  已加载: $addons"
echo ""

# Error count
echo "--- 日志错误 ---"
errors=$(grep -c "ERROR\|Error\|Fontconfig error" /tmp/fcitx5.log 2>/dev/null || echo 0)
echo "  错误数: $errors"
echo ""

# Git info
echo "--- Git ---"
echo "  分支: $(git branch --show-current)"
echo "  最新: $(git log --oneline -1)"
echo ""

# S3
echo "--- S3 ---"
rclone ls cst:xbz/pinyin-ime/v0.9.84a/ 2>/dev/null | while read size path; do
    echo "  $path ($size bytes)"
done
echo ""

echo "=========================================="
echo "  测试完成"
echo "=========================================="
