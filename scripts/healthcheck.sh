#!/bin/bash
# pinyin-ime 健康检查脚本
# 检查输入法所有组件是否正常工作
# 用法: bash scripts/healthcheck.sh

set -uo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NB="${NB:-/root/workspace/native-build}"
BUNDLE="${BUNDLE:-/root/.local/pinyin-ime/opt/fcitx5-x11}"
DEPSLIB="${DEPSLIB:-/root/.local/pinyin-ime/opt/deps-lib}"
LIBIME="$NB/opt/libime/lib64"
ENV_DIR="$NB/mamba-env"
S3_DIR="$PROJECT_ROOT/pinyin-ime-s3"

pass=0
fail=0
warn=0

check() {
    local name="$1"
    local result="$2"
    local detail="${3:-}"
    if [ "$result" = "1" ]; then
        echo "  ✅ $name ${detail:+($detail)}"
        pass=$((pass + 1))
    else
        echo "  ❌ $name ${detail:+($detail)}"
        fail=$((fail + 1))
    fi
}

warn_check() {
    local name="$1"
    local result="$2"
    local detail="${3:-}"
    if [ "$result" = "1" ]; then
        echo "  ✅ $name ${detail:+($detail)}"
        pass=$((pass + 1))
    else
        echo "  ⚠️  $name ${detail:+($detail)}"
        warn=$((warn + 1))
    fi
}

echo "=========================================="
echo "  pinyin-ime 健康检查"
echo "=========================================="
echo ""

echo "--- 1. 服务进程 ---"
check "Xvfb" "$(pgrep -x Xvfb > /dev/null && echo 1 || echo 0)"
check "dbus-daemon" "$(pgrep -x dbus-daemon > /dev/null && echo 1 || echo 0)"
check "fcitx5" "$(pgrep -x fcitx5 > /dev/null && echo 1 || echo 0)"
echo ""

echo "--- 2. 核心库文件 ---"
check "libIMECore.so" "$(ls $LIBIME/libIMECore.so 2>/dev/null > /dev/null && echo 1 || echo 0)"
check "libIMEPinyin.so" "$(ls $LIBIME/libIMEPinyin.so 2>/dev/null > /dev/null && echo 1 || echo 0)"
check "libIMETable.so" "$(ls $LIBIME/libIMETable.so 2>/dev/null > /dev/null && echo 1 || echo 0)"
check "libpinyinime.so (addon)" "$(ls $BUNDLE/lib/fcitx5/libpinyinime.so 2>/dev/null > /dev/null && echo 1 || echo 0)"
check "libxcb-imdkit.so (XIM)" "$(ls $DEPSLIB/lib64/libxcb-imdkit.so.1 2>/dev/null > /dev/null && echo 1 || echo 0)"
echo ""

echo "--- 3. 数据文件 ---"
LM=/root/.local/pinyin-ime/share/libime/libime/zh_CN.lm
DICT=/root/.local/pinyin-ime/share/libime/sc.dict
check "zh_CN.lm (语言模型)" "$(ls $LM 2>/dev/null > /dev/null && echo 1 || echo 0)" "$(du -h $LM 2>/dev/null | cut -f1)"
check "sc.dict (词典)" "$(ls $DICT 2>/dev/null > /dev/null && echo 1 || echo 0)" "$(du -h $DICT 2>/dev/null | cut -f1)"
echo ""

echo "--- 4. 路径符号链接 ---"
check "/workspace/native-build/mamba-env" "$([ -L /workspace/native-build/mamba-env ] && echo 1 || echo 0)"
check "xkeyboard-config-2" "$(ls $ENV_DIR/share/xkeyboard-config-2/rules/evdev 2>/dev/null > /dev/null && echo 1 || echo 0)"
check "fontconfig config" "$(ls $ENV_DIR/etc/fonts/fonts.conf 2>/dev/null > /dev/null && echo 1 || echo 0)"
echo ""

echo "--- 5. fcitx5 配置 ---"
check "profile (DefaultIM=pinyinime)" "$(grep -q pinyinime /root/.config/fcitx5/profile 2>/dev/null && echo 1 || echo 0)"
check "pinyinime.conf" "$(ls /root/.config/fcitx5/conf/pinyinime.conf 2>/dev/null > /dev/null && echo 1 || echo 0)"
echo ""

echo "--- 6. CLI 冒烟测试 ---"
CLI="$S3_DIR/pinyin_ime_cli_v0.9.83"
S3_LM="$S3_DIR/zh_CN.lm"
S3_DICT="$S3_DIR/sc.dict"
if [ -x "$CLI" ]; then
    export LD_LIBRARY_PATH="$LIBIME:$DEPSLIB:$BUNDLE/lib:$ENV_DIR/lib"
    for input in "zhongguo:中国" "nihao:你好" "shijie:世界" "pinyin:拼音" "shurufa:输入法"; do
        py=$(echo $input | cut -d: -f1)
        expected=$(echo $input | cut -d: -f2)
        result=$(echo "$py" | $CLI "$S3_LM" "$S3_DICT" 2>/dev/null | head -1 | cut -f1)
        check "CLI: $py → $expected" "$([ "$result" = "$expected" ] && echo 1 || echo 0)" "got: $result"
    done
else
    echo "  ⚠️  CLI not found at $CLI"
    warn=$((warn + 1))
fi
echo ""

echo "--- 7. DBus 端到端测试 ---"
export PATH="$BUNDLE/bin:$ENV_DIR/bin:${PATH:-}"
export LD_LIBRARY_PATH="$LIBIME:$DEPSLIB:$DEPSLIB/lib64:$BUNDLE/lib:$ENV_DIR/lib"
export DISPLAY=:99
export FCITX_ADDON_DIRS="$BUNDLE/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$BUNDLE/share/fcitx5/addon"
export FCITX_DATA_DIR="$BUNDLE/share/fcitx5"
if [ -f /tmp/dbus_addr ]; then
    export DBUS_SESSION_BUS_ADDRESS="$(cat /tmp/dbus_addr 2>/dev/null || true)"
fi

if [ -f "$PROJECT_ROOT/scripts/e2e_input_test.py" ] && pgrep -x fcitx5 > /dev/null; then
    e2e_result=$(python3.9 "$PROJECT_ROOT/scripts/e2e_input_test.py" 2>&1)
    e2e_pass=$(echo "$e2e_result" | grep "Tests passed:" | grep -oP '\d+(?=/)')
    e2e_total=$(echo "$e2e_result" | grep "Tests passed:" | grep -oP '/\K\d+')
    check "e2e_input_test" "$([ "$e2e_pass" = "$e2e_total" ] && echo 1 || echo 0)" "$e2e_pass/$e2e_total"
else
    warn_check "e2e_input_test" "0" "fcitx5 not running or test script missing"
fi

if [ -f "$PROJECT_ROOT/scripts/interaction_test.py" ] && pgrep -x fcitx5 > /dev/null; then
    int_result=$(python3.9 "$PROJECT_ROOT/scripts/interaction_test.py" 2>&1)
    if echo "$int_result" | grep -q "ALL PASS"; then
        check "interaction_test" "1"
    else
        check "interaction_test" "0"
    fi
else
    warn_check "interaction_test" "0" "fcitx5 not running or test script missing"
fi

if [ -f "$PROJECT_ROOT/scripts/select_test.py" ] && pgrep -x fcitx5 > /dev/null; then
    sel_result=$(python3.9 "$PROJECT_ROOT/scripts/select_test.py" 2>&1)
    if echo "$sel_result" | grep -q "ALL PASS"; then
        check "select_test" "1"
    else
        check "select_test" "0"
    fi
else
    warn_check "select_test" "0" "fcitx5 not running or test script missing"
fi
echo ""

echo "--- 8. fcitx5 addon 加载状态 ---"
if pgrep -x fcitx5 > /dev/null; then
    # Check fcitx5 log for addon loading
    log_file=/tmp/fcitx5.log
    if [ -f "$log_file" ]; then
        check "pinyinime loaded" "$(grep -q 'Loaded addon pinyinime' $log_file && echo 1 || echo 0)"
        check "xim loaded" "$(grep -q 'Loaded addon xim' $log_file && echo 1 || echo 0)"
        check "dbusfrontend loaded" "$(grep -q 'Loaded addon dbusfrontend' $log_file && echo 1 || echo 0)"
        check "classicui loaded" "$(grep -q 'Loaded addon classicui' $log_file && echo 1 || echo 0)"
        warn_check "no XKB errors" "$(grep -q 'xkbcommon: ERROR' $log_file && echo 0 || echo 1)"
        warn_check "no fontconfig errors" "$(grep -q 'Fontconfig error' $log_file && echo 0 || echo 1)"
    else
        echo "  ⚠️  No fcitx5 log file found"
        warn=$((warn + 1))
    fi
else
    echo "  ⚠️  fcitx5 not running, skipping addon checks"
    warn=$((warn + 1))
fi
echo ""

echo "=========================================="
echo "  总结: ✅ $pass 通过  ❌ $fail 失败  ⚠️  $warn 警告"
echo "=========================================="

[ $fail -eq 0 ] && exit 0 || exit 1
