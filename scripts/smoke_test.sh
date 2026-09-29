#!/usr/bin/env bash
# 「能打字」自动回归：行模式喂音 → 断言 top1 首选是否命中。
# 覆盖全拼 / 简拼 / 模糊音 / 双拼（小鹤）四档，任一 FAIL 返回非零。
#
# 用法：bash scripts/smoke_test.sh
set -uo pipefail

NB="${NB:-/root/workspace/native-build}"
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

# 每行「拼音|期望首选」；额外 flag 走 run_case 调用参数
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

SHUANGPIN="nihc|你好
vsgo|中国
bwjkdaxt|北京大学
ufme|什么
veli|这里
iifj|吃饭
jiuu|技术
wouiyige|我是一个"

# 隔离用户数据目录：测试之间不得互相 learn() 污染默认目录（否则候选排序漂移导致烟测不稳）。
UD="${SMOKE_UD:-$(mktemp -d "${TMPDIR:-/tmp}/pinyin-ime-smoke-ud.XXXXXX")}"
trap 'rm -rf "$UD"' EXIT

pass=0
fail=0

# run_case <拼音> <期望首选> [flags...]
run_case() {
    local pin="$1" want="$2"; shift 2
    local top
    top="$(printf '%s\n' "$pin" | "$CLI" --user-data "$UD" "$@" "$LM" "$DICT" 5 2>/dev/null | cut -f1)"
    if [ "$top" = "$want" ]; then
        printf '  PASS  %-20s -> %s\n' "$pin" "$top"
        pass=$((pass + 1))
    else
        printf '  FAIL  %-20s -> 「%s」期望「%s」\n' "$pin" "$top" "$want"
        fail=$((fail + 1))
    fi
}

echo "== 全拼 =="
while IFS='|' read -r p w; do [ -n "$p" ] && run_case "$p" "$w"; done <<< "$QUANPIN"
echo "== 简拼 =="
while IFS='|' read -r p w; do [ -n "$p" ] && run_case "$p" "$w"; done <<< "$JIANPIN"
echo "== 模糊音 =="
while IFS='|' read -r p w; do [ -n "$p" ] && run_case "$p" "$w" --fuzzy; done <<< "$FUZZY"
echo "== 双拼（小鹤）=="
while IFS='|' read -r p w; do [ -n "$p" ] && run_case "$p" "$w" --shuangpin; done <<< "$SHUANGPIN"

echo "==== 结果：$pass 通过 / $fail 失败 ===="
[ "$fail" -eq 0 ]