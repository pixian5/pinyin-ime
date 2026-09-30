#!/bin/bash
# 性能对比：不同配置下的输入法性能
# 对比：默认 / 模糊音 / 纠错 / 双拼 / 简拼

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"
source ./setup_env.sh 2>/dev/null

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

# Test sentences
SENTENCES=(
    "nihao" "shijie" "zhongguo" "beijingdaxue" "qinghuadaxue"
    "woaizhongguo" "jintiandetianqi" "qingbangwochaxunyixia"
    "woxiangdingyigefangjian" "rengongzhineng"
    "jiqixuexi" "shenduxuexi" "shurufa" "wangluo" "shujuku"
    "woyaochifan" "mingtianjian" "zhuyuandankuaile"
    "qinggeiwohuidianhua" "woaini" "nihaozhongguo"
)

bench() {
    local label="$1"; shift
    local start=$(date +%s%N)
    local count=0
    for s in "${SENTENCES[@]}"; do
        echo "$s" | "$CLI" --quiet --top1 "$@" "$LM" "$DICT" 2>/dev/null >/dev/null
        count=$((count + 1))
    done
    local end=$(date +%s%N)
    local elapsed=$(( (end - start) / 1000000 ))
    local rate=$(echo "scale=1; $count * 1000 / $elapsed" | bc 2>/dev/null || echo "N/A")
    printf "  %-20s %3d句  %4dms  %s句/秒\n" "$label" "$count" "$elapsed" "$rate"
}

echo "╔══════════════════════════════════════════════════════╗"
echo "║          pinyin-ime 性能对比                          ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""
echo "  配置                  句数   耗时   吞吐量"
echo "  ─────────────────────────────────────────────────"
bench "默认（全拼+简拼）"
bench "模糊音" --fuzzy
bench "纠错" --correct
bench "模糊+纠错" --fuzzy --correct
bench "双拼(小鹤)" --shuangpin --sp-scheme xiaohe
echo ""
echo "  测试句数: ${#SENTENCES[@]}"
