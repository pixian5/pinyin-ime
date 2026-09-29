#!/bin/bash
# pinyin-ime 一键安装脚本
# 安装 CLI + fcitx5 addon + libime 数据到系统路径
# 用法: sudo bash scripts/install_all.sh
set -euo pipefail

NB="${NB:-/root/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${1:-/usr/local}"

echo "=== pinyin-ime 安装脚本 ==="

# 1. Install CLI
echo "[1/4] 安装 CLI..."
install -D "$REPO_DIR/cpp/build/pinyin_ime_cli" "$PREFIX/bin/pinyin_ime_cli"
echo "  → $PREFIX/bin/pinyin_ime_cli"

# 2. Install libime data symlinks
echo "[2/4] 安装 libime 数据..."
mkdir -p /usr/share/libime/libime
ln -sf "$OPT/libime/lib/libime/zh_CN.lm" /usr/share/libime/libime/zh_CN.lm
ln -sf "$OPT/libime/lib/libime/zh_CN.lm.predict" /usr/share/libime/libime/zh_CN.lm.predict
ln -sf "$OPT/libime/share/libime/sc.dict" /usr/share/libime/sc.dict
echo "  → /usr/share/libime/libime/zh_CN.lm"
echo "  → /usr/share/libime/sc.dict"

# 3. Install fcitx5 addon
echo "[3/4] 安装 fcitx5 addon..."
FCITX5_LIBDIR="${FCITX5_LIBDIR:-/usr/lib64/fcitx5}"
FCITX5_SHAREDIR="${FCITX5_SHAREDIR:-/usr/share/fcitx5}"
install -D "$REPO_DIR/fcitx5-addon/build/libpinyinime.so" "$FCITX5_LIBDIR/libpinyinime.so"
install -D "$REPO_DIR/fcitx5-addon/build/pinyinime.conf" "$FCITX5_SHAREDIR/addon/pinyinime.conf"
echo "  → $FCITX5_LIBDIR/libpinyinime.so"
echo "  → $FCITX5_SHAREDIR/addon/pinyinime.conf"

# 4. Install libraries
echo "[4/4] 安装运行时库..."
mkdir -p "$PREFIX/lib"
for lib in libIMECore.so libIMEPinyin.so; do
    src="$OPT/libime/lib/$lib"
    [ -f "$src" ] && install "$src" "$PREFIX/lib/" && echo "  → $PREFIX/lib/$lib"
done
for lib in libFcitx5Core.so libFcitx5Utils.so libFcitx5Config.so; do
    src="$OPT/fcitx5-server/lib/$lib"
    [ -f "$src" ] && install "$src" "$PREFIX/lib/" && echo "  → $PREFIX/lib/$lib"
done

echo ""
echo "=== 安装完成 ==="
echo "CLI: $PREFIX/bin/pinyin_ime_cli"
echo "测试: pinyin_ime_cli /usr/share/libime/libime/zh_CN.lm /usr/share/libime/sc.dict"
