#!/usr/bin/env bash
# 一步式 XIM 实测：起 fcitx5（若未起）→ 用 xim_test_client 发脚本 → 打印客户端与 fcitx5 日志
# 用法: bash scripts/xvfb_smoke.sh '<按键脚本>' [附加 nihao/zhongguo 断言]
set -uo pipefail
NB="${NB:-/root/workspace/native-build}"
cd "$(dirname "${BASH_SOURCE[0]}")/.."

bash scripts/xvfb_test_env.sh start > /tmp/xvfb-env.out 2>&1
tail -2 /tmp/xvfb-env.out

SCRIPT="${1:-'nihao\\s'}"
export DISPLAY=:99 XMODIFIERS=@im=fcitx LANG=zh_CN.utf8 LC_ALL=zh_CN.utf8
export LD_LIBRARY_PATH="$NB/opt/fcitx5-x11/lib:$NB/opt/libime/lib:$NB/mamba-env/lib"

echo "=== 客户端发送: $SCRIPT ==="
./tools/xim_test_client "$SCRIPT" 2>/tmp/xim-err.log | tee /tmp/xim-out.log
echo "--- stderr ---"
tail -5 /tmp/xim-err.log
echo "=== fcitx5 存活? ==="
ps -eo comm | grep -c '^fcitx5'