#!/bin/bash
# release_check.sh — 发布前门禁（v0.9.191）
#
# 把「发版前要人工确认的六处一致性」变成一条命令：
#   VERSION ↔ CLI 二进制 / addon 配置 / CHANGELOG / README / 文档索引 / 准确率基线
# 再加静态检查、单元测试、CLI 快照。任何一项失败即拒绝通过。
#
# 用法:
#   bash scripts/release_check.sh            # 常规门禁（不需要编译产物齐全）
#   bash scripts/release_check.sh --full     # 追加：安装矩阵 + chroot 准确率评测
#
# 退出码: 0=可发布, 1=有阻断项
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
VERSION=$(cat VERSION)
FULL=0
[ "${1:-}" = "--full" ] && FULL=1

PASS=0; FAIL=0; WARN=0; BLOCKERS=()
ok(){ PASS=$((PASS+1)); echo "  ✅ $*"; }
ng(){ FAIL=$((FAIL+1)); BLOCKERS+=("$*"); echo "  ❌ $*"; }
warn(){ WARN=$((WARN+1)); echo "  ⚠ $*"; }
# check 名字 命令...；命令必须自行返回 0/非 0
check(){ local name="$1"; shift; if "$@" >/tmp/rc_out.log 2>&1; then ok "$name"; else ng "$name"; sed -n '1,4p' /tmp/rc_out.log | sed 's/^/     /'; fi; }

echo "=== 版本一致性（VERSION=$VERSION）==="
check "VERSION 格式为 x.y.z" bash -c "echo '$VERSION' | grep -Eq '^[0-9]+\\.[0-9]+\\.[0-9]+$'"
check "CHANGELOG 含 v$VERSION 小节" grep -q "^## v$VERSION" CHANGELOG.md
check "README 声明当前源码版本=$VERSION" grep -q "\`$VERSION\`" README.md
check "文档索引含 v$VERSION 条目" grep -q "v$VERSION" 文档索引.md
if [ -x cpp/build/pinyin_ime_cli ]; then
    check "CLI 二进制版本=$VERSION" bash -c "[ \"\$(cpp/build/pinyin_ime_cli --version 2>/dev/null)\" = \"pinyin_ime_cli $VERSION\" ]"
else
    warn "未找到 cpp/build/pinyin_ime_cli，跳过二进制版本校验"
fi
if [ -f fcitx5-addon/build/pinyinime.conf ]; then
    check "addon 配置版本=$VERSION" grep -qx "Version=$VERSION" fcitx5-addon/build/pinyinime.conf
else
    warn "未找到 fcitx5-addon/build/pinyinime.conf，跳过 addon 版本校验"
fi
base_ver=$(python3 -c "import json;print(json.load(open('bench/baseline.json'))['version'])" 2>/dev/null || echo "")
if [ -n "$base_ver" ]; then
    if [ "$base_ver" = "$VERSION" ]; then ok "准确率基线版本=$VERSION"; else warn "基线版本为 $base_ver（未随版本前移，确认是否有意）"; fi
else
    warn "读不到 bench/baseline.json 的 version"
fi

echo "=== 静态检查 ==="
check "ci_static.sh" bash scripts/ci_static.sh

echo "=== 单元测试 ==="
CXX="${CXX:-g++}"
if command -v "$CXX" >/dev/null 2>&1; then
    if "$CXX" -std=c++20 -O1 -Icpp cpp/quick_input_test.cpp -o /tmp/rc_qt >/tmp/rc_qt.log 2>&1; then
        if /tmp/rc_qt >/tmp/rc_qt_run.log 2>&1; then ok "quick_input_test 全通过（$(grep -o '[0-9]* passed' /tmp/rc_qt_run.log | head -1)）"; else ng "quick_input_test 有失败"; tail -3 /tmp/rc_qt_run.log | sed 's/^/     /'; fi
    else
        ng "quick_input_test 编译失败"; sed -n 1,3p /tmp/rc_qt.log | sed 's/^/     /'
    fi
else
    warn "无 $CXX，跳过单元测试"
fi

echo "=== 编译警告 ==="
check "无 -Wall -Wextra 警告（单测）" bash scripts/warnings_check.sh

echo "=== CLI 功能（快捷词）==="
if [ -x cpp/build/pinyin_ime_cli ]; then
    check "phrase-add/del/list 行为" bash scripts/cli_features_test.sh
else
    warn "缺 CLI，跳过快捷词行为测试"
fi

echo "=== 覆盖率 / Sanitizer（单测）==="
check "单测覆盖率 ≥90%" bash scripts/coverage.sh --unit
check "ASan/UBSan 单测" bash scripts/sanitizer_test.sh

echo "=== CLI 快照 ==="
if [ -x cpp/build/pinyin_ime_cli ] && [ -f /usr/share/libime/zh_CN.lm ]; then
    check "快照逐条一致" bash scripts/cli_snapshot_test.sh
else
    warn "缺 CLI 或系统 LM，跳过快照测试"
fi

if [ "$FULL" = 1 ]; then
    echo "=== 安装矩阵 ==="
    check "三条安装路径" bash scripts/test_install.sh --skip-build
    echo "=== 准确率评测 ==="
    check "准确率不低于基线" bash scripts/eval_chroot.sh
fi

echo
echo "===== 发布门禁：通过 $PASS / 阻断 $FAIL / 提醒 $WARN ====="
if [ "$FAIL" -gt 0 ]; then
    printf '阻断项:\n'; printf '  - %s\n' "${BLOCKERS[@]}"
    exit 1
fi
echo "可以发布 ✅"
