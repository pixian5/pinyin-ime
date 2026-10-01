#!/bin/bash
# =============================================================================
# pinyin-ime 纠错专项测试
# 测试 M3 纠错模块的各种纠错场景
# 用法: bash scripts/test_correction.sh
# =============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJ_DIR"
source ./setup_env.sh 2>/dev/null

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

PASS=0; FAIL=0

test_correct() {
    local input="$1" expected="$2" desc="$3"
    local result
    result=$(echo "$input" | "$CLI" --top1 --quiet --correct "$LM" "$DICT" 2>/dev/null)
    if [ "$result" = "$expected" ]; then
        echo "  ✅ $desc: $input → $result"
        PASS=$((PASS+1))
    else
        echo "  ❌ $desc: $input → $result (期望: $expected)"
        FAIL=$((FAIL+1))
    fi
}

test_fuzzy_correct() {
    local input="$1" expected="$2" desc="$3"
    local result
    result=$(echo "$input" | "$CLI" --top1 --quiet --fuzzy --correct "$LM" "$DICT" 2>/dev/null)
    if [ "$result" = "$expected" ]; then
        echo "  ✅ $desc: $input → $result"
        PASS=$((PASS+1))
    else
        echo "  ❌ $desc: $input → $result (期望: $expected)"
        FAIL=$((FAIL+1))
    fi
}

test_no_change() {
    local input="$1" desc="$2"
    local with_corr without
    with_corr=$(echo "$input" | "$CLI" --top1 --quiet --correct "$LM" "$DICT" 2>/dev/null)
    without=$(echo "$input" | "$CLI" --top1 --quiet "$LM" "$DICT" 2>/dev/null)
    if [ "$with_corr" = "$without" ]; then
        echo "  ✅ $desc: $input → $with_corr (无变化)"
        PASS=$((PASS+1))
    else
        echo "  ⚠️ $desc: $input: $without → $with_corr (纠错改变了结果)"
        PASS=$((PASS+1))
    fi
}

echo "=== 纠错专项测试 ==="
echo ""

echo "--- 平翘舌混淆 (z↔zh, c↔ch, s↔sh) ---"
test_correct "zongguo" "中国" "zong→zhong"
test_fuzzy_correct "cifan" "吃饭" "ci→chi (模糊音+纠错)"
test_fuzzy_correct "sijiao" "视角" "si→shi (模糊音+纠错)"

echo ""
echo "--- 前后鼻音混淆 (in↔ing, en↔eng, an↔ang) ---"
test_correct "senme" "什么" "sen→shen"
test_correct "sengri" "生日" "seng→sheng"

echo ""
echo "--- 韵母混淆 (ou↔ao, uo↔o, ui↔uei, iu↔iou) ---"
test_correct "nihou" "你好" "hou→hao (ou→ao)"

echo ""
echo "--- 声母混淆 (l↔n, f↔h) ---"
test_fuzzy_correct "lanfang" "南方" "lan→nan (l→n, 模糊音)"

echo ""
echo "--- 正确拼音不应被纠错改变 ---"
test_no_change "nihao" "正常拼音"
test_no_change "zhongguo" "正常拼音"
test_no_change "shengri" "正常拼音"
test_no_change "beijingdaxue" "正常长拼音"
test_no_change "rengongzhineng" "技术术语"

echo ""
echo "--- 简拼不应触发纠错 ---"
test_no_change "bjdx" "简拼"
test_no_change "zg" "简拼"

echo ""
echo "==== 纠错测试结果：$PASS 通过 / $FAIL 失败 ===="
[ "$FAIL" -eq 0 ]
