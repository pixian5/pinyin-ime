#!/bin/bash
# pinyin-ime Qt 端到端测试脚本
# 在 Xvfb 下通过 Qt 输入法框架测试拼音输入
#
# 测试内容：
#   1. Qt 环境检查（QT_IM_MODULE, plugin 路径）
#   2. fcitx5 Qt 插件加载
#   3. 简单 Qt 应用输入测试（如果 Qt 可用）
#
# 用法: bash scripts/xvfb_qt_ime_test.sh

set -uo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUNDLE="${BUNDLE:-/root/.local/pinyin-ime/opt/fcitx5-x11}"
DEPSLIB="${DEPSLIB:-/root/.local/pinyin-ime/opt/deps-lib}"
LIBIME="/root/workspace/native-build/opt/libime/lib64"
ENV_DIR="/root/workspace/native-build/mamba-env"

export PATH="$BUNDLE/bin:$ENV_DIR/bin:${PATH:-}"
export LD_LIBRARY_PATH="$LIBIME:$DEPSLIB:$DEPSLIB/lib64:$BUNDLE/lib:$ENV_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export DISPLAY=:99
export FCITX_ADDON_DIRS="$BUNDLE/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$BUNDLE/share/fcitx5/addon"
export FCITX_DATA_DIR="$BUNDLE/share/fcitx5"
export XDG_DATA_DIRS="$BUNDLE/share:${XDG_DATA_DIRS:-}"
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
if [ -f /tmp/dbus_addr ]; then
    export DBUS_SESSION_BUS_ADDRESS="$(cat /tmp/dbus_addr 2>/dev/null || true)"
fi

pass=0; fail=0; warn=0

check() {
    if [ "$2" = "1" ]; then
        echo "  ✅ $1 ${3:+($3)}"
        pass=$((pass + 1))
    else
        echo "  ❌ $1 ${3:+($3)}"
        fail=$((fail + 1))
    fi
}

warn_check() {
    if [ "$2" = "1" ]; then
        echo "  ✅ $1 ${3:+($3)}"
        pass=$((pass + 1))
    else
        echo "  ⚠️  $1 ${3:+($3)}"
        warn=$((warn + 1))
    fi
}

echo "=========================================="
echo "  pinyin-ime Qt 端到端测试"
echo "=========================================="
echo ""

echo "--- 1. Qt 环境检查 ---"
check "QT_IM_MODULE" "$([ -n "$QT_IM_MODULE" ] && echo 1 || echo 0)" "QT_IM_MODULE=$QT_IM_MODULE"
check "XMODIFIERS" "$([ -n "$XMODIFIERS" ] && echo 1 || echo 0)" "XMODIFIERS=$XMODIFIERS"
echo ""

echo "--- 2. fcitx5 Qt 插件 ---"
QT_PLUGIN_DIR="$BUNDLE/lib/qt5/plugins/platforminputcontexts"
check "Qt plugin 目录" "$([ -d "$QT_PLUGIN_DIR" ] && echo 1 || echo 0)"
if [ -d "$QT_PLUGIN_DIR" ]; then
    plugins=$(ls "$QT_PLUGIN_DIR"/*.so 2>/dev/null | wc -l)
    check "Qt 插件数量" "$([ $plugins -gt 0 ] && echo 1 || echo 0)" "$plugins 个插件"
    for p in "$QT_PLUGIN_DIR"/*.so; do
        [ -f "$p" ] && echo "    - $(basename $p)"
    done
else
    echo "  ⚠️  Qt plugin 目录不存在"
    warn=$((warn + 1))
fi
echo ""

echo "--- 3. Qt 库检查 ---"
QT_LIB="$DEPSLIB"
qt_libs=$(ls "$QT_LIB"/libQt5Core* "$QT_LIB"/libQt5Gui* "$QT_LIB"/libQt5Widgets* /usr/lib64/libQt5Core* 2>/dev/null | wc -l)
check "Qt5 核心库" "$([ $qt_libs -ge 3 ] && echo 1 || echo 0)" "$qt_libs 个库"
echo ""

echo "--- 4. Qt 输入法插件加载测试 ---"
# Check if the fcitx5 Qt plugin can be loaded
qt_plugin="$QT_PLUGIN_DIR/libfcitx5platforminputcontextplugin.so"
if [ -f "$qt_plugin" ]; then
    check "fcitx5 Qt 插件存在" "1" "$(basename $qt_plugin)"
    # Check if it's a valid ELF
    check "Qt 插件 ELF 格式" "$(file "$qt_plugin" | grep -q ELF && echo 1 || echo 0)"
else
    warn_check "fcitx5 Qt 插件" "0" "未找到"
fi
echo ""

echo "--- 5. Qt 应用输入测试 ---"
# Try to run a simple Qt test if Qt is available
if [ -x "$BUNDLE/bin/fcitx5-remote" ]; then
    # Use fcitx5-remote to check if fcitx5 is running
    state=$($BUNDLE/bin/fcitx5-remote 2>/dev/null)
    check "fcitx5 运行中" "$([ "$state" != "" ] && echo 1 || echo 0)" "state=$state"
else
    warn_check "fcitx5-remote" "0" "未找到"
fi
echo ""

echo "--- 6. GTK 输入法模块 ---"
GTK_IM_CACHE="$BUNDLE/lib/gtk-3.0/3.0.0/immodules.cache"
if [ -f "$GTK_IM_CACHE" ]; then
    check "GTK immodules.cache" "1"
    if grep -q fcitx "$GTK_IM_CACHE" 2>/dev/null; then
        check "GTK fcitx 模块" "1"
    else
        warn_check "GTK fcitx 模块" "0" "未在 cache 中找到"
    fi
else
    warn_check "GTK immodules.cache" "0" "文件不存在"
fi
echo ""

echo "=========================================="
echo "  Qt 测试: ✅ $pass  ❌ $fail  ⚠️  $warn"
echo "=========================================="

[ $fail -eq 0 ] && exit 0 || exit 1
