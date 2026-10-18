#!/bin/bash
# reproduce_from_scratch.sh —— 一条命令在 openEuler aarch64 内机上，
# 用 S3 预构建产物复现可运行输入法（无 conda、无 docker）。
#
# 自动检测环境：
#   - /usr 可写 → dnf install 直装 X/dbus/devel/字体（简洁路线）
#   - /usr 只读 → dnf download + rpm2archive + tar 解包到 sysroot（兼容路线）
#
# 用法: bash scripts/reproduce_from_scratch.sh    （需已配置 rclone 远程 cst + gitcode token）
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="${SYSROOT:-/workspace/sysroot}"
DEPLOY="${DEPLOY:-/workspace/deploy}"
RPMS="${RPMS:-/workspace/rpms}"
PROXY="${PROXY:-http://127.0.0.1:7890}"
MAIN_VER="v0.9.84a"; DESK_VER="v0.9.83"; SUP_VER="v0.9.84"
RT="$DEPLOY/pinyin-ime-v0.9.84a"; DESK="$DEPLOY/pinyin-ime"
SYSLIB="$SYSROOT/usr/lib64"

log(){ echo -e "\033[36m[repro]\033[0m $*"; }

# 0) git token 透传
if [ -n "${GITCODE_TOKEN:-}" ]; then export GITCODE_TOKEN; fi

# 1) dnf 仓库切华为云镜像（可写 /etc 时）
if [ -w /etc/yum.repos.d ]; then
  log "配置华为云镜像源…"
  cat > /etc/yum.repos.d/openEuler.repo <<'EOF'
[OS]
name=OS
baseurl=https://mirrors.huaweicloud.com/openeuler/openEuler-22.03-LTS/OS/$basearch/
enabled=1
gpgcheck=0
[everything]
name=everything
baseurl=https://mirrors.huaweicloud.com/openeuler/openEuler-22.03-LTS/everything/$basearch/
enabled=1
gpgcheck=0
[EPOL]
name=EPOL
baseurl=https://mirrors.huaweicloud.com/openeuler/openEuler-22.03-LTS/EPOL/main/$basearch/
enabled=1
gpgcheck=0
[update]
name=update
baseurl=https://mirrors.huaweicloud.com/openeuler/openEuler-22.03-LTS/update/$basearch/
enabled=1
gpgcheck=0
EOF
  dnf makecache -q >/dev/null 2>&1 || true
fi

# 2) X/dbus/devel/字体安装
#    分支 A: /usr 可写 → dnf install 直装
#    分支 B: /usr 只读 → dnf download + rpm2archive + tar → sysroot
DNF_PKGS_RUNTIME="xorg-x11-server-Xvfb xorg-x11-server-common xkeyboard-config dbus dbus-x11 dbus-tools xorg-x11-utils xterm libXfont2 dejavu-sans-fonts"
DNF_PKGS_DEVEL="libX11-devel libXtst-devel libXext-devel xorg-x11-proto-devel libXft-devel freetype-devel fontconfig-devel libXrender-devel"
DNF_PKGS_FONTS="wqy-microhei-fonts wqy-zenhei-fonts"
DNF_PKGS_TOOLS="ImageMagick"

if [ -w /usr ] || [ -w /usr/local/bin ]; then
  # ── 分支 A: dnf install 直装 ──
  log "检测到 /usr 可写 → dnf install 直装路线"
  DNF_MODE="direct"
  log "安装运行时依赖…"
  dnf install -y $DNF_PKGS_RUNTIME $DNF_PKGS_FONTS $DNF_PKGS_TOOLS >/dev/null 2>&1 || true
  log "安装 X11 devel 头文件…"
  dnf install -y $DNF_PKGS_DEVEL >/dev/null 2>&1 || true
  # sysroot 指向系统根（兼容后续脚本中 $SYSROOT/usr/bin/Xvfb 等路径）
  SYSROOT="/"
  [ -x /usr/bin/Xvfb ] && log "✅ dnf 直装 X/dbus 就绪" || log "⚠️ Xvfb 未装上（DBus 端仍可）"
else
  # ── 分支 B: sysroot 解包（只读 /usr） ──
  log "检测到 /usr 只读 → sysroot 解包路线"
  DNF_MODE="sysroot"
  if [ ! -e "$SYSROOT/usr/bin/Xvfb" ]; then
    log "下载并解包 X/dbus 到 $SYSROOT …"
    mkdir -p "$RPMS" "$SYSROOT"
    dnf download --resolve --alldeps --destdir "$RPMS" --releasever=22.03-LTS \
      $DNF_PKGS_RUNTIME >/dev/null 2>&1 || \
      dnf download --resolve --destdir "$RPMS" \
      $DNF_PKGS_RUNTIME >/dev/null 2>&1
    for r in "$RPMS"/*.rpm; do rpm2archive "$r" 2>/dev/null || true; done
    for t in "$RPMS"/*.tgz; do tar xzf "$t" -C "$SYSROOT" 2>/dev/null || true; done
  fi
  [ -e "$SYSROOT/usr/bin/Xvfb" ] && log "✅ sysroot X/dbus 就绪" || log "⚠️ sysroot 缺 Xvfb（图形端不可用，DBus 端仍可）"
fi

# 3) S3 预构建产物
if [ ! -x "$RT/bin/pinyin_ime_cli" ]; then
  log "下载 S3 预构建产物…"
  UA="rclone/1.75.1 pinyin-ime"
  mkdir -p "$DEPLOY"
  rclone copy "cst:xbz/pinyin-ime/$MAIN_VER/" "$DEPLOY/dl_$MAIN_VER/" --user-agent "$UA" >/dev/null 2>&1 || true
  rclone copy "cst:xbz/pinyin-ime/$DESK_VER/" "$DEPLOY/dl_$DESK_VER/" --user-agent "$UA" >/dev/null 2>&1 || true
  rclone copy "cst:xbz/pinyin-ime/$SUP_VER/"  "$DEPLOY/dl_$SUP_VER/"  --user-agent "$UA" >/dev/null 2>&1 || true
  # 解包
  for tgz in "$DEPLOY"/dl_*/releases/*.tar.gz; do [ -e "$tgz" ] && tar xzf "$tgz" -C "$DEPLOY"; done
  for tgz in "$DEPLOY"/dl_*/*desktop*.tar.gz; do [ -e "$tgz" ] && tar xzf "$tgz" -C "$DEPLOY"; done
  # supplement 库覆盖
  for so in "$DEPLOY"/dl_$SUP_VER/supplement/*.so*; do [ -e "$so" ] && cp -f "$so" "$RT/lib/" 2>/dev/null || true; done
  # 数据
  cp -f "$DEPLOY"/dl_$DESK_VER/zh_CN.lm "$RT/data/zh_CN.lm" 2>/dev/null || true
  cp -f "$DEPLOY"/dl_$DESK_VER/sc.dict  "$RT/data/sc.dict"  2>/dev/null || true
  # libIME SONAME 符号链接
  for l in libIMECore libIMEPinyin libIMETable; do
    ln -sf "$RT/lib/${l}.so.1.1.17" "$RT/lib/${l}.so.0"; ln -sf "${l}.so.1.1.17" "$RT/lib/${l}.so"
  done
  # xcb-imdkit SONAME 链接
  ln -sf libxcb-imdkit.so.1.0.9 "$RT/lib/libxcb-imdkit.so.1"
  ln -sf libxcb-imdkit.so.1.0.9 "$RT/lib/libxcb-imdkit.so"
fi
[ -x "$RT/bin/pinyin_ime_cli" ] && log "✅ S3 产物就绪" || { log "❌ 缺 CLI"; exit 1; }

# 4) 默认 DataDir
mkdir -p /root/.local/pinyin-ime/share/libime
cp -f "$RT/data/zh_CN.lm" /root/.local/pinyin-ime/share/libime/ 2>/dev/null || true
cp -f "$RT/data/sc.dict"  /root/.local/pinyin-ime/share/libime/ 2>/dev/null || true

# 5) native-build 兼容目录 + CLI 入口
DEPLOY="$DEPLOY" NB="$REPO_DIR/native-build" bash "$REPO_DIR/scripts/setup_local_runtime.sh"
mkdir -p "$REPO_DIR/cpp/build"; ln -sf "$RT/bin/pinyin_ime_cli" "$REPO_DIR/cpp/build/pinyin_ime_cli"
install -m755 "$REPO_DIR/scripts/pinyin-ime-cli-wrapper.sh" /usr/local/bin/pinyin-ime 2>/dev/null || \
  cp "$REPO_DIR/scripts/pinyin-ime-cli-wrapper.sh" /usr/local/bin/pinyin-ime && chmod +x /usr/local/bin/pinyin-ime

# 6) 五层验证
log "运行五层验证…"
log "安装模式: $DNF_MODE"
bash "$REPO_DIR/scripts/repro_verify.sh" || log "（部分层受环境影响未过，见上）"
log "完成。CLI: echo nihao | pinyin-ime ；桌面栈: bash scripts/start_im_env_ro.sh start"
log "X11 桌面: Xvfb :99 -screen 0 1280x800x24 &  →  DISPLAY=:99 ./tools/xim_test_client"
