#!/bin/bash
# 从 CSTCloud S3 下载 pinyin-ime 构建产物
# 用法: ./download_from_s3.sh [目标目录] [版本号]
set -e

DEST="${1:-./pinyin-ime-artifacts}"
VERSION="${2:-v0.9.77}"
BUCKET="xbz"
S3_PATH="cst:${BUCKET}/pinyin-ime/${VERSION}/"

echo "=== pinyin-ime S3 下载工具 ==="
echo "版本: ${VERSION}"
echo "目标: ${DEST}"
echo "来源: ${S3_PATH}"

# 检查 rclone
if ! command -v rclone &>/dev/null; then
    echo "rclone 未安装，正在安装..."
    ARCH=$(uname -m)
    case "$ARCH" in
        x86_64)  RARCH="linux-amd64" ;;
        aarch64) RARCH="linux-arm64" ;;
        *) echo "不支持的架构: $ARCH"; exit 1 ;;
    esac
    curl -O "https://downloads.rclone.org/rclone-current-${RARCH}.zip"
    unzip "rclone-current-${RARCH}.zip"
    cp "rclone-current-${RARCH}/rclone" /usr/local/bin/
    rm -rf "rclone-current-${RARCH}"*
fi

# 检查 S3 配置
if ! grep -q "\[cst\]" ~/.config/rclone/rclone.conf 2>/dev/null; then
    echo "错误: rclone 未配置 cst 远端"
    echo "请先配置:"
    echo "  mkdir -p ~/.config/rclone"
    echo "  cat > ~/.config/rclone/rclone.conf << 'EOF'"
    echo "  [cst]"
    echo "  type = s3"
    echo "  endpoint = https://s3.cstcloud.cn"
    echo "  access_key_id = <your-ak>"
    echo "  secret_access_key = <your-sk>"
    echo "  force_path_style = true"
    echo "  acl = private"
    echo "  provider = Other"
    echo "  EOF"
    exit 1
fi

# 下载
mkdir -p "$DEST"
echo "正在下载..."
rclone copy "$S3_PATH" "$DEST/" \
    --user-agent "rclone/1.75.1 pinyin-ime" \
    --progress

echo ""
echo "=== 下载完成 ==="
echo "文件列表:"
find "$DEST" -type f | sort

# 显示 MANIFEST
if [ -f "$DEST/MANIFEST.txt" ]; then
    echo ""
    echo "=== 产物清单 ==="
    cat "$DEST/MANIFEST.txt"
fi

echo ""
echo "=== 快速验证 ==="
if [ -f "$DEST/bin/pinyin_ime_cli" ] && [ -f "$DEST/data/zh_CN.lm" ] && [ -f "$DEST/data/sc.dict" ]; then
    echo "nihao" | "$DEST/bin/pinyin_ime_cli" "$DEST/data/zh_CN.lm" "$DEST/data/sc.dict" 2>/dev/null && echo "✅ CLI 可用" || echo "⚠️  CLI 需要运行时库 (libime, fcitx5)"
else
    echo "⚠️  关键文件缺失"
fi
