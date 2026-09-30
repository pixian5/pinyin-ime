#!/bin/bash
# pinyin-ime CLI 运行脚本 —— 自动设置环境变量并启动命令行输入法
NB=/workspace/native-build
ENV="$NB/mamba-env"
OPT="$NB/opt"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/libime/lib:$OPT/fcitx5/lib"
LM="$OPT/libime/lib/libime/zh_CN.lm"
DICT="$OPT/libime/share/libime/sc.dict"
CLI="$(dirname "$0")/cpp/build/pinyin_ime_cli"
exec "$CLI" "$LM" "$DICT" "${@}"
