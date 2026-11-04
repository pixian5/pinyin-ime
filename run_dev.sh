#!/bin/bash
# 便捷运行脚本：使用 S3 预构建 CLI + libime 库
export LD_LIBRARY_PATH="/root/workspace/pinyin-ime/pinyin-ime-s3/v0.9.139/lib:${LD_LIBRARY_PATH:-}"
CLI="/root/workspace/pinyin-ime/pinyin-ime-s3/v0.9.166/pinyin_ime_cli"
LM="/root/workspace/pinyin-ime/pinyin-ime-s3/v0.9.139/share/libime/libime/zh_CN.lm"
DICT="/root/workspace/pinyin-ime/pinyin-ime-s3/v0.9.139/share/libime/libime/sc.dict"

if [ $# -eq 0 ]; then
    # Interactive mode
    exec "$CLI" "$LM" "$DICT"
else
    # Line mode: pass input as argument
    echo "$1" | "$CLI" "$LM" "$DICT"
fi
