#!/bin/bash
# pinyin-ime 输入法启动脚本
# 用法: bash run_im.sh [start|stop|test|status]

BUNDLE=/root/.local/pinyin-ime/opt/fcitx5-x11
DEPSLIB=/root/.local/pinyin-ime/opt/deps-lib
LIBIME=/root/workspace/native-build/opt/libime/lib64
ENV_DIR=/root/workspace/native-build/mamba-env

export PATH="$BUNDLE/bin:$ENV_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$LIBIME:$DEPSLIB:$BUNDLE/lib:$ENV_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XMODIFIERS=@im=fcitx
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export FCITX_ADDON_DIRS="$BUNDLE/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$BUNDLE/share/fcitx5/addon"
export FCITX_DATA_DIR="$BUNDLE/share/fcitx5"
export GTK_IM_MODULE_FILE="$BUNDLE/lib/gtk-3.0/3.0.0/immodules.cache"
export QT_QPA_PLATFORMINPUTCONTEXT_PATH="$BUNDLE/lib/qt5/plugins/platforminputcontexts"
export XDG_DATA_DIRS="$BUNDLE/share:$XDG_DATA_DIRS"

case "${1:-start}" in
    start)
        # Start Xvfb if not running
        if ! pgrep -x Xvfb > /dev/null; then
            Xvfb :99 -screen 0 1280x1024x24 > /dev/null 2>&1 &
            sleep 1
            echo "Xvfb started on :99"
        fi
        export DISPLAY=:99
        
        # Start dbus if not running
        if ! pgrep -x dbus-daemon > /dev/null; then
            eval $(dbus-launch --sh-syntax)
            echo "dbus started: $DBUS_SESSION_BUS_ADDRESS"
        fi
        
        # Start fcitx5
        if ! pgrep -x fcitx5 > /dev/null; then
            setsid fcitx5 -d --display :99 > /dev/null 2>&1 &
            sleep 2
            echo "fcitx5 started"
        fi
        ;;
    stop)
        pkill -x fcitx5 2>/dev/null
        pkill -x Xvfb 2>/dev/null
        pkill -x dbus-daemon 2>/dev/null
        echo "All stopped"
        ;;
    status)
        echo "Xvfb: $(pgrep -x Xvfb > /dev/null && echo 'running' || echo 'stopped')"
        echo "dbus: $(pgrep -x dbus-daemon > /dev/null && echo 'running' || echo 'stopped')"
        echo "fcitx5: $(pgrep -x fcitx5 > /dev/null && echo 'running' || echo 'stopped')"
        ;;
    test)
        export DISPLAY=:99
        # Test fcitx5-remote
        fcitx5-remote 2>&1
        ;;
esac
