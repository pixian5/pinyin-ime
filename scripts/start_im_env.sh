#!/bin/bash
# pinyin-ime 桌面输入法环境启动脚本
# 启动 Xvfb + DBus + fcitx5，配置 pinyinime 为默认输入法
# 用法: bash scripts/start_im_env.sh [start|stop|status|test]

set -e

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
D="/root/.local/pinyin-ime"
MAMBA="/root/workspace/native-build/mamba-env"
PID_DIR="/tmp/pinyin-ime-pids"
mkdir -p "$PID_DIR"

# Environment setup
setup_env() {
    export DISPLAY=:99
    export XDG_RUNTIME_DIR=/run/user/0
    mkdir -p /run/user/0
    
    export XDG_DATA_DIRS="$D/opt/fcitx5-x11/share:$D/share:/usr/share"
    export XDG_CONFIG_DIRS="$D/opt/fcitx5-x11/etc/xdg:/etc/xdg"
    export LD_LIBRARY_PATH="/workspace/native-build/opt/fcitx5-x11/lib:$D/opt/fcitx5-x11/lib:$D/opt/deps-lib:$MAMBA/lib:/usr/lib64"
    export PATH="$D/opt/fcitx5-x11/bin:$PATH"
    export PYTHONPATH="/usr/lib64/python3.9/site-packages:${PYTHONPATH:-}"
    
    # Source DBus address if available
    if [ -f "$PID_DIR/dbus_addr" ]; then
        export DBUS_SESSION_BUS_ADDRESS=$(cat "$PID_DIR/dbus_addr")
    fi
}

start() {
    # Check if already running
    if [ -f "$PID_DIR/fcitx5.pid" ] && kill -0 $(cat "$PID_DIR/fcitx5.pid") 2>/dev/null; then
        echo "✅ fcitx5 already running (PID: $(cat "$PID_DIR/fcitx5.pid"))"
        return 0
    fi
    
    echo "=== Starting pinyin-ime desktop environment ==="
    
    # Kill any existing instances
    killall Xvfb 2>/dev/null || true
    killall dbus-daemon 2>/dev/null || true
    killall fcitx5 2>/dev/null || true
    sleep 1
    
    # Start Xvfb
    Xvfb :99 -screen 0 1280x1024x24 &
    echo $! > "$PID_DIR/xvfb.pid"
    sleep 2
    echo "✅ Xvfb started (DISPLAY=:99)"
    
    # Start DBus
    export LD_LIBRARY_PATH="$MAMBA/lib:/usr/lib64"
    dbus-daemon --session --print-address=1 > "$PID_DIR/dbus_addr" 2>/dev/null &
    echo $! > "$PID_DIR/dbus.pid"
    sleep 2
    
    DBUS_ADDR=$(cat "$PID_DIR/dbus_addr")
    export DBUS_SESSION_BUS_ADDRESS="$DBUS_ADDR"
    echo "✅ DBus started ($DBUS_ADDR)"
    
    # Setup full environment
    setup_env
    
    # Configure fcitx5 profile
    mkdir -p /root/.config/fcitx5
    cat > /root/.config/fcitx5/profile << 'PROFILE'
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
    
    # Start fcitx5
    $D/opt/fcitx5-x11/bin/fcitx5 -d --verbose '*=5' 2>/tmp/fcitx5_startup.log &
    echo $! > "$PID_DIR/fcitx5.pid"
    sleep 3
    
    # Verify
    if pgrep -x fcitx5 > /dev/null; then
        echo "✅ fcitx5 started (PID: $(cat "$PID_DIR/fcitx5.pid"))"
        grep -q "Loaded addon pinyinime" /tmp/fcitx5_startup.log && echo "✅ pinyinime addon loaded" || echo "⚠️ pinyinime addon not found in log"
        grep -q "Found 1 input method" /tmp/fcitx5_startup.log && echo "✅ pinyinime IM registered" || echo "⚠️ IM registration unclear"
    else
        echo "❌ fcitx5 failed to start"
        cat /tmp/fcitx5_startup.log | tail -20
        return 1
    fi
    
    echo ""
    echo "=== Environment ready ==="
    echo "DISPLAY=$DISPLAY"
    echo "DBUS_SESSION_BUS_ADDRESS=$DBUS_SESSION_BUS_ADDRESS"
    echo ""
    echo "Use 'bash scripts/start_im_env.sh test' to run tests"
    echo "Use 'pinyin-ime nihao' for CLI input"
}

stop() {
    echo "Stopping pinyin-ime environment..."
    killall fcitx5 2>/dev/null && echo "✅ fcitx5 stopped" || echo "fcitx5 not running"
    killall dbus-daemon 2>/dev/null && echo "✅ DBus stopped" || echo "DBus not running"
    killall Xvfb 2>/dev/null && echo "✅ Xvfb stopped" || echo "Xvfb not running"
    rm -f "$PID_DIR"/*.pid
}

status() {
    echo "=== pinyin-ime Environment Status ==="
    if [ -f "$PID_DIR/fcitx5.pid" ] && kill -0 $(cat "$PID_DIR/fcitx5.pid") 2>/dev/null; then
        echo "✅ fcitx5: running (PID: $(cat "$PID_DIR/fcitx5.pid"))"
    else
        echo "❌ fcitx5: not running"
    fi
    
    if [ -f "$PID_DIR/xvfb.pid" ] && kill -0 $(cat "$PID_DIR/xvfb.pid") 2>/dev/null; then
        echo "✅ Xvfb: running (PID: $(cat "$PID_DIR/xvfb.pid"))"
    else
        echo "❌ Xvfb: not running"
    fi
    
    if [ -f "$PID_DIR/dbus.pid" ] && kill -0 $(cat "$PID_DIR/dbus.pid") 2>/dev/null; then
        echo "✅ DBus: running (PID: $(cat "$PID_DIR/dbus.pid"))"
    else
        echo "❌ DBus: not running"
    fi
    
    # Check CLI
    if command -v pinyin-ime &>/dev/null; then
        result=$(pinyin-ime "nihao" 2>/dev/null)
        echo "✅ CLI: $result"
    else
        echo "❌ CLI: not found"
    fi
}

test_all() {
    setup_env
    cd "$REPO_DIR"
    source ./setup_env.sh 2>/dev/null
    export LD_LIBRARY_PATH="$MAMBA/lib:$LD_LIBRARY_PATH"
    
    echo "=== Running all tests ==="
    echo ""
    echo "--- CLI Smoke Test ---"
    bash scripts/smoke_test.sh 2>&1 | tail -5
    echo ""
    echo "--- E2E Input Test ---"
    python3 scripts/e2e_input_test.py 2>&1 | tail -15
    echo ""
    echo "--- Interaction Test ---"
    python3 scripts/interaction_test.py 2>&1 | grep -E "(Test|PASS|FAIL|Overall)"
    echo ""
    echo "--- Select Test ---"
    python3 scripts/select_test.py 2>&1 | grep -E "(Test|PASS|FAIL|Overall)"
    echo ""
    echo "--- X11 E2E Test ---"
    python3 scripts/x11_e2e_test.py 2>&1 | tail -5
    echo ""
    echo "--- XIM Test ---"
    python3 scripts/xim_test.py 2>&1 | tail -5
}

case "${1:-start}" in
    start)   start ;;
    stop)    stop ;;
    status)  status ;;
    test)    test_all ;;
    restart) stop; sleep 2; start ;;
    *)       echo "Usage: $0 {start|stop|status|test|restart}"; exit 1 ;;
esac
