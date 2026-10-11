#!/bin/bash
# pinyin-ime 输入法启动脚本 (v0.9.84)
# 用法: bash run_im.sh [start|stop|test|status]
#
# 修复项：
# - DataDir: addon源码默认路径已修正，无需workaround
# - XIM: 编译安装libxcb-imdkit.so.1支持XIM协议
# - XKB: 符号链接xkeyboard-config到系统/usr/share/X11/xkb
# - fontconfig: 创建fontconfig配置文件指向系统字体

BUNDLE=/root/.local/pinyin-ime/opt/fcitx5-x11
DEPSLIB=/root/.local/pinyin-ime/opt/deps-lib
LIBIME=/root/workspace/native-build/opt/libime/lib64
ENV_DIR=/root/workspace/native-build/mamba-env

export PATH="$BUNDLE/bin:$ENV_DIR/bin:$PATH"
export LD_LIBRARY_PATH="$LIBIME:$DEPSLIB:$DEPSLIB/lib64:$BUNDLE/lib:$ENV_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export XMODIFIERS=@im=fcitx
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export FCITX_ADDON_DIRS="$BUNDLE/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$BUNDLE/share/fcitx5/addon"
export FCITX_DATA_DIR="$BUNDLE/share/fcitx5"
export GTK_IM_MODULE_FILE="$BUNDLE/lib/gtk-3.0/3.0.0/immodules.cache"
export QT_QPA_PLATFORMINPUTCONTEXT_PATH="$BUNDLE/lib/qt5/plugins/platforminputcontexts"
export XDG_DATA_DIRS="$BUNDLE/share:${XDG_DATA_DIRS:-}"

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
        
        # Show loaded addons
        echo "--- addon status ---"
        fcitx5-remote -e 2>/dev/null | grep -E "pinyinime|xim|keyboard|dbusfrontend" || true
        ;;
    stop)
        pkill -x fcitx5 2>/dev/null
        pkill -x Xvfb 2>/dev/null
        pkill -x dbus-daemon 2>/dev/null
        echo "All stopped"
        ;;
    status)
        echo "=== 服务状态 ==="
        echo "Xvfb:   $(pgrep -x Xvfb > /dev/null && echo '✅ running' || echo '❌ stopped')"
        echo "dbus:   $(pgrep -x dbus-daemon > /dev/null && echo '✅ running' || echo '❌ stopped')"
        echo "fcitx5: $(pgrep -x fcitx5 > /dev/null && echo '✅ running' || echo '❌ stopped')"
        echo ""
        echo "=== addon 状态 ==="
        echo "pinyinime: $(ls $BUNDLE/lib/fcitx5/libpinyinime.so 2>/dev/null > /dev/null && echo '✅ installed' || echo '❌ missing')"
        echo "xim:       $(ls $DEPSLIB/lib64/libxcb-imdkit.so.1 2>/dev/null > /dev/null && echo '✅ supported' || echo '❌ missing libxcb-imdkit')"
        echo "xkb:       $(ls /workspace/native-build/mamba-env/share/xkeyboard-config-2/rules/evdev 2>/dev/null > /dev/null && echo '✅ configured' || echo '❌ missing')"
        echo "fontconfig: $(ls /workspace/native-build/mamba-env/etc/fonts/fonts.conf 2>/dev/null > /dev/null && echo '✅ configured' || echo '❌ missing')"
        echo ""
        echo "=== 数据文件 ==="
        echo "LM:   $(ls /root/.local/pinyin-ime/share/libime/libime/zh_CN.lm 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
        echo "Dict: $(ls /root/.local/pinyin-ime/share/libime/sc.dict 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
        ;;
    test)
        export DISPLAY=:99
        echo "=== CLI smoke test ==="
        CLI=/root/workspace/pinyin-ime/pinyin-ime-s3/pinyin_ime_cli_v0.9.83
        LM=/root/.local/pinyin-ime/share/libime/libime/zh_CN.lm
        DICT=/root/.local/pinyin-ime/share/libime/sc.dict
        for word in zhongguo nihao shurufa shijie hello pinyin; do
            result=$(echo "$word" | $CLI "$LM" "$DICT" 2>/dev/null | head -1 | cut -f1)
            echo "  $word → $result"
        done
        echo ""
        echo "=== fcitx5-remote ==="
        fcitx5-remote 2>&1
        ;;
esac
