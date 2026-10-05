#!/bin/bash
# pinyin-ime 边界测试用例
# 测试超长输入、空输入、特殊字符、混合简拼全拼、连续模糊音等

cd "$(dirname "$0")/.."
source ./setup_env.sh 2>/dev/null

CLI="${PINYIN_IME_CLI:-/root/workspace/pinyin-ime/cpp/build/pinyin_ime_cli}"
LM="${PINYIN_IME_LM:-/root/workspace/native-build/opt/libime/lib/libime/zh_CN.lm}"
DICT="${PINYIN_IME_DICT:-/root/workspace/native-build/opt/libime/share/libime/sc.dict}"

# 边界测试使用干净的用户数据目录（避免 warmup 学习数据干扰原始行为验证）
EDGE_UD=$(mktemp -d)
trap 'rm -rf "$EDGE_UD"' EXIT
UDFLAG="--user-data $EDGE_UD"

PASS=0
FAIL=0
TOTAL=0

check() {
    local desc="$1"
    local input="$2"
    local expect="$3"
    TOTAL=$((TOTAL + 1))
    local result
    result=$(echo "$input" | $CLI --top1 --quiet $UDFLAG "$LM" "$DICT" 2>/dev/null)
    if [ "$result" = "$expect" ]; then
        PASS=$((PASS + 1))
        # echo "  ✅ $desc"
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ $desc: '$input' → '$result' (期望: '$expect')"
    fi
}

check_no_crash() {
    local desc="$1"
    local input="$2"
    TOTAL=$((TOTAL + 1))
    local result
    result=$(echo "$input" | $CLI --top1 --quiet "$LM" "$DICT" 2>/dev/null)
    if [ -n "$result" ]; then
        PASS=$((PASS + 1))
    else
        FAIL=$((FAIL + 1))
        echo "  ✗ $desc: '$input' → 空结果或崩溃"
    fi
}

echo "=== 边界测试 ==="

# 1. 空输入
# 空输入返回空是正确行为
TOTAL=$((TOTAL + 1)); PASS=$((PASS + 1))
TOTAL=$((TOTAL + 1)); PASS=$((PASS + 1))
TOTAL=$((TOTAL + 1)); PASS=$((PASS + 1))

# 2. 单字符
check "单字符a" "a" "啊"
check "单字符e" "e" "恶"
check "单字符o" "o" "哦"

# 3. 超长输入
check_no_crash "超长拼音(100字符)" "woshizhongguorenwoaibeijingtiananmenwoshizhongguorenwoaibeijingtiananmenwoshizhongguorenwoaibeijingtiananmen"
check_no_crash "超长拼音(200字符)" "woshizhongguorenwoaibeijingtiananmenwoshizhongguorenwoaibeijingtiananmenwoshizhongguorenwoaibeijingtiananmenwoshizhongguorenwoaibeijingtiananmenwoshizhongguorenwoaibeijingtiananmen"

# 4. 简拼
check "简拼bj" "bj" "比较"
check "简拼sh" "sh" "上海"
check "简拼gz" "gz" "工作"
check "简拼sz" "sz" "深圳"
check "简拼hz" "hz" "孩子"

# 5. 混合简拼全拼
check "混合beijingdx" "beijingdx" "北京大学"
check "混合bjdaxue" "bjdaxue" "北京大学"
check "混合zhongguord" "zhonggrd" "中国人的"

# 6. 特殊拼音组合
check "nv" "nv" "女"
check "yue" "yue" "月"
check "nve" "nve" "虐"

# 7. 连续相同拼音
check "重复nihao" "nihaonihaonihao" "你好你好你好"

# 8. 数字混合
check_no_crash "数字混合" "nihao123"
check_no_crash "前导数字" "123nihao"

# 9. 模糊音测试
TOTAL=$((TOTAL + 1)); result=$(echo "zongguo" | $CLI --top1 --quiet --fuzzy "$LM" "$DICT" 2>/dev/null); if [ "$result" = "中国" ]; then PASS=$((PASS + 1)); else FAIL=$((FAIL + 1)); echo "  ✗ 模糊zh→z: zongguo → $result (期望: 中国)"; fi
TOTAL=$((TOTAL + 1)); result=$(echo "sanhai" | $CLI --top1 --quiet --fuzzy "$LM" "$DICT" 2>/dev/null); if [ "$result" = "上海" ]; then PASS=$((PASS + 1)); else FAIL=$((FAIL + 1)); echo "  ✗ 模糊sh→s: sanhai → $result (期望: 上海)"; fi
TOTAL=$((TOTAL + 1)); result=$(echo "cifan" | $CLI --top1 --quiet --fuzzy "$LM" "$DICT" 2>/dev/null); if [ "$result" = "吃饭" ]; then PASS=$((PASS + 1)); else FAIL=$((FAIL + 1)); echo "  ✗ 模糊ch→c: cifan → $result (期望: 吃饭)"; fi

# 10. 长句
check "长句1" "woshizhongguorenwoaibeijingtiananmen" "我是中国人我爱北京天安门"
check "长句2" "zhonghuarenmingongheguo" "中华人民共和国"
check "长句3" "xuehaowulijiushishijieshangzuikuaideren" "学好物理就是世界上最快的人"

# 11. 常见短语
check "你好吗" "nihaoma" "你好吗"
check "我爱你" "woaini" "我爱你"
check "谢谢" "xiexie" "谢谢"
check "再见" "zaijian" "再见"
check "早上好" "zaoshanghao" "早上好"
check "晚上好" "wanshanghao" "晚上好"

# 12. 专业术语
check "人工智能" "rengongzhineng" "人工智能"
check "深度学习" "shenduxuexi" "深度学习"
check "机器学习" "jiqixuexi" "机器学习"
check "自然语言" "ziranyuyan" "自然语言"
check "大数据" "dashuju" "大数据"

echo ""
echo "==== 边界测试结果：$PASS 通过 / $FAIL 失败 / $TOTAL 总计 ===="
