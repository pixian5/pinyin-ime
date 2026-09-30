#!/bin/bash
# pinyin-ime 一键安装脚本
# 用法: ./scripts/install.sh [--cli-only] [--prefix /usr] [--uninstall]
set -e

PREFIX="/usr"
CLI_ONLY=false
UNINSTALL=false

while [[ $# -gt 0 ]]; do
    case $1 in
        --cli-only) CLI_ONLY=true; shift ;;
        --prefix) PREFIX="$2"; shift 2 ;;
        --uninstall) UNINSTALL=true; shift ;;
        *) echo "未知选项: $1"; exit 1 ;;
    esac
done

# 颜色输出
GREEN() { echo -e "\033[32m$*\033[0m"; }
RED()   { echo -e "\033[31m$*\033[0m"; }
YELLOW(){ echo -e "\033[33m$*\033[0m"; }

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_DIR="$(dirname "$SCRIPT_DIR")"

# ==================== 卸载 ====================
if $UNINSTALL; then
    YELLOW "卸载 pinyin-ime..."
    rm -f "$PREFIX/bin/pinyin_ime_cli"
    if ! $CLI_ONLY; then
        rm -f "$PREFIX/lib/fcitx5/libpinyinime.so"
        rm -f "$PREFIX/share/fcitx5/addon/pinyinime.conf"
        rm -f "$PREFIX/share/fcitx5/inputmethod/pinyinime.conf"
    fi
    GREEN "卸载完成"
    exit 0
fi

# ==================== 前置检查 ====================
GREEN "=== pinyin-ime 安装脚本 ==="

# 检查编译器
if ! command -v g++ &>/dev/null; then
    RED "错误: 未找到 g++。请先安装 C++ 编译器。"
    exit 1
fi

# 检查 cmake
if ! command -v cmake &>/dev/null; then
    RED "错误: 未找到 cmake。请先安装 cmake。"
    exit 1
fi

# 检查 ninja
BUILD_TOOL="make"
if command -v ninja &>/dev/null; then
    BUILD_TOOL="ninja"
elif command -v make &>/dev/null; then
    BUILD_TOOL="make"
else
    RED "错误: 未找到 ninja 或 make。"
    exit 1
fi

echo "编译器: $(g++ --version | head -1)"
echo "构建工具: $BUILD_TOOL"
echo "安装前缀: $PREFIX"
echo ""

# ==================== 构建 CLI ====================
GREEN "=== 构建 CLI ==="
CLI_BUILD="$PROJECT_DIR/cpp/build"
mkdir -p "$CLI_BUILD"
cd "$CLI_BUILD"

if [ "$BUILD_TOOL" = "ninja" ]; then
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -GNinja
    ninja
else
    cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
    make -j$(nproc)
fi

# 安装 CLI
if [ "$PREFIX" = "/usr" ] && [ "$EUID" -ne 0 ]; then
    YELLOW "需要 sudo 权限安装到 $PREFIX"
    sudo ninja install 2>/dev/null || sudo make install
else
    $BUILD_TOOL install
fi

# 验证 CLI
CLI_BIN="$PREFIX/bin/pinyin_ime_cli"
if [ -f "$CLI_BIN" ]; then
    GREEN "CLI 安装成功: $CLI_BIN"
    echo "  版本: $($CLI_BIN --version)"
    echo "  测试: $(echo nihao | $CLI_BIN --top1)"
else
    RED "CLI 安装失败"
    exit 1
fi

# ==================== 构建 fcitx5 addon ====================
if ! $CLI_ONLY; then
    echo ""
    GREEN "=== 构建 fcitx5 addon ==="

    # 检查 fcitx5 开发文件
    FCITX5_FOUND=false
    for dir in /usr/include/Fcitx5 /usr/local/include/Fcitx5; do
        if [ -d "$dir" ]; then
            FCITX5_FOUND=true
            break
        fi
    done

    # 也检查 pkg-config
    if ! $FCITX5_FOUND; then
        if pkg-config --exists fcitx5-core 2>/dev/null; then
            FCITX5_FOUND=true
        fi
    fi

    if ! $FCITX5_FOUND; then
        YELLOW "警告: 未找到 fcitx5 开发文件，跳过 addon 构建"
        YELLOW "安装 fcitx5 开发包后重新运行此脚本以构建 addon"
        echo ""
        YELLOW "Ubuntu/Debian: sudo apt install libfcitx5core-dev"
        YELLOW "Fedora/RHEL:  sudo dnf install fcitx5-devel"
    else
        ADDON_BUILD="$PROJECT_DIR/fcitx5-addon/build"
        mkdir -p "$ADDON_BUILD"
        cd "$ADDON_BUILD"

        if [ "$BUILD_TOOL" = "ninja" ]; then
            cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX" -GNinja
            ninja
        else
            cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX="$PREFIX"
            make -j$(nproc)
        fi

        # 安装 addon
        if [ "$PREFIX" = "/usr" ] && [ "$EUID" -ne 0 ]; then
            sudo $BUILD_TOOL install
        else
            $BUILD_TOOL install
        fi

        ADDON_LIB="$PREFIX/lib/fcitx5/libpinyinime.so"
        if [ -f "$ADDON_LIB" ]; then
            GREEN "fcitx5 addon 安装成功: $ADDON_LIB"
        else
            YELLOW "addon 可能未正确安装，请检查日志"
        fi
    fi
fi

# ==================== 配置 fcitx5 ====================
if ! $CLI_ONLY; then
    echo ""
    GREEN "=== 配置 fcitx5 ==="

    # 创建用户配置目录
    CONFIG_DIR="$HOME/.config/fcitx5/conf"
    mkdir -p "$CONFIG_DIR"

    # 创建默认配置
    if [ ! -f "$CONFIG_DIR/pinyinime.conf" ]; then
        cat > "$CONFIG_DIR/pinyinime.conf" << 'CONF'
NBest=45
PageSize=9
DataDir=/usr/share/libime
UserDataDir=~/.local/share/pinyin-ime
EnableFuzzy=False
EnableCorrect=False
EnableShuangpin=False
ShuangpinScheme=xiaohe
CONF
        GREEN "已创建默认配置: $CONFIG_DIR/pinyinime.conf"
    else
        YELLOW "配置已存在，跳过: $CONFIG_DIR/pinyinime.conf"
    fi

    # 创建用户数据目录
    USER_DATA_DIR="$HOME/.local/share/pinyin-ime"
    mkdir -p "$USER_DATA_DIR"
    GREEN "用户数据目录: $USER_DATA_DIR"

    echo ""
    GREEN "=== 安装完成 ==="
    echo ""
    echo "下一步:"
    echo "  1. 运行 fcitx5-configtool 添加 'Pinyin IME' 输入法"
    echo "  2. 或手动编辑 ~/.config/fcitx5/profile 添加 pinyinime"
    echo "  3. 重启 fcitx5: fcitx5 -r"
    echo "  4. 按 Ctrl+Space 切换到拼音输入法"
    echo ""
    echo "CLI 独立使用:"
    echo "  $CLI_BIN              # 交互模式"
    echo "  echo nihao | $CLI_BIN --top1   # 行模式"
else
    echo ""
    GREEN "=== CLI 安装完成 ==="
    echo "运行: $CLI_BIN"
fi
