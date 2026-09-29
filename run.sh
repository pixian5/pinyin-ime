#!/bin/bash
# pinyin-ime 一键运行脚本
# 用法: ./run.sh [输入拼音]
# 不带参数则进入交互模式
set -euo pipefail

NB="/root/workspace/native-build"
ENV="$NB/mamba-env"
OPT="$NB/opt"
REPO_DIR="$(cd "$(dirname "$0")" && pwd)"

export PATH="$ENV/bin:$PATH"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5/lib:$OPT/libime/lib"

LM="$OPT/libime/lib/libime/zh_CN.lm"
DICT="$OPT/libime/share/libime/sc.dict"
CLI="$REPO_DIR/cpp/build/pinyin_ime_cli"

if [ ! -f "$CLI" ]; then
    echo "错误: CLI 未编译，请先运行构建。"
    exit 1
fi

if [ $# -gt 0 ]; then
    # 命令行模式
    echo "$@" | "$CLI" "$LM" "$DICT"
else
    # 交互模式
    echo "pinyin-ime 交互模式（输入 q 退出）"
    echo "================================"
    "$CLI" "$LM" "$DICT"
fi
