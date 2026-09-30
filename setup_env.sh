#!/bin/bash
# pinyin-ime environment setup
export NB="${NB:-/root/workspace/native-build}"
export ENV="$NB/mamba-env"
export OPT="$NB/opt"
export PATH="$ENV/bin:$PATH"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5/lib:$OPT/libime/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PINYIN_IME_CLI="/root/workspace/pinyin-ime/cpp/build/pinyin_ime_cli"
export PINYIN_IME_LM="$OPT/libime/lib/libime/zh_CN.lm"
export PINYIN_IME_DICT="$OPT/libime/share/libime/sc.dict"
