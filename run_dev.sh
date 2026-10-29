#!/usr/bin/env bash
# 便捷开发运行脚本
# 用法: ./run_dev.sh [拼音输入] 或交互模式
set -euo pipefail

ENV=/root/workspace/native-build/mamba-env
S3DIR=/root/workspace/pinyin-ime/pinyin-ime-s3
export LD_LIBRARY_PATH=$S3DIR/lib:$ENV/lib
export PATH=$ENV/bin:$PATH

CLI="$S3DIR/pinyin_ime_cli"
LM="$S3DIR/share/libime/libime/zh_CN.lm"
DICT="$S3DIR/share/libime/libime/sc.dict"

if [ $# -gt 0 ]; then
    echo "$@" | "$CLI" "$LM" "$DICT"
else
    "$CLI" "$LM" "$DICT"
fi
