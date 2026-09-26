#!/usr/bin/env bash
# 「能打字」自动回归：行模式喂音 → 断言 top1 首选是否命中。
# 覆盖全拼 / 简拼 / 模糊音三档，任一 FAIL 返回非零。
#
# 用法：bash scripts/smoke_test.sh
set -uo pipefail

NB="${NB:-/workspace/native-build}"
OPT="$NB/opt"
ENV="$NB/mamba-env"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="$REPO_DIR/cpp/build/pinyin_ime_cli"
LM="$OPT/libime/lib/libime/zh_CN.lm"
DICT="$OPT/libime/share/libime/sc.dict"

export LD_LIBRARY_PATH="$OPT/libime/lib:$OPT/fcitx5/lib:$ENV/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

[ -x "$CLI" ] || { echo "SKIP: 未找到 $CLI（请先构建）"; exit 1; }
[ -f "$LM" ] || { echo "SKIP: 未找到语言模型 $LM"; exit 1; }
[ -f "$DICT" ] || { echo "SKIP: 未找到词典 $DICT"; exit 1; }

# 每行「拼音|期望首选」
QUANPIN="nihao|你好
zhongguo|中国
woshiyige|我是一个
beijingdaxue|北京大学
zhongguoruanjian|中国软件"

JIANPIN="bjdx|北京大学
zg|中国
js|技术"

FUZZY="cifan|吃饭
senme|什么
zeli|这里
lanfang|南方"

pass=0
fail=0

run_case() {
    local pin="$1" want="$2" fuzzy="${3:-0}"
    local -a extra=()
    [ "$fuzzy" = "1" ] && extra=(--fuzzy)
    local top
    top="$(printf '%s\n' "$pin" | "$CLI" "${extra[@]}" "$LM" "$DICT" 5 | cut -f1)"
    if [ "$top" = "$want" ]; then
        printf '  PASS  %-20s -> %s\n' "$pin" "$top"
        pass=$((pass + 1))
    else
        printf '  FAIL  %-20s -> 「%s」期望「%s」\n' "$pin" "$top" "$want"
        fail=$((fail + 1))
    fi
}

echo "== 全拼 =="
while IFS='|' read -r p w; do [ -n "$p" ] && run_case "$p" "$w" 0; done <<< "$QUANPIN"
echo "== 简拼 =="
while IFS='|' read -r p w; do [ -n "$p" ] && run_case "$p" "$w" 0; done <<< "$JIANPIN"
echo "== 模糊音 =="
while IFS='|' read -r p w; do [ -n "$p" ] && run_case "$p" "$w" 1; done <<< "$FUZZY"

echo "==== 结果：$pass 通过 / $fail 失败 ===="
[ "$fail" -eq 0 ]