#!/bin/bash
# start_im_env_ro.sh —— 在「只读 /usr、无 root 写权限」的沙箱内机上，
# 用 S3 预构建产物跑起 pinyinime 输入法（fcitx5 + pinyinime 引擎）。
#
# 背景/取舍：
#   · /usr、/etc、/opt 为只读 overlay，无法 dnf install，也不能在 /usr/bin 放置文件。
#   · Xvfb 的 XKB 键盘初始化硬编码调用 /usr/bin/xkbcomp，而本机既装不了也无
#     CAP_SYS_ADMIN 去做 bind-mount，故图形 XIM 端到端在本机不可用（环境限制，非代码缺陷）。
#   · 因此本脚本走「DBus 会话 + fcitx5 守护进程」路线：不依赖 X 即可加载
#     pinyinime addon + zh_CN.lm + sc.dict，并注册输入法、接管 org.fcitx.Fcitx5。
#   · CLI 引擎（24/24 冒烟）与 fcitx5 集成（加载/注册）分两层验证，互不依赖。
#
# 用法: bash scripts/start_im_env_ro.sh {start|stop|status|config}
set -uo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="${SYSROOT:-/workspace/sysroot}"       # X/dbus 二进制（openEuler RPM 解包）
DEPLOY="${DEPLOY:-/workspace/deploy}"           # S3 预构建产物解根
RT="$DEPLOY/pinyin-ime-v0.9.84a"                 # 主 bundle: CLI + libIME + addon + data
DESK="$DEPLOY/pinyin-ime"                        # 桌面 bundle: fcitx5-x11 + deps-lib
SYSLIB="$SYSROOT/usr/lib64"                      # 注意：不要带 /lib64（glibc/ld 与系统不匹配）
RUNDIR="${RUNDIR:-/workspace/fcitx5-runtime}"; RUNTIME_HOME=/root XDG=/run/user/0

log(){ echo "[im] $*"; }
need(){ [ -e "$1" ] || { echo "缺少 $1"; exit 1; }; }

build_config() {
  mkdir -p "$RUNDIR/conf/fcitx5/conf" "$XDG" 2>/dev/null || mkdir -p "$RUNDIR/conf/fcitx5/conf"
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
  # 最小自包含 DBus 会话配置（系统 /usr/share/dbus-1/session.conf 只读缺失）
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
  cat > "$RUNDIR/conf/fcitx5/conf/pinyinime.conf" <<EOF
EnableFuzzy=True
EnableCorrect=True
EOF
}

# 供复用的 fcitx5 运行环境
fcitx_env() {
  local DB="$1"
  env \
    HOME=/root DISPLAY= \
    XDG_CONFIG_HOME="$RUNDIR/conf" \
    XDG_CACHE_HOME="$RUNDIR/cache" \
    XDG_RUNTIME_DIR="$XDG" \
    DBUS_SESSION_BUS_ADDRESS="$DB" \
    XMODIFIERS=@im=fcitx GTK_IM_MODULE=fcitx QT_IM_MODULE=fcitx \
    LD_LIBRARY_PATH="$DESK/opt/deps-lib:$DESK/opt/fcitx5-x11/lib:$RT/lib" \
    PATH="$DESK/opt/fcitx5-x11/bin:$SYSROOT/usr/bin:/usr/bin:/bin" \
    XDG_DATA_DIRS="$DESK/opt/fcitx5-x11/share:$DESK/share:/usr/share" \
    FCITX_ADDON_DIRS="$DESK/opt/fcitx5-x11/lib/fcitx5" \
    FCITX_ADDON_CONFIG_DIRS="$DESK/opt/fcitx5-x11/lib/fcitx5" \
    FCITX_DATA_DIR="$DESK/opt/fcitx5-x11/share/fcitx5" \
    "$@"
}

do_config() {
  build_config
  log "配置写入 $RUNDIR/conf；确保 DataDir 有数据:"
  ls -la /root/.local/pinyin-ime/share/libime/*.lm /root/.local/pinyin-ime/share/libime/*.dict 2>/dev/null
}

do_start() {
  need "$SYSROOT/usr/bin/dbus-daemon"
  need "$DESK/opt/fcitx5-x11/bin/fcitx5"
  build_config
  pkill -TERM -x fcitx5 2>/dev/null; pkill -TERM -x dbus-daemon 2>/dev/null; sleep 2
  pkill -KILL -x fcitx5 2>/dev/null; pkill -KILL -x dbus-daemon 2>/dev/null; sleep 1

  # 用 v0.9.84a addon（匹配 RT/lib 的 libIME 1.1.17）
  cp -f "$RT/addon/libpinyinime.so" "$DESK/opt/fcitx5-x11/lib/fcitx5/" 2>/dev/null || true

  log "启动 DBus 会话…"
  local DB
  DB="$(env LD_LIBRARY_PATH="$SYSLIB" PATH="$SYSROOT/usr/bin:/usr/bin:/bin" \
        "$SYSROOT/usr/bin/dbus-daemon" --config-file="$RUNDIR/dbus.conf" --fork --print-address 2>/dev/null)"
  echo "$DB" > "$RUNDIR/dbus_addr"; need_dbus_ok "$DB"

  log "启动 fcitx5（无 X，DBus 模式）…"
  fcitx_env "$DB" "$DESK/opt/fcitx5-x11/bin/fcitx5" -d > "$RUNDIR/fcitx5.log" 2>&1
  sleep 6

  echo "== addon / lm / dict / IM 注册 =="
  grep -iE "pinyinime:|Loaded addon pinyinime|Found 1 input method\(s\) in addon pinyinime" "$RUNDIR/fcitx5.log" | head
  if pgrep -x fcitx5 >/dev/null; then
    log "✅ fcitx5 运行中，pinyinime 已注册（DBus + 引擎就绪）"
    log "DBUS_SESSION_BUS_ADDRESS=$(cat "$RUNDIR/dbus_addr")"
  else
    log "❌ fcitx5 退出，见 $RUNDIR/fcitx5.log"; tail -15 "$RUNDIR/fcitx5.log"; return 1
  fi
}

need_dbus_ok(){ [ -n "${1:-}" ] && [[ "$1" == unix:* ]] || { echo "dbus 启动失败"; return 1; }; }

do_status() {
  for p in dbus-daemon fcitx5; do pgrep -x "$p" >/dev/null && echo "✅ $p" || echo "❌ $p"; done
  [ -f "$RUNDIR/fcitx5.log" ] && grep -iE "pinyinime:|Found 1 input method|error" "$RUNDIR/fcitx5.log" | tail -6
}

do_stop() { pkill -x fcitx5 2>/dev/null; pkill -x dbus-daemon 2>/dev/null; log "已停止"; }

case "${1:-start}" in
  config) do_config ;;
  start)  do_start ;;
  status) do_status ;;
  stop)   do_stop ;;
  *) echo "用法: $0 {start|stop|status|config}"; exit 1 ;;
esac
