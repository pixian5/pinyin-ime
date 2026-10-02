#!/bin/bash
# pinyin-ime 综合性能基准测试
# 测量：启动时间、单次查询延迟、长句处理、内存占用、吞吐量、准确率

set -e
cd "$(dirname "$0")/.."
source ./setup_env.sh 2>/dev/null

CLI="${PINYIN_IME_CLI:-/root/workspace/pinyin-ime/cpp/build/pinyin_ime_cli}"
LM="${PINYIN_IME_LM:-/root/workspace/native-build/opt/libime/lib/libime/zh_CN.lm}"
DICT="${PINYIN_IME_DICT:-/root/workspace/native-build/opt/libime/share/libime/sc.dict}"

if [ ! -f "$CLI" ] || [ ! -f "$LM" ] || [ ! -f "$DICT" ]; then
    echo "错误: 缺少必要文件，请先构建" >&2
    exit 1
fi

echo "╔════════════════════════════════════════════════════════════╗"
echo "║          pinyin-ime 综合性能基准测试                        ║"
echo "╚════════════════════════════════════════════════════════════╝"
echo ""

# 系统信息
echo "【系统信息】"
echo "  OS: $(uname -s) $(uname -r)"
echo "  Arch: $(uname -m)"
echo "  CPU: $(nproc) cores"
echo "  LM: $(ls -lh "$LM" | awk '{print $5}')"
echo "  Dict: $(ls -lh "$DICT" | awk '{print $5}')"
echo ""

# 1. 启动时间（冷启动）
echo "【1. 冷启动时间】"
TOTAL=0
for i in 1 2 3 4 5; do
    START=$(date +%s%N)
    echo "nihao" | $CLI --top1 --quiet "$LM" "$DICT" 2>/dev/null > /dev/null
    END=$(date +%s%N)
    MS=$(( (END - START) / 1000000 ))
    echo "  第${i}次: ${MS}ms"
    TOTAL=$((TOTAL + MS))
done
AVG_STARTUP=$((TOTAL / 5))
echo "  平均: ${AVG_STARTUP}ms"
echo ""

# 2. 单次查询延迟
echo "【2. 单次查询延迟】"
QUERIES=("nihao" "zhongguo" "beijing" "shanghai" "xiexie" "zaijian"
         "woaini" "mingbai" "kexue" "jishu" "shijian" "kongzhi"
         "yinyue" "dianying" "youxi" "xuexi" "gongzuo" "shenghuo")
TOTAL_LATENCY=0
for q in "${QUERIES[@]}"; do
    START=$(date +%s%N)
    RESULT=$(echo "$q" | $CLI --top1 --quiet "$LM" "$DICT" 2>/dev/null)
    END=$(date +%s%N)
    MS=$(( (END - START) / 1000000 ))
    TOTAL_LATENCY=$((TOTAL_LATENCY + MS))
    printf "  %-12s → %-10s  %dms\n" "$q" "$RESULT" "$MS"
done
AVG_LATENCY=$((TOTAL_LATENCY / ${#QUERIES[@]}))
echo "  平均延迟: ${AVG_LATENCY}ms"
echo ""

# 3. 长句处理
echo "【3. 长句处理】"
LONG_SENTENCES=(
    "woshizhongguorenwoaibeijingtiananmen"
    "jintiantiankizhenhaowomenqugongyuanwanshuaba"
    "xuehaowulijiushishijieshangzuikuaideren"
    "zhonghuarenmingongheguowansui"
    "kejixingguojiaxingguoyongkejishixianmengxiang"
)
for sentence in "${LONG_SENTENCES[@]}"; do
    LEN=${#sentence}
    START=$(date +%s%N)
    RESULT=$(echo "$sentence" | $CLI --top1 --quiet "$LM" "$DICT" 2>/dev/null)
    END=$(date +%s%N)
    MS=$(( (END - START) / 1000000 ))
    printf "  [%d字符] %dms  %s\n" "$LEN" "$MS" "$RESULT"
done
echo ""

# 4. 内存占用
echo "【4. 内存占用】"
echo "nihao" | $CLI --stats "$LM" "$DICT" 2>&1 | grep -i "memory\|mem\|内存" || true
# 用 /usr/bin/time 测量
# 启动CLI并测量内存
echo "nihao" | $CLI --top1 --quiet "$LM" "$DICT" 2>/dev/null > /dev/null &
CLIPID=$!
sleep 0.05
MEM=0
if [ -f "/proc/$CLIPID/status" ]; then
    MEM=$(grep VmRSS /proc/$CLIPID/status 2>/dev/null | awk '{print $2}')
fi
wait $CLIPID 2>/dev/null
if [ "$MEM" -eq 0 ]; then
    MEM=$(/usr/bin/time -v sh -c "echo 'nihao' | $CLI --top1 --quiet '$LM' '$DICT' 2>/dev/null" 2>&1 | grep "Maximum resident" | awk '{print $NF}')
    MEM=$((MEM / 1024))
fi
echo "  峰值RSS: ${MEM}MB"
echo ""

# 5. 吞吐量
echo "【5. 吞吐量】"
# 生成100个查询
INPUTS=""
for i in $(seq 1 100); do
    INPUTS="${INPUTS}nihao\n"
done
START=$(date +%s%N)
printf "$INPUTS" | $CLI --top1 --quiet "$LM" "$DICT" 2>/dev/null > /dev/null
END=$(date +%s%N)
MS=$(( (END - START) / 1000000 ))
QPS=$((100 * 1000 / (MS + 1)))
echo "  100查询: ${MS}ms (${QPS} qps)"
echo ""

# 6. 准确率
echo "【6. 准确率】"
# 常用短语准确率
CORRECT=0
TOTAL_TEST=0
declare -a TEST_PAIRS=(
    "nihao|你好"
    "zhongguo|中国"
    "beijing|北京"
    "shanghai|上海"
    "xiexie|谢谢"
    "zaijian|再见"
    "woaini|我爱你"
    "mingbai|明白"
    "kexue|科学"
    "jishu|技术"
    "shijian|时间"
    "kongzhi|控制"
    "yinyue|音乐"
    "dianying|电影"
    "youxi|游戏"
    "xuexi|学习"
    "gongzuo|工作"
    "shenghuo|生活"
    "shijieshang|世界上"
    "zhonghuarenmin|中国人民"
    "woshizhongguoren|我是中国人"
    "woaibeijingtiananmen|我爱北京天安门"
    "jintiandetiankongzhenhao|今天的天空真好"
    "xuehaowuli|学好物理"
    "kejixingguo|科技兴国"
)
for pair in "${TEST_PAIRS[@]}"; do
    PINYIN="${pair%%|*}"
    EXPECTED="${pair##*|}"
    RESULT=$(echo "$PINYIN" | $CLI --top1 --quiet "$LM" "$DICT" 2>/dev/null)
    TOTAL_TEST=$((TOTAL_TEST + 1))
    if [ "$RESULT" = "$EXPECTED" ]; then
        CORRECT=$((CORRECT + 1))
    else
        printf "  ✗ %-30s → %-12s (期望: %s)\n" "$PINYIN" "$RESULT" "$EXPECTED"
    fi
done
ACC=$((CORRECT * 100 / TOTAL_TEST))
echo "  准确率: ${CORRECT}/${TOTAL_TEST} = ${ACC}%"
echo ""

# 7. 模糊音性能
echo "【7. 模糊音模式】"
START=$(date +%s%N)
echo "zhongguo" | $CLI --top1 --quiet --fuzzy "$LM" "$DICT" 2>/dev/null > /dev/null
END=$(date +%s%N)
FUZZY_MS=$(( (END - START) / 1000000 ))
echo "  模糊音延迟: ${FUZZY_MS}ms (普通: ${AVG_LATENCY}ms)"
echo ""

# 8. 纠错模式
echo "【8. 纠错模式】"
CORRECT_INPUTS=("zhongguo" "nihao" "shijie")
for input in "${CORRECT_INPUTS[@]}"; do
    START=$(date +%s%N)
    RESULT=$(echo "$input" | $CLI --top1 --quiet --correct "$LM" "$DICT" 2>/dev/null)
    END=$(date +%s%N)
    MS=$(( (END - START) / 1000000 ))
    printf "  %-12s → %-10s  %dms\n" "$input" "$RESULT" "$MS"
done
echo ""

# 汇总
echo "╔════════════════════════════════════════════════════════════╗"
echo "║                        基准测试汇总                          ║"
echo "╠════════════════════════════════════════════════════════════╣"
printf "║  平均启动时间:    %-42s ║\n" "${AVG_STARTUP}ms"
printf "║  平均查询延迟:    %-42s ║\n" "${AVG_LATENCY}ms"
printf "║  吞吐量:          %-42s ║\n" "${QPS} qps"
printf "║  峰值内存:        %-42s ║\n" "${MEM}MB"
printf "║  准确率:          %-42s ║\n" "${CORRECT}/${TOTAL_TEST} = ${ACC}%"
printf "║  模糊音延迟:      %-42s ║\n" "${FUZZY_MS}ms"
echo "╚════════════════════════════════════════════════════════════╝"
