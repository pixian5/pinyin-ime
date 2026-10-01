#!/usr/bin/env bash
# 用 conda-forge 工具链（gcc 15 + boost/fmt/zstd/libuv/ECM/gettext/libuuid）从源码构建 fcitx5 5.1.23 与 libime 1.1.17。
# 系统 /usr 为只读，无法用 dnf，故全部依赖走 /root/workspace/native-build/mamba-env。
set -euo pipefail

NB=${NB:-/root/workspace/native-build}
ENV=$NB/mamba-env
OPT=$NB/opt
BUILD=$NB/build

export PATH="$ENV/bin:$PATH"
export CC="$ENV/bin/aarch64-conda-linux-gnu-cc"
export CXX="$ENV/bin/aarch64-conda-linux-gnu-c++"
export CMAKE_PREFIX_PATH="$ENV"
export PKG_CONFIG_PATH="$ENV/lib/pkgconfig:$ENV/share/pkgconfig"
export LDFLAGS="-Wl,-rpath,$ENV/lib"
export LD_LIBRARY_PATH="$ENV/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

# 国内直连 download.fcitx-im.org / github 慢，统一走本地 mihomo 代理（可空=直连）
PROXY="${PROXY:-http://127.0.0.1:7890}"
if [ -n "$PROXY" ]; then
    export http_proxy="$PROXY" https_proxy="$PROXY" all_proxy="$PROXY"
    export HTTP_PROXY="$PROXY" HTTPS_PROXY="$PROXY" ALL_PROXY="$PROXY"
fi

# ---------- fcitx5 ----------
FCITX_BUILD=$BUILD/fcitx5
mkdir -p "$FCITX_BUILD"
echo "=== configure fcitx5 ==="
cmake -S "$NB/src/fcitx5-5.1.23" -B "$FCITX_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$OPT/fcitx5" \
    -DCMAKE_INSTALL_LIBDIR=lib \
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
    > "$BUILD/fcitx5-configure.log" 2>&1
echo "=== build fcitx5 ==="
ninja -C "$FCITX_BUILD" > "$BUILD/fcitx5-build.log" 2>&1
echo "=== install fcitx5 ==="
ninja -C "$FCITX_BUILD" install > "$BUILD/fcitx5-install.log" 2>&1
echo "FCITX5 DONE"

# ---------- libime ----------
export CMAKE_PREFIX_PATH="$OPT/fcitx5;$ENV"
LIBIME_BUILD=$BUILD/libime
mkdir -p "$LIBIME_BUILD"
echo "=== configure libime ==="
cmake -S "$NB/src/libime" -B "$LIBIME_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$OPT/libime" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DCMAKE_PREFIX_PATH="$OPT/fcitx5;$ENV" \
    -DFcitx5Utils_DIR="$OPT/fcitx5/lib/cmake/Fcitx5Utils" \
    -DENABLE_TEST=Off \
    -DENABLE_DOC=Off \
    -DENABLE_DATA=On \
    -DENABLE_TOOLS=On \
    > "$BUILD/libime-configure.log" 2>&1
echo "=== build libime (incl. kenlm + zh_CN.lm + sc.dict) ==="
ninja -C "$LIBIME_BUILD" > "$BUILD/libime-build.log" 2>&1
echo "=== install libime ==="
ninja -C "$LIBIME_BUILD" install > "$BUILD/libime-install.log" 2>&1
echo "ALL BUILDS DONE"
# ---------- lmplz（独立 KenLM，仅用于「训练」；解码无需）----------
KENLM_SRC=$NB/src/libime/src/libime/core/kenlm
KENLM_BUILD=$BUILD/kenlm
mkdir -p "$KENLM_BUILD"
cmake -S "$KENLM_SRC" -B "$KENLM_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DBOOST_ROOT="$ENV" -DBoost_NO_SYSTEM_PATHS=ON \
    -DBUILD_TESTING=Off -DENABLE_PYTHON=Off -DENABLE_INTERPOLATE=Off \
    > "$BUILD/kenlm-configure.log" 2>&1
ninja -C "$KENLM_BUILD" lmplz > "$BUILD/kenlm-build.log" 2>&1
echo "LMPLZ DONE -> $KENLM_BUILD/bin/lmplz"
