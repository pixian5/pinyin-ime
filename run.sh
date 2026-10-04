#!/bin/bash
# pinyin-ime 便捷运行脚本
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
source "$SCRIPT_DIR/setup_env.sh" 2>/dev/null

CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

if [ "$1" = "--test" ]; then
    bash "$SCRIPT_DIR/scripts/smoke_test.sh"
    exit 0
fi

if [ "$1" = "--interactive" ] || [ "$1" = "-i" ]; then
    exec "$CLI" "$LM" "$DICT"
fi

if [ -n "$1" ] && [ "$1" != "--top1" ] && [ "$1" != "--json" ] && [ "$1" != "--fuzzy" ] && [ "$1" != "--correct" ] && [ "$1" != "--shuangpin" ]; then
    # 单次输入
    echo "$1" | "$CLI" --top1 --quiet "$LM" "$DICT" 2>/dev/null
else
    # 透传参数（管道模式）
    if [ -z "$1" ]; then
        exec "$CLI" --top1 --quiet "$LM" "$DICT"
    else
        exec "$CLI" "$@" "$LM" "$DICT"
    fi
fi
