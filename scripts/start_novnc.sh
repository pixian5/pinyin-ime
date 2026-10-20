#!/bin/bash
# noVNC 可视化演示启动脚本
# 用法: bash scripts/start_novnc.sh
NB="${NB:-/root/workspace/native-build}"
OPT="$NB/opt"
export DISPLAY=:99
export DBUS_SESSION_BUS_ADDRESS="$(cat /tmp/dbus_addr 2>/dev/null)"
export XMODIFIERS=@im=fcitx GTK_IM_MODULE=fcitx QT_IM_MODULE=fcitx
export LANG=zh_CN.utf8 LC_ALL=zh_CN.utf8
export LD_LIBRARY_PATH="$OPT/fcitx5-x11/lib:$NB/mamba-env/lib:$OPT/libime/lib:$OPT/xcb-imdkit/lib64"
export PATH="$OPT/fcitx5-x11/bin:$NB/mamba-env/bin:$PATH"

# VNC password
mkdir -p /root/.vnc
echo "pinyin123" | vncpasswd -f > /root/.vnc/passwd
chmod 600 /root/.vnc/passwd

# Start VNC server
pkill -x x0vncserver 2>/dev/null; sleep 1
x0vncserver -Display :99 -PasswordFile /root/.vnc/passwd -rfbport 5901 -Geometry 1280x1024 &
sleep 2

# Start websockify proxy
pkill -f "websockify 6080" 2>/dev/null; sleep 1
python3.11 -m websockify 6080 localhost:5901 &
sleep 2

# Start HTTP server for noVNC client
pkill -f "http.server 6081" 2>/dev/null; sleep 1
python3.11 -m http.server 6081 --directory /opt/novnc &
sleep 2

IP=$(head -1 ip.txt 2>/dev/null || echo localhost)
echo "============================================"
echo "  pinyin-ime noVNC 演示已启动"
echo "============================================"
echo "  noVNC Web:  http://${IP}:6081/vnc.html"
echo "  VNC 直连:   ${IP}:5901 (密码: pinyin123)"
echo "============================================"
echo "  在浏览器中打开 noVNC URL，"
echo "  按 Ctrl+Space 切换中英文输入"
echo "============================================"
