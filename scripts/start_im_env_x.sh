#!/bin/bash
# start_im_env_x.sh —— 在「只读 /usr」沙箱内启动 X11 模式 fcitx5 + pinyinime。
#
# 与 start_im_env_ro.sh（DBus-only）的区别：本脚本额外拉起 Xvfb 虚拟显示，
# 使 fcitx5 的 xim 前端可用，从而能跑「真实 X 应用窗口内的输入链路」。
#
# 两个关键突破（此前的环境限制）：
#   1) Xvfb 的 XKB 初始化硬编码调用 /usr/bin/xkbcomp，而本机 /usr 只读、系统未装。
#      用 tools/xkbcomp_redirect.c 编译的 LD_PRELOAD 垫片把 /usr/bin/xkbcomp
#      透明重定向到 sysroot 中的真实二进制。
#   2) 桌面 bundle 的 libxkbcommon 编译期数据路径指向 mamba-env（不存在），
#      用 XKB_CONFIG_ROOT 指向 sysroot 的 xkb 数据；fontconfig 用自生成的
#      fonts.conf 指向 sysroot 字体（避免 classicui 渲染崩溃）。
#
# 用法: XDISPLAY=:99 bash scripts/start_im_env_x.sh {start|stop|status}
set -uo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="${SYSROOT:-/workspace/sysroot}"
DEPLOY="${DEPLOY:-/workspace/deploy}"
RT="$DEPLOY/pinyin-ime-v0.9.84a"
DESK="$DEPLOY/pinyin-ime"
RUNDIR="${RUNDIR:-/workspace/fcitx5-runtime}"
SHIM="${SHIM:-/workspace/xkbcomp_redirect.so}"
XDG=/run/user/0
XKBCOMP_REAL="$SYSROOT/usr/bin/xkbcomp"
XDISPLAY="${XDISPLAY:-:99}"
DISP_NUM="${XDISPLAY#:}"

log(){ echo "[im-x] $*"; }
need(){ [ -e "$1" ] || { echo "缺少 $1"; exit 1; }; }

# 编译 xkbcomp 重定向垫片
build_shim() {
  if [ ! -e "$SHIM" ]; then
    log "编译 xkbcomp 重定向垫片…"
    gcc -shared -fPIC -O2 -o "$SHIM" "$REPO_DIR/tools/xkbcomp_redirect.c" -ldl
  fi
}

# 生成 fcitx5 配置（默认输入法 pinyinime）
build_config() {
  mkdir -p "$RUNDIR/conf/fcitx5/conf" "$XDG"
  cat > "$RUNDIR/conf/fcitx5/profile" <<'EOF'
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
  cat > "$RUNDIR/conf/fcitx5/conf/pinyinime.conf" <<'EOF'
EnableFuzzy=True
EnableCorrect=True
EOF
  # fontconfig：指向 sysroot 字体（只读 /usr 上没有 dejavu 之外的字体）
  cat > "$RUNDIR/fonts.conf" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <dir>$SYSROOT/usr/share/fonts</dir>
  <cachedir>$RUNDIR/fontcache</cachedir>
</fontconfig>
EOF
  mkdir -p "$RUNDIR/fontcache"
}

# 最小自包含 DBus 会话配置（系统 /usr/share/dbus-1/session.conf 只读）
dbus_conf() {
  cat > "$RUNDIR/dbus.conf" <<'DBUSEOF'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <auth>ANONYMOUS</auth>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*"/>
    <allow receive_sender="*"/>
    <allow own="*"/>
  </policy>
</busconfig>
DBUSEOF
}

start_xvfb() {
  if pgrep -f "[X]vfb $XDISPLAY" >/dev/null 2>&1; then
    log "Xvfb $XDISPLAY 已在运行"; return 0
  fi
  rm -f "/tmp/.X${DISP_NUM}-lock" "/tmp/.X11-unix/X${DISP_NUM}" 2>/dev/null
  log "启动 Xvfb $XDISPLAY（LD_PRELOAD 重定向 xkbcomp）"
  LD_PRELOAD="$SHIM" XKBCOMP_REAL="$XKBCOMP_REAL" LD_LIBRARY_PATH="$SYSROOT/usr/lib64" \
    setsid "$SYSROOT/usr/bin/Xvfb" "$XDISPLAY" -screen 0 1280x800x24 -nolisten tcp \
      -xkbdir "$SYSROOT/usr/share/X11/xkb" > "$RUNDIR/xvfb.log" 2>&1 < /dev/null &
  sleep 3
  pgrep -f "[X]vfb $XDISPLAY" >/dev/null 2>&1 && log "✅ Xvfb $XDISPLAY 运行中" \
    || { log "❌ Xvfb 启动失败"; cat "$RUNDIR/xvfb.log"; return 1; }
}

start_dbus() {
  local DB
  DB="$(env LD_LIBRARY_PATH="$SYSROOT/usr/lib64" PATH="$SYSROOT/usr/bin:/usr/bin:/bin" \
        "$SYSROOT/usr/bin/dbus-daemon" --config-file="$RUNDIR/dbus.conf" --fork --print-address 2>/dev/null)"
  echo "$DB" > "$RUNDIR/dbus_addr"
  [ -n "$DB" ] && log "✅ DBus 会话: $DB" || { log "❌ DBus 启动失败"; return 1; }
}

start() {
  need "$SYSROOT/usr/bin/Xvfb"
  need "$DESK/opt/fcitx5-x11/bin/fcitx5"
  build_shim; build_config; dbus_conf
  pkill -9 -x fcitx5 2>/dev/null; sleep 1
  start_xvfb || return 1
  start_dbus || return 1
  local DB; DB="$(cat "$RUNDIR/dbus_addr")"

  # 用 v0.9.84a addon（匹配 RT/lib 的 libIME 1.1.17）；并确保 xcb-imdkit SONAME 链接
  cp -f "$RT/addon/libpinyinime.so" "$DESK/opt/fcitx5-x11/lib/fcitx5/" 2>/dev/null || true
  ln -sf libxcb-imdkit.so.1.0.9 "$RT/lib/libxcb-imdkit.so.1" 2>/dev/null || true

  log "启动 fcitx5（X11 模式 / XIM / pinyinime）…"
  env HOME=/root DISPLAY="$XDISPLAY" XMODIFIERS=@im=fcitx GTK_IM_MODULE=fcitx QT_IM_MODULE=fcitx \
    XDG_CONFIG_HOME="$RUNDIR/conf" XDG_CACHE_HOME="$RUNDIR/cache" XDG_RUNTIME_DIR="$XDG" \
    DBUS_SESSION_BUS_ADDRESS="$DB" \
    XKB_CONFIG_ROOT="$SYSROOT/usr/share/X11/xkb" \
    FONTCONFIG_FILE="$RUNDIR/fonts.conf" FONTCONFIG_PATH="$RUNDIR" \
    LD_LIBRARY_PATH="$DESK/opt/deps-lib:$DESK/opt/fcitx5-x11/lib:$RT/lib" \
    PATH="$DESK/opt/fcitx5-x11/bin:$SYSROOT/usr/bin:/usr/bin:/bin" \
    XDG_DATA_DIRS="$DESK/opt/fcitx5-x11/share:$DESK/share:/usr/share" \
    FCITX_ADDON_DIRS="$DESK/opt/fcitx5-x11/lib/fcitx5" \
    FCITX_DATA_DIR="$DESK/opt/fcitx5-x11/share/fcitx5" \
    "$DESK/opt/fcitx5-x11/bin/fcitx5" -d > "$RUNDIR/fcitx5_x.log" 2>&1 &
  sleep 7
  if pgrep -x fcitx5 >/dev/null 2>&1 && grep -q "Loaded addon xim" "$RUNDIR/fcitx5_x.log"; then
    log "✅ fcitx5 运行中（X11/DBus），xim + pinyinime 已加载"
    grep -iE "Loaded addon pinyinime|Loaded addon xim|Found 1 input method\(s\) in addon pinyinime" "$RUNDIR/fcitx5_x.log" | head
  else
    log "❌ fcitx5 未就绪，日志：" ; tail -20 "$RUNDIR/fcitx5_x.log"; return 1
  fi
}

stop() {
  pkill -x fcitx5 2>/dev/null
  pkill -f "[X]vfb $XDISPLAY" 2>/dev/null
  log "已停止 fcitx5 / Xvfb $XDISPLAY"
}

status() {
  pgrep -f "[X]vfb $XDISPLAY" >/dev/null && echo "✅ Xvfb $XDISPLAY" || echo "❌ Xvfb"
  pgrep -x fcitx5 >/dev/null && echo "✅ fcitx5" || echo "❌ fcitx5"
  DISPLAY="$XDISPLAY" LD_LIBRARY_PATH="$SYSROOT/usr/lib64" "$SYSROOT/usr/bin/xdpyinfo" 2>/dev/null | head -1
}

case "${1:-start}" in
  start) start ;;
  stop) stop ;;
  status) status ;;
  *) echo "用法: XDISPLAY=:99 $0 {start|stop|status}"; exit 1 ;;
esac
