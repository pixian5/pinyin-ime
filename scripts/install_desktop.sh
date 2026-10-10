#!/usr/bin/env bash
# pinyin-ime 桌面免 CLI 集成 —— 跑一次，之后打字全靠图形界面（对标搜狗拼音的开箱体验）。
#
# 装完后做到：
#   1. 登录桌面自动拉起 fcitx5（XDG autostart + ~/.xprofile 双保险）；
#   2. 环境变量免 shell 配置（~/.config/environment.d + ~/.xprofile + env.sh），
#      GTK/Qt/XIM 三类应用都走 fcitx；
#   3. 默认输入法就是 pinyinime（fcitx5 profile），Ctrl+Space 直接打拼音；
#   4. 候选窗中文字体就位（文泉驿微米黑，缺则自动从国内镜像下载）；
#   5. 全部资产自包含在 PREFIX（可整体拷贝到别的机器）。
#
# 用法：
#   bash scripts/install_desktop.sh            # 沙箱/用户级安装（PREFIX=$HOME/.local/pinyin-ime）
#   sudo bash scripts/install_desktop.sh /usr/local/pinyin-ime   # 系统级安装
#
# 依赖：先跑通构建（scripts/full_build.sh 或 S3 产物 + build_fcitx5_x11/gtk/qt）。
set -euo pipefail

NB="${NB:-/root/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
PREFIX="${1:-$HOME/.local/pinyin-ime}"
# 真实机器上 libime 数据装进 PREFIX/share/libime；沙箱为省 52MB 复制直接指向构建产物
LIBIME_SRC="$OPT/libime"

log() { echo "[install-desktop] $*"; }

# ---------- 0. 前置检查 ----------
for need in "$OPT/fcitx5-x11/bin/fcitx5" "$OPT/fcitx5-x11/lib/fcitx5/libpinyinime.so"; do
    [ -e "$need" ] || { echo "!! 缺构建产物: $need（先跑 build_fcitx5_x11.sh + 装 addon）" >&2; exit 1; }
done

# ---------- 1. 资产就位 ----------
log "安装前缀: $PREFIX"
mkdir -p "$PREFIX"
# fcitx5-x11（含 pinyinime addon + GTK im module + Qt im module）整体复制
mkdir -p "$PREFIX/opt"
cp -a "$OPT/fcitx5-x11" "$PREFIX/opt/"
# libime 数据：复制（自包含、可拷走）
mkdir -p "$PREFIX/share/libime/libime"
cp -a "$LIBIME_SRC/lib/libime/." "$PREFIX/share/libime/libime/"
cp -a "$LIBIME_SRC/share/libime/." "$PREFIX/share/libime/" 2>/dev/null || true
# sc.dict 的 DataDir 布局对齐（addon 在 DataDir/libime 下找 dict 与 LM）
ln -sf "$PREFIX/share/libime/sc.dict" "$PREFIX/share/libime/libime/sc.dict" 2>/dev/null || true

# ---------- 2. 中文字体（候选窗显示） ----------
FONT_DIR="$HOME/.local/share/fonts"
# fc-list 可能只在 conda env / RPM 前缀里（沙箱），真实机器则是系统自带
FCLIST="$(command -v fc-list || true)"
[ -z "$FCLIST" ] && [ -x "$ENV/bin/fc-list" ] && FCLIST="$ENV/bin/fc-list"
[ -z "$FCLIST" ] && [ -x /workspace/xroot/usr/bin/fc-list ] && FCLIST=/workspace/xroot/usr/bin/fc-list
# 注意：grep -q 会在首个匹配后提前关管道，fc-list 收 SIGPIPE 退出 141，
# 叠加 pipefail 会把「已装字体」误判成「没装」→ 用 grep -c 读全量再判断。
FONT_HITS=$("$FCLIST" 2>/dev/null | grep -ci 'wqy-microhei\|wenquanyi' || true)
if [ "${FONT_HITS:-0}" -eq 0 ]; then
    log "安装中文字体（文泉驿微米黑）"
    mkdir -p "$FONT_DIR"
    if [ ! -f "$FONT_DIR/wqy-microhei.ttc" ]; then
        curl -fsSL --retry 2 -o /tmp/wqy-microhei.deb \
            https://mirrors.aliyun.com/debian/pool/main/f/fonts-wqy-microhei/fonts-wqy-microhei_0.2.0-beta-3.1_all.deb || {
                echo "!! 字体下载失败，候选窗中文可能显示为方块" >&2; }
        if [ -f /tmp/wqy-microhei.deb ]; then
            local_tmp=$(mktemp -d)
            ( cd "$local_tmp" && ar x /tmp/wqy-microhei.deb && tar xf data.tar.* && \
              cp usr/share/fonts/truetype/wqy/wqy-microhei.ttc "$FONT_DIR/" ) || true
            rm -rf "$local_tmp" /tmp/wqy-microhei.deb
        fi
    fi
    fc-cache -f "$FONT_DIR" >/dev/null 2>&1 || true
fi

# ---------- 3. 环境变量（免 CLI 的核心） ----------
cat > "$PREFIX/env.sh" <<EOF
# pinyin-ime 桌面环境（由 install_desktop.sh 生成，勿手改——重新安装会覆盖）
export PINYIN_IME_PREFIX="$PREFIX"
export PATH="\$PINYIN_IME_PREFIX/opt/fcitx5-x11/bin:\$PATH"
export LD_LIBRARY_PATH="\$PINYIN_IME_PREFIX/opt/fcitx5-x11/lib${ENV:+:$ENV/lib}\${LD_LIBRARY_PATH:+:\$LD_LIBRARY_PATH}"
export XMODIFIERS=@im=fcitx
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export GTK_IM_MODULE_FILE="\$PINYIN_IME_PREFIX/opt/fcitx5-x11/lib/gtk-3.0/3.0.0/immodules.cache"
export QT_QPA_PLATFORMINPUTCONTEXT_PATH="\$PINYIN_IME_PREFIX/opt/fcitx5-x11/lib/qt5/plugins/platforminputcontexts"
export FCITX_ADDON_DIRS="\$PINYIN_IME_PREFIX/opt/fcitx5-x11/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="\$PINYIN_IME_PREFIX/opt/fcitx5-x11/share/fcitx5/addon"
export FCITX_DATA_DIR="\$PINYIN_IME_PREFIX/opt/fcitx5-x11/share/fcitx5"
EOF

# systemd 用户会话：environment.d（GNOME/KDE 等现代桌面登录即生效）
mkdir -p "$HOME/.config/environment.d"
cat > "$HOME/.config/environment.d/90-pinyin-ime.conf" <<EOF
XMODIFIERS=@im=fcitx
GTK_IM_MODULE=fcitx
QT_IM_MODULE=fcitx
GTK_IM_MODULE_FILE=$PREFIX/opt/fcitx5-x11/lib/gtk-3.0/3.0.0/immodules.cache
QT_QPA_PLATFORMINPUTCONTEXT_PATH=$PREFIX/opt/fcitx5-x11/lib/qt5/plugins/platforminputcontexts
EOF
# 非 systemd 的传统 X 会话
touch "$HOME/.xprofile"
grep -q "pinyin-ime/env.sh" "$HOME/.xprofile" 2>/dev/null || \
    echo "[ -r \"$PREFIX/env.sh\" ] && . \"$PREFIX/env.sh\"" >> "$HOME/.xprofile"

# ---------- 4. fcitx5 配置：默认输入法 = pinyinime ----------
mkdir -p "$HOME/.config/fcitx5/conf"
if [ ! -f "$HOME/.config/fcitx5/profile" ]; then
    cat > "$HOME/.config/fcitx5/profile" <<'EOF'
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
EOF
fi
cat > "$HOME/.config/fcitx5/conf/pinyinime.conf" <<EOF
DataDir=$PREFIX/share/libime
UserDataDir=$HOME/.local/share/pinyin-ime
NBest=45
PageSize=9
EOF

# ---------- 5. 启动器 + 登录自启 ----------
mkdir -p "$PREFIX/bin"
cat > "$PREFIX/bin/fcitx5-pinyin" <<EOF
#!/usr/bin/env bash
# pinyin-ime 启动器：装载环境后拉起 fcitx5
[ -r "$PREFIX/env.sh" ] && . "$PREFIX/env.sh"
exec "$PREFIX/opt/fcitx5-x11/bin/fcitx5" "\$@"
EOF
chmod +x "$PREFIX/bin/fcitx5-pinyin"

mkdir -p "$HOME/.config/autostart"
cat > "$HOME/.config/autostart/pinyin-ime.desktop" <<EOF
[Desktop Entry]
Type=Application
Name=Pinyin IME (fcitx5)
Comment=pinyin-ime 拼音输入法守护进程
Exec=$PREFIX/bin/fcitx5-pinyin -r
X-GNOME-Autostart-enabled=true
StartupNotify=false
EOF

# ---------- 6. 自检 ----------
log "自检："
check() { if eval "$2" >/dev/null 2>&1; then echo "  ✅ $1"; else echo "  ❌ $1"; fi; }
check "fcitx5 二进制"        "test -x $PREFIX/opt/fcitx5-x11/bin/fcitx5"
check "pinyinime addon"      "test -f $PREFIX/opt/fcitx5-x11/lib/fcitx5/libpinyinime.so"
check "libime 语言模型"      "test -f $PREFIX/share/libime/libime/zh_CN.lm"
check "GTK im module 缓存"   "test -f $PREFIX/opt/fcitx5-x11/lib/gtk-3.0/3.0.0/immodules.cache"
check "Qt im module 插件"    "test -f $PREFIX/opt/fcitx5-x11/lib/qt5/plugins/platforminputcontexts/libfcitx5platforminputcontextplugin.so"
check "中文字体"             "[ \"\${FONT_HITS:-0}\" -gt 0 ]"
check "登录自启项"           "test -f $HOME/.config/autostart/pinyin-ime.desktop"
check "environment.d"        "test -f $HOME/.config/environment.d/90-pinyin-ime.conf"

cat <<EOF

[install-desktop] 完成。从现在起无需命令行：
  · 重新登录桌面（或双击托盘图标/运行 $PREFIX/bin/fcitx5-pinyin）即有中文输入；
  · Ctrl+Space 切换中英；候选窗、自学习、模糊音、纠错在「配置」里调（fcitx5-configtool）。
  · 沙箱/无显示服务器验证：bash scripts/xvfb_desktop_ime_test.sh
EOF
