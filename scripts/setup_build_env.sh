#!/usr/bin/env bash
# 从零搭建 libime 原生构建环境（micromamba + conda-forge gcc 15），下载源码并打 4-gram 补丁。
# 跑完后执行 scripts/build_conda_forge.sh 做实际编译。
#
# 这套步骤是 2026-09 在「openEuler 22.03 aarch64 / dnf 无 gcc-toolset、无 fcitx5 包」的
# 沙箱里实测踩出来的，几个关键坑（对应 PLAN-A §14 那台只读 /usr 沙箱的变体）：
#
#   1. dnf 里只有 gcc 10.3.1，无 gcc-toolset-13 / fcitx5 —— 必须走 conda-forge 工具链；
#   2. micro.mamba.pm 直连的 S3 源慢，GitHub release/codeload 归档快且稳；
#   3. github.com 的 git 智能协议在本沙箱偶发「HTTP2 framing / connection timed out」，
#      但 codeload.github.com 的归档下载稳定 —— 所以 kenlm 用归档、不走 git submodule；
#   4. libime 默认 KENLM_MAX_ORDER=3，开 4 必须同时放大 languagemodel.h 的 StateSize，
#      否则静态断言 sizeof(void*)+sizeof(lm::ngram::State)<=StateSize 直接编译失败。
set -euo pipefail

NB=${NB:-/workspace/native-build}
ENV="$NB/mamba-env"
MAMBA_BIN="$NB/bin/micromamba"
export MAMBA_ROOT_PREFIX="$NB/mamba-root"
mkdir -p "$NB"/{bin,opt,build,src,corpus} "$MAMBA_ROOT_PREFIX"

MIRROR="https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge"

# GitHub 加速镜像（可空=直连/proxy；本沙箱 release-assets 直连慢，用 ghfast.top 加速）
GITHUB_MIRROR="${GITHUB_MIRROR:-https://ghfast.top/}"
# GitHub archive/codeload 归档走本地 mihomo 代理（ghfast.top 对 codeload 偶发 SSL_ERROR_SYSCALL）
PROXY="${PROXY:-http://127.0.0.1:7890}"
PROXY_ARG=(); [ -n "$PROXY" ] && PROXY_ARG=(-x "$PROXY")

# ---------- 1. micromamba ----------
if [ ! -x "$MAMBA_BIN" ]; then
    echo "=== 下载 micromamba（经 $GITHUB_MIRROR 加速）==="
    TMP=$(mktemp -d)
    # GitHub release 直链（codeload 只覆盖源码 tar.gz，二进制走 releases）
    curl -L --retry 5 --retry-delay 3 -o "$TMP/mm.tar.bz2" \
        "${GITHUB_MIRROR}https://github.com/mamba-org/micromamba-releases/releases/download/2.9.0-0/micromamba-linux-aarch64.tar.bz2"
    tar -xjf "$TMP/mm.tar.bz2" -C "$TMP" bin/micromamba
    cp "$TMP/bin/micromamba" "$MAMBA_BIN" && chmod +x "$MAMBA_BIN"
    rm -rf "$TMP"
fi
"$MAMBA_BIN" --version

# ---------- 2. conda env（gcc 15 + 构建依赖）----------
if [ ! -d "$ENV/conda-meta" ]; then
    echo "=== 创建 conda env ==="
    "$MAMBA_BIN" create -y -p "$ENV" -c "$MIRROR" \
        gcc gxx boost-cpp fmt zstd libuv extra-cmake-modules gettext libuuid \
        cmake ninja pkg-config make
fi
test -x "$ENV/bin/aarch64-conda-linux-gnu-cc" || { echo "工具链缺 aarch64-conda-linux-gnu-cc"; exit 1; }

# ---------- 3. 源码 ----------
FCITX_VER=5.1.23
LIBIME_TAG=1.1.17

if [ ! -d "$NB/src/fcitx5-$FCITX_VER" ]; then
    echo "=== 下载 fcitx5 $FCITX_VER（GitHub archive；download.fcitx-im.org 该路径 404）==="
    curl -L --retry 5 "${PROXY_ARG[@]}" -o "$NB/src/fcitx5.tar.gz" \
        "https://github.com/fcitx/fcitx5/archive/refs/tags/$FCITX_VER.tar.gz"
    tar -xzf "$NB/src/fcitx5.tar.gz" -C "$NB/src"
    # github archive 已解包为 fcitx5-$FCITX_VER 目录，无需再改名（原 mv 是自移动、会报错）
fi

if [ ! -d "$NB/src/libime" ]; then
    echo "=== 下载 libime $LIBIME_TAG（archive 归档，走代理避免 git 智能协议不稳）==="
    curl -L --retry 5 "${PROXY_ARG[@]}" -o "$NB/src/libime.tar.gz" \
        "https://github.com/fcitx/libime/archive/refs/tags/$LIBIME_TAG.tar.gz"
    tar -xzf "$NB/src/libime.tar.gz" -C "$NB/src"
    mv "$NB/src"/libime-* "$NB/src/libime"
fi

# kenlm：git 智能协议不稳，用 codeload 归档 + 钉死的 commit（libime 1.1.17 的 gitlink）
if [ ! -f "$NB/src/libime/src/libime/core/kenlm/lm/lm_exception.hh" ]; then
    echo "=== 下载 kenlm 子模块（codeload 归档，走代理）==="
    KENLM_COMMIT="${KENLM_COMMIT:-4cb443e60b7bf2c0ddf3c745378f76cb59e254e5}"
    curl -L --retry 5 "${PROXY_ARG[@]}" -o "$NB/src/kenlm.tar.gz" \
        "https://codeload.github.com/kpu/kenlm/tar.gz/$KENLM_COMMIT"
    tar -xzf "$NB/src/kenlm.tar.gz" -C "$NB/src"
    rm -rf "$NB/src/libime/src/libime/core/kenlm"
    mv "$NB/src/kenlm-$KENLM_COMMIT" "$NB/src/libime/src/libime/core/kenlm"
fi

# ---------- 4. 4-gram 补丁（KENLM_MAX_ORDER=4 + StateSize 放大）----------
echo "=== 打 4-gram 补丁 ==="
sed -i 's/-DKENLM_MAX_ORDER=3/-DKENLM_MAX_ORDER=4/' "$NB/src/libime/src/libime/core/CMakeLists.txt"
sed -i 's#constexpr size_t StateSize = 20 + sizeof(void \*);#constexpr size_t StateSize = 32 + sizeof(void *);#' \
    "$NB/src/libime/src/libime/core/languagemodel.h"

echo "=== 环境就绪，下一步：bash scripts/build_conda_forge.sh ==="