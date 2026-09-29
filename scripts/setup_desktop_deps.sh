#!/usr/bin/env bash
# 「真桌面」终验前置依赖：DBus 开发头 + GTK3（开发头） + fcitx5-gtk 源码。
#
# 是 scripts/build_fcitx5_gtk.sh 的前置（一次性）。openEuler 22.03 aarch64 实测：
#   - fcitx5 的 DBus 模块需要 libdbus（dbus-devel 提供 dbus-1.pc）；
#   - fcitx5-gtk 是纯客户端库，用【系统 gcc】编译，需系统 gtk3-devel + ninja；
#   - fcitx5-gtk 源码走 GitHub（国外），国内直连慢 → 用代理（PROXY）下载；
#   - openEuler GTK3 的 immodule 机制是「immodules.cache 缓存文件」（非运行时扫描），
#     配套工具是 gtk-query-immodules-3.0-64（gtk3 包自带，-64 是 openEuler 的 multiarch 命名）。
set -euo pipefail

NB=${NB:-/root/workspace/native-build}
SRC="$NB/src"
PROXY="${PROXY:-http://127.0.0.1:7890}"
FCITX5_GTK_VER=5.1.7

# ---------- 1. 系统依赖（dnf，openEuler 国内源）----------
echo "=== dnf 安装 DBus/GTK3 开发头 + system ninja ==="
dnf install -y --setopt=install_weak_deps=False \
    dbus-devel dbus-libs gtk3 gtk3-devel ninja-build

# ---------- 2. fcitx5-gtk 源码（GitHub，走代理）----------
if [ ! -f "$SRC/fcitx5-gtk-$FCITX5_GTK_VER/CMakeLists.txt" ]; then
    echo "=== 下载 fcitx5-gtk $FCITX5_GTK_VER ==="
    mkdir -p "$SRC"
    curl -L --retry 5 -x "$PROXY" -o "$SRC/fcitx5-gtk.tar.gz" \
        "https://codeload.github.com/fcitx/fcitx5-gtk/tar.gz/refs/tags/$FCITX5_GTK_VER"
    tar -xzf "$SRC/fcitx5-gtk.tar.gz" -C "$SRC"
fi

echo "=== 真桌面依赖就绪，下一步：bash scripts/build_fcitx5_gtk.sh ==="