#!/bin/bash
# setup_local_runtime.sh —— 在只读 /usr 的机器上，用 S3 预构建产物搭出
# native-build 运行目录（不写入 /usr、/etc），供 smoke_test.sh 等复用。
#
# 用法: bash scripts/setup_local_runtime.sh
#   环境变量:
#     DEPLOY  = 预构建产物解压根 (默认 /workspace/deploy)
#              需含 pinyin-ime-v0.9.84a/ 与 pinyin-ime/
#     NB      = 目标 native-build 根 (默认 $REPO/native-build)
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPLOY="${DEPLOY:-/workspace/deploy}"
NB="${NB:-$REPO_DIR/native-build}"

RT="$DEPLOY/pinyin-ime-v0.9.84a"          # 主 bundle: CLI + libIME + addon + data
DESK="$DEPLOY/pinyin-ime"                  # 桌面 bundle: fcitx5-x11 + deps-lib + data
FCITX_LIB="$DESK/opt/fcitx5-x11/lib"
DEPS_LIB="$DESK/opt/deps-lib"
LIBIME="$RT/lib"

echo "[setup] REPO=$REPO_DIR  DEPLOY=$DEPLOY  NB=$NB"

[ -x "$RT/bin/pinyin_ime_cli" ] || { echo "缺失 $RT/bin/pinyin_ime_cli"; exit 1; }

mkdir -p \
  "$NB/opt/libime/lib/libime" \
  "$NB/opt/libime/share/libime" \
  "$NB/opt/fcitx5/lib" \
  "$NB/mamba-env/lib" \
  "$NB/cpp/build"

# 1) 数据文件
cp -f "$RT/data/zh_CN.lm" "$NB/opt/libime/lib/libime/zh_CN.lm"
cp -f "$RT/data/sc.dict"  "$NB/opt/libime/share/libime/sc.dict"
cp -f "$RT/data/zh_CN.lm" "$NB/opt/libime/share/libime/zh_CN.lm" 2>/dev/null || true
cp -f "$RT/data/sc.dict"  "$NB/opt/libime/lib/libime/sc.dict" 2>/dev/null || true

# 2) libIME 库 + SONAME 符号链接 (.0 / 无后缀)
for l in libIMECore libIMEPinyin libIMETable; do
  ln -sf "$LIBIME/${l}.so.1.1.17" "$NB/opt/libime/lib/${l}.so.1.1.17"
  ln -sf "${l}.so.1.1.17"          "$NB/opt/libime/lib/${l}.so.0"
  ln -sf "${l}.so.1.1.17"          "$NB/opt/libime/lib/${l}.so"
done
[ -f "$LIBIME/libxcb-imdkit.so.1.0.9" ] && {
  ln -sf "$LIBIME/libxcb-imdkit.so.1.0.9" "$NB/opt/libime/lib/libxcb-imdkit.so.1.0.9"
  ln -sf "libxcb-imdkit.so.1.0.9" "$NB/opt/libime/lib/libxcb-imdkit.so.1"
  ln -sf "libxcb-imdkit.so.1.0.9" "$NB/opt/libime/lib/libxcb-imdkit.so"
}

# 3) Fcitx5 核心库 (Utils/Core/Config/GClient/xkbfile)
for so in "$FCITX_LIB"/libFcitx5*.so*; do
  [ -e "$so" ] && ln -sf "$so" "$NB/opt/fcitx5/lib/"
done
for xk in "$FCITX_LIB"/libxkbfile.so*; do
  [ -e "$xk" ] && ln -sf "$xk" "$NB/opt/fcitx5/lib/"
done

# 4) mamba-env/lib = deps-lib (提供新版 libstdc++/ICU 等)
for lib in "$DEPS_LIB"/*.so*; do
  [ -e "$lib" ] && ln -sf "$lib" "$NB/mamba-env/lib/"
done

# 5) CLI 二进制
cp -f "$RT/bin/pinyin_ime_cli" "$NB/cpp/build/pinyin_ime_cli"
chmod +x "$NB/cpp/build/pinyin_ime_cli"

echo "[setup] 完成。可用: NB=$NB bash scripts/smoke_test.sh"
echo "[setup] 本机 ip.txt 首行: $(head -1 "$REPO_DIR/ip.txt")"
