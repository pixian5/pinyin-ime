#!/bin/bash
# repro_verify.sh —— 一层层验证 pinyin-ime 输入法在本机「真正能跑」。
#   第 1 层：CLI 引擎冒烟（24/24：全拼/简拼/模糊音/双拼/CLI 功能）
#   第 2 层：安装 /usr/local/bin/pinyin-ime 命令行入口
#   第 3 层：fcitx5 加载 pinyinime addon + zh_CN.lm + sc.dict + 注册 IM
# 用法: bash scripts/repro_verify.sh
set -uo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPLOY="${DEPLOY:-/workspace/deploy}"
RT="$DEPLOY/pinyin-ime-v0.9.84a"
PASS=0; FAIL=0
ok(){ echo "  ✅ $*"; PASS=$((PASS+1)); }
ng(){ echo "  ❌ $*"; FAIL=$((FAIL+1)); }

echo "==================== 第 1 层：CLI 引擎冒烟 ===================="
if SMOKE_UD=/root/repro_ud NB="$REPO_DIR/native-build" bash "$REPO_DIR/scripts/smoke_test.sh" >/tmp/repro_smoke.log 2>&1; then
  tail -1 /tmp/repro_smoke.log; ok "CLI 冒烟 $(grep -c PASS /tmp/repro_smoke.log) 通过"
else
  tail -3 /tmp/repro_smoke.log; ng "CLI 冒烟未全过"
fi

echo "==================== 第 2 层：pinyin-ime 命令 ===================="
out="$(command -v pinyin-ime && echo nihao | pinyin-ime --top1 2>/dev/null)"
[ "$(echo "$out" | tail -1)" = "你好" ] && ok "pinyin-ime nihao → 你好" || ng "pinyin-ime 命令异常: $out"

echo "==================== 第 3 层：fcitx5 + pinyinime 集成 ===================="
bash "$REPO_DIR/scripts/start_im_env_ro.sh" start >/tmp/repro_im.log 2>&1
sleep 1
LOGF="/workspace/fcitx5-runtime/fcitx5.log"
grep -q "pinyinime: loaded lm=" "$LOGF" 2>/dev/null && ok "addon 加载 LM+dict" || ng "未加载 LM/dict"
grep -q "Loaded addon pinyinime" "$LOGF" 2>/dev/null && ok "Loaded addon pinyinime" || ng "pinyinime addon 未加载"
grep -q "Found 1 input method(s) in addon pinyinime" "$LOGF" 2>/dev/null && ok "注册输入法 pinyinime" || ng "未注册输入法"
DB="$(cat /workspace/fcitx5-runtime/dbus_addr 2>/dev/null)"
if [ -n "$DB" ]; then
  names="$(DBUS_SESSION_BUS_ADDRESS="$DB" LD_LIBRARY_PATH=/workspace/sysroot/usr/lib64 /workspace/sysroot/usr/bin/gdbus call --session --dest org.freedesktop.DBus --object-path /org/freedesktop/DBus --method org.freedesktop.DBus.ListNames 2>/dev/null)"
  if echo "$names" | grep -q org.fcitx.Fcitx5; then ok "org.fcitx.Fcitx5 在线(DBus)"; else ng "fcitx5 未接管 DBus"; fi
fi

echo "================================================================"
echo "结果：$PASS 通过 / $FAIL 失败"
[ "$FAIL" -eq 0 ]
