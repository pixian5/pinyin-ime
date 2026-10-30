#!/usr/bin/env bash
# 便捷开发运行脚本 — v0.9.144 从源码编译版
# 用法: ./run_dev.sh [拼音输入] 或交互模式
set -euo pipefail

NB=/root/workspace/native-build
ENV="$NB/mamba-env"
S3DIR=/root/workspace/pinyin-ime/pinyin-ime-s3/v0.9.139
FCITX5_LIB=/root/workspace/pinyin-ime/pinyin-ime-s3/pinyin-ime-deploy-v0.9.125/runtime/opt/fcitx5-x11/lib
export LD_LIBRARY_PATH=$S3DIR/lib:$FCITX5_LIB:$ENV/lib
export PATH=$ENV/bin:$PATH

# 优先用源码编译的 CLI（v0.9.144），回退到 S3 预构建（v0.9.139）
if [ -x /root/workspace/pinyin-ime/cpp/build/pinyin_ime_cli ]; then
    CLI=/root/workspace/pinyin-ime/cpp/build/pinyin_ime_cli
else
    CLI=$S3DIR/pinyin_ime_cli
fi
LM="$S3DIR/share/libime/libime/zh_CN.lm"
DICT="$S3DIR/share/libime/libime/sc.dict"

if [ $# -gt 0 ]; then
    echo "$@" | "$CLI" "$LM" "$DICT"
else
    "$CLI" "$LM" "$DICT"
fi
