#!/bin/bash
# 系统级安装脚本：将 pinyin-ime 安装到系统路径，使真实桌面会话可用
# 用法: bash scripts/install_system.sh
set -euo pipefail

BUNDLE="/root/.local/pinyin-ime/opt/fcitx5-x11"
DEPSLIB="/root/.local/pinyin-ime/opt/deps-lib"
ENV_DIR="/root/workspace/native-build/mamba-env"
NB="/root/workspace/native-build"
DATADIR="/root/.local/pinyin-ime/share/libime"

log() { echo -e "\033[32m[install]\033[0m $*"; }

# 1. 安装 fcitx5 addon 到系统路径
log "安装 fcitx5 addon 到系统路径..."
mkdir -p /usr/lib64/fcitx5
cp -f "$BUNDLE/lib/fcitx5/libpinyinime.so" /usr/lib64/fcitx5/

mkdir -p /usr/share/fcitx5/addon
cp -f "$BUNDLE/share/fcitx5/addon/pinyinime.conf" /usr/share/fcitx5/addon/ 2>/dev/null || true

# 安装输入法描述文件
find "$BUNDLE/share/fcitx5/inputmethod" -name "pinyinime*" -exec cp -f {} /usr/share/fcitx5/inputmethod/ \; 2>/dev/null || true
mkdir -p /usr/share/fcitx5/inputmethod
# 创建输入法描述文件
cat > /usr/share/fcitx5/inputmethod/pinyinime.conf << 'EOF'
[InputMethod]
Name=PinyinIME
Icon=fcitx-pinyin
Label=拼
LangCode=zh_CN
Addon=pinyinime
Configurable=True
EOF

# 2. 安装 libIME 库到系统路径
log "安装 libIME 库到系统路径..."
cp -f "$DEPSLIB"/libIMEPinyin.so* /usr/lib64/ 2>/dev/null || true
cp -f "$DEPSLIB"/libIMECore.so* /usr/lib64/ 2>/dev/null || true
cp -f "$DEPSLIB"/libIMETable.so* /usr/lib64/ 2>/dev/null || true
cp -f "$DEPSLIB"/libxcb-imdkit.so* /usr/lib64/ 2>/dev/null || true
# 刷新动态链接器缓存
ldconfig 2>/dev/null || true

# 3. 安装数据文件到系统路径
log "安装数据文件到系统路径..."
mkdir -p /usr/share/libime/libime
cp -f "$DATADIR/libime/zh_CN.lm" /usr/share/libime/libime/
cp -f "$DATADIR/libime/sc.dict" /usr/share/libime/libime/
cp -f "$DATADIR/sc.dict" /usr/share/libime/ 2>/dev/null || true

# 4. 安装 CLI 到系统路径
log "安装 CLI 到系统路径..."
cp -f "$BUNDLE/bin/fcitx5" /usr/bin/ 2>/dev/null || true
cp -f "$BUNDLE/bin/fcitx5-remote" /usr/bin/ 2>/dev/null || true
# CLI 二进制
if [ -f "/root/workspace/pinyin-ime/dist/pinyin_ime_cli" ]; then
    cp -f /root/workspace/pinyin-ime/dist/pinyin_ime_cli /usr/bin/pinyin_ime_cli
fi

# 5. 配置环境变量
log "配置环境变量..."
mkdir -p /etc/environment.d
cat > /etc/environment.d/pinyin-ime.conf << 'EOF'
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
XMODIFIERS=@im=fcitx
EOF

# 6. 配置 fcitx5 profile
log "配置 fcitx5 profile..."
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

cat > /root/.config/fcitx5/conf/pinyinime.conf << 'CONF'
DataDir=/usr/share/libime/libime
UserDataDir=/root/.local/share/pinyin-ime
EnableCorrect=True
EnableFuzzy=True
CONF

# 7. 创建 autostart 入口
log "创建 autostart 入口..."
mkdir -p /root/.config/autostart
cat > /root/.config/autostart/fcitx5.desktop << 'EOF'
[Desktop Entry]
Type=Application
Name=Fcitx 5
GenericName=Input Method
Comment=Start Input Method
Exec=fcitx5
Icon=fcitx
Categories=System;Utility;
X-GNOME-Autostart-Phase=Applications
X-GNOME-AutoRestart=false
X-GNOME-Autostart-Notify=false
X-KDE-autostart-after=panel
EOF

# 8. 验证安装
log "验证安装..."
echo "  addon: $(ls /usr/lib64/fcitx5/libpinyinime.so 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
echo "  IM config: $(ls /usr/share/fcitx5/inputmethod/pinyinime.conf 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
echo "  libIME: $(ldconfig -p 2>/dev/null | grep libIMEPinyin | head -1 && echo '✅' || echo '❌')"
echo "  data: $(ls /usr/share/libime/libime/zh_CN.lm 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
echo "  CLI: $(which pinyin_ime_cli 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
echo "  env: $(cat /etc/environment.d/pinyin-ime.conf 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
echo "  autostart: $(ls /root/.config/autostart/fcitx5.desktop 2>/dev/null > /dev/null && echo '✅' || echo '❌')"

log "系统级安装完成！"
log "在桌面会话中，fcitx5 将自动启动，Ctrl+Space 切换中英文"

# 9. 安装 CLI 包装脚本
log "安装 CLI 包装脚本..."
cp -f "$PROJECT_ROOT/scripts/pinyin-ime-wrapper.sh" /usr/bin/pinyin-ime
chmod +x /usr/bin/pinyin-ime
echo "  wrapper: $(which pinyin-ime 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
