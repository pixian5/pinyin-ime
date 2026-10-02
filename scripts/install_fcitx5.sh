#!/bin/bash
# Install pinyin-ime as fcitx5 addon
set -e

NB="${NATIVE_BUILD:-/root/workspace/native-build}"
FCITX5_DATA="$HOME/.local/share/fcitx5"
FCITX5_CONFIG="$HOME/.config/fcitx5"

echo "=== Installing pinyin-ime fcitx5 addon ==="

# Create directories
mkdir -p "$FCITX5_DATA/addon" "$FCITX5_DATA/pinyinime" "$FCITX5_CONFIG/conf"

# Copy addon library
cp -v "$NB/opt/fcitx5/lib/fcitx5/libpinyinime.so" "$FCITX5_DATA/addon/"

# Copy addon config
cp -v "$NB/opt/fcitx5/share/fcitx5/addon/pinyinime.conf" "$FCITX5_DATA/addon/"

# Copy language model and dictionary
cp -v "$NB/opt/libime/lib/libime/zh_CN.lm" "$FCITX5_DATA/pinyinime/"
cp -v "$NB/opt/libime/share/libime/sc.dict" "$FCITX5_DATA/pinyinime/"

# Copy libime libraries to ldconfig path
echo "$NB/opt/libime/lib" > /etc/ld.so.conf.d/libime.conf
echo "$NB/opt/fcitx5/lib" >> /etc/ld.so.conf.d/libime.conf
ldconfig 2>/dev/null || true

echo "=== Installation complete ==="
echo "Addon: $FCITX5_DATA/addon/libpinyinime.so"
echo "LM:    $FCITX5_DATA/pinyinime/zh_CN.lm"
echo "Dict:  $FCITX5_DATA/pinyinime/sc.dict"
echo ""
echo "To use: restart fcitx5 and add 'Pinyin IME' in fcitx5-configtool"
