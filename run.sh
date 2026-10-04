#!/bin/bash
# pinyin-ime 便捷运行脚本
# 用法:
#   ./run.sh "nihao"           # 单次输入 → 你好
#   ./run.sh --interactive      # 交互模式
#   ./run.sh --test             # 运行测试
#   echo "nihao" | ./run.sh    # 管道模式 → 你好

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/setup_env.sh" 2>/dev/null

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

if [ "$1" = "--test" ]; then
    bash "$SCRIPT_DIR/scripts/test_all.sh"
    exit 0
fi

if [ "$1" = "--interactive" ]; then
    exec "$CLI" "$LM" "$DICT"
fi

if [ -n "$1" ] && [ "$1" != "--top1" ] && [ "$1" != "--json" ] && [ "$1" != "--fuzzy" ] && [ "$1" != "--correct" ] && [ "$1" != "--shuangpin" ]; then
    # 单次输入
    echo "$1" | "$CLI" --top1 --quiet "$LM" "$DICT" 2>/dev/null
else
    # 透传参数（管道模式）-- 默认 --top1 --quiet
    if [ -z "$1" ]; then
        exec "$CLI" --top1 --quiet "$LM" "$DICT"
    else
        exec "$CLI" "$@" "$LM" "$DICT"
    fi
fi
