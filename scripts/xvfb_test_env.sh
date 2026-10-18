#!/usr/bin/env bash
# 虚拟桌面（Xvfb）实测：启动/重启 fcitx5（XIM + pinyinime addon）。
#
# headless 沙箱没有显示服务器，本脚本用 Xvfb 虚拟显示 + 自编的 XIM 前端
# （tools/xim_test_client.c）真正把 Fcitx5 pinyinime addon 跑起来验证。
#
# 用法：
#   bash scripts/xvfb_test_env.sh start   # 启动 dbus + Xvfb + fcitx5
#   bash scripts/xvfb_test_env.sh stop    # 停止 fcitx5（保留 Xvfb/dbus）
#   bash scripts/xvfb_test_env.sh status
#
# 依赖环境变量：NB（native-build 根，默认 /root/workspace/native-build）
set -uo pipefail

NB="${NB:-/root/workspace/native-build}"
FCITX_BIN="$NB/opt/fcitx5-x11/bin/fcitx5"

# 显示号可外部传入（如 DISPLAY=:88 bash scripts/...），默认 :99。
# 每个 shell 命令运行在独立 PID namespace，跨命令无法 kill 旧实例；测试用
# 新显示号可绕开历史遗留进程，实现单命令闭环。日志/pid 文件按显示号隔离。
export DISPLAY="${DISPLAY:-:99}"
DSUF="${DISPLAY#:}"; DSUF="${DSUF%%.*}"
LOG="/tmp/fcitx5-x11-${DSUF}.log"
PIDF="/tmp/fcitx5-x11-${DSUF}.pid"
export XMODIFIERS=@im=fcitx
export LANG=zh_CN.utf8
export LC_ALL=zh_CN.utf8
export LD_LIBRARY_PATH="$NB/opt/fcitx5-x11/lib:$NB/opt/libime/lib:$NB/mamba-env/lib"
export FCITX_ADDON_DIRS="$NB/opt/fcitx5-x11/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$NB/opt/fcitx5-x11/share/fcitx5/addon"
export FCITX_DATA_DIR="$NB/opt/fcitx5-x11/share/fcitx5"

ensure_display() {
    # 判活依据：能否用 xdpyinfo 连上该显示号。既避免「Xvfb 被 SIGKILL 留下
    # 僵尸 socket」导致的假活，也支持多显示号并存（新显示号必然要新起 Xvfb）。
    if ! xdpyinfo -display "$DISPLAY" > /dev/null 2>&1; then
        rm -f "/tmp/.X11-unix/X${DSUF}" "/tmp/.X${DSUF}-lock" 2>/dev/null
        echo "[env] 启动 Xvfb $DISPLAY"
        setsid Xvfb "$DISPLAY" -screen 0 1280x800x24 -nolisten tcp > "/tmp/xvfb-${DSUF}.log" 2>&1 < /dev/null &
        sleep 2
    fi
    if ! ps -eo args | grep -q "[d]bus-daemon --syslog.*--session"; then
        echo "[env] 启动 dbus session"
        dbus-launch --sh-syntax 2>/dev/null > /tmp/dbus-env.sh || dbus-daemon --session --fork 2>/dev/null
    fi
    # 无论本脚本是否新起了 dbus，都从文件恢复会话地址：
    # start 是被 xvfb_ime_test.sh 以子进程调用的，export 无法传回父进程，
    # fcitx5 只有读到 DBUS_SESSION_BUS_ADDRESS 才能加载 dbus addon。
    [ -f /tmp/dbus-env.sh ] && . /tmp/dbus-env.sh
}

start_fcitx5() {
    ensure_display
    # 老进程（按 pid 文件）
    if [ -f "$PIDF" ]; then
        old=$(cat "$PIDF")
        kill "$old" 2>/dev/null && echo "[env] 停止旧 fcitx5 ($old)"
        sleep 1
    fi
    echo "[env] 启动 fcitx5（Xvfb $DISPLAY / XIM / pinyinime）"
    nohup "$FCITX_BIN" > "$LOG" 2>&1 &
    echo $! > "$PIDF"
    sleep 4
    echo "[env] fcitx5 PID $(cat "$PIDF")，日志尾部："
    tail -6 "$LOG"
}

stop_fcitx5() {
    if [ -f "$PIDF" ]; then
        kill "$(cat "$PIDF")" 2>/dev/null && echo "[env] fcitx5 已停止"
        rm -f "$PIDF"
    fi
}

case "${1:-start}" in
    start) start_fcitx5 ;;
    stop)  stop_fcitx5 ;;
    status)
        [ -f "$PIDF" ] && kill -0 "$(cat "$PIDF")" 2>/dev/null && echo "fcitx5 运行中 (PID $(cat "$PIDF"))" || echo "fcitx5 未运行"
        [ -S /tmp/.X11-unix/X99 ] && echo "Xvfb :99 运行中" || echo "Xvfb 未运行"
        ;;
    *) echo "用法: $0 {start|stop|status}"; exit 2 ;;
esac