#!/usr/bin/env bash
# 用 conda-forge 工具链（gcc 13 + boost/fmt/zstd/libuv/ECM/gettext/libuuid）从源码构建
# fcitx5 5.1.23 与 libime 1.1.17。
#
# 与 build_conda_forge.sh（原 aarch64 版）的区别：
#   - 本机是 x86_64，conda 工具链前缀为 x86_64-conda-linux-gnu-*（非 aarch64-*）
#   - 系统 apt 已装 cmake/ninja/pkg-config 与部分 dev 头（fcitx5 configure 仍需
#     部分 X11/dbus dev 头，apt 补齐见下方 apt-get 注释）
#   - 不依赖 docker，直接在宿主编译
#
# 前置（Debian 12，已验证可行）：
#   apt-get install -y cmake ninja-build pkg-config libuv1-dev \
#     extra-cmake-modules nlohmann-json3-dev libxcb-imdkit-dev \
#     libxkbcommon-dev libxcb-keysyms1-dev libxcb-xkb-dev libxkbcommon-x11-dev \
#     libudev-dev libxml2-dev libsqlite3-dev gettext libdbus-1-dev libgpgme-dev \
#     libx11-dev libxtst-dev uuid-dev zlib1g-dev iso-codes libxcb-icccm4-dev \
#     libxcb-image0-dev libxcb-render-util0-dev libxcb-xinerama0-dev libxkbfile-dev \
#     libx11-xcb-dev
#
# 已知坑（踩过）：
#   1. fcitx5 5.1.23 源码用 `#include <format>`，需 GCC 13+；Debian 12 系统 gcc 是 12，
#      必须用 conda-forge 的 gcc-13（本脚本 PATH 前置 mamba-env/bin 即可）
#   2. fcitx5 configure 关 USE_SYSTEMD=Off 后需要 libuv 作 event loop backend，
#      否则报 "Failed to find a valid event loop backend. Backends checked: libuv"
#   3. libime 源码里 kenlm 是 git submodule，用 tarball 下载会缺
#      src/libime/core/kenlm/lm/build_binary_main.cc，必须 `git clone --recurse-submodules`
#   4. libime install 后 LM 被 RENAME 成 zh_CN.lm（在 lib/libime/ 下），
#      不是 share/libime/sc.lm；dict 才在 share/libime/sc.dict
#   5. conda 工具链的 cmake 是 4.x，比系统的 3.25 新，二者可共存但 configure 时
#      必须显式指定 CMAKE_C_COMPILER / CMAKE_CXX_COMPILER 指向 conda 的 cc/c++
#   6. 内存仅 3.7GB 无 swap：lmplz 训大 LM 会 OOM，需 -S 限制排序内存或加 swap
set -euo pipefail

NB=/root/workspace/native-build
ENV=$NB/mamba-env
OPT=$NB/opt
BUILD=$NB/build
SRC=$NB/src

# ---------- 前置检查 ----------
echo "=== 前置检查 ==="
fail=0
check() { if ! command -v "$1" >/dev/null 2>&1; then echo "  缺少: $1"; fail=1; else echo "  找到: $1 ($($1 --version 2>&1 | head -1))"; fi; }
check cmake
check ninja
check pkg-config
[ -d "$ENV" ] || { echo "缺少 conda env: $ENV"; fail=1; }
[ -d "$SRC/fcitx5-5.1.23" ] || { echo "缺少源码: $SRC/fcitx5-5.1.23"; fail=1; }
[ -d "$SRC/libime-1.1.17" ] || { echo "缺少源码: $SRC/libime-1.1.17"; fail=1; }
[ -f "$SRC/libime-1.1.17/src/libime/core/kenlm/lm/build_binary_main.cc" ] || {
  echo "缺少 kenlm submodule，请先: git clone --depth 1 --branch 1.1.17 --recurse-submodules https://github.com/fcitx/libime.git $SRC/libime-1.1.17"; fail=1; }
if [ "$fail" -ne 0 ]; then echo "前置检查未通过"; exit 1; fi
echo "前置检查通过"
echo

export PATH="$ENV/bin:$PATH"
CC="$ENV/bin/x86_64-conda-linux-gnu-cc"
CXX="$ENV/bin/x86_64-conda-linux-gnu-c++"
export CMAKE_PREFIX_PATH="$ENV"
export PKG_CONFIG_PATH="$ENV/lib/pkgconfig:$ENV/share/pkgconfig"
export LDFLAGS="-Wl,-rpath,$ENV/lib"
export LD_LIBRARY_PATH="$ENV/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# ---------- fcitx5 ----------
FCITX_BUILD=$BUILD/fcitx5
mkdir -p "$FCITX_BUILD" "$OPT/fcitx5" "$NB/logs"
echo "=== configure fcitx5 ==="
cmake -S "$SRC/fcitx5-5.1.23" -B "$FCITX_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$OPT/fcitx5" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_PREFIX_PATH="$ENV" \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DENABLE_TESTING_ADDONS=Off \
    -DENABLE_TEST=Off \
    -DENABLE_COVERAGE=Off \
    -DENABLE_ENCHANT=Off \
    -DENABLE_X11=Off \
    -DENABLE_WAYLAND=Off \
    -DENABLE_DBUS=Off \
    -DENABLE_DOC=Off \
    -DENABLE_SERVER=Off \
    -DENABLE_KEYBOARD=Off \
    -DUSE_SYSTEMD=Off \
    -DENABLE_XDGAUTOSTART=Off \
    -DENABLE_EMOJI=Off \
    -DBUILD_SPELL_DICT=Off \
    -DENABLE_LIBUUID=On \
    -DENABLE_DL=On \
    > "$NB/logs/fcitx5-configure.log" 2>&1
echo "=== build fcitx5 ==="
ninja -C "$FCITX_BUILD" > "$NB/logs/fcitx5-build.log" 2>&1
echo "=== install fcitx5 ==="
ninja -C "$FCITX_BUILD" install > "$NB/logs/fcitx5-install.log" 2>&1
echo "FCITX5 DONE"

# ---------- libime ----------
export CMAKE_PREFIX_PATH="$OPT/fcitx5;$ENV"
LIBIME_BUILD=$BUILD/libime
mkdir -p "$LIBIME_BUILD" "$OPT/libime"
echo "=== configure libime ==="
cmake -S "$SRC/libime-1.1.17" -B "$LIBIME_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$OPT/libime" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_PREFIX_PATH="$OPT/fcitx5;$ENV" \
    -DCMAKE_C_COMPILER="$CC" \
    -DCMAKE_CXX_COMPILER="$CXX" \
    -DFcitx5Utils_DIR="$OPT/fcitx5/lib/cmake/Fcitx5Utils" \
    -DENABLE_TEST=Off \
    -DENABLE_DOC=Off \
    -DENABLE_DATA=On \
    -DENABLE_TOOLS=On \
    > "$NB/logs/libime-configure.log" 2>&1
echo "=== build libime (incl. kenlm + zh_CN.lm + sc.dict) ==="
ninja -C "$LIBIME_BUILD" > "$NB/logs/libime-build.log" 2>&1
echo "=== install libime ==="
ninja -C "$LIBIME_BUILD" install > "$NB/logs/libime-install.log" 2>&1
echo "LIBIME DONE"

# ---------- 产物确认 ----------
echo
echo "=== 产物确认 ==="
LM="$OPT/libime/lib/libime/zh_CN.lm"
DICT="$OPT/libime/share/libime/sc.dict"
if [ -f "$LM" ]; then echo "  LM:   $LM ($(stat -c%s "$LM") B)"; else echo "  LM 缺失!"; exit 1; fi
if [ -f "$DICT" ]; then echo "  DICT: $DICT ($(stat -c%s "$DICT") B)"; else echo "  DICT 缺失!"; exit 1; fi
echo "ALL BUILDS DONE"
echo
echo "下一步（验证构建真实可用）："
echo "  cd <repo>/bench && .venv/bin/python run_bench.py --corpus corpus/news.txt \\"
echo "    --cli ../cpp/build/libime_bench --cli-arg $LM --cli-arg $DICT --cli-arg 5"
echo "  预期: 句级 39.60% / 字级 88.82%（与 README 基线一致）"
