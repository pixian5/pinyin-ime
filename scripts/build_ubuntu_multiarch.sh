#!/usr/bin/env bash
# 用 docker buildx 编译 pinyin-ime 的 Ubuntu 24.04 双架构版本（arm64 / amd64）。
# arm64 在本机（Apple Silicon）原生构建，amd64 由 buildx 通过 QEMU 模拟构建。
# 两个架构的产物分别导出到 dist/ubuntu/arm64 与 dist/ubuntu/amd64。
#
# 产物：
#   pinyin_ime_cli         命令行前端（CLI）
#   libime_bench           整句基线评测工具
#   pinyin_corrector_test  M3 纠错对拍自检
#   quick_input_test       搜狗式快速输入单元测试
#   libpinyinime.so        fcitx5 addon
#   pinyinime.conf         addon 配置
#
# 注意：产物依赖目标机的 libime / fcitx5 运行库（apt install libimepinyin libimecore
#       libfcitx5core libfcitx5config libfcitx5utils libime-data 即可）。

set -euo pipefail

REPO="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$REPO/dist/ubuntu"
mkdir -p "$OUT"

build_one() {
    local arch="$1"
    local dest="$OUT/$arch"
    echo "=================================================="
    echo " 构建 Ubuntu 24.04 ($arch)  —  linux/$arch"
    echo " 导出目录: $dest"
    echo "=================================================="
    rm -rf "$dest"
    docker buildx build \
        --platform "linux/$arch" \
        -f "$REPO/docker/Dockerfile.ubuntu" \
        --output "type=local,dest=$dest" \
        "$REPO"
    echo "✅ $arch 产物已导出到 $dest"
    echo "--- 产物清单 ---"
    ls -lh "$dest"
}

build_one arm64
build_one amd64

echo ""
echo "🎉 全部完成。产物目录: $OUT/{arm64,amd64}"
echo "下一步验证："
echo "  file dist/ubuntu/arm64/pinyin_ime_cli dist/ubuntu/amd64/pinyin_ime_cli"
