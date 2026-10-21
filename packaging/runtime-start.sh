#!/bin/bash
# pinyin-ime 便携运行时启动器：不写系统目录，配置/用户数据都放在 bundle 内。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CONFIG_HOME="${PINYIN_IME_CONFIG_HOME:-$ROOT/config}"
CACHE_HOME="${PINYIN_IME_CACHE_HOME:-$ROOT/cache}"
RUNTIME_DIR="${XDG_RUNTIME_DIR:-/run/user/$(id -u)}"
FCITX_BIN="$ROOT/opt/fcitx5-x11/bin/fcitx5"
LOG_DIR="$ROOT/logs"

mkdir -p "$CONFIG_HOME/fcitx5/conf" "$CACHE_HOME" "$ROOT/userdata" "$RUNTIME_DIR" "$LOG_DIR"

# 只在不存在时生成默认配置，避免覆盖用户已有配置。
if [ ! -f "$CONFIG_HOME/fcitx5/profile" ]; then
    cat > "$CONFIG_HOME/fcitx5/profile" <<'EOF'
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
fi
if [ ! -f "$CONFIG_HOME/fcitx5/conf/pinyinime.conf" ]; then
    cat > "$CONFIG_HOME/fcitx5/conf/pinyinime.conf" <<EOF
NBest=45
PageSize=9
DataDir=$ROOT/share/libime/libime
UserDataDir=$ROOT/userdata
EnableFuzzy=True
EnableCorrect=True
EOF
fi
if [ ! -f "$ROOT/fonts.conf" ]; then
    cat > "$ROOT/fonts.conf" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <dir>$ROOT/share/fonts</dir>
  <cachedir>$CACHE_HOME/fonts</cachedir>
</fontconfig>
EOF
fi

export LD_LIBRARY_PATH="$ROOT/opt/deps-lib:$ROOT/opt/fcitx5-x11/lib:$ROOT/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PATH="$ROOT/opt/fcitx5-x11/bin:$PATH"
export XDG_CONFIG_HOME="$CONFIG_HOME"
export XDG_CACHE_HOME="$CACHE_HOME"
export XDG_DATA_DIRS="$ROOT/share:$ROOT/opt/fcitx5-x11/share:${XDG_DATA_DIRS:-/usr/share}"
export XDG_RUNTIME_DIR="$RUNTIME_DIR"
# addon 必须放进 fcitx5-x11 自身的 addon/data 树：只指向 bundle 私有目录会导致
# classicui/dbus 等官方 addon 找不到而全部加载失败。
export FCITX_ADDON_DIRS="$ROOT/opt/fcitx5-x11/lib/fcitx5"
export FCITX_DATA_DIR="$ROOT/opt/fcitx5-x11/share/fcitx5"
export FONTCONFIG_FILE="$ROOT/fonts.conf"
export FONTCONFIG_PATH="$ROOT"
export XMODIFIERS=@im=fcitx
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx

# 只停掉属于本 bundle 的 fcitx5（按 /proc/PID/exe 匹配），不碰系统输入法。
# kill 时优先用 NSpid：部分沙箱容器里 /proc 显示的是宿主 PID，直接 kill 无效。
stop_bundle_instances() {
    local p inner exe
    if [ -f "$ROOT/fcitx5.pid" ]; then
        kill "$(cat "$ROOT/fcitx5.pid")" 2>/dev/null || true
        rm -f "$ROOT/fcitx5.pid"
    fi
    command -v pgrep >/dev/null 2>&1 || return 0
    for p in $(pgrep -x fcitx5 2>/dev/null || true); do
        exe="$(readlink "/proc/$p/exe" 2>/dev/null || true)"
        case "$exe" in
            "$ROOT"/*)
                inner="$(awk '/^NSpid:/{print $NF}' "/proc/$p/status" 2>/dev/null || true)"
                kill "${inner:-$p}" 2>/dev/null || kill "$p" 2>/dev/null || true
                ;;
        esac
    done
}

if [ -x "$FCITX_BIN" ]; then
    stop_bundle_instances
    sleep 1
fi

if [ ! -x "$FCITX_BIN" ]; then
    echo "找不到 fcitx5：$FCITX_BIN" >&2
    exit 1
fi
if [ -z "${DISPLAY:-}" ] && [ -z "${WAYLAND_DISPLAY:-}" ]; then
    echo "未检测到 DISPLAY/WAYLAND_DISPLAY；桌面环境下请先进入图形会话。" >&2
    echo "无桌面演示可先启动 Xvfb 并 export DISPLAY=:99。" >&2
    exit 1
fi

# 前台模式(-D) + 重定向：拿得到真实 PID 和日志，stop.sh 才能可靠停止并验证加载。
"$FCITX_BIN" -D >"$LOG_DIR/fcitx5.log" 2>&1 &
echo $! >"$ROOT/fcitx5.pid"

ready=0
for _ in $(seq 1 80); do
    if grep -q 'Loaded addon pinyinime' "$LOG_DIR/fcitx5.log" 2>/dev/null &&
       grep -q 'loaded lm=' "$LOG_DIR/fcitx5.log" 2>/dev/null; then
        ready=1
        break
    fi
    kill -0 "$(cat "$ROOT/fcitx5.pid")" 2>/dev/null || break
    sleep 0.25
done
if [ "$ready" != 1 ]; then
    echo "启动失败：pinyinime addon / LM 未加载，日志尾部：" >&2
    tail -40 "$LOG_DIR/fcitx5.log" >&2 || true
    exit 1
fi

if command -v timeout >/dev/null 2>&1; then
    timeout 10 fcitx5-remote -o >/dev/null 2>&1 || true
else
    fcitx5-remote -o >/dev/null 2>&1 || true
fi

cat <<EOF
pinyin-ime 运行时已启动
  ROOT:       $ROOT
  配置:       $CONFIG_HOME
  数据:       $ROOT/share/libime/libime
  CLI:        $ROOT/bin/pinyin_ime_cli
  日志:       $LOG_DIR/fcitx5.log
  Ctrl+Space: 切换中英文
停止:         $ROOT/scripts/stop.sh
EOF
grep -m1 'loaded lm=' "$LOG_DIR/fcitx5.log" | sed 's/^/  /' || true
