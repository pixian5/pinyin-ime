#!/bin/bash
# Start IM environment: Xvfb + dbus + fcitx5
BUNDLE=/root/.local/pinyin-ime
FCITX5=$BUNDLE/opt/fcitx5-x11

# Kill existing
pkill -f fcitx5 2>/dev/null
pkill -f "Xvfb :99" 2>/dev/null
pkill -f dbus-daemon 2>/dev/null
sleep 2

# Start Xvfb
nohup Xvfb :99 -screen 0 1280x1024x24 > /tmp/xvfb.log 2>&1 &
sleep 2

# Start dbus
eval $(dbus-launch --sh-syntax)
echo "$DBUS_SESSION_BUS_ADDRESS" > /tmp/dbus_addr
echo "$DBUS_SESSION_BUS_PID" > /tmp/dbus_pid
sleep 1

# Start fcitx5
export DISPLAY=:99
export LD_LIBRARY_PATH=$FCITX5/lib:$BUNDLE/opt/deps-lib:/usr/lib64
export XDG_DATA_DIRS=$FCITX5/share:/usr/share
export XDG_CONFIG_DIRS=$FCITX5/etc/xdg:/etc/xdg
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx

nohup $FCITX5/bin/fcitx5 -d --replace > /tmp/fcitx5.log 2>&1 &
sleep 4

# Verify
echo "=== Status ==="
pgrep -a Xvfb && echo "Xvfb: OK" || echo "Xvfb: FAIL"
pgrep -a fcitx5 && echo "fcitx5: OK" || echo "fcitx5: FAIL"
echo "DISPLAY=:99"
echo "DBUS=$(cat /tmp/dbus_addr)"
