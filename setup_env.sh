#!/bin/bash
# pinyin-ime 环境变量设置脚本
# 用法: source setup_env.sh
# 设置好所有库路径和环境变量，之后可直接运行 pinyin_ime_cli

NB=${NB:-/root/workspace/native-build}
ENV="$NB/mamba-env"
OPT="$NB/opt"

export PATH="$ENV/bin:$PATH"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5/lib:$OPT/libime/lib"
export PKG_CONFIG_PATH="$ENV/lib/pkgconfig:$ENV/share/pkgconfig:$OPT/fcitx5/lib/pkgconfig:$OPT/libime/lib/pkgconfig"

# 输入法数据路径
export PINYIN_IME_LM="$OPT/libime/lib/libime/zh_CN.lm"
export PINYIN_IME_DICT="$OPT/libime/share/libime/sc.dict"
export PINYIN_IME_CLI="/root/workspace/pinyin-ime/cpp/build/pinyin_ime_cli"
export PINYIN_IME_LMPLZ="$NB/build/kenlm/bin/lmplz"

echo "pinyin-ime 环境已设置："
echo "  CLI:    $PINYIN_IME_CLI"
echo "  LM:     $PINYIN_IME_LM"
echo "  DICT:   $PINYIN_IME_DICT"
echo "  LMPLZ:  $PINYIN_IME_LMPLZ"
