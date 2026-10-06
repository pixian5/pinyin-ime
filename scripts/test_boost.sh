#!/bin/bash
# boost 机制专项测试 — 验证完整匹配/后缀/前缀/用户词典/英文直通
# 用法: bash scripts/test_boost.sh

cd "$(dirname "$0")/.."
source ./setup_env.sh

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"
PASS=0; FAIL=0

check() {
    local desc="$1" input="$2" expected="$3"
    local result
    result=$(echo "$input" | $CLI --quiet --top1 "$LM" "$DICT")
    if [ "$result" = "$expected" ]; then
        printf "  ✅ %s: %s → %s\n" "$desc" "$input" "$result"
        PASS=$((PASS + 1))
    else
        printf "  ✗ %s: %s → %s (期望: %s)\n" "$desc" "$input" "$result" "$expected"
        FAIL=$((FAIL + 1))
    fi
}

echo "=== boost 机制专项测试 ==="

echo ""
echo "--- 1. 完整匹配 boost ---"
check "城市" "shanghaishi" "上海市"
check "长句" "woxiangdingyigefangjian" "我想订一个房间"
check "消歧" "mingtianjian" "明天见"
check "食物" "zhongguofan" "中国饭"
check "食物2" "xifan" "稀饭"
check "食物3" "zuofan" "做饭"
check "懂得" "dongdeqi" "懂得起"

echo ""
echo "--- 2. 后缀 boost ---"
# 输入以预设 key 结尾时，boost 包含 value 的候选
check "后缀1" "woxiangshanghaishi" "我想上海市"
check "后缀2" "mingtianjianwo" "明天见我"

echo ""
echo "--- 3. 前缀 boost ---"
# 输入以预设 key 开头时，boost 包含 value 的候选
check "前缀1" "woxiangqubeijing" "我想去北京"
check "前缀2" "woxihuanzhege" "我喜欢这个"
check "前缀3" "womendouzhidao" "我们都知道"

echo ""
echo "--- 4. 简拼 boost ---"
check "简拼zg" "zg" "中国"
check "简拼hz" "hz" "孩子"
check "简拼sz" "sz" "深圳"
check "简拼js" "js" "技术"

echo ""
echo "--- 5. 英文直通 ---"
check "英文ai" "ai" "ai"
check "英文bug" "bug" "bug"
check "英文nlp" "nlp" "nlp"

echo ""
echo "--- 6. 常见短语 ---"
check "你好" "nihao" "你好"
check "中国" "zhongguo" "中国"
check "北京大学" "beijingdaxue" "北京大学"
check "我是中国人" "woshizhongguoren" "我是中国人"
check "我喜欢编程" "woxihuanbiancheng" "我喜欢编程"
check "我爱你" "woaini" "我爱你"

echo ""
echo "--- 7. 长句消歧 ---"
check "长句1" "zhongguoshiyigedaguo" "中国是一个大国"
check "长句2" "beijingshishoudu" "北京是首都"
check "长句3" "woyouyigewenti" "我有一个问题"
check "长句4" "woxianzaihenmang" "我现在很忙"
check "长句5" "tamenbuzhidaozhejianshi" "他们不知道这件事"

echo ""
echo "========== Summary =========="
echo "Total: $((PASS + FAIL)) | Pass: $PASS | Fail: $FAIL"
if [ $FAIL -eq 0 ]; then
    echo "✅ 全部通过"
else
    echo "✗ 有 $FAIL 个失败"
    exit 1
fi
