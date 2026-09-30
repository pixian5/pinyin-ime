#!/bin/bash
# fcitx5 addon 实机验收脚本
# 在 Xvfb 虚拟显示下启动 fcitx5 server，验证 pinyinime addon 加载
set -uo pipefail

NB="${NB:-/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"

export PATH="$ENV/bin:$OPT/fcitx5-server/bin:$PATH"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5-server/lib:$OPT/libime/lib"
export XDG_DATA_DIRS="$OPT/fcitx5-server/share:$ENV/share:/usr/share"
export XDG_CONFIG_DIRS="$OPT/fcitx5-server/etc/xdg:/etc/xdg"
export XDG_RUNTIME_DIR="/tmp/runtime-root"
mkdir -p "$XDG_RUNTIME_DIR"

PASS=0; FAIL=0; SKIP=0
check() { if [ "$1" = "0" ]; then echo "  ✓ $2"; PASS=$((PASS+1)); else echo "  ✗ $2"; FAIL=$((FAIL+1)); fi; }

echo "=== fcitx5 addon 实机验收 ==="
echo ""

# 1. 检查二进制文件存在
echo "[1] 检查 addon 二进制"
test -f "$OPT/fcitx5/lib/fcitx5/libpinyinime.so"
check $? "libpinyinime.so 存在"

# 2. 检查 addon 导出符号
echo "[2] 检查 addon 导出符号"
nm -D "$OPT/fcitx5/lib/fcitx5/libpinyinime.so" 2>/dev/null | grep -q "fcitx_addon_factory"
check $? "fcitx_addon_factory 符号导出"

# 3. 检查 addon 配置文件
echo "[3] 检查 addon 配置"
test -f "$OPT/fcitx5/share/fcitx5/addon/pinyinime.conf"
check $? "addon pinyinime.conf 存在"
grep -q "\[Addon\]" "$OPT/fcitx5/share/fcitx5/addon/pinyinime.conf" 2>/dev/null
check $? "addon conf 含 [Addon] 段"
grep -q "Library=libpinyinime" "$OPT/fcitx5/share/fcitx5/addon/pinyinime.conf" 2>/dev/null
check $? "addon conf 指定 Library=libpinyinime"

# 4. 检查 libime 数据
echo "[4] 检查 libime 数据"
test -f "$OPT/libime/lib/libime/zh_CN.lm"
check $? "zh_CN.lm 存在"
test -f "$OPT/libime/share/libime/sc.dict"
check $? "sc.dict 存在"

# 5. 启动 Xvfb
echo "[5] 启动 Xvfb 虚拟显示"
pkill -x Xvfb 2>/dev/null; sleep 1
Xvfb :99 -screen 0 1024x768x24 &
sleep 1
export DISPLAY=:99
pgrep -x Xvfb >/dev/null 2>&1
check $? "Xvfb 运行中"

# 6. 启动 dbus
echo "[6] 启动 DBus session"
pkill -f "dbus-daemon.*session" 2>/dev/null; sleep 1
eval $(dbus-launch --sh-syntax)
test -n "$DBUS_SESSION_BUS_ADDRESS"
check $? "DBus session 已启动"

# 7. 确保 libime 数据可被 addon 找到
echo "[7] 链接 libime 数据到 /usr/share/libime"
mkdir -p /usr/share/libime
ln -sf "$OPT/libime/lib/libime/zh_CN.lm" /usr/share/libime/zh_CN.lm 2>/dev/null
ln -sf "$OPT/libime/share/libime/sc.dict" /usr/share/libime/sc.dict 2>/dev/null
test -f /usr/share/libime/zh_CN.lm && test -f /usr/share/libime/sc.dict
check $? "libime 数据已链接"

# 8. 安装 addon 到 fcitx5-server
echo "[8] 安装 addon 到 fcitx5-server"
cp -f "$OPT/fcitx5/lib/fcitx5/libpinyinime.so" "$OPT/fcitx5-server/lib/fcitx5/" 2>/dev/null
cp -f "$OPT/fcitx5/share/fcitx5/addon/pinyinime.conf" "$OPT/fcitx5-server/share/fcitx5/addon/" 2>/dev/null
mkdir -p "$OPT/fcitx5-server/share/fcitx5/inputmethod"
cat > "$OPT/fcitx5-server/share/fcitx5/inputmethod/pinyinime.conf" << 'EOF'
[InputMethod]
Name=Pinyin IME
Icon=fcitx-pinyin
Label=拼
LangCode=zh_CN
Addon=pinyinime
Configurable=True
EOF
test -f "$OPT/fcitx5-server/lib/fcitx5/libpinyinime.so"
check $? "addon 已安装到 fcitx5-server"

# 9. 启动 fcitx5 并验证 addon 加载
echo "[9] 启动 fcitx5 server"
pkill -f "fcitx5" 2>/dev/null; sleep 1

mkdir -p ~/.config/fcitx5
cat > ~/.config/fcitx5/profile << 'PROFILE'
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

# 用 timeout 启动 fcitx5，捕获日志
LOG=/tmp/fcitx5_addon_test.log
timeout 5 fcitx5 --replace > "$LOG" 2>&1
# fcitx5 会被 SIGTERM 结束（exit 124），这是正常的

# 10. 验证 addon 被加载
echo "[10] 验证 pinyinime addon 加载"
grep -q "Loaded addon pinyinime" "$LOG" 2>/dev/null
check $? "fcitx5 日志含 'Loaded addon pinyinime'"

# 11. 验证 LM 和 dict 加载
echo "[11] 验证语言模型和字典加载"
grep -q "pinyinime: loaded lm=" "$LOG" 2>/dev/null
check $? "fcitx5 日志含 LM 加载成功"

# 12. 验证输入法注册
echo "[12] 验证输入法注册"
grep -q "Found 1 input method(s) in addon pinyinime" "$LOG" 2>/dev/null
check $? "fcitx5 注册了 pinyinime 输入法"

# 13. 验证无致命错误
echo "[13] 验证无致命错误"
grep -qi "FATAL\|SEGFAULT\|core dump" "$LOG" 2>/dev/null
if [ $? -ne 0 ]; then
    echo "  ✓ 无致命错误"; PASS=$((PASS+1))
else
    echo "  ✗ 发现致命错误"; FAIL=$((FAIL+1))
fi

# 14. 验证 DBus 服务注册
echo "[14] 验证 DBus 服务注册"
grep -q "org.fcitx.Fcitx5" "$LOG" 2>/dev/null
check $? "fcitx5 DBus 服务名已注册"

# 清理
pkill -f "fcitx5" 2>/dev/null

echo ""
echo "=== 验收结果 ==="
echo "  通过: $PASS"
echo "  失败: $FAIL"
echo "  总计: $((PASS+FAIL))"

if [ "$FAIL" -eq 0 ]; then
    echo ""
    echo "✓ fcitx5 addon 实机验收全部通过"
    exit 0
else
    echo ""
    echo "✗ 有 $FAIL 项未通过"
    echo "日志: $LOG"
    exit 1
fi
