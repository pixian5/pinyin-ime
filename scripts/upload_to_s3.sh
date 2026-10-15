#!/bin/bash
# upload_to_s3.sh —— 把 pinyin-ime 的「只读 /usr 运行脚本包」上传到 CSTCloud S3。
#
# 真实布局（rclone 远程 cst，桶 xbz）：
#   cst:xbz/pinyin-ime/<version>/pinyin-ime-ro-runtime-<version>.tar.gz(.sha256)
#
# 用法:
#   bash scripts/upload_to_s3.sh [version]     # 版本如 v0.9.94 或 0.9.94；默认取 VERSION
#   RCLONE_REMOTE=cst S3_BUCKET=xbz S3_PREFIX=pinyin-ime bash scripts/upload_to_s3.sh v0.9.94
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

RAW="${1:-$(cat VERSION 2>/dev/null || echo unknown)}"
VERSION="v${RAW#v}"
REMOTE="${RCLONE_REMOTE:-cst}"
BUCKET="${S3_BUCKET:-xbz}"
PREFIX="${S3_PREFIX:-pinyin-ime}"
DEST="${REMOTE}:${BUCKET}/${PREFIX}/${VERSION}/"
UA="rclone/1.75.1 pinyin-ime"

log(){ echo -e "\033[32m[s3]\033[0m $*"; }
err(){ echo -e "\033[31m[s3]\033[0m $*" >&2; }

command -v rclone >/dev/null || { err "rclone 未安装"; exit 1; }
rclone listremotes 2>/dev/null | grep -q "^${REMOTE}:" || { err "未配置 rclone 远程 ${REMOTE}"; exit 1; }

STAGE="$(mktemp -d)"
trap 'rm -rf "$STAGE"' EXIT
BUNDLE="pinyin-ime-ro-${VERSION}"
mkdir -p "$STAGE/$BUNDLE/scripts" "$STAGE/$BUNDLE/tools"

# 只读 /usr 沙箱复现所需脚本 + 垫片源码
copy_if() { [ -e "$1" ] && cp -f "$1" "$2/" || true; }
for f in start_im_env_ro.sh start_im_env_x.sh repro_verify.sh \
         reproduce_from_scratch.sh setup_local_runtime.sh \
         e2e_run.sh pinyin-ime-cli-wrapper.sh xvfb_x11_ime_test.sh; do
    copy_if "scripts/$f" "$STAGE/$BUNDLE/scripts"
done
copy_if "scripts/e2e_input_test.py" "$STAGE/$BUNDLE/scripts"
copy_if "tools/xkbcomp_redirect.c" "$STAGE/$BUNDLE/tools"

cat > "$STAGE/$BUNDLE/README-RO.txt" <<EOF
pinyin-ime 只读 /usr 沙箱运行脚本包 ${VERSION}
- 用途：在 /usr、/etc 只读、无 CAP_SYS_ADMIN、无 conda/docker 的内机上，
  以 S3 预构建产物（v0.9.84a + v0.9.83 + v0.9.84）跑起 fcitx5 + pinyinime。
- 新增 ${VERSION}：X11 图形层打通（Xvfb + XIM）。
  tools/xkbcomp_redirect.c 为 LD_PRELOAD 垫片，把 X server 硬编码的
  /usr/bin/xkbcomp 重定向到 sysroot；scripts/start_im_env_x.sh 启动 X 模式；
  scripts/xvfb_x11_ime_test.sh 为 XIM 桌面用例矩阵。
- 先决条件：已克隆仓库、rclone 远程 cst 就绪、/workspace/sysroot（X/dbus）、
  /workspace/deploy（S3 产物）。见 scripts/reproduce_from_scratch.sh。
EOF

cat > "$STAGE/$BUNDLE/MANIFEST.txt" <<EOF
v${RAW#v}
$(cd "$STAGE/$BUNDLE" && find . -type f | sort)
EOF

log "打包 ro-runtime ${VERSION} …"
tar czf "$STAGE/pinyin-ime-ro-runtime-${VERSION}.tar.gz" -C "$STAGE" "$BUNDLE"
( cd "$STAGE" && sha256sum "pinyin-ime-ro-runtime-${VERSION}.tar.gz" \
    > "pinyin-ime-ro-runtime-${VERSION}.tar.gz.sha256" )

log "上传到 ${DEST} …"
rclone copy "$STAGE/pinyin-ime-ro-runtime-${VERSION}.tar.gz"        "$DEST" --user-agent "$UA"
rclone copy "$STAGE/pinyin-ime-ro-runtime-${VERSION}.tar.gz.sha256" "$DEST" --user-agent "$UA"

log "完成。远端文件："
rclone ls "$DEST" --user-agent "$UA" | sed "s#^#  #"
