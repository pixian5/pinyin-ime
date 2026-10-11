#!/bin/bash
# pinyin-ime 一键设置与启动脚本 (v0.9.84)
# 在当前机器上从 S3 预构建产物 + 本地编译 libime + xcb-imdkit 搭建完整可运行的输入法
#
# 用法:
#   bash scripts/setup_and_run.sh setup   # 首次设置
#   bash scripts/setup_and_run.sh start   # 启动 fcitx5
#   bash scripts/setup_and_run.sh stop    # 停止
#   bash scripts/setup_and_run.sh test    # 运行测试
#   bash scripts/setup_and_run.sh status  # 查看状态
#
# 2026-10-11 v0.9.84: 修复 DataDir+XIM+XKB+fontconfig+/workspace符号链接

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NB="${NB:-/root/workspace/native-build}"
BUNDLE="${BUNDLE:-/root/.local/pinyin-ime/opt/fcitx5-x11}"
DEPSLIB="${DEPSLIB:-/root/.local/pinyin-ime/opt/deps-lib}"
LIBIME="$NB/opt/libime/lib64"
ENV_DIR="$NB/mamba-env"
S3_DIR="$PROJECT_ROOT/pinyin-ime-s3"
VERSION="0.9.83"

log() { echo -e "\033[32m[pinyin-ime]\033[0m $*"; }

export_env() {
    export PATH="$BUNDLE/bin:$ENV_DIR/bin:${PATH:-}"
    export LD_LIBRARY_PATH="$LIBIME:$DEPSLIB:$DEPSLIB/lib64:$BUNDLE/lib:$ENV_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export FCITX_ADDON_DIRS="$BUNDLE/lib/fcitx5"
    export FCITX_ADDON_CONFIG_DIRS="$BUNDLE/share/fcitx5/addon"
    export FCITX_DATA_DIR="$BUNDLE/share/fcitx5"
    export XDG_DATA_DIRS="$BUNDLE/share:${XDG_DATA_DIRS:-}"
    export GTK_IM_MODULE=fcitx
    export QT_IM_MODULE=fcitx
    export XMODIFIERS=@im=fcitx
    if [ -f /tmp/dbus_addr ]; then
        export DBUS_SESSION_BUS_ADDRESS="$(cat /tmp/dbus_addr 2>/dev/null || true)"
    fi
}

do_setup() {
    log "Step 1: 下载 S3 预构建产物 v$VERSION..."
    mkdir -p "$S3_DIR"
    rclone copy "cst:xbz/pinyin-ime/v$VERSION/" "$S3_DIR/" \
        --user-agent "rclone/1.75.1 pinyin-ime" 2>/dev/null || true
    log "S3 产物已下载"

    log "Step 2: 解压桌面 bundle..."
    mkdir -p /root/.local
    if [ ! -d /root/.local/pinyin-ime ]; then
        tar xzf "$S3_DIR/pinyin-ime-desktop-v$VERSION.tar.gz" -C /root/.local
    fi
    log "桌面 bundle已解压"

    log "Step 3: 安装系统依赖..."
    dnf install -y xorg-x11-server-Xvfb dbus xorg-x11-xkb-utils \
        boost-devel fmt-devel zstd-devel libuv-devel ninja-build \
        extra-cmake-modules gettext libuuid-devel python3-dbus 2>/dev/null || true
    log "系统依赖已安装"

    log "Step 4: 安装 micromamba + conda GCC 13..."
    mkdir -p "$NB"/{bin,src,opt,build,mamba-root}
    export MAMBA_ROOT_PREFIX="$NB/mamba-root"
    if [ ! -x "$NB/bin/micromamba" ]; then
        TMP=$(mktemp -d)
        curl -sL -o "$TMP/mm.tar.bz2" \
            "https://gh-proxy.com/https://github.com/mamba-org/micromamba-releases/releases/download/2.9.0-0/micromamba-linux-aarch64.tar.bz2"
        tar -xjf "$TMP/mm.tar.bz2" -C "$TMP" bin/micromamba
        cp "$TMP/bin/micromamba" "$NB/bin/micromamba" && chmod +x "$NB/bin/micromamba"
        rm -rf "$TMP"
    fi
    if [ ! -d "$ENV_DIR/conda-meta" ]; then
        "$NB/bin/micromamba" create -y -p "$ENV_DIR" \
            -c https://mirrors.bfsu.edu.cn/anaconda/cloud/conda-forge \
            gcc=13 gxx=13 boost-cpp fmt zstd libuv extra-cmake-modules gettext libuuid \
            cmake ninja pkg-config make
    fi
    log "Conda GCC 13 环境就绪"

    log "Step 5: 下载并编译 libime..."
    if [ ! -d "$NB/src/libime" ]; then
        curl -sL -o "$NB/src/libime.tar.gz" \
            "https://gh-proxy.com/https://github.com/fcitx/libime/archive/refs/tags/1.1.17.tar.gz"
        tar -xzf "$NB/src/libime.tar.gz" -C "$NB/src"
        mv "$NB/src"/libime-* "$NB/src/libime"

        curl -sL -o "$NB/src/kenlm.tar.gz" \
            "https://gh-proxy.com/https://codeload.github.com/kpu/kenlm/tar.gz/4cb443e60b7bf2c0ddf3c745378f76cb59e254e5"
        mkdir -p "$NB/src/kenlm-tmp"
        tar -xzf "$NB/src/kenlm.tar.gz" -C "$NB/src/kenlm-tmp"
        cp -r "$NB/src/kenlm-tmp"/kenlm-*/* "$NB/src/libime/src/libime/core/kenlm/"

        cat > "$NB/src/libime/src/libime/core/bit_cast_compat.h" << 'PATCH'
#pragma once
#include <cstring>
#include <type_traits>
#if __cplusplus < 202302L
namespace std {
template <typename To, typename From>
constexpr To bit_cast(const From& from) noexcept {
    static_assert(sizeof(To) == sizeof(From), "bit_cast requires same size");
    static_assert(std::is_trivially_copyable_v<To>, "To must be trivially copyable");
    static_assert(std::is_trivially_copyable_v<From>, "From must be trivially copyable");
    To to;
    std::memcpy(&to, &from, sizeof(To));
    return to;
}
}
#endif
PATCH
        for f in "$NB/src/libime/src/libime/core/utils_p.h" "$NB/src/libime/src/libime/core/datrie.cpp"; do
            sed -i '1i #include "bit_cast_compat.h"' "$f"
        done
        sed -i 's/find_package(Boost 1.61 CONFIG REQUIRED COMPONENTS iostreams)/find_package(Boost 1.61 REQUIRED COMPONENTS iostreams)/' \
            "$NB/src/libime/CMakeLists.txt"
    fi

    ln -sf /root/.local/pinyin-ime/opt/fcitx5-x11 "$NB/opt/fcitx5-x11"
    export PATH="$ENV_DIR/bin:$PATH"
    export LD_LIBRARY_PATH="$DEPSLIB:$BUNDLE/lib:$ENV_DIR/lib"
    cmake -S "$NB/src/libime" -B "$NB/build/libime" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$NB/opt/libime" \
        -DCMAKE_PREFIX_PATH="$BUNDLE;$ENV_DIR" \
        -DENABLE_TEST=OFF -DENABLE_DOC=OFF -DENABLE_DATA=OFF -DENABLE_TOOLS=OFF
    cmake --build "$NB/build/libime" -j$(nproc)
    cmake --install "$NB/build/libime"
    log "libime 编译安装完成"

    log "Step 6: 编译 xcb-imdkit（XIM 协议支持）..."
    if [ ! -d "$NB/src/xcb-imdkit" ]; then
        git clone https://gh-proxy.com/https://github.com/fcitx/xcb-imdkit.git "$NB/src/xcb-imdkit"
    fi
    cmake -S "$NB/src/xcb-imdkit" -B "$NB/build/xcb-imdkit" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$DEPSLIB" \
        -DCMAKE_PREFIX_PATH="$ENV_DIR"
    cmake --build "$NB/build/xcb-imdkit"
    cmake --install "$NB/build/xcb-imdkit"
    log "xcb-imdkit 编译安装完成"

    log "Step 7: 重建 pinyinime addon（修复 DataDir）..."
    sed -i 's|"/workspace/native-build/opt/libime/lib"|"/root/.local/pinyin-ime/share/libime"|' \
        "$PROJECT_ROOT/fcitx5-addon/pinyinime.h" 2>/dev/null || true
    sed -i 's/project(pinyinime VERSION 0.9.76/project(pinyinime VERSION 0.9.84/' \
        "$PROJECT_ROOT/fcitx5-addon/CMakeLists.txt" 2>/dev/null || true
    cmake -S "$PROJECT_ROOT/fcitx5-addon" -B "$PROJECT_ROOT/fcitx5-addon/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$BUNDLE;$NB/opt/libime;$ENV_DIR"
    cmake --build "$PROJECT_ROOT/fcitx5-addon/build"
    cp "$PROJECT_ROOT/fcitx5-addon/build/libpinyinime.so" "$BUNDLE/lib/fcitx5/"
    cp "$PROJECT_ROOT/fcitx5-addon/build/pinyinime.conf" "$BUNDLE/share/fcitx5/addon/"
    log "pinyinime addon 重建完成"

    log "Step 8: 部署库和数据文件..."
    cp "$LIBIME"/libIMEPinyin.so* "$BUNDLE/lib/"
    cp "$LIBIME"/libIMECore.so* "$BUNDLE/lib/"
    cp "$LIBIME"/libIMETable.so* "$BUNDLE/lib/"
    if [ -L /root/.local/pinyin-ime/share/libime/libime/sc.dict ]; then
        rm -f /root/.local/pinyin-ime/share/libime/libime/sc.dict
        cp /root/.local/pinyin-ime/share/libime/sc.dict /root/.local/pinyin-ime/share/libime/libime/sc.dict
    fi

    log "Step 9: 修复路径符号链接..."
    mkdir -p /workspace/native-build
    rm -rf /workspace/native-build/mamba-env
    ln -sf "$ENV_DIR" /workspace/native-build/mamba-env
    ln -sf "$NB/opt" /workspace/native-build/opt 2>/dev/null || true
    rm -rf "$ENV_DIR/share/xkeyboard-config-2"
    ln -sf /usr/share/X11/xkb "$ENV_DIR/share/xkeyboard-config-2"
    mkdir -p "$ENV_DIR/share/X11"
    ln -sf /usr/share/X11/xkb "$ENV_DIR/share/X11/xkb" 2>/dev/null || true
    mkdir -p "$ENV_DIR/etc/fonts" "$ENV_DIR/var/cache/fontconfig"
    cat > "$ENV_DIR/etc/fonts/fonts.conf" << 'FONTCFG'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <include ignore_missing="yes">/etc/fonts/fonts.conf</include>
  <dir>/usr/share/fonts</dir>
  <cachedir>/root/workspace/native-build/mamba-env/var/cache/fontconfig</cachedir>
</fontconfig>
FONTCFG
    log "路径符号链接已修复"

    log "Step 10: 配置 fcitx5..."
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
    log "设置完成！运行 'bash scripts/setup_and_run.sh start' 启动输入法"
}

do_start() {
    export_env
    if ! pgrep -x Xvfb > /dev/null; then
        Xvfb :99 -screen 0 1280x1024x24 > /dev/null 2>&1 &
        sleep 1
        log "Xvfb 已启动 (:99)"
    fi
    export DISPLAY=:99
    if ! pgrep -x dbus-daemon > /dev/null; then
        dbus-daemon --session --print-address > /tmp/dbus_addr 2>/dev/null &
        sleep 2
        export DBUS_SESSION_BUS_ADDRESS="$(cat /tmp/dbus_addr)"
        log "dbus 已启动"
    fi
    if ! pgrep -x fcitx5 > /dev/null; then
        setsid fcitx5 -d 2>&1 | head -5
        sleep 3
        log "fcitx5 已启动"
    fi
    log "输入法已启动！Ctrl+Space 切换中英文"
}

do_stop() {
    pkill -x fcitx5 2>/dev/null || true
    pkill -x Xvfb 2>/dev/null || true
    pkill -x dbus-daemon 2>/dev/null || true
    log "已停止"
}

do_test() {
    export_env
    export DISPLAY=:99
    log "运行 CLI 冒烟测试..."
    CLI="$S3_DIR/pinyin_ime_cli_v$VERSION"
    LM="$S3_DIR/zh_CN.lm"
    DICT="$S3_DIR/sc.dict"
    local pass=0 total=0
    for input in "zhongguo:中国" "nihao:你好" "shijie:世界" "pinyin:拼音" "shurufa:输入法"; do
        py=$(echo $input | cut -d: -f1)
        expected=$(echo $input | cut -d: -f2)
        result=$(echo "$py" | $CLI "$LM" "$DICT" 2>/dev/null | head -1 | cut -f1)
        total=$((total + 1))
        [ "$result" = "$expected" ] && pass=$((pass + 1)) && echo "  ✅ $py → $result" || echo "  ❌ $py → $result"
    done
    log "CLI: $pass/$total"
    log "运行 DBus 端到端测试..."
    python3.9 "$PROJECT_ROOT/scripts/e2e_input_test.py" 2>&1 | tail -5
    python3.9 "$PROJECT_ROOT/scripts/interaction_test.py" 2>&1 | tail -5
    python3.9 "$PROJECT_ROOT/scripts/select_test.py" 2>&1 | tail -5
}

do_status() {
    echo "=== 服务状态 ==="
    echo "Xvfb:     $(pgrep -x Xvfb > /dev/null && echo '✅ running' || echo '❌ stopped')"
    echo "dbus:     $(pgrep -x dbus-daemon > /dev/null && echo '✅ running' || echo '❌ stopped')"
    echo "fcitx5:   $(pgrep -x fcitx5 > /dev/null && echo '✅ running' || echo '❌ stopped')"
    echo ""
    echo "=== addon 状态 ==="
    echo "pinyinime:  $(ls $BUNDLE/lib/fcitx5/libpinyinime.so 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
    echo "xim:        $(ls $DEPSLIB/lib64/libxcb-imdkit.so.1 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
    echo "xkb:        $(ls $ENV_DIR/share/xkeyboard-config-2/rules/evdev 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
    echo "fontconfig: $(ls $ENV_DIR/etc/fonts/fonts.conf 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
    echo ""
    echo "=== 数据文件 ==="
    echo "LM:   $(ls /root/.local/pinyin-ime/share/libime/libime/zh_CN.lm 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
    echo "Dict: $(ls /root/.local/pinyin-ime/share/libime/sc.dict 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
}

case "${1:-status}" in
    setup)  do_setup ;;
    start)  do_start ;;
    stop)   do_stop ;;
    test)   do_test ;;
    status) do_status ;;
    *)      echo "用法: $0 {setup|start|stop|test|status}"; exit 1 ;;
esac
