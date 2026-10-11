#!/bin/bash
# pinyin-ime CI 回归测试脚本
# 在 CI 环境中运行全量测试，输出 JUnit 风格结果
# 用法: bash scripts/ci_test.sh
#
# 退出码: 0=全通过, 1=有失败

set -uo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NB="${NB:-/root/workspace/native-build}"
BUNDLE="${BUNDLE:-/root/.local/pinyin-ime/opt/fcitx5-x11}"
DEPSLIB="${DEPSLIB:-/root/.local/pinyin-ime/opt/deps-lib}"
LIBIME="$NB/opt/libime/lib64"
ENV_DIR="$NB/mamba-env"
S3_DIR="$PROJECT_ROOT/pinyin-ime-s3"

export PATH="$BUNDLE/bin:$ENV_DIR/bin:${PATH:-}"
export LD_LIBRARY_PATH="$LIBIME:$DEPSLIB:$DEPSLIB/lib64:$BUNDLE/lib:$ENV_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DISPLAY=:99
export FCITX_ADDON_DIRS="$BUNDLE/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$BUNDLE/share/fcitx5/addon"
export FCITX_DATA_DIR="$BUNDLE/share/fcitx5"
export XDG_DATA_DIRS="$BUNDLE/share:${XDG_DATA_DIRS:-}"
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
if [ -f /tmp/dbus_addr ]; then
    export DBUS_SESSION_BUS_ADDRESS="$(cat /tmp/dbus_addr 2>/dev/null || true)"
fi

total_pass=0
total_fail=0
test_results=""

record() {
    local name="$1" result="$2" detail="${3:-}"
    if [ "$result" = "PASS" ]; then
        total_pass=$((total_pass + 1))
        test_results="$test_results\n  <testcase name=\"$name\" />"
        echo "  ✅ $name $detail"
    else
        total_fail=$((total_fail + 1))
        test_results="$test_results\n  <testcase name=\"$name\">\n    <failure message=\"$detail\" />\n  </testcase>"
        echo "  ❌ $name $detail"
    fi
}

echo "=========================================="
echo "  pinyin-ime CI 回归测试"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "=========================================="
echo ""

# --- Test 1: CLI 冒烟测试 ---
echo "--- Test 1: CLI 冒烟测试 (10词) ---"
CLI="$S3_DIR/pinyin_ime_cli_v0.9.83"
LM="$S3_DIR/zh_CN.lm"
DICT="$S3_DIR/sc.dict"

cli_words="zhongguo:中国 nihao:你好 shijie:世界 pinyin:拼音 shurufa:输入法 \
nihao:你好 zhongwen:中文 diannao:电脑 jianpan:键盘 shubiao:鼠标"

cli_pass=0; cli_total=0
for input in $cli_words; do
    py=$(echo $input | cut -d: -f1)
    expected=$(echo $input | cut -d: -f2)
    result=$(echo "$py" | $CLI "$LM" "$DICT" 2>/dev/null | head -1 | cut -f1)
    cli_total=$((cli_total + 1))
    if [ "$result" = "$expected" ]; then
        cli_pass=$((cli_pass + 1))
    fi
done
record "CLI冒烟测试" "$([ $cli_pass -eq $cli_total ] && echo PASS || echo FAIL)" "$cli_pass/$cli_total"
echo ""

# --- Test 2: e2e DBus 测试 ---
echo "--- Test 2: DBus 端到端测试 ---"
if pgrep -x fcitx5 > /dev/null; then
    e2e_output=$(python3.9 "$PROJECT_ROOT/scripts/e2e_input_test.py" 2>&1)
    e2e_pass=$(echo "$e2e_output" | grep "Tests passed:" | grep -oP '\d+(?=/)')
    e2e_total=$(echo "$e2e_output" | grep "Tests passed:" | grep -oP '/\K\d+')
    record "e2e_input_test" "$([ "$e2e_pass" = "$e2e_total" ] && echo PASS || echo FAIL)" "${e2e_pass:-0}/${e2e_total:-0}"
else
    record "e2e_input_test" "FAIL" "fcitx5 not running"
fi
echo ""

# --- Test 3: 交互测试 ---
echo "--- Test 3: 交互测试 ---"
if pgrep -x fcitx5 > /dev/null; then
    int_output=$(python3.9 "$PROJECT_ROOT/scripts/interaction_test.py" 2>&1)
    record "interaction_test" "$(echo "$int_output" | grep -q 'ALL PASS' && echo PASS || echo FAIL)"
else
    record "interaction_test" "FAIL" "fcitx5 not running"
fi
echo ""

# --- Test 4: 选词测试 ---
echo "--- Test 4: 选词测试 ---"
if pgrep -x fcitx5 > /dev/null; then
    sel_output=$(python3.9 "$PROJECT_ROOT/scripts/select_test.py" 2>&1)
    record "select_test" "$(echo "$sel_output" | grep -q 'ALL PASS' && echo PASS || echo FAIL)"
else
    record "select_test" "FAIL" "fcitx5 not running"
fi
echo ""

# --- Test 5: addon 加载检查 ---
echo "--- Test 5: addon 加载检查 ---"
log_file=/tmp/fcitx5.log
if [ -f "$log_file" ]; then
    record "pinyinime addon" "$(grep -q 'Loaded addon pinyinime' $log_file && echo PASS || echo FAIL)"
    record "xim addon" "$(grep -q 'Loaded addon xim' $log_file && echo PASS || echo FAIL)"
    record "dbusfrontend addon" "$(grep -q 'Loaded addon dbusfrontend' $log_file && echo PASS || echo FAIL)"
    record "classicui addon" "$(grep -q 'Loaded addon classicui' $log_file && echo PASS || echo FAIL)"
    record "no XKB errors" "$(grep -q 'xkbcommon: ERROR' $log_file && echo FAIL || echo PASS)"
    record "no fontconfig errors" "$(grep -q 'Fontconfig error' $log_file && echo FAIL || echo PASS)"
else
    record "addon加载检查" "FAIL" "no log file"
fi
echo ""

# --- Test 6: 数据文件完整性 ---
echo "--- Test 6: 数据文件完整性 ---"
LM_REAL=/root/.local/pinyin-ime/share/libime/libime/zh_CN.lm
DICT_REAL=/root/.local/pinyin-ime/share/libime/sc.dict
record "zh_CN.lm 存在" "$([ -f $LM_REAL ] && echo PASS || echo FAIL)" "$(du -h $LM_REAL 2>/dev/null | cut -f1)"
record "sc.dict 存在" "$([ -f $DICT_REAL ] && echo PASS || echo FAIL)" "$(du -h $DICT_REAL 2>/dev/null | cut -f1)"
echo ""

# --- Summary ---
total=$((total_pass + total_fail))
echo "=========================================="
echo "  CI 结果: $total_pass/$total 通过"
if [ $total_fail -gt 0 ]; then
    echo "  ❌ $total_fail 个测试失败"
fi
echo "=========================================="

# Write JUnit XML
xml_file="$PROJECT_ROOT/test-results.xml"
cat > "$xml_file" << XML
<?xml version="1.0" encoding="UTF-8"?>
<testsuite name="pinyin-ime" tests="$total" failures="$total_fail" timestamp="$(date -Iseconds)">
$(echo -e "$test_results")
</testsuite>
XML
echo "JUnit XML: $xml_file"

[ $total_fail -eq 0 ] && exit 0 || exit 1
