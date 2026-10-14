#!/bin/bash
BUNDLE=/root/.local/pinyin-ime
FCITX5=$BUNDLE/opt/fcitx5-x11
export LD_LIBRARY_PATH=$FCITX5/lib:$BUNDLE/opt/deps-lib:/usr/lib64
export XDG_DATA_DIRS=$FCITX5/share:/usr/share
export XDG_CONFIG_DIRS=$FCITX5/etc/xdg:/etc/xdg

# Kill existing
pkill -f "Xvfb :99" 2>/dev/null
pkill -f fcitx5 2>/dev/null
pkill -f dbus-daemon 2>/dev/null
sleep 1

# Start Xvfb
Xvfb :99 -screen 0 1280x1024x24 &
sleep 2
export DISPLAY=:99

# Start dbus
eval $(dbus-launch --sh-syntax)
echo "DBUS=$DBUS_SESSION_BUS_ADDRESS"
sleep 1

# Start fcitx5
$FCITX5/bin/fcitx5 -d --replace 2>/tmp/fcitx5_err.log &
sleep 3

# Check
echo "=== Process check ==="
pgrep -a fcitx5 || echo "fcitx5 NOT running"
echo "=== fcitx5 error log ==="
cat /tmp/fcitx5_err.log 2>/dev/null | head -30
