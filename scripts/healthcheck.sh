#!/bin/bash
# pinyin-ime 健康检查
# 验证安装是否完整，所有组件是否可用

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
cd "$PROJECT_ROOT"

echo "╔══════════════════════════════════════════════════════╗"
echo "║          pinyin-ime 健康检查                          ║"
echo "╚══════════════════════════════════════════════════════╝"
echo ""

PASS=0; FAIL=0; WARN=0
ok()   { echo "  ✅ $1"; PASS=$((PASS+1)); }
fail() { echo "  ❌ $1"; FAIL=$((FAIL+1)); }
warn() { echo "  ⚠️ $1"; WARN=$((WARN+1)); }

# 1. Check source files
echo "【1】源文件"
[ -f cpp/pinyin_ime_cli.cpp ] && ok "CLI源码" || fail "CLI源码缺失"
[ -f fcitx5-addon/pinyinime.cpp ] && ok "addon源码" || fail "addon源码缺失"
[ -f scripts/build_cli.sh ] && ok "构建脚本" || fail "构建脚本缺失"
echo ""

# 2. Check build artifacts
echo "【2】构建产物"
source ./setup_env.sh 2>/dev/null
if [ -n "$PINYIN_IME_CLI" ] && [ -x "$PINYIN_IME_CLI" ]; then
    ok "CLI二进制: $PINYIN_IME_CLI"
else
    fail "CLI二进制未构建 (run scripts/build_cpp.sh)"
fi
if [ -n "$PINYIN_IME_LM" ] && [ -f "$PINYIN_IME_LM" ]; then
    ok "语言模型: $(du -h "$PINYIN_IME_LM" | cut -f1)"
else
    fail "语言模型缺失 (run scripts/build_cli.sh)"
fi
if [ -n "$PINYIN_IME_DICT" ] && [ -f "$PINYIN_IME_DICT" ]; then
    ok "系统词典: $(du -h "$PINYIN_IME_DICT" | cut -f1)"
else
    fail "系统词典缺失"
fi
echo ""

# 3. Check basic functionality
echo "【3】基本功能"
if [ -n "$PINYIN_IME_CLI" ] && [ -x "$PINYIN_IME_CLI" ]; then
    RESULT=$(echo "nihao" | "$PINYIN_IME_CLI" --quiet --top1 "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 2>/dev/null)
    [ "$RESULT" = "你好" ] && ok "基本输入: nihao→你好" || fail "基本输入异常: nihao→$RESULT"
    
    RESULT=$(echo "zhongguo" | "$PINYIN_IME_CLI" --quiet --top1 "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 2>/dev/null)
    [ "$RESULT" = "中国" ] && ok "词组输入: zhongguo→中国" || fail "词组输入异常: zhongguo→$RESULT"
    
    RESULT=$(echo "xianbeijing" | "$PINYIN_IME_CLI" --quiet --top1 "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 2>/dev/null)
    [ "$RESULT" = "西安北京" ] && ok "消歧: xianbeijing→西安北京" || warn "消歧: xianbeijing→$RESULT"
fi
echo ""

# 4. Check user data
echo "【4】用户数据"
USER_DATA="${HOME}/.local/share/pinyin-ime"
if [ -f "$USER_DATA/user.history" ]; then
    ok "用户历史: $(du -h "$USER_DATA/user.history" | cut -f1)"
else
    warn "用户历史不存在 (run scripts/warmup.sh)"
fi
if [ -f "$USER_DATA/user.dict" ]; then
    ok "用户词典: $(du -h "$USER_DATA/user.dict" | cut -f1)"
else
    warn "用户词典不存在"
fi
echo ""

# 5. Check E2E test binary
echo "【5】E2E测试"
NB="/root/workspace/native-build"
if [ -f "$NB/test_pinyin_e2e" ]; then
    ok "E2E测试二进制"
else
    warn "E2E测试未构建 (run scripts/build_e2e_test.sh)"
fi
echo ""

# 6. Check Python tools
echo "【6】Python工具"
if /usr/local/bin/python3 -c "import pypinyin" 2>/dev/null; then
    ok "pypinyin 模块"
else
    warn "pypinyin 未安装 (pip install pypinyin)"
fi
[ -f scripts/eval_daily.py ] && ok "daily评测脚本" || fail "daily评测脚本缺失"
[ -f scripts/benchmark.py ] && ok "性能基准脚本" || fail "性能基准脚本缺失"
echo ""

# Summary
echo "╔══════════════════════════════════════════════════════╗"
echo "║  结果: ✅$PASS  ❌$FAIL  ⚠️$WARN"
if [ "$FAIL" -gt 0 ]; then
    echo "║  ❌ 有 $FAIL 项失败，请修复后重试"
elif [ "$WARN" -gt 0 ]; then
    echo "║  ⚠️ 有 $WARN 项警告，不影响基本使用"
    echo "║  ✅ 核心功能正常"
else
    echo "║  ✅ 全部通过，安装完整"
fi
echo "╚══════════════════════════════════════════════════════╝"
