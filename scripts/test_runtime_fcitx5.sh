#!/bin/bash
# =============================================================================
# fcitx5 运行时冒烟测试
#
# 目的：验证 addon 在真实 fcitx5 进程里能被加载、注册输入法、读取语言模型，
#       而不是只停留在「编译通过」。没有 fcitx5/Xvfb 的环境自动跳过（SKIP）。
#
# 做法：私有 Xvfb 显示 + 私有 HOME + dbus-run-session 启动 fcitx5，
#       从 fcitx5 日志与（可选）D-Bus 控制器断言四件事：
#         1. addon pinyinime 被加载
#         2. 打印 loaded lm=... dict=...（说明数据文件路径正确）
#         3. 输入法列表里出现 pinyinime
#         4. （有 python3-dbus 时）控制器能列出 pinyinime 并切换输入法组
#
# 依赖：fcitx5、Xvfb、dbus-run-session；python3-dbus 可选。
# 用法：bash scripts/test_runtime_fcitx5.sh [-v]
#   退出码：0 = 通过或跳过；1 = 断言失败。
# =============================================================================
set -uo pipefail

VERBOSE=0
[ "${1:-}" = "-v" ] && VERBOSE=1

PASS=0
FAIL=0
SKIP=0
ok()   { PASS=$((PASS + 1)); echo "  [PASS] $*"; }
bad()  { FAIL=$((FAIL + 1)); echo "  [FAIL] $*"; }
skip() { SKIP=$((SKIP + 1));  echo "  [SKIP] $*"; }

command -v fcitx5 >/dev/null 2>&1 || { echo "[SKIP] 未安装 fcitx5，跳过运行时测试"; exit 0; }
command -v Xvfb >/dev/null 2>&1 || { echo "[SKIP] 未安装 Xvfb，跳过运行时测试"; exit 0; }
command -v dbus-run-session >/dev/null 2>&1 || { echo "[SKIP] 未安装 dbus-run-session，跳过运行时测试"; exit 0; }

# addon 是否已安装到 fcitx5 能找到的位置（deb 路径 / 源码 build 路径）
ADDON_SO=""
for d in /usr/lib/x86_64-linux-gnu/fcitx5 /usr/lib64/fcitx5 /usr/lib/fcitx5; do
    [ -f "$d/libpinyinime.so" ] && ADDON_SO="$d/libpinyinime.so" && break
done
if [ -z "$ADDON_SO" ]; then
    echo "[SKIP] 未安装 libpinyinime.so，跳过运行时测试（先装 deb/runtime 包）"
    exit 0
fi
echo "[info] addon: $ADDON_SO"

WORK="$(mktemp -d /tmp/pinyin-runtime.XXXXXX)" || exit 1
HOME_DIR="$WORK/home"
mkdir -p "$HOME_DIR/.config/fcitx5"
cat > "$HOME_DIR/.config/fcitx5/profile" <<'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=pinyinime

[Groups/0/Items/0]
Name=pinyinime
Layout=

[GroupOrder]
0=Default
EOF

# 选一个没被占用的显示号
DISPLAY_NUM=""
mkdir -p /tmp/.X11-unix 2>/dev/null || true
for n in $(seq 90 110); do
    if [ ! -e "/tmp/.X11-unix/X$n" ]; then
        DISPLAY_NUM=$n
        break
    fi
done
if [ -z "$DISPLAY_NUM" ]; then
    echo "[FAIL] 找不到空闲的 X 显示号"
    rm -rf "$WORK"
    exit 1
fi

export DISPLAY=":$DISPLAY_NUM"
export HOME="$HOME_DIR"
export XDG_CONFIG_HOME="$HOME_DIR/.config"
export LC_ALL=C.UTF-8
LOG="$WORK/fcitx5.log"

XVFB_PID=""
cleanup() {
    [ -n "$XVFB_PID" ] && kill "$XVFB_PID" 2>/dev/null
    sleep 0.5
    rm -rf "$WORK"
}
trap cleanup EXIT

Xvfb ":$DISPLAY_NUM" -screen 0 800x600x24 >"$WORK/xvfb.log" 2>&1 &
XVFB_PID=$!
for _ in $(seq 1 10); do [ -e "/tmp/.X11-unix/X$DISPLAY_NUM" ] && break; sleep 0.3; done

# D-Bus 断言脚本（有 python3-dbus 才启用）
CHECK_PY=""
if command -v python3 >/dev/null 2>&1 && python3 -c "import dbus" >/dev/null 2>&1; then
    CHECK_PY="$WORK/check.py"
    cat > "$CHECK_PY" <<'PY'
import dbus, sys
try:
    bus = dbus.SessionBus()
    iface = dbus.Interface(bus.get_object("org.fcitx.Fcitx5", "/controller"),
                           "org.fcitx.Fcitx.Controller1")
    names = [str(s[0]) for s in iface.AvailableInputMethods()]
    if "pinyinime" not in names:
        print("FAIL: pinyinime 不在 AvailableInputMethods")
        sys.exit(1)
    items = dbus.Array([dbus.Struct(("pinyinime", "us"), signature="ss")], signature="(ss)")
    iface.SetInputMethodGroupInfo("Default", "us", items)
    iface.SwitchInputMethodGroup("Default")
    print("OK: D-Bus 可见 pinyinime，输入法组切换成功")
except Exception as e:
    print("FAIL: D-Bus 验证异常: %r" % (e,))
    sys.exit(1)
PY
fi

# 真实上屏参数：默认仍是 nihao→你好；可用环境变量做多短语验收。
TYPE_INPUT="${PINYINIME_TEST_INPUT:-nihao}"
TYPE_EXPECT="${PINYINIME_TEST_EXPECT:-你好}"
# 可选：用数字键选择其他候选（默认空格选首选）。
TYPE_SELECT_KEY="${PINYINIME_TEST_SELECT_KEY:-space}"

# 真实上屏测试脚本（GTK3 + fcitx5 GTK im module，有 xdotool 才启用）
TYPE_PY=""
if command -v xdotool >/dev/null 2>&1 && command -v python3 >/dev/null 2>&1 \
   && python3 -c "import gi; gi.require_version('Gtk','3.0'); from gi.repository import Gtk" >/dev/null 2>&1; then
    TYPE_PY="$WORK/type_test.py"
    cat > "$TYPE_PY" <<'PY'
import gi, sys
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib
win = Gtk.Window(title="pinyin-ime-test")
entry = Gtk.Entry()
win.add(entry)
win.show_all()
win.present()
entry.grab_focus()
print("XID=%d" % win.get_window().get_xid()); sys.stdout.flush()
def dump():
    print("TEXT=[%s]" % entry.get_text()); sys.stdout.flush()
    Gtk.main_quit(); return False
GLib.timeout_add(25000, dump)
Gtk.main()
PY
    echo "enabled" > "$WORK/type.enabled"
fi

# 在同一个 dbus 会话里启动 fcitx5、等待加载、做 D-Bus 断言，然后退出
INNER="$WORK/inner.sh"
cat > "$INNER" <<EOF
#!/bin/bash
export GTK_IM_MODULE=fcitx XMODIFIERS=@im=fcitx
fcitx5 -D --verbose default=4 >"$LOG" 2>&1 &
FPID=\$!
for _ in \$(seq 1 40); do
    grep -q "Loaded addon pinyinime" "$LOG" 2>/dev/null && break
    sleep 0.5
done
CHECK_PY="$CHECK_PY"
if [ -n "\$CHECK_PY" ]; then
    python3 "\$CHECK_PY" >"$WORK/check.out" 2>&1
    echo \$? >"$WORK/check.rc"
else
    echo 255 >"$WORK/check.rc"
fi
TYPE_PY="$TYPE_PY"
if [ -n "\$TYPE_PY" ] && [ "\$(cat "$WORK/check.rc" 2>/dev/null)" = "0" ]; then
    python3 "\$TYPE_PY" >"$WORK/type.out" 2>"$WORK/type.err" &
    TPID=\$!
    sleep 4
    xdotool search --name pinyin-ime-test windowfocus 2>/dev/null || true
    sleep 1
    xdotool type --delay 120 "$TYPE_INPUT"
    sleep 1
    xdotool key "$TYPE_SELECT_KEY"
    for _ in \$(seq 1 30); do
        grep -q "TEXT=" "$WORK/type.out" 2>/dev/null && break
        sleep 1
    done
    wait \$TPID 2>/dev/null
fi
kill \$FPID 2>/dev/null
wait \$FPID 2>/dev/null
exit 0
EOF
chmod +x "$INNER"

dbus-run-session -- bash "$INNER" >/dev/null 2>&1

[ "$VERBOSE" = 1 ] && { echo "----- fcitx5 log -----"; cat "$LOG"; echo "----------------------"; }

grep -q "Loaded addon pinyinime" "$LOG" \
    && ok "addon pinyinime 被 fcitx5 加载" \
    || bad "addon pinyinime 未被加载（见日志 $LOG）"
grep -q "pinyinime: loaded lm=" "$LOG" \
    && ok "addon 读取到语言模型/词典" \
    || bad "addon 未打印 loaded lm=（数据路径可能不对）"
grep -q "Found 1 input method(s) in addon pinyinime" "$LOG" \
    && ok "pinyinime 注册为输入法" \
    || bad "pinyinime 未注册为输入法"
if grep -qE "Failed to create addon.*pinyinime|Could not load addon pinyinime" "$LOG"; then
    bad "addon 加载日志中出现错误"
else
    ok "addon 加载无错误"
fi

if [ -n "$CHECK_PY" ]; then
    rc="$(cat "$WORK/check.rc" 2>/dev/null || echo 1)"
    if [ "$rc" = "0" ]; then
        ok "$(cat "$WORK/check.out" 2>/dev/null | tail -1)"
    else
        bad "D-Bus 验证失败：$(tail -1 "$WORK/check.out" 2>/dev/null)"
    fi
else
    skip "未安装 python3-dbus，跳过 D-Bus 控制器断言"
fi

if [ -f "$WORK/type.enabled" ]; then
    if grep -Fq "TEXT=[$TYPE_EXPECT]" "$WORK/type.out" 2>/dev/null; then
        ok "真实按键上屏：$TYPE_INPUT + $TYPE_SELECT_KEY → $TYPE_EXPECT（GTK3 + fcitx5 im module）"
    else
        bad "真实按键上屏未得到「$TYPE_EXPECT」：$(tr '\n' ' ' < "$WORK/type.out" 2>/dev/null | tail -c 200)"
    fi
else
    skip "缺 xdotool 或 python3-gi/GTK3，跳过真实按键上屏"
fi

echo ""
echo "运行时冒烟：$PASS 通过，$FAIL 失败，$SKIP 跳过"
[ "$FAIL" -eq 0 ] || exit 1
