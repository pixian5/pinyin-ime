#!/usr/bin/env bash
# 编译 cpp 下的「可运行输入法 CLI」+「行协议适配器」。
# 前提：已先跑 scripts/setup_build_env.sh && scripts/build_conda_forge.sh（产出 libime 库 + LM + 词典）。
# 构建位置默认 /workspace/native-build，可用 NB=/your/path 覆盖（三个构建脚本保持一致）。
set -euo pipefail

NB="${NB:-/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"

export PATH="$ENV/bin:$PATH"

cmake -S "$REPO_DIR/cpp" -B "$REPO_DIR/cpp/build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$OPT/fcitx5;$OPT/libime;$ENV"

cmake --build "$REPO_DIR/cpp/build"

echo "构建完成：$REPO_DIR/cpp/build/pinyin_ime_cli"