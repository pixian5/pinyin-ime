#!/usr/bin/env bash
# install_sogou_theme.sh — 安装「搜狗风格」fcitx5 classicui 皮肤
#
# 作用：
#   1. 把 config/themes/sogou-{light,dark} 装到 ~/.local/share/fcitx5/themes/
#   2. 写 ~/.config/fcitx5/conf/classicui.conf：竖排候选 + 主题名 + 字号 + 模糊
#   3. 重载 fcitx5 配置（fcitx5-remote -r）
#
# 用法：
#   bash scripts/install_sogou_theme.sh            # 默认浅色 + 竖排
#   bash scripts/install_sogou_theme.sh dark       # 深色
#   bash scripts/install_sogou_theme.sh light horizontal   # 横排
#   bash scripts/install_sogou_theme.sh --uninstall
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
THEME_SRC="$ROOT/config/themes"
THEME_DST="${XDG_DATA_HOME:-$HOME/.local/share}/fcitx5/themes"
CONF_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/fcitx5/conf"
CONF="$CONF_DIR/classicui.conf"

variant="${1:-light}"
orientation="${2:-vertical}"

if [ "$variant" = "--uninstall" ]; then
    rm -rf "$THEME_DST/sogou-light" "$THEME_DST/sogou-dark"
    [ -f "$CONF" ] && sed -i 's/^Theme=.*/Theme=default/' "$CONF"
    echo "已卸载搜狗风格皮肤（classicui 主题回退为 default）"
    command -v fcitx5-remote >/dev/null && DISPLAY="${DISPLAY:-:1}" fcitx5-remote -r 2>/dev/null
    exit 0
fi

case "$variant" in
    light)  theme_name="sogou-light" ;;
    dark)   theme_name="sogou-dark" ;;
    *)      echo "未知皮肤：$variant（可选 light/dark）" >&2; exit 2 ;;
esac

case "$orientation" in
    vertical)   vert="True" ;;
    horizontal) vert="False" ;;
    *) echo "未知排布：$orientation（可选 vertical/horizontal）" >&2; exit 2 ;;
esac

mkdir -p "$THEME_DST" "$CONF_DIR"
for t in sogou-light sogou-dark; do
    [ -d "$THEME_SRC/$t" ] || { echo "缺少主题目录：$THEME_SRC/$t" >&2; exit 1; }
    rm -rf "$THEME_DST/$t"
    cp -r "$THEME_SRC/$t" "$THEME_DST/$t"
done

# 保留用户已有的其他键，只覆盖本脚本负责的几项
if [ -f "$CONF" ]; then
    cp "$CONF" "$CONF.bak.$$"
fi
{
    # fcitx5 的 addon 配置文件不带 section 头（顶层直接是键值）
    echo "# 由 scripts/install_sogou_theme.sh 生成"
    echo "Theme=$theme_name"
    echo "DarkTheme=sogou-dark"
    echo "UseDarkTheme=False"
    echo "Vertical Candidate List=$vert"
    echo "Font=\"Noto Sans CJK SC 12\""
    echo "MenuFont=\"Noto Sans CJK SC 12\""
    echo "WheelForPaging=True"
    echo "PerScreenDPI=True"
} > "$CONF"

echo "已安装主题：$theme_name（$( [ "$vert" = True ] && echo 竖排 || echo 横排 )）"
echo "  主题目录：$THEME_DST/$theme_name"
echo "  配置文件：$CONF"

if command -v fcitx5-remote >/dev/null 2>&1; then
    DISPLAY="${DISPLAY:-:1}" fcitx5-remote -r 2>/dev/null \
        && echo "  已重载 fcitx5 配置" \
        || echo "  fcitx5 未在运行，下次启动生效"
fi
