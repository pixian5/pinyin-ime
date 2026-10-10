#!/usr/bin/env bash
# fcitx5 桌面集成验证脚本
# 验证：Xvfb 虚拟显示 + DBus 会话 + fcitx5 服务 + pinyinime addon 加载 + 数据就绪
#
# 用法：bash scripts/desktop_integration_test.sh
# 环境：需已构建 fcitx5-x11（X11+DBus+Server）+ pinyinime addon
set -uo pipefail
NB="${NB:-/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"
SYSROOT=/workspace/sysroot
PASS=0; FAIL=0

check() {
    if eval "$2"; then
        echo "  ✅ $1"
        PASS=$((PASS+1))
    else
        echo "  ❌ $1"
        FAIL=$((FAIL+1))
    fi
}

echo "=== fcitx5 桌面集成验证 ==="
echo ""

# ---------- 1. 环境检查 ----------
echo "--- 环境检查 ---"
check "Xvfb 可用" "test -f '$SYSROOT/usr/bin/Xvfb'"
check "dbus-daemon 可用" "test -f '$SYSROOT/usr/bin/dbus-daemon'"
check "xkbcomp 可用" "test -f '$SYSROOT/usr/bin/xkbcomp'"
check "xkb_hook.so 可用" "test -f /tmp/opencode/xkb_hook.so"
check "fcitx5 二进制" "test -x '$OPT/fcitx5-x11/bin/fcitx5'"
check "Fcitx5Core 库" "test -f '$OPT/fcitx5-x11/lib/libFcitx5Core.so'"
check "pinyinime addon" "test -f '$OPT/fcitx5-x11/lib/fcitx5/libpinyinime.so'"
check "zh_CN.lm 语言模型" "test -f '$OPT/libime/lib/libime/zh_CN.lm'"
check "sc.dict 词典" "test -f '$OPT/libime/lib/libime/sc.dict'"

# ---------- 2. 启动 Xvfb ----------
echo ""
echo "--- 启动 Xvfb 虚拟显示 ---"
export LD_LIBRARY_PATH="$SYSROOT/usr/lib64:$SYSROOT/usr/lib"
export LD_PRELOAD="/tmp/opencode/xkb_hook.so"
pkill -9 Xvfb 2>/dev/null; sleep 1
rm -f /tmp/.X11-unix/X99 /tmp/.X99-lock 2>/dev/null
setsid "$SYSROOT/usr/bin/Xvfb" :99 -screen 0 1280x800x24 -nolisten tcp >/tmp/xvfb.log 2>&1 < /dev/null &
sleep 2
check "Xvfb 运行在 :99" "pgrep -x Xvfb > /dev/null 2>&1"

# ---------- 3. 启动 DBus ----------
echo ""
echo "--- 启动 DBus 会话 ---"
pkill -9 dbus-daemon 2>/dev/null; sleep 1
export LD_LIBRARY_PATH="$SYSROOT/usr/lib64:$SYSROOT/usr/lib:$ENV/lib:/usr/lib64"
mkdir -p /tmp/opencode/dbus
cat > /tmp/opencode/dbus/session.conf << 'DBUSCONF'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-BUS Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:path=/tmp/opencode/dbus/session_bus_socket</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*" eavesdrop="true"/>
    <allow eavesdrop="true"/>
    <allow own="*"/>
    <allow user="*"/>
  </policy>
</busconfig>
DBUSCONF
rm -f /tmp/opencode/dbus/session_bus_socket
"$SYSROOT/usr/bin/dbus-daemon" --config-file=/tmp/opencode/dbus/session.conf \
    --fork --print-address=1 > /tmp/dbus-info 2>/dev/null
sleep 1
DBUS_ADDR="$(grep -m1 '^unix:' /tmp/dbus-info)"
check "DBus 会话运行" "test -S '/tmp/opencode/dbus/session_bus_socket'"

# ---------- 4. 启动 fcitx5 ----------
echo ""
echo "--- 启动 fcitx5 服务 ---"
pkill -9 fcitx5 2>/dev/null; sleep 1
export LD_LIBRARY_PATH="$OPT/fcitx5-x11/lib:$OPT/libime/lib:$ENV/lib:$SYSROOT/usr/lib64:$SYSROOT/usr/lib"
export LD_PRELOAD="/tmp/opencode/xkb_hook.so"
export DISPLAY=:99 XMODIFIERS=@im=fcitx LANG=zh_CN.utf8 LC_ALL=zh_CN.utf8
export FCITX_ADDON_DIRS="$OPT/fcitx5-x11/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$OPT/fcitx5-x11/share/fcitx5/addon"
export FCITX_DATA_DIR="$OPT/fcitx5-x11/share/fcitx5"
export DBUS_SESSION_BUS_ADDRESS="unix:path=/tmp/opencode/dbus/session_bus_socket"
export XKB_BINDIR="$SYSROOT/usr/bin"
export HOME=/workspace
rm -rf /tmp/pinyin-ime-test-ud
nohup "$OPT/fcitx5-x11/bin/fcitx5" > /tmp/fcitx5.log 2>&1 &
sleep 5
check "fcitx5 服务运行" "pgrep -x fcitx5 > /dev/null 2>&1"

# ---------- 5. 验证 addon 加载 ----------
echo ""
echo "--- 验证 addon 加载 ---"
check "pinyinime addon 已加载" "grep -q 'Loaded addon pinyinime' /tmp/fcitx5.log"
check "语言模型已加载" "grep -q 'loaded lm=' /tmp/fcitx5.log"
check "词典已加载" "grep -q 'dict=' /tmp/fcitx5.log"
check "XIM 前端已加载" "grep -q 'Loaded addon xim' /tmp/fcitx5.log"
check "DBus 前端已加载" "grep -q 'Loaded addon dbusfrontend' /tmp/fcitx5.log"
check "Classic UI 已加载" "grep -q 'Loaded addon classicui' /tmp/fcitx5.log"
check "XCB 模块已加载" "grep -q 'Loaded addon xcb' /tmp/fcitx5.log"
check "Keyboard 已加载" "grep -q 'Loaded addon keyboard' /tmp/fcitx5.log"
check "DBus 服务已注册" "grep -q 'org.fcitx.Fcitx5' /tmp/fcitx5.log"

# ---------- 6. 统计 ----------
echo ""
echo "=== 验证结果 ==="
echo "  通过: $PASS"
echo "  失败: $FAIL"
echo "  总计: $((PASS+FAIL))"
if [ "$FAIL" -eq 0 ]; then
    echo "  🎉 全部通过！fcitx5 桌面集成验证成功"
else
    echo "  ⚠️ 有 $FAIL 项未通过"
fi

# 清理
pkill -9 fcitx5 2>/dev/null
pkill -9 Xvfb 2>/dev/null
pkill -9 dbus-daemon 2>/dev/null
unset LD_PRELOAD
