#!/bin/bash
# =============================================================================
# pinyin-ime v0.9.3 综合测试脚本
# 测试所有功能：全拼、简拼、模糊音、纠错、双拼(8方案)、用户学习、整句、JSON、联想
# 用法: bash scripts/test_all.sh
# 退出码: 0=全通过, 1=有失败
# =============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
CLI="$PROJ_DIR/cpp/build/pinyin_ime_cli"

export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}"
LD_LIBRARY_PATH="/root/workspace/native-build/mamba-env/lib:/root/workspace/native-build/opt/fcitx5/lib:/root/workspace/native-build/opt/libime/lib:$LD_LIBRARY_PATH"
export LD_LIBRARY_PATH

if [ ! -f "$CLI" ]; then
    echo "❌ CLI 未构建: $CLI"
    exit 1
fi

PASS=0; FAIL=0; SKIP=0

check() {
    local desc="$1" got="$2" want="$3"
    if [ "$got" = "$want" ]; then
        echo "  ✅ $desc"
        PASS=$((PASS+1))
    else
        echo "  ❌ $desc (got: '$got', want: '$want')"
        FAIL=$((FAIL+1))
    fi
}

check_contains() {
    local desc="$1" got="$2" pattern="$3"
    if echo "$got" | grep -q "$pattern"; then
        echo "  ✅ $desc"
        PASS=$((PASS+1))
    else
        echo "  ❌ $desc (output: '$got', missing: '$pattern')"
        FAIL=$((FAIL+1))
    fi
}

run() { printf "%s\n" "$1" | timeout 10 $CLI --quiet --top1 ${@:2} 2>&1; }

echo "========================================"
echo "  pinyin-ime v$(cat "$PROJ_DIR/VERSION") 综合测试"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "========================================"

# --- 自检 ---
echo ""
echo "--- 0. 自检 ---"
if timeout 10 $CLI --self-test 2>&1 | grep -q "通过"; then
    echo "  ✅ 自检通过"; PASS=$((PASS+1))
else
    echo "  ❌ 自检失败"; FAIL=$((FAIL+1))
fi

# --- 1. 全拼 ---
echo ""
echo "--- 1. 全拼 ---"
check "nihao→你好" "$(run nihao)" "你好"
check "shijie→世界" "$(run shijie)" "世界"
check "zhongguo→中国" "$(run zhongguo)" "中国"
check "beijingdaxue→北京大学" "$(run beijingdaxue)" "北京大学"
check "xiexie→谢谢" "$(run xiexie)" "谢谢"
check "woaini→我爱你" "$(run woaini)" "我爱你"

# --- 2. 简拼 ---
echo ""
echo "--- 2. 简拼 ---"
check "bjdx→北京大学" "$(run bjdx)" "北京大学"
check "zg→中国" "$(run zg)" "中国"
check "tm→他们" "$(run tm)" "他们"
check "sh→是" "$(run sh)" "是"

# --- 3. 模糊音 ---
echo ""
echo "--- 3. 模糊音 (平翘舌/前后鼻音/n-l) ---"
check "cifan→吃饭" "$(run cifan --fuzzy)" "吃饭"
check "zeli→这里" "$(run zeli --fuzzy)" "这里"
check "lanfang→南方" "$(run lanfang --fuzzy)" "南方"
check "fenfeng→纷纷" "$(run fenfeng --fuzzy)" "纷纷"

# --- 4. 纠错 ---
echo ""
echo "--- 4. 纠错 (CHIME+二元转移) ---"
check "senme→什么" "$(run senme --correct)" "什么"
check "zongguo→中国" "$(run zongguo --correct --fuzzy)" "中国"
check "shengri→生日" "$(run shengri --correct)" "生日"

# --- 5. 双拼 ---
echo ""
echo "--- 5. 双拼 (8方案) ---"
# 小鹤: n+i=ni, h+c=hao → nihc
check "小鹤 nihc→你好" "$(run nihc --shuangpin)" "你好"
check "小鹤 uijm→时间" "$(run uijm --shuangpin)" "时间"
# 自然码: n+i=ni, h+k=hao → nihk
check "自然码 nihk→你好" "$(run nihk --shuangpin --sp-scheme ziranma)" "你好"
# 微软: n+i=ni, h+k=hao → nihk
check "微软 nihk→你好" "$(run nihk --shuangpin --sp-scheme ms)" "你好"
# 紫光
check "紫光 nihq→你好" "$(run nihq --shuangpin --sp-scheme ziguang)" "你好"

# --- 6. 用户学习 ---
echo ""
echo "--- 6. 用户学习 ---"
check "woaizhongguo→我爱中国" "$(run woaizhongguo)" "我爱中国"
check "shanghaishi→上海是" "$(run shanghaishi)" "上海是"

# --- 7. 整句 ---
echo ""
echo "--- 7. 整句 ---"
check "我是中国人" "$(run woshizhongguoren)" "我是中国人"
check "我想订一个房间" "$(run woxiangdingyigefangjian)" "我想订一个房间"
check "你好中国" "$(run nihaozhongguo)" "你好中国"

# --- 8. JSON 输出 ---
echo ""
echo "--- 8. JSON 输出 ---"
JSON=$(printf 'nihao\n' | timeout 5 $CLI --quiet --json 2>&1)
check_contains "JSON含你好" "$JSON" "你好"
check_contains "JSON格式正确" "$JSON" '^\['

# --- 9. 联想预测 ---
echo ""
echo "--- 9. 联想预测 ---"
PRED=$(echo "你好" | timeout 5 $CLI --predict --quiet 2>&1 | head -1)
check_contains "联想有输出" "$PRED" "."

# --- 10. 多候选 ---
echo ""
echo "--- 10. 多候选 ---"
CANDS=$(printf 'nihao\n' | timeout 5 $CLI --quiet 2>&1 | head -1)
check_contains "多候选含你好" "$CANDS" "你好"
NCAND=$(echo "$CANDS" | tr '\t' '\n' | wc -l)
if [ "$NCAND" -ge 3 ]; then
    echo "  ✅ 候选数≥3 ($NCAND)"; PASS=$((PASS+1))
else
    echo "  ❌ 候选数<3 ($NCAND)"; FAIL=$((FAIL+1))
fi

# --- 11. 统计 ---
echo ""
echo "--- 11. 引擎统计 ---"
STATS=$(timeout 5 $CLI --stats 2>&1)
check_contains "统计含LM路径" "$STATS" "lm_path"
check_contains "统计含词典路径" "$STATS" "dict_path"

# --- 12. 准确率 ---
echo ""
echo "--- 12. 准确率 (daily 40句) ---"
if [ -f "$PROJ_DIR/scripts/eval_daily.py" ]; then
    EVAL=$(timeout 30 /usr/local/bin/python3 "$PROJ_DIR/scripts/eval_daily.py" 2>&1)
    ACC=$(echo "$EVAL" | grep "Sentence accuracy" | grep -oP '\d+/\d+ = \d+\.\d+%')
    echo "  句级准确率: $ACC"
    if echo "$ACC" | grep -qP '9[0-9]\.\d+%|100\.0%'; then
        echo "  ✅ 准确率≥90%"; PASS=$((PASS+1))
    else
        echo "  ⚠️ 准确率<90%"; FAIL=$((FAIL+1))
    fi
else
    echo "  ⏭️ eval_daily.py 不存在"; SKIP=$((SKIP+1))
fi

# --- 13. 拼音消歧 ---
echo ""
echo "--- 13. 拼音消歧 ---"
check "xianbeijing→西安北京" "$(run xianbeijing)" "西安北京"
check "xianyou→西安有" "$(run xianyou)" "西安有"
check "xianshi→显示" "$(run xianshi)" "显示"
check "xiandaishishiyong→现代史使用" "$(run xiandaishishiyong)" "现代史使用"
check "xian→先" "$(run xian)" "先"

# --- 汇总 ---
echo ""
echo "========================================"
echo "  测试汇总"
echo "  总计: $((PASS+FAIL+SKIP)) | 通过: $PASS | 失败: $FAIL | 跳过: $SKIP"
if [ $FAIL -eq 0 ]; then
    echo "  🎉 全部通过！"
    echo "========================================"
    exit 0
else
    echo "  ⚠️ 有 $FAIL 个失败"
    echo "========================================"
    exit 1
fi
