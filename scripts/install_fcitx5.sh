#!/bin/bash
# 安装 pinyin-ime fcitx5 addon 到系统
# 用法: bash scripts/install_fcitx5.sh

set -e
NB="/root/workspace/native-build"
PREFIX="${1:-/usr/local}"

echo "安装 pinyin-ime fcitx5 addon..."

# 1. 安装 addon 共享库
cp -v "$NB/opt/fcitx5/lib/fcitx5/libpinyinime.so" "$PREFIX/lib/fcitx5/" 2>/dev/null || {
    mkdir -p "$PREFIX/lib/fcitx5/"
    cp "$NB/opt/fcitx5/lib/fcitx5/libpinyinime.so" "$PREFIX/lib/fcitx5/"
}

# 2. 安装 addon 元数据
mkdir -p "$PREFIX/share/fcitx5/addon/"
cat > "$PREFIX/share/fcitx5/addon/pinyinime.conf" << 'EOF'
[Addon]
Name=pinyinime
Category=InputMethod
Library=pinyinime
Type=SharedLibrary
OnDemand=True
Comment=pinyin-ime Input Method
EOF

# 3. 安装输入方法配置
mkdir -p "$PREFIX/share/fcitx5/inputmethod/"
cat > "$PREFIX/share/fcitx5/inputmethod/pinyinime.conf" << 'EOF'
[InputMethod]
Name=pinyinime
Icon=fcitx-pinyin
Label=拼
LangCode=zh_CN
Addon=pinyinime
Configurable=True
EOF

# 4. 安装 libime 数据
mkdir -p "$PREFIX/share/libime/"
cp -v "$NB/opt/libime/share/libime/sc.dict" "$PREFIX/share/libime/" 2>/dev/null || true
cp -v "$NB/opt/libime/lib/libime/zh_CN.lm" "$PREFIX/share/libime/" 2>/dev/null || {
    mkdir -p "$PREFIX/lib/libime/"
    cp "$NB/opt/libime/lib/libime/zh_CN.lm" "$PREFIX/lib/libime/"
}

# 5. 安装用户配置
mkdir -p ~/.config/fcitx5/conf/
cat > ~/.config/fcitx5/conf/pinyinime.conf << 'EOF'
NBest=45
PageSize=9
EnableFuzzy=False
EnableCorrect=False
EnableShuangpin=False
ShuangpinScheme=xiaohe
EOF

echo ""
echo "✅ 安装完成！"
echo "  Addon: $PREFIX/lib/fcitx5/libpinyinime.so"
echo "  数据: $PREFIX/share/libime/"
echo ""
echo "使用方法:"
echo "  1. 重启 fcitx5: fcitx5 -r"
echo "  2. 在 fcitx5 配置中添加 'pinyinime' 输入法"
echo "  3. 切换到 pinyinime 输入法即可使用"
