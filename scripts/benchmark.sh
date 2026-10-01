#!/bin/bash
# =============================================================================
# pinyin-ime 性能基准测试
# 使用 --batch 模式测量纯解码延迟（不含 CLI 启动开销）
# 用法: bash scripts/benchmark.sh
# =============================================================================
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
cd "$PROJ_DIR"
source ./setup_env.sh 2>/dev/null

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

# 生成测试数据
TMPFILE=$(mktemp)
trap "rm -f $TMPFILE" EXIT

# 短输入（1-2音节）
for w in nihao zhongguo beijing shanghai shenzhen xianxiang tianqi; do
    echo "$w" >> "$TMPFILE"
done

# 中等输入（3-5音节）
for w in beijingdaxue qinghuadaxue rengongzhineng jiqixuexi tianqiyubao; do
    echo "$w" >> "$TMPFILE"
done

# 长输入（6+音节）
for w in woxiangqubeijinglvyou jintiantianqizhenhao xueshengxuexijiqiyurengongzhineng; do
    echo "$w" >> "$TMPFILE"
done

# 简拼
for w in bjdx zg rxzg; do
    echo "$w" >> "$TMPFILE"
done

TOTAL=$(wc -l < "$TMPFILE")

echo "=== 性能基准测试 ==="
echo "测试输入: $TOTAL 条"
echo ""

# 1. 纯解码延迟（batch 模式，单次启动）
echo "--- 纯解码延迟（batch 模式）---"
python3 -c "
import subprocess, time, os

cli = os.environ.get('PINYIN_IME_CLI', '$CLI')
lm = os.environ.get('PINYIN_IME_LM', '$LM')
dict_ = os.environ.get('PINYIN_IME_DICT', '$DICT')

inputs = open('$TMPFILE').read().strip().split('\n')

# 预热
subprocess.run([cli, '--top1', '--quiet', lm, dict_], input='nihao\n', capture_output=True, text=True)

# 测量 batch 模式
t0 = time.time()
result = subprocess.run([cli, '--batch', '$TMPFILE', '--top1', '--quiet', lm, dict_], capture_output=True, text=True)
batch_time = time.time() - t0

# 测量逐条模式（含启动开销）
t0 = time.time()
for inp in inputs:
    subprocess.run([cli, '--top1', '--quiet', lm, dict_], input=inp+'\n', capture_output=True, text=True)
per_query_time = time.time() - t0

print(f'  Batch 总时间: {batch_time*1000:.1f}ms ({len(inputs)} 条)')
print(f'  Batch 平均: {batch_time*1000/len(inputs):.1f}ms/条')
print(f'  逐条总时间: {per_query_time*1000:.1f}ms ({len(inputs)} 条)')
print(f'  逐条平均: {per_query_time*1000/len(inputs):.1f}ms/条')
print(f'  CLI 启动开销: {(per_query_time - batch_time)*1000/len(inputs):.1f}ms/条')
print(f'  吞吐量: {len(inputs)/batch_time:.0f} 条/秒')
"

echo ""
echo "--- 各输入长度延迟 ---"
for input in "ni" "nihao" "nihaozhongguo" "beijingdaxueqinghuadaxue" "woxiangqubeijinglvyoukanyikantiananmen"; do
    ms=$(python3 -c "
import subprocess, time
t0 = time.time()
for _ in range(10):
    subprocess.run(['$CLI', '--top1', '--quiet', '$LM', '$DICT'], input='$input\n', capture_output=True, text=True)
print(f'{(time.time()-t0)*100:.1f}')
")
    result=$(echo "$input" | "$CLI" --top1 --quiet "$LM" "$DICT" 2>/dev/null)
    echo "  ${#input}字符: ${ms}ms/条 → $result"
done

echo ""
echo "--- 纠错模式延迟 ---"
for input in "nihao" "nihou" "senme" "zongguo"; do
    ms=$(python3 -c "
import subprocess, time
t0 = time.time()
for _ in range(10):
    subprocess.run(['$CLI', '--top1', '--quiet', '--correct', '$LM', '$DICT'], input='$input\n', capture_output=True, text=True)
print(f'{(time.time()-t0)*100:.1f}')
")
    result=$(echo "$input" | "$CLI" --top1 --quiet --correct "$LM" "$DICT" 2>/dev/null)
    echo "  $input: ${ms}ms/条 → $result"
done

echo ""
echo "=== 基准测试完成 ==="
