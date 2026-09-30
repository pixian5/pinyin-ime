#!/bin/bash
# pinyin-ime 演示脚本
# 展示输入法的各项功能

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"
source ./setup_env.sh 2>/dev/null

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

run() { echo "$1" | "$CLI" --quiet --top1 "$LM" "$DICT" 2>/dev/null; }
cands() { echo "$1" | "$CLI" --quiet "$LM" "$DICT" 5 2>/dev/null; }

echo "╔══════════════════════════════════════════════════════╗"
echo "║          pinyin-ime 拼音输入法演示                    ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

echo "【1】基础输入"
echo "─────────────────────────────────────────"
for p in nihao shijie zhongguo beijing xuesheng; do
    printf "  %-12s → %s\n" "$p" "$(run $p)"
done

echo ""
echo "【2】整句输入"
echo "─────────────────────────────────────────"
for p in "woaizhongguo" "jintiandetianqi" "qingbangwochaxunyixia" "woxiangdingyigefangjian"; do
    printf "  %-30s → %s\n" "$p" "$(run $p)"
done

echo ""
echo "【3】简拼输入"
echo "─────────────────────────────────────────"
for p in "bjdx" "zg" "xsh"; do
    printf "  %-12s → %s\n" "$p" "$(run $p)"
done

echo ""
echo "【4】模糊音（平翘舌、前后鼻音、n/l）"
echo "─────────────────────────────────────────"
for p in "cifan" "zeli" "lanfang"; do
    result=$(echo "$p" | "$CLI" --quiet --top1 --fuzzy "$LM" "$DICT" 2>/dev/null)
    printf "  %-12s → %s (模糊音)\n" "$p" "$result"
done

echo ""
echo "【5】纠错（M3模型）"
echo "─────────────────────────────────────────"
for p in "senme" "zongguo" "sengri"; do
    result=$(echo "$p" | "$CLI" --quiet --top1 --correct "$LM" "$DICT" 2>/dev/null)
    printf "  %-12s → %s (纠错)\n" "$p" "$result"
done

echo ""
echo "【6】双拼（小鹤方案）"
echo "─────────────────────────────────────────"
for p in "nihc" "uijm"; do
    result=$(echo "$p" | "$CLI" --quiet --top1 --shuangpin --sp-scheme xiaohe "$LM" "$DICT" 2>/dev/null)
    printf "  %-12s → %s (小鹤双拼)\n" "$p" "$result"
done

echo ""
echo "【7】拼音消歧"
echo "─────────────────────────────────────────"
for p in "xian" "xianbeijing" "xianshi" "xianyou"; do
    printf "  %-18s → %s\n" "$p" "$(run $p)"
done

echo ""
echo "【8】技术术语"
echo "─────────────────────────────────────────"
for p in "rengongzhineng" "jiqixuexi" "shenduxuexi" "shurufa"; do
    printf "  %-18s → %s\n" "$p" "$(run $p)"
done

echo ""
echo "【9】多候选展示"
echo "─────────────────────────────────────────"
printf "  nihao 候选: %s\n" "$(cands nihao | tr '\n' ' ')"
printf "  xian  候选: %s\n" "$(cands xian | tr '\n' ' ')"

echo ""
echo "【10】JSON 输出"
echo "─────────────────────────────────────────"
echo "  nihao JSON:"
echo "nihao" | "$CLI" --json "$LM" "$DICT" 2>/dev/null | head -5 | sed 's/^/    /'

echo ""
echo "╔══════════════════════════════════════════════════════╗"
echo "║  演示结束。交互模式请运行: ./run.sh                   ║"
echo "╚══════════════════════════════════════════════════════╝"
