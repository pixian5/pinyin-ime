#!/bin/bash
# pinyin_ime CLI 包装脚本
# 自动使用系统安装的数据文件，无需手动指定路径
# 用法: echo "nihao" | pinyin-ime
#       pinyin-ime "nihao"

LM="/usr/share/libime/libime/zh_CN.lm"
DICT="/usr/share/libime/libime/sc.dict"

# 如果用户配置存在，使用配置中的路径
if [ -f ~/.config/pinyin-ime/config ]; then
    while IFS='=' read -r key val; do
        case "$key" in
            lm_path) LM="$val" ;;
            dict_path) DICT="$val" ;;
        esac
    done < ~/.config/pinyin-ime/config
fi

# 检查数据文件
if [ ! -f "$LM" ] || [ ! -f "$DICT" ]; then
    echo "错误: 数据文件不存在" >&2
    echo "  LM:   $LM" >&2
    echo "  Dict: $DICT" >&2
    echo "请运行: bash scripts/install_system.sh" >&2
    exit 1
fi

# 如果有参数且不是选项，当作输入
if [ $# -gt 0 ] && [[ "$1" != -* ]]; then
    echo "$1" | /usr/bin/pinyin_ime_cli "$LM" "$DICT"
else
    /usr/bin/pinyin_ime_cli "$@" "$LM" "$DICT"
fi
