#!/usr/bin/env bash
# scripts/eval_chroot.sh —— 一键真实评测（Ubuntu 24.04 chroot + libime + 1200 句 holdout）
#
# 背景：本机（macOS）没有 libime 开发包，评测只能在 Linux 环境跑。历史上每次都是手工
# 敲 rsync / cmake / make / eval.py 一串命令，容易漏步骤、版本对不上。本脚本把它固定成一条：
#
#   bash scripts/eval_chroot.sh                 # 同步 → 编译 → 评测 → 与基线比对
#   bash scripts/eval_chroot.sh --skip-build    # 只评测（复用已有产物，快）
#   CHROOT=/path/to/chroot bash scripts/eval_chroot.sh
#
# 前置条件（只需准备一次）：
#   - Ubuntu 24.04 rootfs 解包在 $CHROOT（默认 /home/user/chroots/noble24）
#   - 该 rootfs 内已 apt 安装 build-essential cmake libimepinyin-dev libimecore-dev
#     libfcitx5utils-dev libboost-dev libcurl4-openssl-dev python3
#   - 宿主有 sudo + rsync；容器/沙箱需允许 chroot
#
# 退出码：0 = 达标；2 = 低于基线阈值（句级 −1.0pp / 字级 −0.5pp）；其它 = 构建或环境错误。

set -euo pipefail

REPO_DIR=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
CHROOT=${CHROOT:-/home/user/chroots/noble24}
SRC=${SRC:-"$CHROOT/src"}
BUILD=${BUILD:-/tmp/build}
LM=${LM:-/usr/lib/x86_64-linux-gnu/libime/zh_CN.lm}
DICT=${DICT:-/usr/share/libime/sc.dict}
SKIP_BUILD=0

for arg in "$@"; do
    case "$arg" in
        --skip-build) SKIP_BUILD=1 ;;
        -h|--help) sed -n '2,20p' "$0"; exit 0 ;;
        *) echo "未知参数：$arg" >&2; exit 2 ;;
    esac
done

if [[ ! -d "$CHROOT" ]]; then
    echo "找不到 chroot：$CHROOT（用 CHROOT=/path 指定）" >&2
    exit 2
fi
if [[ ! -f "$CHROOT/usr/lib/x86_64-linux-gnu/libime/zh_CN.lm" ]]; then
    echo "chroot 内缺少语言模型 $LM（先 apt 安装 libimepinyin-dev）" >&2
    exit 2
fi

VERSION=$(cat "$REPO_DIR/VERSION")
echo "[1/4] 同步源码 $REPO_DIR → $SRC"
sudo mkdir -p "$SRC"
sudo rsync -a --delete --exclude '.git' --exclude 'bench/results' "$REPO_DIR/" "$SRC/"

if [[ "$SKIP_BUILD" -eq 0 ]]; then
    echo "[2/4] 编译 pinyin_ime_cli（Release）"
    sudo chroot "$CHROOT" /bin/bash -lc "
        set -e
        cmake -S /src/cpp -B $BUILD -DCMAKE_BUILD_TYPE=Release >/tmp/eval_cmake.log 2>&1
        make -C $BUILD pinyin_ime_cli -j\$(nproc) >/tmp/eval_make.log 2>&1
    " || { echo "编译失败，日志见 chroot 内 /tmp/eval_make.log" >&2; exit 1; }
else
    echo "[2/4] 跳过编译（--skip-build）"
fi

echo "[3/4] 评测：1200 句零泄漏 holdout，比对基线 bench/baseline.json"
set +e   # 评测退出码 2 = 低于基线阈值，属于「正常结果」，不能中断脚本
sudo chroot "$CHROOT" /bin/bash -lc "
    set -e
    cd /src
    python3 bench/eval.py --engine cli \
        --cli $BUILD/pinyin_ime_cli \
        --lm $LM --dict $DICT \
        --baseline /src/bench/baseline.json \
        --out /tmp/eval_out_$VERSION
"
EVAL_RC=$?
set -e

echo "[4/4] 回写报告到 bench/results/$VERSION/"
sudo mkdir -p "$REPO_DIR/bench/results/$VERSION"
sudo cp -r "$CHROOT/tmp/eval_out_$VERSION/." "$REPO_DIR/bench/results/$VERSION/"
sudo chown -R "$(id -u):$(id -g)" "$REPO_DIR/bench/results" 2>/dev/null || true
ls "$REPO_DIR/bench/results/$VERSION" 2>/dev/null || echo "（报告目录为空，可能评测提前失败）"

exit "$EVAL_RC"
