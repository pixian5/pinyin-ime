#!/bin/bash
# pinyin-ime environment setup (non-Docker build)
# Source this file to set up paths for the pinyin input method
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
export NB="${NB:-/root/workspace/native-build}"
export ENV="$NB/mamba-env"
export OPT="$NB/opt"
export PATH="$ENV/bin:$PATH"
# Include mamba-env/lib for newer libstdc++ (GLIBCXX_3.4.36)
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5/lib:$OPT/libime/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
export PINYIN_IME_CLI="$REPO_DIR/cpp/build/pinyin_ime_cli"
export PINYIN_IME_LM="$OPT/libime/lib/libime/zh_CN.lm"
export PINYIN_IME_DICT="$OPT/libime/share/libime/sc.dict"
