#!/usr/bin/env bash
# 上传 pinyin-ime 构建产物到 CSTCloud S3
#
# 用法：bash scripts/upload_to_s3.sh [version]
#   不指定 version 则从 VERSION 文件读取
#
# 上传内容：
#   - 源码 tarball（packaging/build/pinyin-ime-{ver}.tar.gz）
#   - 二进制发行包（packaging/build/pinyin-ime-{ver}-bin.tar.gz）
#   - CLI 二进制（cpp/build/pinyin_ime_cli）
#   - fcitx5 addon（fcitx5-addon/build/libpinyinime.so）
#   - fcitx5 服务（$OPT/fcitx5-x11/bin/fcitx5，桌面集成版）
#   - 语言模型（zh_CN.lm, 34MB）
#   - 词典（sc.dict, 4.3MB）
#   - SHA256SUMS 校验文件
#
# S3 路径：cst:xbz/pinyin-ime/{version}/
# rclone 配置见 scripts/download_from_s3.sh 或 INSTALL.md
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "${0}")" && pwd)"
PROJ_DIR="$(dirname "${SCRIPT_DIR}")"
NB="${NB:-/workspace/native-build}"
OPT="$NB/opt"
ENV="$NB/mamba-env"

# 版本号
if [ $# -ge 1 ]; then
    VERSION="$1"
elif [ -f "$PROJ_DIR/VERSION" ]; then
    VERSION=$(cat "$PROJ_DIR/VERSION")
else
    VERSION="0.9.77"
fi
echo "=== 上传 pinyin-ime v${VERSION} 到 CSTCloud S3 ==="

# 检查 rclone
if ! command -v rclone &>/dev/null; then
    echo "错误：rclone 未安装。请先安装：curl -O https://downloads.rclone.org/rclone-current-linux-arm64.zip"
    exit 1
fi
if ! rclone listremotes 2>/dev/null | grep -q '^cst:'; then
    echo "错误：rclone 未配置 cst 远程。请参考 INSTALL.md 配置 ~/.config/rclone/rclone.conf"
    exit 1
fi

# 准备临时目录
STAGE=$(mktemp -d)
trap "rm -rf $STAGE" EXIT

# 收集文件
echo "--- 收集构建产物 ---"
cp "$PROJ_DIR/packaging/build/pinyin-ime-${VERSION}.tar.gz" "$STAGE/" 2>/dev/null || echo "  警 源码 tarball 不存在"
cp "$PROJ_DIR/packaging/build/pinyin-ime-${VERSION}-bin.tar.gz" "$STAGE/" 2>/dev/null || echo "  覦 二进制包不存在"
cp "$PROJ_DIR/cpp/build/pinyin_ime_cli" "$STAGE/pinyin_ime_cli_v${VERSION}" 2>/dev/null || echo "  覦 CLI 不存在"
cp "$PROJ_DIR/fcitx5-addon/build/libpinyinime.so" "$STAGE/libpinyinime_v${VERSION}.so" 2>/dev/null || echo "  覦 addon 不存在"
cp "$OPT/fcitx5-x11/bin/fcitx5" "$STAGE/fcitx5_v${VERSION}" 2>/dev/null || echo "  覦 fcitx5-x11 不存在"
cp "$OPT/libime/lib/libime/zh_CN.lm" "$STAGE/zh_CN.lm" 2>/dev/null || echo "  覦 zh_CN.lm 不存在"
cp "$OPT/libime/share/libime/sc.dict" "$STAGE/sc.dict" 2>/dev/null || echo "  覦 sc.dict 不存在"

# 生成 SHA256SUMS
echo "--- 生成 SHA256SUMS ---"
cd "$STAGE"
sha256sum * > "SHA256SUMS_v${VERSION}"
cat "SHA256SUMS_v${VERSION}"

# 上传
echo ""
echo "--- 上传到 cst:xbz/pinyin-ime/v${VERSION}/ ---"
rclone copy "$STAGE/" "cst:xbz/pinyin-ime/v${VERSION}/" \
    --user-agent "rclone/1.75.1 pinyin-ime" \
    --progress

echo ""
echo "=== 上传完成 ==="
echo "下载路径：cst:xbz/pinyin-ime/v${VERSION}/"
echo "文件列表："
rclone ls "cst:xbz/pinyin-ime/v${VERSION}/"
