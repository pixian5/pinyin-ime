#!/usr/bin/env bash
# 一键运行输入法 cpp/build/pinyin_ime_cli（自动注入运行库路径）。
#
# 用法：
#   bash scripts/run_cli.sh                          # 交互模式（stdin 为 TTY 时）
#   echo "nihao" | bash scripts/run_cli.sh           # 行模式（全拼/简拼 → 候选）
#   echo "cifan" | bash scripts/run_cli.sh --fuzzy   # 模糊音
#   bash scripts/run_cli.sh --user-data ~/.local/share/pinyin-ime
#
# 语言模型/词典默认由 CLI 按内置 fallback 路径自动查找（/workspace/native-build/...）；
# 若把引擎构建到了别的目录，可用 NB=/your/path 覆盖（会同步注入该目录的运行库），
# 或直接显式传 <lm> <dict> [nbest] 位置参数覆盖（CLI 选项与位置参数可交错）。
set -euo pipefail

NB="${NB:-/workspace/native-build}"
OPT="$NB/opt"
ENV="$NB/mamba-env"
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
CLI="$REPO_DIR/cpp/build/pinyin_ime_cli"

[ -x "$CLI" ] || { echo "未找到 $CLI；请先构建引擎并编译（见 README「构建与运行」）" >&2; exit 1; }

export LD_LIBRARY_PATH="$OPT/libime/lib:$OPT/fcitx5/lib:$ENV/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

exec "$CLI" "$@"