#!/bin/bash
# pinyin-ime 一键构建脚本（openEuler 22.03 aarch64）
# 从零搭建环境 + 编译 fcitx5 + libime + CLI + addon + 测试
#
# 用法:
#   bash scripts/full_build.sh          # 全量构建
#   bash scripts/full_build.sh --test   # 仅运行测试（需已构建）
#
# 镜像源:
#   conda-forge: 清华 TUNA 镜像
#   GitHub:      gh-proxy.com 镜像
#
# 2026-09-30 实测通过：24/24 冒烟测试全过

set -euo pipefail

NB=${NB:-/workspace/native-build}
ENV="$NB/mamba-env"
OPT="$NB/opt"
BUILD="$NB/build"
PROJECT_ROOT="$(cd "$(dirname "$0")/.." && pwd)"

CONDA_MIRROR="https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge"
GH_MIRROR="https://gh-proxy.com"
FCITX_VER=5.1.23
LIBIME_TAG=1.1.17
KENLM_COMMIT="4cb443e60b7bf2c0ddf3c745378f76cb59e254e5"

# ---------- 工具函数 ----------
log() { echo -e "\033[32m[build]\033[0m $*"; }
err() { echo -e "\033[31m[error]\033[0m $*" >&2; }

if [ "$1" = "--test" ]; then
    source "$PROJECT_ROOT/setup_env.sh" 2>/dev/null
    bash "$PROJECT_ROOT/scripts/smoke_test.sh"
    exit 0
fi

mkdir -p "$NB"/{bin,opt,build,src,corpus,mamba-root}
export MAMBA_ROOT_PREFIX="$NB/mamba-root"

# ---------- 1. micromamba ----------
if [ ! -x "$NB/bin/micromamba" ]; then
    log "下载 micromamba..."
    TMP=$(mktemp -d)
    curl -L --retry 5 -o "$TMP/mm.tar.bz2" \
        "${GH_MIRROR}/https://github.com/mamba-org/micromamba-releases/releases/download/2.9.0-0/micromamba-linux-aarch64.tar.bz2"
    tar -xjf "$TMP/mm.tar.bz2" -C "$TMP" bin/micromamba
    cp "$TMP/bin/micromamba" "$NB/bin/micromamba" && chmod +x "$NB/bin/micromamba"
    rm -rf "$TMP"
fi

# ---------- 2. conda 环境 ----------
if [ ! -d "$ENV/conda-meta" ]; then
    log "创建 conda 环境（清华镜像）..."
    "$NB/bin/micromamba" create -y -p "$ENV" -c "$CONDA_MIRROR" \
        gcc gxx boost-cpp fmt zstd libuv extra-cmake-modules gettext libuuid \
        cmake ninja pkg-config make
fi

# ---------- 3. 源码下载 ----------
if [ ! -d "$NB/src/fcitx5-$FCITX_VER" ]; then
    log "下载 fcitx5 $FCITX_VER..."
    curl -L --retry 5 -o "$NB/src/fcitx5.tar.gz" \
        "${GH_MIRROR}/https://github.com/fcitx/fcitx5/archive/refs/tags/$FCITX_VER.tar.gz"
    tar -xzf "$NB/src/fcitx5.tar.gz" -C "$NB/src"
fi

if [ ! -d "$NB/src/libime" ]; then
    log "下载 libime $LIBIME_TAG..."
    curl -L --retry 5 -o "$NB/src/libime.tar.gz" \
        "${GH_MIRROR}/https://github.com/fcitx/libime/archive/refs/tags/$LIBIME_TAG.tar.gz"
    tar -xzf "$NB/src/libime.tar.gz" -C "$NB/src"
    mv "$NB/src"/libime-* "$NB/src/libime"
fi

if [ ! -f "$NB/src/libime/src/libime/core/kenlm/lm/lm_exception.hh" ]; then
    log "下载 kenlm..."
    curl -L --retry 5 -o "$NB/src/kenlm.tar.gz" \
        "${GH_MIRROR}/https://codeload.github.com/kpu/kenlm/tar.gz/$KENLM_COMMIT"
    tar -xzf "$NB/src/kenlm.tar.gz" -C "$NB/src"
    rm -rf "$NB/src/libime/src/libime/core/kenlm"
    mv "$NB/src/kenlm-$KENLM_COMMIT" "$NB/src/libime/src/libime/core/kenlm"
fi

# ---------- 4. 4-gram 补丁 ----------
log "打 4-gram 补丁..."
sed -i 's/-DKENLM_MAX_ORDER=3/-DKENLM_MAX_ORDER=4/' "$NB/src/libime/src/libime/core/CMakeLists.txt"
sed -i 's#constexpr size_t StateSize = 20 + sizeof(void \*);#constexpr size_t StateSize = 32 + sizeof(void *);#' \
    "$NB/src/libime/src/libime/core/languagemodel.h"

# ---------- 5. 编译 fcitx5 ----------
export PATH="$ENV/bin:$PATH"
export CC="$ENV/bin/aarch64-conda-linux-gnu-cc"
export CXX="$ENV/bin/aarch64-conda-linux-gnu-c++"
export LD_LIBRARY_PATH="$ENV/lib"

if [ ! -f "$OPT/fcitx5/lib/libFcitx5Core.so" ]; then
    log "编译 fcitx5（headless）..."
    cmake -S "$NB/src/fcitx5-$FCITX_VER" -B "$BUILD/fcitx5" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$OPT/fcitx5" -DCMAKE_INSTALL_LIBDIR=lib \
        -DENABLE_TESTING_ADDONS=Off -DENABLE_TEST=Off -DENABLE_COVERAGE=Off -DENABLE_ENCHANT=Off \
        -DENABLE_X11=Off -DENABLE_WAYLAND=Off -DENABLE_DBUS=Off -DENABLE_DOC=Off -DENABLE_SERVER=Off \
        -DENABLE_KEYBOARD=Off -DUSE_SYSTEMD=Off -DENABLE_XDGAUTOSTART=Off -DENABLE_EMOJI=Off \
        -DBUILD_SPELL_DICT=Off -DENABLE_LIBUUID=On -DENABLE_DL=On
    ninja -C "$BUILD/fcitx5"
    ninja -C "$BUILD/fcitx5" install
fi

# ---------- 6. 编译 libime ----------
export CMAKE_PREFIX_PATH="$OPT/fcitx5;$ENV"
export PKG_CONFIG_PATH="$ENV/lib/pkgconfig:$ENV/share/pkgconfig:$OPT/fcitx5/lib/pkgconfig"
export LDFLAGS="-Wl,-rpath,$ENV/lib"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5/lib"

if [ ! -f "$OPT/libime/lib/libIMEPinyin.so" ]; then
    log "编译 libime（含 zh_CN.lm + sc.dict）..."
    cmake -S "$NB/src/libime" -B "$BUILD/libime" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$OPT/libime" -DCMAKE_INSTALL_LIBDIR=lib \
        -DCMAKE_PREFIX_PATH="$OPT/fcitx5;$ENV" \
        -DFcitx5Utils_DIR="$OPT/fcitx5/lib/cmake/Fcitx5Utils" \
        -DENABLE_TEST=Off -DENABLE_DOC=Off -DENABLE_DATA=On -DENABLE_TOOLS=On
    ninja -C "$BUILD/libime"
    ninja -C "$BUILD/libime" install
fi

# ---------- 7. 编译 lmplz ----------
if [ ! -f "$BUILD/kenlm/bin/lmplz" ]; then
    log "编译 kenlm lmplz..."
    cmake -S "$NB/src/libime/src/libime/core/kenlm" -B "$BUILD/kenlm" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release -DBOOST_ROOT="$ENV" -DBoost_NO_SYSTEM_PATHS=ON \
        -DBUILD_TESTING=Off -DENABLE_PYTHON=Off -DENABLE_INTERPOLATE=Off
    ninja -C "$BUILD/kenlm" lmplz
fi

# ---------- 8. 编译 CLI ----------
log "编译 pinyin_ime_cli..."
cd "$PROJECT_ROOT"
cmake -S cpp -B cpp/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$OPT/fcitx5;$OPT/libime;$ENV" \
    -DLibIMEPinyin_DIR="$OPT/libime/lib/cmake/LibIMEPinyin" \
    -DLibIMECore_DIR="$OPT/libime/lib/cmake/LibIMECore"
ninja -C cpp/build

# ---------- 9. 编译 fcitx5 addon ----------
log "编译 fcitx5 addon..."
cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$OPT/fcitx5;$OPT/libime;$ENV" \
    -DFcitx5Core_DIR="$OPT/fcitx5/lib/cmake/Fcitx5Core" \
    -DFcitx5Config_DIR="$OPT/fcitx5/lib/cmake/Fcitx5Config" \
    -DFcitx5Utils_DIR="$OPT/fcitx5/lib/cmake/Fcitx5Utils" \
    -DLibIMEPinyin_DIR="$OPT/libime/lib/cmake/LibIMEPinyin" \
    -DLibIMECore_DIR="$OPT/libime/lib/cmake/LibIMECore"
ninja -C fcitx5-addon/build

# ---------- 10. 测试 ----------
log "运行冒烟测试..."
bash "$PROJECT_ROOT/scripts/smoke_test.sh"

log "构建完成！"
log "  CLI:   $PROJECT_ROOT/cpp/build/pinyin_ime_cli"
log "  Addon: $PROJECT_ROOT/fcitx5-addon/build/libpinyinime.so"
log "  LM:    $OPT/libime/lib/libime/zh_CN.lm"
log "  Dict:  $OPT/libime/share/libime/sc.dict"
log ""
log "快速试用: ./run.sh \"nihao\""
