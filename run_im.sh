#!/usr/bin/env bash
# pinyin-ime 便捷运行脚本
# 用法:
#   ./run_im.sh "nihao"           # 单次查询
#   ./run_im.sh                   # 交互模式
#   echo "nihao" | ./run_im.sh    # 管道模式
set -euo pipefail

NB="${NB:-/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"
REPO_DIR="$(cd "$(dirname "$0")" && pwd)"
CLI="$REPO_DIR/cpp/build/pinyin_ime_cli"
LM="$OPT/libime/lib/libime/zh_CN.lm"
DICT="$OPT/libime/share/libime/sc.dict"

export LD_LIBRARY_PATH="$OPT/libime/lib:$OPT/fcitx5/lib:$ENV/lib"

if [ ! -x "$CLI" ]; then
    echo "错误: CLI 未构建。请先运行构建。" >&2
    exit 1
fi

if [ $# -gt 0 ]; then
    # 单次查询模式
    echo "$1" | "$CLI" "$LM" "$DICT" 5
else
    # 交互/管道模式
    "$CLI" "$LM" "$DICT" 5
fi
