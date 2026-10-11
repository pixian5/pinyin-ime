#!/bin/bash
# pinyin-ime 打包脚本
# 将当前构建产物打包成可分发的 tarball
#
# 产物结构:
#   pinyin-ime-v{VERSION}.tar.gz
#   ├── addon/libpinyinime.so
#   ├── addon/pinyinime.conf
#   ├── lib/libIMECore.so.1.1.17
#   ├── lib/libIMEPinyin.so.1.1.17
#   ├── lib/libIMETable.so.1.1.17
#   ├── lib/libxcb-imdkit.so.1.0.9
#   ├── data/zh_CN.lm
#   ├── data/sc.dict
#   ├── bin/pinyin_ime_cli
#   ├── scripts/setup_and_run.sh
#   ├── scripts/run_im.sh
#   ├── scripts/healthcheck.sh
#   ├── scripts/ci_test.sh
#   ├── scripts/install.sh
#   └── README.md

set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

VERSION=$(cat VERSION 2>/dev/null || echo "unknown")
PKG_NAME="pinyin-ime-v${VERSION}"
PKG_DIR="/tmp/${PKG_NAME}"
OUTPUT="/tmp/${PKG_NAME}.tar.gz"

BUNDLE="/root/.local/pinyin-ime/opt/fcitx5-x11"
DEPSLIB="/root/.local/pinyin-ime/opt/deps-lib"
LIBIME="/root/workspace/native-build/opt/libime/lib64"
DATA_DIR="/root/.local/pinyin-ime/share/libime"
CLI="${PROJECT_ROOT}/pinyin-ime-s3/pinyin_ime_cli_v0.9.83"
LM="${PROJECT_ROOT}/pinyin-ime-s3/zh_CN.lm"
DICT="${PROJECT_ROOT}/pinyin-ime-s3/sc.dict"

log() { echo -e "\033[32m[package]\033[0m $*"; }

log "打包 pinyin-ime v${VERSION}..."

# Clean
rm -rf "$PKG_DIR" "$OUTPUT"
mkdir -p "$PKG_DIR"/{addon,lib,data,bin,scripts}

# 1. Addon
log "Step 1: 复制 addon..."
cp "$BUNDLE/lib/fcitx5/libpinyinime.so" "$PKG_DIR/addon/"
cp "$BUNDLE/share/fcitx5/addon/pinyinime.conf" "$PKG_DIR/addon/"

# 2. Libraries
log "Step 2: 复制 libIME 库..."
cp "$LIBIME"/libIME{Core,Pinyin,Table}.so.1.1.17 "$PKG_DIR/lib/"
if [ -f "$DEPSLIB/lib64/libxcb-imdkit.so.1.0.9" ]; then
    cp "$DEPSLIB/lib64/libxcb-imdkit.so.1.0.9" "$PKG_DIR/lib/"
fi

# 3. Data files
log "Step 3: 复制数据文件..."
cp "$LM" "$PKG_DIR/data/"
cp "$DICT" "$PKG_DIR/data/"

# 4. CLI binary
log "Step 4: 复制 CLI..."
cp "$CLI" "$PKG_DIR/bin/pinyin_ime_cli"

# 5. Scripts
log "Step 5: 复制脚本..."
cp scripts/setup_and_run.sh "$PKG_DIR/scripts/"
cp run_im.sh "$PKG_DIR/scripts/"
cp scripts/healthcheck.sh "$PKG_DIR/scripts/"
cp scripts/ci_test.sh "$PKG_DIR/scripts/"
cp scripts/install.sh "$PKG_DIR/scripts/"
cp scripts/fuzzy_pinyin_test.py "$PKG_DIR/scripts/" 2>/dev/null || true
cp scripts/performance_test.py "$PKG_DIR/scripts/" 2>/dev/null || true
cp scripts/user_dict_persistence_test.py "$PKG_DIR/scripts/" 2>/dev/null || true

# 6. Docs
log "Step 6: 复制文档..."
cp README.md "$PKG_DIR/"
cp VERSION "$PKG_DIR/"

# 7. Create tarball
log "Step 7: 创建 tarball..."
tar czf "$OUTPUT" -C /tmp "$PKG_NAME"

# 8. Checksum
log "Step 8: 生成校验和..."
sha256sum "$OUTPUT" > "${OUTPUT}.sha256"

# Summary
log "打包完成！"
echo ""
echo "  产物:     $OUTPUT"
echo "  大小:     $(du -h "$OUTPUT" | cut -f1)"
echo "  SHA256:   $(cat "${OUTPUT}.sha256" | awk '{print $1}')"
echo "  内容:"
tar tzf "$OUTPUT" | head -20
echo ""
