#!/bin/bash
# e2e_run.sh —— 通过 fcitx5 的 DBus InputContext 协议驱动真实按键链路
# （= GTK/Qt 输入法模块底层的同一条路），验证"按键→预编辑→候选→上屏"。
# 依赖 fcitx5 已在 DBus 会话运行（scripts/start_im_env_ro.sh start）。
set -uo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
RUNDIR="${RUNDIR:-/workspace/fcitx5-runtime}"

if ! pgrep -x fcitx5 >/dev/null; then
  echo "[e2e] fcitx5 未运行，先启动…"; bash "$REPO_DIR/scripts/start_im_env_ro.sh" start >/dev/null 2>&1; sleep 2
fi
DB="$(cat "$RUNDIR/dbus_addr" 2>/dev/null)"
[ -n "$DB" ] || { echo "缺少 $RUNDIR/dbus_addr，请先 start_im_env_ro.sh"; exit 1; }

env \
  DBUS_SESSION_BUS_ADDRESS="$DB" \
  LD_LIBRARY_PATH="/workspace/sysroot/usr/lib64:/workspace/deploy/pinyin-ime/opt/deps-lib:/workspace/deploy/pinyin-ime/opt/fcitx5-x11/lib" \
  PATH="/workspace/sysroot/usr/bin:$PATH" \
  /usr/bin/python3 "$REPO_DIR/scripts/e2e_input_test.py"
