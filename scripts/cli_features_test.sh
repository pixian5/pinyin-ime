#!/bin/bash
# cli_features_test.sh — CLI 功能级回归（v0.9.192）
#
# 覆盖快照测不到的东西：命令的副作用与退出码。
# 目前专门盯自定义快捷词（--phrase-add/--phrase-del/--phrase-list）——
# 这三个选项在 v0.9.191 之前只解析不执行，属于「帮助写了、功能没有」。
#
# 用法: CLI=/path/pinyin_ime_cli bash scripts/cli_features_test.sh
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="${CLI:-$ROOT/cpp/build/pinyin_ime_cli}"
[ -x "$CLI" ] || { echo "!! 找不到 CLI: $CLI"; exit 2; }
UD=$(mktemp -d); trap 'rm -rf "$UD"' EXIT
PASS=0; FAIL=0
ok(){ PASS=$((PASS+1)); echo "  ✅ $*"; }
ng(){ FAIL=$((FAIL+1)); echo "  ❌ $*"; }

OUT=$("$CLI" --phrase-add addr=北京市海淀区中关村 --user-data "$UD" 2>&1)
grep -q '已添加 addr=' <<<"$OUT" && ok "phrase-add 输出确认" || ng "phrase-add 无确认输出: $OUT"
grep -qx 'addr=北京市海淀区中关村' "$UD/custom_phrases.txt" && ok "phrase-add 落盘格式正确" || ng "custom_phrases.txt 内容不对"

OUT=$("$CLI" --phrase-list --user-data "$UD" 2>&1)
grep -q 'addr=北京市海淀区中关村' <<<"$OUT" && ok "phrase-list 能列出新词" || ng "phrase-list 未列出: $OUT"

OUT=$("$CLI" --phrase-add addr=上海市浦东新区 --user-data "$UD" 2>&1)
grep -q '已更新 addr=' <<<"$OUT" && ok "重复缩写走更新" || ng "重复缩写未更新"
grep -c '^addr=' "$UD/custom_phrases.txt" | grep -qx 1 && ok "更新后文件只有一条 addr" || ng "更新产生重复条目"

OUT=$("$CLI" --phrase-del addr --user-data "$UD" 2>&1)
grep -q '已删除 addr' <<<"$OUT" && ok "phrase-del 删除" || ng "phrase-del 未删: $OUT"
grep -q '^addr=' "$UD/custom_phrases.txt" && ng "删除后仍有 addr" || ok "删除后文件无 addr"

"$CLI" --phrase-add broken --user-data "$UD" >/tmp/cf_out.log 2>&1
[ $? -eq 1 ] && ok "非法格式退出码 1" || ng "非法格式未拒绝"

OUT=$("$CLI" --phrase-del notexist --user-data "$UD" 2>&1)
grep -q '没有找到' <<<"$OUT" && ok "删除不存在的词给出提示" || ng "删除不存在无提示"

echo "===== CLI 功能回归：通过 $PASS / 失败 $FAIL ====="
[ "$FAIL" -eq 0 ] || exit 1
