#!/bin/bash
# ci_package.sh — GitHub Actions / 本地发布打包入口（v0.9.198）
#
# 只做“编译 + 打包 + 校验”，不跑单测/评测；完整的 build-eval 仍走 ci_build.sh。
# 这样 package job 不会重复 5 分钟的单测，但产物与 ci_build 的结构一致。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
VERSION="$(cat VERSION)"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo "== 打包构建 pinyin-ime v$VERSION（$JOBS 线程）=="
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build cpp/build -j"$JOBS"
cmake -S fcitx5-addon -B fcitx5-addon/build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build fcitx5-addon/build -j"$JOBS"

bash scripts/build_packages.sh all

echo "== 校验三件套 =="
cd dist
sha256sum -c "SHA256SUMS_v$VERSION"
