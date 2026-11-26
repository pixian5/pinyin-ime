#!/bin/bash
# cli_snapshot_test.sh — CLI 行为快照测试（v0.9.191）
#
# 为什么需要：721 条单测都在纯函数层，CLI 壳层（消歧、v11 捷径、批量路径）
# 只能靠 holdout 总量准确率兜；掉 0.1pp 根本看不出是哪类输入退化。
# 这里用固定输入逐条比对 top1，任何壳层行为变化都会落到具体 case 上。
#
# 用法:
#   bash scripts/cli_snapshot_test.sh                 # 校验（默认 cpp/build/pinyin_ime_cli）
#   bash scripts/cli_snapshot_test.sh --update        # 按当前实现重写期望值
#   CLI=/path/pinyin_ime_cli LM=/usr/share/libime/zh_CN.lm DICT=/usr/share/libime/sc.dict \
#       bash scripts/cli_snapshot_test.sh
#
# 说明:
#   - 每次运行都用全新的临时 user-data 目录，避免自学习污染结果；
#   - 期望值与环境强绑定（LM/dict 版本），文件头记录 LM/dict 的 md5，
#     不一致时给出「数据变了，不一定是代码回归」的提示；
#   - 退出码: 0=全部一致, 1=有差异, 2=环境缺失。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CASES="$ROOT/bench/snapshots/cli_cases.txt"
EXPECTED="$ROOT/bench/snapshots/cli_expected.tsv"
CLI="${CLI:-$ROOT/cpp/build/pinyin_ime_cli}"
MODE="${1:-check}"

[ -f "$CASES" ] || { echo "!! 缺用例文件 $CASES"; exit 2; }
[ -x "$CLI" ] || { echo "!! 找不到可执行 CLI: $CLI（先编译或传 CLI=）"; exit 2; }

# 数据路径：显式参数优先，否则交给 CLI 自己搜索系统默认路径
ARGS=(--batch "$CASES" --top1 --quiet --user-data "$(mktemp -d)")
[ -n "${LM:-}" ] && ARGS+=(--lm "$LM")
[ -n "${DICT:-}" ] && ARGS+=(--dict "$DICT")

OUT="$(mktemp)"
trap 'rm -f "$OUT"' EXIT
if ! "$CLI" "${ARGS[@]}" > "$OUT" 2>/tmp/snap_err.log; then
    echo "!! CLI 执行失败："; tail -5 /tmp/snap_err.log; exit 2
fi

n_cases=$(grep -cve '^\s*$' "$CASES")
n_out=$(awk 'END{print NR}' "$OUT")
if [ "$n_out" -ne "$n_cases" ]; then
    echo "!! 输出行数 $n_out 与用例行数 $n_cases 不一致"; exit 1
fi

lm_md5=$(md5sum "${LM:-/usr/share/libime/zh_CN.lm}" 2>/dev/null | cut -d' ' -f1)
dict_md5=$(md5sum "${DICT:-/usr/share/libime/sc.dict}" 2>/dev/null | cut -d' ' -f1)
version=$(cat "$ROOT/VERSION")

if [ "$MODE" = "--update" ]; then
    {
        echo "# cli snapshot v$version"
        echo "# lm_md5=$lm_md5 dict_md5=$dict_md5"
        paste -d'\t' "$CASES" "$OUT"
    } > "$EXPECTED"
    echo "已更新 $EXPECTED（$n_cases 例，LM=$lm_md5）"
    exit 0
fi

[ -f "$EXPECTED" ] || { echo "!! 缺期望文件 $EXPECTED，先跑 --update"; exit 2; }

# 数据版本提示（不算失败，只提示）
head_dlm=$(grep -m1 '^# lm_md5=' "$EXPECTED" | sed 's/# lm_md5=\([^ ]*\).*/\1/')
if [ -n "$head_dlm" ] && [ -n "$lm_md5" ] && [ "$head_dlm" != "$lm_md5" ]; then
    echo "⚠ 快照记录的语言模型 md5 ($head_dlm) 与当前 ($lm_md5) 不同："
    echo "  这属于数据变化，可能整片 diff；确认是数据升级后再 --update。"
fi

# 用 python 做 UTF-8 逐行比对（bash 对多字节不友好）
python3 - "$CASES" "$OUT" "$EXPECTED" <<'PY'
import sys
cases = [l.rstrip('\n') for l in open(sys.argv[1], encoding='utf-8') if l.strip()]
got = [l.rstrip('\n') for l in open(sys.argv[2], encoding='utf-8')]
exp = {}
for l in open(sys.argv[3], encoding='utf-8'):
    if l.startswith('#') or not l.strip():
        continue
    k, _, v = l.rstrip('\n').partition('\t')
    exp[k] = v
fails = 0
for i, c in enumerate(cases):
    g = got[i] if i < len(got) else '<缺行>'
    e = exp.get(c, '<无期望>')
    if g != e:
        fails += 1
        if fails <= 10:
            print(f"  ❌ {c!r}: 期望 {e!r} 实际 {g!r}")
print(f"共 {len(cases)} 例，差异 {fails} 例")
sys.exit(1 if fails else 0)
PY
rc=$?
if [ "$rc" -eq 0 ]; then
    echo "✅ 快照全部一致（$n_cases 例）"
else
    echo "❌ 快照存在差异（确认是有意变更后再 --update）"
fi
exit "$rc"
