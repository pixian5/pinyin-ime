#!/bin/bash
# 用户词典管理工具
# 用法:
#   bash scripts/user_dict.sh list          # 列出所有用户词条
#   bash scripts/user_dict.sh add <py> <hz> # 添加词条
#   bash scripts/user_dict.sh search <py>   # 搜索词条
#   bash scripts/user_dict.sh stats         # 统计信息
#   bash scripts/user_dict.sh export <file> # 导出到文件
#   bash scripts/user_dict.sh clear         # 清空用户数据

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"
source ./setup_env.sh 2>/dev/null

USER_DATA="${HOME}/.local/share/pinyin-ime"
CLI="$PINYIN_IME_CLI"
LM="$PINYIN_IME_LM"
DICT="$PINYIN_IME_DICT"

case "${1:-help}" in
    list)
        echo "=== 用户词典内容 ==="
        echo "dummy" | "$CLI" --dump-user --user-data "$USER_DATA" "$LM" "$DICT" 2>/dev/null
        ;;
    
    add)
        if [ -z "$2" ] || [ -z "$3" ]; then
            echo "用法: user_dict.sh add <拼音> <中文>"
            exit 1
        fi
        TMPFILE=$(mktemp)
        for i in 1 2 3 4 5; do
            printf "%s\t%s\n" "$2" "$3"
        done > "$TMPFILE"
        echo "dummy" | "$CLI" --quiet --learn-file "$TMPFILE" --user-data "$USER_DATA" "$LM" "$DICT" 2>&1
        rm -f "$TMPFILE"
        # Verify
        RESULT=$(echo "$2" | "$CLI" --quiet --top1 "$LM" "$DICT" 2>/dev/null)
        echo "验证: $2 → $RESULT"
        ;;
    
    search)
        if [ -z "$2" ]; then
            echo "用法: user_dict.sh search <拼音>"
            exit 1
        fi
        echo "=== 搜索: $2 ==="
        echo "$2" | "$CLI" --quiet "$LM" "$DICT" 10 2>/dev/null | tr '\n' ' '
        echo ""
        ;;
    
    stats)
        echo "=== 用户数据统计 ==="
        echo "数据目录: $USER_DATA"
        if [ -f "$USER_DATA/user.history" ]; then
            SIZE=$(du -h "$USER_DATA/user.history" | cut -f1)
            echo "user.history: $SIZE"
        else
            echo "user.history: 不存在"
        fi
        if [ -f "$USER_DATA/user.dict" ]; then
            SIZE=$(du -h "$USER_DATA/user.dict" | cut -f1)
            echo "user.dict: $SIZE"
        else
            echo "user.dict: 不存在"
        fi
        echo ""
        echo "词条数:"
        echo "dummy" | "$CLI" --dump-user --user-data "$USER_DATA" "$LM" "$DICT" 2>/dev/null | wc -l
        ;;
    
    export)
        if [ -z "$2" ]; then
            echo "用法: user_dict.sh export <文件>"
            exit 1
        fi
        echo "dummy" | "$CLI" --dump-user --user-data "$USER_DATA" "$LM" "$DICT" 2>/dev/null > "$2"
        echo "已导出到 $2 ($(wc -l < "$2") 条)"
        ;;
    
    clear)
        echo "⚠️ 将清空用户学习数据: $USER_DATA"
        read -p "确认? (yes/no): " confirm
        if [ "$confirm" = "yes" ]; then
            rm -f "$USER_DATA/user.history" "$USER_DATA/user.dict"
            echo "✅ 已清空"
        else
            echo "已取消"
        fi
        ;;
    
    *)
        echo "pinyin-ime 用户词典管理工具"
        echo ""
        echo "用法:"
        echo "  $0 list          列出所有用户词条"
        echo "  $0 add <py> <hz> 添加词条 (5x权重)"
        echo "  $0 search <py>   搜索候选"
        echo "  $0 stats         统计信息"
        echo "  $0 export <file> 导出到文件"
        echo "  $0 clear         清空用户数据"
        ;;
esac
