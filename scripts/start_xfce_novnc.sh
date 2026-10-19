#!/usr/bin/env bash
# 启动 XFCE 桌面 + fcitx5 + noVNC，通过浏览器远程访问
set -euo pipefail

NB=/root/workspace/native-build
OPT="$NB/opt"
ENV="$NB/mamba-env"
DISPLAY_NUM=${1:-1}
VNC_PORT=${2:-5901}
WEB_PORT=${3:-6080}

# 杀旧进程
pkill -f "Xvfb :${DISPLAY_NUM}" 2>/dev/null || true
pkill -f "Xorg.*:${DISPLAY_NUM}" 2>/dev/null || true
pkill -f "xfce4-session" 2>/dev/null || true
pkill -f "fcitx5" 2>/dev/null || true
pkill -f "websockify" 2>/dev/null || true
sleep 1

export DISPLAY=:${DISPLAY_NUM}
export DBUS_SESSION_BUS_ADDRESS="unix:path=/tmp/dbus-session-bus-${DISPLAY_NUM}"

# dbus
if [ ! -S /tmp/dbus-session-bus-${DISPLAY_NUM} ]; then
    dbus-daemon --session --address="$DBUS_SESSION_BUS_ADDRESS" --print-address=1 > /dev/null 2>&1 &
    sleep 1
fi

# Xvfb (大分辨率)
Xvfb :${DISPLAY_NUM} -screen 0 1920x1080x24 +extension GLX +extension RANDR +extension COMPOSITE -nolisten tcp &
sleep 2

# fcitx5 环境变量
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export QT_QPA_PLATFORMINPUTCONTEXT_PATH="$OPT/fcitx5-x11/lib/qt5/plugins/platforminputcontexts"
export XIM_PROGRAM=fcitx5
export PATH="$OPT/fcitx5-x11/bin:$ENV/bin:$PATH"
export LD_LIBRARY_PATH="$OPT/fcitx5-x11/lib:$OPT/libime/lib:$ENV/lib:${LD_LIBRARY_PATH:-}"
export FCITX_CONFIG_FILE="$OPT/fcitx5-x11/etc/fcitx5/profile"

# fcitx5
fcitx5 -d --replace 2>/dev/null &
sleep 2

# XFCE 桌面
xfce4-session 2>/dev/null &
sleep 3

# x11vnc
x11vnc -display :${DISPLAY_NUM} -rfbport ${VNC_PORT} -nopw -forever -shared -noxdamage 2>/dev/null &
sleep 1

# noVNC web
websockify --web /usr/share/novnc ${WEB_PORT} localhost:${VNC_PORT} 2>/dev/null &
sleep 1

PUBLIC_IP=$(head -1 /root/workspace/pinyin-ime/ip.txt 2>/dev/null || echo "localhost")
echo "========================================"
echo "  XFCE 桌面已启动"
echo "  noVNC: http://${PUBLIC_IP}:${WEB_PORT}/vnc.html"
echo "  VNC:   ${PUBLIC_IP}:${VNC_PORT}"
echo "  Display: :${DISPLAY_NUM}"
echo "  fcitx5 + pinyinime 已加载"
echo "========================================"
