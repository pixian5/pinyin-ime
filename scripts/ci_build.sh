#!/bin/bash
# =============================================================================
# 本地编译 + 门禁 + 真实评测（v0.9.198）
#
# 这是本地 / 任意 Ubuntu 24.04 环境里跑的「一条命令」，也是 GitHub Actions
# 的 build-eval job 实际调用的脚本。
#   编译 CLI 与 addon → 单测 → 警告 → 快捷词 → fuzz → clean 语料重训 → 快照 → holdout 评测
# 与 scripts/release_check.sh 的区别：不碰系统安装（不 dpkg/rpm），只验证代码。
#
# 前置：cmake、g++、libime*-dev、libfcitx5*-dev、boost、libcurl、python3、
#       libime-data（提供 /usr/share/libime/zh_CN.lm 与 sc.dict）。
# 用法：bash scripts/ci_build.sh
# =============================================================================
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
VERSION="$(cat VERSION)"
JOBS="$(getconf _NPROCESSORS_ONLN 2>/dev/null || echo 4)"

echo "== CI 构建 pinyin-ime v$VERSION（$JOBS 线程）=="
cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build cpp/build -j"$JOBS"
cmake -S fcitx5-addon -B fcitx5-addon/build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build fcitx5-addon/build -j"$JOBS"

echo "== 单元测试 =="
./cpp/build/quick_input_test | tail -1

echo "== 编译警告（单测/CLI/addon）=="
bash scripts/warnings_check.sh --all

echo "== CLI 快捷词行为 =="
bash scripts/cli_features_test.sh

echo "== 模糊测试（500 次）=="
bash scripts/fuzz_test.sh 500

# 找发行版数据：Ubuntu 的 libime-data 两个常见位置
LM=""
DICT=""
for p in /usr/share/libime/zh_CN.lm /usr/lib/x86_64-linux-gnu/libime/zh_CN.lm /usr/lib64/libime/zh_CN.lm; do
    [ -f "$p" ] && LM="$p" && break
done
for p in /usr/share/libime/sc.dict /usr/lib/x86_64-linux-gnu/libime/sc.dict; do
    [ -f "$p" ] && DICT="$p" && break
done
if [ -z "$LM" ] || [ -z "$DICT" ]; then
    echo "!! 找不到 libime 数据（装 libime-data）" >&2
    exit 2
fi
echo "== 清洗训练语料 + 重训字符模型 =="
CLEAN_DIR="$(mktemp -d)"
trap 'rm -rf "$CLEAN_DIR"' EXIT
python3 bench/clean_corpus.py --out "$CLEAN_DIR/corpus" --near-dup-threshold 2
python3 bench/check_train_leakage.py --train "$CLEAN_DIR/corpus" --near-dup-threshold 2
python3 scripts/train_char_lm.py --corpus "$CLEAN_DIR/corpus" \
    --out "$CLEAN_DIR/char_trigram.bin" \
    --meta-out "$CLEAN_DIR/char_trigram.meta.json" --alpha 0.25
# 发布模型必须能从 clean split 确定性重训；不一致说明 data/ 里的模型来路不明
cmp "$CLEAN_DIR/char_trigram.bin" data/char_trigram.bin
export PINYINIME_CHAR_LM="$CLEAN_DIR/char_trigram.bin"

echo "== CLI 快照（LM=$LM）=="
LM="$LM" DICT="$DICT" bash scripts/cli_snapshot_test.sh

echo "== holdout 评测（基线门禁）=="
python3 bench/eval.py --engine cli --cli cpp/build/pinyin_ime_cli \
    --lm "$LM" --dict "$DICT" --baseline bench/baseline.json | tail -8

echo "CI 构建与评测全部通过 ✅"
