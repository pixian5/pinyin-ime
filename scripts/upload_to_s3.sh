#!/bin/bash
# pinyin-ime S3 上传脚本
# 将打包好的 tarball 上传到 S3 (CSTCloud)
#
# 用法:
#   bash scripts/upload_to_s3.sh                    # 上传最新打包
#   bash scripts/upload_to_s3.sh /path/to/file.tar.gz  # 上传指定文件

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

VERSION=$(cat VERSION 2>/dev/null || echo "unknown")
S3_BUCKET="pinyin-ime"
S3_PREFIX="releases"

log() { echo -e "\033[32m[s3]\033[0m $*"; }
err() { echo -e "\033[31m[error]\033[0m $*" >&2; }

# Determine file to upload
if [ -n "${1:-}" ]; then
    TARBALL="$1"
else
    TARBALL="/tmp/pinyin-ime-v${VERSION}.tar.gz"
fi

if [ ! -f "$TARBALL" ]; then
    err "文件不存在: $TARBALL"
    err "请先运行: bash scripts/package.sh"
    exit 1
fi

# Check rclone
if ! command -v rclone &>/dev/null; then
    err "rclone 未安装"
    exit 1
fi

# Check rclone remote
if ! rclone listremotes 2>/dev/null | grep -q "s3:"; then
    err "rclone 未配置 s3 remote"
    err "请运行: rclone config create s3 s3 ..."
    exit 1
fi

SHA256_FILE="${TARBALL}.sha256"
if [ ! -f "$SHA256_FILE" ]; then
    log "生成校验和..."
    sha256sum "$TARBALL" > "$SHA256_FILE"
fi

log "上传 pinyin-ime v${VERSION} 到 S3..."
echo "  文件: $TARBALL"
echo "  大小: $(du -h "$TARBALL" | cut -f1)"
echo ""

# Upload tarball
log "Step 1: 上传 tarball..."
rclone copy "$TARBALL" "s3:${S3_BUCKET}/${S3_PREFIX}/v${VERSION}/" --progress
echo ""

# Upload checksum
log "Step 2: 上传 SHA256..."
rclone copy "$SHA256_FILE" "s3:${S3_BUCKET}/${S3_PREFIX}/v${VERSION}/"
echo ""

# Verify
log "Step 3: 验证上传..."
rclone ls "s3:${S3_BUCKET}/${S3_PREFIX}/v${VERSION}/" | while read size path; do
    echo "  ✅ $path ($size bytes)"
done

echo ""
log "上传完成！"
log "下载地址: s3://${S3_BUCKET}/${S3_PREFIX}/v${VERSION}/$(basename "$TARBALL")"
