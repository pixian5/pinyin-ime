#!/bin/bash
# 从 CSTCloud S3 下载 pinyin-ime 构建产物。
#
# 用法:
#   bash scripts/download_from_s3.sh [remote] [version] [dest]
#     remote   默认 cst（rclone 远程名）
#     version  默认取远端最新版本（rclone lsd 取字典序最新）
#     dest     默认 ./pinyin-ime-s3/<version>/
#
# 例:
#   bash scripts/download_from_s3.sh                     # 最新版本 → ./pinyin-ime-s3/<v>/
#   bash scripts/download_from_s3.sh cst v0.9.84a        # 指定版本
#   bash scripts/download_from_s3.sh cst v0.9.83 ./tmp   # 指定目录
set -euo pipefail

REMOTE="${1:-cst}"
VERSION="${2:-}"
BUCKET="${S3_BUCKET:-xbz}"
PREFIX="${S3_PREFIX:-pinyin-ime}"
UA="rclone/1.75.1 pinyin-ime"

echo "=== pinyin-ime S3 下载工具 ==="
echo "远程: ${REMOTE}  桶: ${BUCKET}  前缀: ${PREFIX}"

# 1) 确保 rclone
if ! command -v rclone >/dev/null 2>&1; then
    echo "rclone 未安装，请先安装（如：gh-proxy 下载静态二进制，或系统包管理器）。"
    exit 1
fi

# 2) 确保远程配置存在
rclone listremotes 2>/dev/null | grep -q "^${REMOTE}:" || {
    cat >&2 <<EOF
错误: rclone 未配置远程 '${REMOTE}'。请先:
  mkdir -p ~/.config/rclone
  cat > ~/.config/rclone/rclone.conf <<'CONF'
[${REMOTE}]
type = s3
provider = Other
endpoint = https://s3.cstcloud.cn
access_key_id = <AK>
secret_access_key = <SK>
acl = private
force_path_style = true
CONF
EOF
    exit 1
}

BASE="${REMOTE}:${BUCKET}/${PREFIX}"

# 3) 取最新版本（若未指定）
if [ -z "$VERSION" ]; then
    VERSION="$(rclone lsf "${BASE}/" --dirs-only --user-agent "$UA" | sed 's#/$##' | sort -V | tail -1)"
    [ -n "$VERSION" ] || { echo "无法获取版本列表"; exit 1; }
    echo "最新版本: ${VERSION}"
fi

DEST="${3:-./pinyin-ime-s3/${VERSION}/}"
SRC="${BASE}/${VERSION}/"
echo "来源: ${SRC}"
echo "目标: ${DEST}"

mkdir -p "$DEST"
echo "正在下载…"
rclone copy "$SRC" "$DEST/" --user-agent "$UA" --progress

echo ""
echo "=== 下载完成，文件列表 ==="
find "$DEST" -type f | sort

# 校验（若含 SHA256SUMS* 或 *.sha256）
found=0
for sf in "$DEST"/SHA256SUMS* "$DEST"/*.sha256; do
    [ -e "$sf" ] || continue
    found=1
    echo ""
    echo "=== 校验 $(basename "$sf") ==="
    ( cd "$DEST" && sha256sum -c "$(basename "$sf")" 2>&1 | tail -20 ) || true
done
[ "$found" = 0 ] && echo "（未发现校验文件）"

echo ""
echo "=== 快速验证 ==="
if [ -f "$DEST/bin/pinyin_ime_cli" ] && [ -f "$DEST/data/zh_CN.lm" ] && [ -f "$DEST/data/sc.dict" ]; then
    echo "nihao" | "$DEST/bin/pinyin_ime_cli" "$DEST/data/zh_CN.lm" "$DEST/data/sc.dict" 2>/dev/null \
        && echo "✅ CLI 可用" || echo "⚠️  CLI 需要运行时库 (libime/fcitx5)"
else
    echo "(无 CLI 目录结构，可能是 ro-runtime/桌面包，见上方文件列表)"
fi
