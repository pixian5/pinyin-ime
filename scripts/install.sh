#!/bin/bash
# pinyin-ime 系统级安装脚本
# 将 pinyin-ime 输入法安装到系统级目录，使所有用户可用
#
# 安装内容：
#   1. pinyinime addon → /usr/lib64/fcitx5/
#   2. libIME 库 → /usr/lib64/
#   3. 数据文件 → /usr/share/libime/
#   4. fcitx5 配置 → /etc/xdg/fcitx5/
#   5. 环境变量 → /etc/profile.d/
#
# 用法:
#   bash scripts/install.sh          # 安装
#   bash scripts/install.sh uninstall # 卸载
#   bash scripts/install.sh check     # 检查安装状态

set -uo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
BUNDLE="${BUNDLE:-/root/.local/pinyin-ime/opt/fcitx5-x11}"
DEPSLIB="${DEPSLIB:-/root/.local/pinyin-ime/opt/deps-lib}"
LIBIME="/root/workspace/native-build/opt/libime/lib64"

# System directories
SYS_LIB="/usr/lib64"
SYS_Fcitx5_LIB="/usr/lib64/fcitx5"
SYS_Fcitx5_DATA="/usr/share/fcitx5"
SYS_LIBIME_DATA="/usr/share/libime"
SYS_PROFILE_D="/etc/profile.d"

log() { echo -e "\033[32m[install]\033[0m $*"; }
err() { echo -e "\033[31m[error]\033[0m $*" >&2; }

do_install() {
    log "安装 pinyin-ime 到系统目录..."
    echo ""

    # 1. Install addon
    log "Step 1: 安装 pinyinime addon..."
    mkdir -p "$SYS_Fcitx5_LIB"
    cp "$BUNDLE/lib/fcitx5/libpinyinime.so" "$SYS_Fcitx5_LIB/"
    mkdir -p "$SYS_Fcitx5_DATA/addon"
    cp "$BUNDLE/share/fcitx5/addon/pinyinime.conf" "$SYS_Fcitx5_DATA/addon/"
    echo "  → $SYS_Fcitx5_LIB/libpinyinime.so"
    echo "  → $SYS_Fcitx5_DATA/addon/pinyinime.conf"
    echo ""

    # 2. Install libIME libraries
    log "Step 2: 安装 libIME 库..."
    for lib in libIMECore.so.1.1.17 libIMEPinyin.so.1.1.17 libIMETable.so.1.1.17; do
        if [ -f "$LIBIME/$lib" ]; then
            cp "$LIBIME/$lib" "$SYS_LIB/"
            base=$(echo $lib | sed 's/\.1\.1\.17//')
            ln -sf "$SYS_LIB/$lib" "$SYS_LIB/${base}.0"
            ln -sf "$SYS_LIB/$lib" "$SYS_LIB/${base}"
            echo "  → $SYS_LIB/$lib"
        fi
    done
    # Also copy xcb-imdkit if available
    if [ -f "$DEPSLIB/lib64/libxcb-imdkit.so.1.0.9" ]; then
        cp "$DEPSLIB/lib64/libxcb-imdkit.so.1.0.9" "$SYS_LIB/"
        ln -sf "$SYS_LIB/libxcb-imdkit.so.1.0.9" "$SYS_LIB/libxcb-imdkit.so.1"
        ln -sf "$SYS_LIB/libxcb-imdkit.so.1.0.9" "$SYS_LIB/libxcb-imdkit.so"
        echo "  → $SYS_LIB/libxcb-imdkit.so.1.0.9"
    fi
    ldconfig
    echo ""

    # 3. Install data files
    log "Step 3: 安装数据文件..."
    mkdir -p "$SYS_LIBIME_DATA/libime"
    if [ -f /root/.local/pinyin-ime/share/libime/libime/zh_CN.lm ]; then
        cp /root/.local/pinyin-ime/share/libime/libime/zh_CN.lm "$SYS_LIBIME_DATA/libime/"
        echo "  → $SYS_LIBIME_DATA/libime/zh_CN.lm"
    fi
    if [ -f /root/.local/pinyin-ime/share/libime/sc.dict ]; then
        cp /root/.local/pinyin-ime/share/libime/sc.dict "$SYS_LIBIME_DATA/"
        cp /root/.local/pinyin-ime/share/libime/sc.dict "$SYS_LIBIME_DATA/libime/"
        echo "  → $SYS_LIBIME_DATA/sc.dict"
    fi
    echo ""

    # 4. Configure fcitx5 profile
    log "Step 4: 配置 fcitx5..."
    mkdir -p /root/.config/fcitx5/conf
    cat > /root/.config/fcitx5/profile << 'PROF'
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
PROF
    echo "  → /root/.config/fcitx5/profile"
    echo ""

    # 5. Environment variables
    log "Step 5: 配置环境变量..."
    cat > "$SYS_PROFILE_D/pinyin-ime.sh" << 'ENVEOF'
# pinyin-ime environment
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
ENVEOF
    echo "  → $SYS_PROFILE_D/pinyin-ime.sh"
    echo ""

    # 6. Verify
    log "Step 6: 验证安装..."
    local ok=1
    [ -f "$SYS_Fcitx5_LIB/libpinyinime.so" ] && echo "  ✅ addon" || { echo "  ❌ addon"; ok=0; }
    [ -f "$SYS_LIB/libIMEPinyin.so.1.1.17" ] && echo "  ✅ libIME" || { echo "  ❌ libIME"; ok=0; }
    [ -f "$SYS_LIBIME_DATA/libime/zh_CN.lm" ] && echo "  ✅ 数据文件" || { echo "  ❌ 数据文件"; ok=0; }
    [ -f "$SYS_PROFILE_D/pinyin-ime.sh" ] && echo "  ✅ 环境变量" || { echo "  ❌ 环境变量"; ok=0; }
    echo ""

    if [ $ok -eq 1 ]; then
        log "安装完成！请重新登录或 source $SYS_PROFILE_D/pinyin-ime.sh"
        log "然后运行 fcitx5 -d 启动输入法"
    else
        err "安装不完整，请检查上述错误"
        return 1
    fi
}

do_uninstall() {
    log "卸载 pinyin-ime..."
    rm -f "$SYS_Fcitx5_LIB/libpinyinime.so"
    rm -f "$SYS_Fcitx5_DATA/addon/pinyinime.conf"
    rm -f "$SYS_LIB"/libIME*.so*
    rm -f "$SYS_LIB"/libxcb-imdkit.so*
    rm -rf "$SYS_LIBIME_DATA"
    rm -f "$SYS_PROFILE_D/pinyin-ime.sh"
    ldconfig
    log "卸载完成"
}

do_check() {
    echo "=== pinyin-ime 安装状态 ==="
    echo ""
    echo "addon:     $([ -f "$SYS_Fcitx5_LIB/libpinyinime.so" ] && echo '✅ 已安装' || echo '❌ 未安装')"
    echo "libIME:    $([ -f "$SYS_LIB/libIMEPinyin.so.1.1.17" ] && echo '✅ 已安装' || echo '❌ 未安装')"
    echo "xcb-imdkit: $([ -f "$SYS_LIB/libxcb-imdkit.so.1" ] && echo '✅ 已安装' || echo '❌ 未安装')"
    echo "数据文件:  $([ -f "$SYS_LIBIME_DATA/libime/zh_CN.lm" ] && echo '✅ 已安装' || echo '❌ 未安装')"
    echo "环境变量:  $([ -f "$SYS_PROFILE_D/pinyin-ime.sh" ] && echo '✅ 已配置' || echo '❌ 未配置')"
    echo ""
    echo "fcitx5:    $(pgrep -x fcitx5 > /dev/null && echo '✅ 运行中' || echo '❌ 未运行')"
}

case "${1:-install}" in
    install)   do_install ;;
    uninstall) do_uninstall ;;
    check)     do_check ;;
    *)         echo "用法: $0 {install|uninstall|check}"; exit 1 ;;
esac
