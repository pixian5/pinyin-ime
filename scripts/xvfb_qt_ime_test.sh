#!/bin/bash
# pinyin-ime Qt 端到端实测：QLineEdit 应用经 fcitx5-qt platforminputcontext 链路打字。
#
#   链路：Qt5 应用 --QT_IM_MODULE=fcitx--> libfcitx5platforminputcontextplugin.so
#         --DBus--> fcitx5 守护进程 --pinyinime addon--> 引擎解码上屏
#
# 与 scripts/xvfb_desktop_ime_test.sh（GTK3 链路）平行：这是桌面 Qt 应用（如 WPS、QQ）打字用的那条路。
# 覆盖：全拼 / 简拼 / 整句 / M3 纠错 / 中英切换 / 中文标点 / 先上屏再标点。
#
# 用法：NB=/workspace/native-build DISPLAY=:88 bash scripts/xvfb_qt_ime_test.sh
#
# 环境前提：
#   - Xvfb 与独立 DBus 会话由本脚本自建（显示号可用 DISPLAY 指定，默认 :88；新显示号可绕开历史遗留实例）
#   - Qt5 运行时：NB/qt-qpa（QPA 插件 libqxcb + 同源 Qt 库覆盖层）、NB/mamba-env（ICU75/xcb 依赖）
#   - fcitx5 bundle：BUNDLE（默认 NB/opt/fcitx5-x11），含 fcitx5 与 fcitx5-qt 插件
#   - tools/qt_ime_test_client（按 scripts/build_fcitx5_qt.sh 同款 conda Qt 环境编译）；
#     tools/keysend_target 缺失时自动编译（需系统 X11/XTest 头）。
set -uo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

NB="${NB:-/workspace/native-build}"
BUNDLE="${BUNDLE:-$NB/opt/fcitx5-x11}"
QTQPA="${QTQPA:-$NB/qt-qpa}"
FCITX_BIN="$BUNDLE/bin/fcitx5"

export XMODIFIERS=@im=fcitx LANG=zh_CN.utf8 LC_ALL=zh_CN.utf8
export DISPLAY="${DISPLAY:-:88}"
DSUF="${DISPLAY#:}"; DSUF="${DSUF%%.*}"
# libxkbcommon 数据目录（sandbox 内 conda 库编译期路径不可用时兜底）
[ -d /usr/share/X11/xkb ] && export XKB_CONFIG_ROOT="${XKB_CONFIG_ROOT:-/usr/share/X11/xkb}"

# ---- 客户端运行时环境 ----
# qt-qpa 覆盖层优先（含 Xvfb 可用 QPA 插件 libqxcb 与同源 Qt 库）；
# mamba-env 提供 ICU75/glib/xcb 等 Qt 依赖；bundle 提供 fcitx5-qt 插件依赖。
export QT_IM_MODULE=fcitx
export QT_QPA_PLATFORM_PLUGIN_PATH="$QTQPA/plugins/platforms"
export QT_PLUGIN_PATH="$BUNDLE/lib/qt5/plugins"
export QT_QPA_PLATFORMINPUTCONTEXT_PATH="$BUNDLE/lib/qt5/plugins/platforminputcontexts"
CLIENT_LD="$QTQPA/lib:$BUNDLE/lib:$NB/mamba-env/lib:$NB/opt/libime/lib"

# ---- 前置检查 ----
pre_fail=0
[ -x "$FCITX_BIN" ] || { echo "!! 缺 fcitx5: $FCITX_BIN"; pre_fail=1; }
[ -f "$QT_QPA_PLATFORM_PLUGIN_PATH/libqxcb.so" ] || { echo "!! 缺 QPA 插件: $QT_QPA_PLATFORM_PLUGIN_PATH/libqxcb.so"; pre_fail=1; }
[ -f "$QT_QPA_PLATFORMINPUTCONTEXT_PATH/libfcitx5platforminputcontextplugin.so" ] || { echo "!! 缺 fcitx5 Qt 插件"; pre_fail=1; }
[ -x tools/qt_ime_test_client ] || { echo "!! 缺 tools/qt_ime_test_client（见文件头编译说明）"; pre_fail=1; }
[ "$pre_fail" = 0 ] || exit 1
[ -x tools/keysend_target ] || \
    gcc -O2 -o tools/keysend_target tools/keysend_target.c -lX11 -lXtst

# ---- 用户配置（数据路径 + 隔离用户目录 + 开纠错）----
mkdir -p ~/.config/fcitx5/conf
cat > ~/.config/fcitx5/conf/pinyinime.conf <<EOF
DataDir=$NB/opt/libime/lib
UserDataDir=/tmp/pinyin-ime-qt-ud
EnableCorrect=True
EOF

# ---- 环境：Xvfb + 独立 DBus 会话 + fcitx5（DBus）----
if ! xdpyinfo -display "$DISPLAY" > /dev/null 2>&1; then
    rm -f "/tmp/.X11-unix/X${DSUF}" "/tmp/.X${DSUF}-lock"
    setsid Xvfb "$DISPLAY" -screen 0 1280x800x24 -nolisten tcp >"/tmp/xvfb-qt-${DSUF}.log" 2>&1 < /dev/null &
    sleep 2
fi
DBUSINFO="/tmp/qt-dbus-info-${DSUF}"
dbus-daemon --session --fork --print-address=1 --print-pid=1 > "$DBUSINFO" 2>/dev/null
export DBUS_SESSION_BUS_ADDRESS="$(grep -m1 '^unix:' "$DBUSINFO")"

pkill -9 -x fcitx5 2>/dev/null
sleep 1
rm -rf /tmp/pinyin-ime-qt-ud
LD_LIBRARY_PATH="$BUNDLE/lib:$NB/opt/libime/lib" nohup "$FCITX_BIN" > "/tmp/qt-fcitx5-${DSUF}.log" 2>&1 &
sleep 5
if ! grep -q "Loaded addon dbusfrontend" "/tmp/qt-fcitx5-${DSUF}.log"; then
    echo "!! fcitx5 未加载 dbusfrontend，退出"; tail -8 "/tmp/qt-fcitx5-${DSUF}.log"; exit 1
fi

pass=0; fail=0
# 跑一个用例：启动客户端 -> 取窗口 -> 注入按键 -> 断言 COMMIT 输出
run_case() {
    local desc="$1" script="$2" want="$3" out wid
    : > "/tmp/qt-client-${DSUF}.log"
    LD_LIBRARY_PATH="$CLIENT_LD" ./tools/qt_ime_test_client > "/tmp/qt-client-${DSUF}.log" 2>&1 &
    local qpid=$!
    local i
    for i in $(seq 1 60); do grep -q WINDOW_ID "/tmp/qt-client-${DSUF}.log" && break; sleep 0.25; done
    wid=$(grep -oE 'WINDOW_ID=0x[0-9a-f]+' "/tmp/qt-client-${DSUF}.log" | cut -d= -f2)
    ./tools/keysend_target "$wid" "$script" > /dev/null 2>&1
    sleep 2
    out=$(grep '^COMMIT:' "/tmp/qt-client-${DSUF}.log" | sed 's/^COMMIT://' | tr -d '\n' | tr ' ' '_')
    kill "$qpid" 2>/dev/null
    if grep '^COMMIT:' "/tmp/qt-client-${DSUF}.log" | grep -qF "$want"; then
        echo "  PASS  $desc  -> $(grep '^COMMIT:' "/tmp/qt-client-${DSUF}.log" | tr '\n' ' ')"
        pass=$((pass + 1))
    else
        echo "  FAIL  $desc  -> [$out] (期望含 $want)"
        fail=$((fail + 1))
    fi
}

echo "== pinyinime addon · Qt5（DBus + fcitx5-qt platforminputcontext）端到端实测 =="
run_case "全拼 nihao"       '\Snihao\s'      "你好"
run_case "全拼 zhongguo"    '\Szhongguo\s'   "中国"
run_case "简拼 zg"          '\Szg\s'         "中国"
run_case "简拼 bjdx"        '\Sbjdx\s'       "北京大学"
run_case "整句 woshiyige"   '\Swoshiyige\s'  "我是一个"
run_case "M3 纠错 senme"    '\Ssenme\s'      "什么"
run_case "M3 纠错 zongguo"  '\Szongguo\s'    "中国"
run_case "中英切换 Tab"     '\S\thello\n'    "hello"
run_case "中文标点 ,"       '\S,'            "，"
run_case "先上屏再标点"     '\Szhongguo!'    "中国"

echo "==== 结果：$pass 通过 / $fail 失败 ===="
[ "$fail" -eq 0 ]
