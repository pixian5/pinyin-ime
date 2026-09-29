#!/bin/bash
# pinyin-ime 一键安装脚本
# 用法: bash scripts/install.sh [--cli-only] [--prefix /usr/local]
set -e

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
PREFIX="${2:-/usr/local}"

echo "=== pinyin-ime 安装脚本 ==="
echo "项目目录: $PROJ_DIR"
echo "安装前缀: $PREFIX"

# 1. 检查构建
CLI_BIN="$PROJ_DIR/cpp/build/pinyin_ime_cli"
if [ ! -f "$CLI_BIN" ]; then
    echo "错误: CLI 未构建，请先运行 cmake 构建"
    exit 1
fi

# 2. 安装 CLI
echo "=== 安装 CLI ==="
install -Dm755 "$CLI_BIN" "$PREFIX/bin/pinyin_ime_cli"
echo "  → $PREFIX/bin/pinyin_ime_cli"

# 3. 安装配置文件
CONFIG_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/pinyin-ime"
mkdir -p "$CONFIG_DIR"
if [ ! -f "$CONFIG_DIR/config" ]; then
    cat > "$CONFIG_DIR/config" << 'CFG'
# pinyin-ime 配置文件
# 语言模型和词典路径（根据实际安装位置修改）
lm_path=/root/workspace/native-build/opt/libime/lib/libime/zh_CN.lm
dict_path=/root/workspace/native-build/opt/libime/share/libime/sc.dict

# 候选词数量
nbest=5

# 模糊音（0=关闭，1=开启）
fuzzy=0

# 双拼（0=关闭，1=开启）
shuangpin=0

# 双拼方案（xiaohe/ziranma/ms/ziguang/abc/zhongwenzhixing/pinyinjiajia/gb）
sp_scheme=xiaohe

# 用户数据目录（自学习持久化）
user_data=~/.local/share/pinyin-ime
CFG
    echo "  → $CONFIG_DIR/config"
else
    echo "  配置已存在，跳过"
fi

# 4. 安装预训练用户 LM（如果有）
USER_DATA_DIR="${XDG_DATA_HOME:-$HOME/.local/share}/pinyin-ime"
if [ -d "$PROJ_DIR/data/user_lm_news" ]; then
    mkdir -p "$USER_DATA_DIR"
    cp -n "$PROJ_DIR/data/user_lm_news/"* "$USER_DATA_DIR/" 2>/dev/null || true
    echo "  → 预训练用户 LM 已安装到 $USER_DATA_DIR"
fi

# 5. 验证
echo "=== 验证 ==="
if echo nihao | "$PREFIX/bin/pinyin_ime_cli" --top1 --quiet 2>/dev/null | grep -q "你好"; then
    echo "  ✓ CLI 工作正常"
else
    echo "  ✗ CLI 验证失败（可能需要配置 LM/词典路径）"
fi

echo ""
echo "安装完成！"
echo "  用法: echo nihao | pinyin_ime_cli"
echo "  交互: pinyin_ime_cli（终端 TTY 模式）"
echo "  帮助: pinyin_ime_cli --help"
