#!/bin/bash
# 功能综合测试 — 模糊音/纠错/双拼/联想/用户词典
# 用法: bash scripts/test_features.sh
cd "$(dirname "$0")/.."
source ./setup_env.sh

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"
PASS=0; FAIL=0; SKIP=0

check() {
    local desc="$1" input="$2" expected="$3"; shift 3
    local result
    result=$(echo "$input" | $CLI --quiet --top1 "$@" "$LM" "$DICT" 2>/dev/null)
    if [ "$result" = "$expected" ]; then
        printf "  ✅ %s: %s → %s\n" "$desc" "$input" "$result"
        PASS=$((PASS + 1))
    else
        printf "  ✗ %s: %s → %s (期望: %s)\n" "$desc" "$input" "$result" "$expected"
        FAIL=$((FAIL + 1))
    fi
}

skip() {
    printf "  ⊘ %s: 跳过\n" "$1"
    SKIP=$((SKIP + 1))
}

echo "=== 功能综合测试 ==="

echo ""
echo "--- 1. 模糊音 (--fuzzy) ---"
# z/zh, c/ch, s/sh, n/l, an/ang, en/eng, in/ing
check "z→zh" "zang" "章" --fuzzy
check "c→ch" "cifan" "吃饭" --fuzzy
check "s→sh" "si" "是" --fuzzy
check "n→l" "liu" "六" --fuzzy
check "an→ang" "shan" "上" --fuzzy

echo ""
echo "--- 2. 纠错 (--correct) ---"
# M3 error correction: 1-char substitution/insertion/deletion
check "替换" "senme" "什么" --correct
check "插入" "shenme" "什么" --correct
check "删除" "shenm" "什么" --correct

echo ""
echo "--- 3. 双拼 (--shuangpin) ---"
# 小鹤双拼: n=i, i=i, h=h, c=ao → nihc = nihao
check "小鹤你好" "nihc" "你好" --shuangpin
check "小鹤中国" "vsgo" "中国" --shuangpin
# 自然码双拼
check "自然你好" "nihk" "你好" --shuangpin --sp-scheme ziranma

echo ""
echo "--- 4. 联想预测 (--predict) ---"
# Predict next word after given text
result=$(echo "你好" | $CLI --predict "$LM" "$DICT" 2>/dev/null | head -1)
if [ -n "$result" ]; then
    printf "  ✅ 联想: 你好 → %s\n" "$result"
    PASS=$((PASS + 1))
else
    printf "  ✗ 联想: 你好 → (空)\n"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- 5. 用户词典 ---"
# Add, test, delete
$CLI --add-user "testfeature=测试功能" "$LM" "$DICT" 2>/dev/null
result=$(echo "testfeature" | $CLI --quiet --top1 "$LM" "$DICT" 2>/dev/null)
if [ "$result" = "测试功能" ]; then
    printf "  ✅ 添加+查询: testfeature → %s\n" "$result"
    PASS=$((PASS + 1))
else
    printf "  ✗ 添加+查询: testfeature → %s (期望: 测试功能)\n" "$result"
    FAIL=$((FAIL + 1))
fi

# List
result=$($CLI --list-user "$LM" "$DICT" 2>/dev/null | grep "testfeature")
if [ -n "$result" ]; then
    printf "  ✅ 列出: %s\n" "$result"
    PASS=$((PASS + 1))
else
    printf "  ✗ 列出: 未找到 testfeature\n"
    FAIL=$((FAIL + 1))
fi

# Delete
$CLI --del-user "testfeature" "$LM" "$DICT" 2>/dev/null
result=$($CLI --list-user "$LM" "$DICT" 2>/dev/null | grep "testfeature")
if [ -z "$result" ]; then
    printf "  ✅ 删除: testfeature 已移除\n"
    PASS=$((PASS + 1))
else
    printf "  ✗ 删除: testfeature 仍存在\n"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- 6. 多候选 ---"
result=$(echo "nihao" | $CLI --quiet "$LM" "$DICT" 5 2>/dev/null | head -1 | cut -f1)
if [ "$result" = "你好" ]; then
    printf "  ✅ 多候选首选: nihao → %s\n" "$result"
    PASS=$((PASS + 1))
else
    printf "  ✗ 多候选首选: nihao → %s (期望: 你好)\n" "$result"
    FAIL=$((FAIL + 1))
fi

echo ""
echo "--- 7. 英文直通 ---"
check "英文1" "hello" "hello"
check "英文2" "error" "error"
check "英文3" "test" "test"
check "英文4" "api" "api"

echo ""
echo "--- 8. 长句 ---"
check "长句1" "woxianzaiyaochifan" "我现在要吃饭"
check "长句2" "jintiantianqihenhaobushima" "今天天气很好不是吗"
check "长句3" "zhandiqilai" "站起来"
check "长句4" "womendouzhidaoshijieshiyigedifang" "我们都知道世界是一个地方"
check "长句5" "xueshengxuyaoxuexihenduozhishi" "学生需要学习很多知识"

echo ""
echo "========== Summary =========="
echo "Total: $((PASS + FAIL + SKIP)) | Pass: $PASS | Fail: $FAIL | Skip: $SKIP"
if [ $FAIL -eq 0 ]; then
    echo "✅ 全部通过"
else
    echo "✗ 有 $FAIL 个失败"
fi
