#!/bin/bash
# pinyin-ime CI 测试脚本
# 在沙箱环境中运行所有测试，返回非零表示失败
set -e

PROJECT_DIR="$(cd "$(dirname "$0")/.." && pwd)"
cd "$PROJECT_DIR"

# 环境变量
export LD_LIBRARY_PATH="${LD_LIBRARY_PATH:-}:/root/workspace/native-build/mamba-env/lib:/root/workspace/native-build/opt/fcitx5/lib:/root/workspace/native-build/opt/libime/lib"
CLI="cpp/build/pinyin_ime_cli"

# 颜色
GREEN() { printf "\033[32m%s\033[0m\n" "$*"; }
RED()   { printf "\033[31m%s\033[0m\n" "$*"; }

FAILURES=0
TOTAL=0
PASS=0

run_test() {
    local name="$1"
    local cmd="$2"
    TOTAL=$((TOTAL + 1))
    if eval "$cmd" >/dev/null 2>&1; then
        GREEN "  [PASS] $name"
        PASS=$((PASS + 1))
    else
        RED "  [FAIL] $name"
        FAILURES=$((FAILURES + 1))
    fi
}

echo "=========================================="
echo "  pinyin-ime CI Test Suite"
echo "  $(date '+%Y-%m-%d %H:%M:%S')"
echo "=========================================="
echo ""

# 1. CLI 自检
echo "=== 1. CLI 自检 ==="
run_test "self-test" "$CLI --self-test"

# 2. 版本检查
echo ""
echo "=== 2. 版本检查 ==="
run_test "version" "$CLI --version | grep -q '0\.9\.'"

# 3. 基础拼音
echo ""
echo "=== 3. 基础拼音 ==="
run_test "nihao" "echo nihao | $CLI --top1 --quiet | grep -q 你好"
run_test "shijie" "echo shijie | $CLI --top1 --quiet | grep -q 世界"
run_test "zhongguo" "echo zhongguo | $CLI --top1 --quiet | grep -q 中国"
run_test "beijing" "echo beijing | $CLI --top1 --quiet | grep -q 北京"
run_test "xiexie" "echo xiexie | $CLI --top1 --quiet | grep -q 谢谢"

# 4. 简拼
echo ""
echo "=== 4. 简拼 ==="
run_test "simplified-bjdx" "echo bjdx | $CLI --top1 --quiet | grep -q 北京大学"
run_test "simplified-zg" "echo zg | $CLI --top1 --quiet | grep -q 中国"

# 5. 模糊音
echo ""
echo "=== 5. 模糊音 ==="
run_test "fuzzy-cifan" "echo cifan | $CLI --fuzzy --top1 --quiet | grep -q 吃饭"
run_test "fuzzy-lanfang" "echo lanfang | $CLI --fuzzy --top1 --quiet | grep -q 南方"

# 6. 纠错
echo ""
echo "=== 6. 纠错 ==="
run_test "correct-senme" "echo senme | $CLI --correct --top1 --quiet | grep -q 什么"

# 7. 双拼
echo ""
echo "=== 7. 双拼 ==="
run_test "shuangpin-xiaohe" "echo vsgo | $CLI --shuangpin --sp-scheme xiaohe --top1 --quiet | grep -q 中国"

# 8. 整句
echo ""
echo "=== 8. 整句 ==="
run_test "sentence-1" "echo woshizhongguoren | $CLI --top1 --quiet | grep -q 我是中国人"
run_test "sentence-2" "echo woaixuexi | $CLI --top1 --quiet | grep -q 我爱学习"
run_test "sentence-3" "echo jintiandetianqi | $CLI --top1 --quiet | grep -q 今天的天气"

# 9. 用户学习
echo ""
echo "=== 9. 用户学习 ==="
run_test "learn" "echo -e 'testword\t测试词' | $CLI --learn-file /dev/stdin --quiet 2>&1 | grep -q '已学习'"

# 10. JSON 输出
echo ""
echo "=== 10. JSON 输出 ==="
run_test "json" "echo nihao | $CLI --json --quiet | grep -q 你好"

# 11. 联想预测
echo ""
echo "=== 11. 联想预测 ==="
run_test "predict" "echo 你好 | $CLI --predict --quiet | grep -q ."

# 12. 引擎统计
echo ""
echo "=== 12. 引擎统计 ==="
run_test "stats" "$CLI --stats --quiet | grep -q 'lm_path'"

# 13. 批量模式
echo ""
echo "=== 13. 批量模式 ==="
echo "nihao" > /tmp/ci_batch.txt
echo "shijie" >> /tmp/ci_batch.txt
run_test "batch" "$CLI --batch /tmp/ci_batch.txt --top1 --quiet | grep -q 你好"
rm -f /tmp/ci_batch.txt

# 14. 准确率
echo ""
echo "=== 14. 准确率 ==="
if command -v python3 &>/dev/null && [ -f scripts/eval_daily.py ]; then
    run_test "accuracy-daily" "python3 scripts/eval_daily.py 2>&1 | grep -qE '97.5|100.0'"
fi

# 15. 性能基准
echo ""
echo "=== 15. 性能基准 ==="
if [ -f scripts/benchmark.py ]; then
    run_test "benchmark" "python3 scripts/benchmark.py 2>&1 | grep -q "句/秒""
fi

# 16. 综合测试脚本
echo ""
echo "=== 16. 综合测试脚本 ==="
if [ -f scripts/test_all.sh ]; then
    run_test "test-all" "bash scripts/test_all.sh 2>&1 | tail -3 | grep -q '全部通过'"
fi

# 结果
echo ""
echo "=========================================="
echo "  结果: $PASS/$TOTAL 通过, $FAILURES 失败"
echo "=========================================="

if [ $FAILURES -gt 0 ]; then
    RED "CI 测试失败"
    exit 1
else
    GREEN "CI 测试全部通过"
    exit 0
fi
