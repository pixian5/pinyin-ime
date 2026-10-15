#!/bin/bash
# pinyin-ime —— libime 命令行拼音输入法统一入口
# 自动设置运行时库路径 + 数据文件。引擎从 stdin 读取拼音，输出候选。
#
# 用法:
#   echo "nihao" | pinyin-ime          # → 你好
#   pinyin-ime "zhongguo"              # → 中国   (尾随裸词当作输入)
#   pinyin-ime --fuzzy "zongguo"       # → 中国   (宽松/模糊音)
#   pinyin-ime --top1 "nihao"          # 只输出首选
#   pinyin-ime --predict "你好"        # 联想下一个词
#   pinyin-ime --stats | --help        # 引擎统计 / 帮助
set -uo pipefail

RUNTIME="${PINYIN_IME_RUNTIME:-/workspace/deploy/pinyin-ime-v0.9.84a}"
DESKTOP="${PINYIN_IME_DESKTOP:-/workspace/deploy/pinyin-ime}"
export LD_LIBRARY_PATH="$DESKTOP/opt/deps-lib:$DESKTOP/opt/fcitx5-x11/lib:$RUNTIME/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

LM="$RUNTIME/data/zh_CN.lm"
DICT="$RUNTIME/data/sc.dict"
CLI="$RUNTIME/bin/pinyin_ime_cli"

# 用户配置可覆盖数据路径（同 CLI: ~/.config/pinyin-ime/config）
if [ -f "$HOME/.config/pinyin-ime/config" ]; then
  while IFS='=' read -r k v; do
    case "$k" in lm_path) LM="${v// /}" ;; dict_path) DICT="${v// /}" ;; esac
  done < "$HOME/.config/pinyin-ime/config"
fi

if [ ! -f "$LM" ] || [ ! -f "$DICT" ]; then
  echo "错误: 数据文件不存在  LM=$LM  DICT=$DICT" >&2; exit 1
fi

# 若尾随是一个"裸词"且其前一个参数不是"取值型选项"，则该裸词即输入拼音，
# 通过 stdin 喂给引擎；其余作为选项跟在 LM/DICT 之后。
declare -a OPTS=("$@")
n=${#OPTS[@]}
INPUT=""
VALUE_OPTS="--sp-scheme --dict-add --dict-del --dict-import --dict-export --dict-search --batch --history-weight --top --train-text --learn-file --user-data --abbr-file"
if [ "$n" -ge 1 ] && [[ "${OPTS[n-1]}" != -* ]]; then
  prev="${OPTS[n-2]:-}"
  is_val=0
  for v in $VALUE_OPTS; do [ "$prev" = "$v" ] && is_val=1; done
  if [ "$is_val" -eq 0 ]; then
    INPUT="${OPTS[n-1]}"; unset 'OPTS[n-1]'
  fi
fi

if [ -n "$INPUT" ]; then
  printf '%s\n' "$INPUT" | "$CLI" "$LM" "$DICT" "${OPTS[@]}"
elif [ "$n" -eq 0 ]; then
  exec "$CLI" "$LM" "$DICT"            # 无参数：交互/管道模式
else
  exec "$CLI" "$LM" "$DICT" "${OPTS[@]}"  # 仅选项：从 stdin 读
fi
