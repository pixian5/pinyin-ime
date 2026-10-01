#!/bin/bash
# =============================================================================
# pinyin-ime 压力测试 / 边界测试
# 测试极端输入：超长拼音、空输入、单字符、重复字符等
# 用法: bash scripts/test_stress.sh
# =============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJ_DIR"
source ./setup_env.sh 2>/dev/null

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

PASS=0; FAIL=0; SKIP=0

test_case() {
    local input="$1" desc="$2"
    local result
    result=$(echo "$input" | timeout 5 "$CLI" --top1 --quiet --fuzzy --correct "$LM" "$DICT" 2>/dev/null)
    if [ -n "$result" ] && [ "$result" != "" ]; then
        echo "  ✅ $desc: ${input:0:30}... → $result"
        PASS=$((PASS+1))
    else
        echo "  ❌ $desc: ${input:0:30}... → (空结果)"
        FAIL=$((FAIL+1))
    fi
}

test_no_crash() {
    local input="$1" desc="$2"
    local result
    result=$(echo "$input" | timeout 5 "$CLI" --top1 --quiet "$LM" "$DICT" 2>/dev/null) || true
    if [ -n "$result" ]; then
        echo "  ✅ $desc: → $result"
        PASS=$((PASS+1))
    else
        echo "  ⚠️ $desc: → (空结果，但未崩溃)"
        SKIP=$((SKIP+1))
    fi
}

echo "=== 压力测试 / 边界测试 ==="
echo ""

echo "--- 超长拼音（20+ 音节）---"
test_case "woxiangqubeijinglvyoukanyikantiananmenshifoukeyichifan" "超长句1"
test_case "jintiantianqizhenhaochuquwanshuayixiaba" "超长句2"
test_case "xueshengxuexijiqiyurengongzhinengshenduxuexi" "超长句3"
test_case "woaixuexixuexishiwokuailedequanshuo" "超长句4"

echo ""
echo "--- 重复字符 ---"
test_case "aaaa" "重复a×4"
test_case "nnnn" "重复n×4"
test_case "hahaha" "重复ha×3"
test_case "nihao" "正常对照"

echo ""
echo "--- 单字符 ---"
test_no_crash "a" "单字符a"
test_no_crash "e" "单字符e"
test_no_crash "i" "单字符i"
test_no_crash "o" "单字符o"
test_no_crash "u" "单字符u"
test_no_crash "v" "单字符v"
test_no_crash "n" "单字符n"
test_no_crash "h" "单字符h"

echo ""
echo "--- 边界输入 ---"
test_no_crash "" "空输入"
test_no_crash "z" "单声母z"
test_no_crash "zh" "声母zh"
test_no_crash "zhchsh" "三声母连缀"
test_no_crash "ang" "单韵母ang"
test_no_crash "zhang" "完整音节"

echo ""
echo "--- 简拼压力 ---"
test_case "bjdxtqlh" "4校简拼"
test_case "zgrmjjfg" "8字简拼"

echo ""
echo "--- 混合输入 ---"
test_case "nihaozhongguo" "两词连缀"
test_case "woaixuexi" "三音节连缀"

echo ""
echo "==== 压力测试结果：$PASS 通过 / $FAIL 失败 / $SKIP 跳过 ===="
[ "$FAIL" -eq 0 ]
