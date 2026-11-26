#!/usr/bin/env bash
# fcitx5-qt 5.1.4 —— Qt5 im module（QT_IM_MODULE=fcitx 的入口），补齐「搜狗式体验」的
# Qt 应用链路（KDE/WPS/qterminal 等）。与 GTK 模块同理：纯客户端插件，经 DBus 与
# fcitx5 守护进程通信，不链接 fcitx5 核心库。
#
# 与 build_fcitx5_gtk.sh 的区别：
#   1. GTK 模块用系统 gcc（系统 GTK3），而本沙箱的 Qt5 来自 conda env（mamba-env），
#      → 必须用 conda gcc 编译，否则 Qt 插件 ABI（libstdc++/Qt）不匹配；
#   2. 插件装进本前缀 lib/qt5/plugins/platforminputcontexts/，运行时由
#      QT_QPA_PLATFORMINPUTCONTEXT_PATH 指过去（真实机器可直接装 Qt 插件目录）。
#
# 沙箱用法：
#   NB=/workspace/native-build bash scripts/build_fcitx5_qt.sh
set -euo pipefail

NB=${NB:-/root/workspace/native-build}
ENV="$NB/mamba-env"
OPT="$NB/opt"
BUILD="$NB/build"
SRC="$NB/src"

QT_SRC="$SRC/fcitx5-qt-5.1.4"
QT_BUILD="$BUILD/fcitx5-qt"
mkdir -p "$QT_BUILD"

export PATH="$ENV/bin:/usr/bin:/bin"
export CC="$ENV/bin/aarch64-conda-linux-gnu-cc"
export CXX="$ENV/bin/aarch64-conda-linux-gnu-c++"
# Fcitx5Core（fcitx5-x11 前缀）+ conda Qt5/ECM
export CMAKE_PREFIX_PATH="$OPT/fcitx5-x11:$ENV"
export PKG_CONFIG_PATH="$ENV/lib/pkgconfig:$ENV/share/pkgconfig"
export LIBRARY_PATH="$ENV/lib"
export LD_LIBRARY_PATH="$ENV/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

echo "=== configure fcitx5-qt（conda gcc + Qt5）==="
cmake -S "$QT_SRC" -B "$QT_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$OPT/fcitx5-x11" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DENABLE_QT4=Off \
    -DENABLE_QT5=On \
    -DENABLE_QT6=Off \
    -DENABLE_X11=On \
    > "$BUILD/fcitx5-qt-configure.log" 2>&1
echo "=== build fcitx5-qt ==="
ninja -C "$QT_BUILD" > "$BUILD/fcitx5-qt-build.log" 2>&1
echo "=== install fcitx5-qt ==="
ninja -C "$QT_BUILD" install > "$BUILD/fcitx5-qt-install.log" 2>&1

PLUGIN_DIR="$OPT/fcitx5-x11/lib/qt5/plugins/platforminputcontexts"
# cmake 会把插件装进 Qt 自己的插件目录（真实机器=系统 Qt 目录，自动被发现；
# 沙箱=conda env 目录）。再复制一份到本前缀，便于打包/自包含部署。
SRC_PLUGIN="$(find "$ENV/plugins" "$OPT/fcitx5-x11" -name 'libfcitx5platforminputcontextplugin.so' 2>/dev/null | head -1)"
if [ -n "$SRC_PLUGIN" ]; then
    mkdir -p "$PLUGIN_DIR"
    cp -a "$SRC_PLUGIN" "$PLUGIN_DIR/"
fi
if [ -f "$PLUGIN_DIR/libfcitx5platforminputcontextplugin.so" ]; then
    echo "FCITX5-QT DONE -> $PLUGIN_DIR"
    echo "  （Qt 自动发现路径: ${SRC_PLUGIN}）"
    echo "  非标准前缀运行时需 export QT_QPA_PLATFORMINPUTCONTEXT_PATH=$PLUGIN_DIR"
else
    echo "!! 插件未生成，检查 $BUILD/fcitx5-qt-install.log" >&2
    exit 1
fi
