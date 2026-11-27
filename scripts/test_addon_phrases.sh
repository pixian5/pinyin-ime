#!/bin/bash
# 多短语 addon 真实上屏验收（v0.9.200）
#
# 复用 test_runtime_fcitx5.sh：每个短语启动一次私有 Xvfb + fcitx5 + GTK3，
# 用 xdotool 真实敲键，检查「拼音 + 空格」提交的整句是否等于期望值。
# 目的不是跑全量 holdout，而是防止 addon 候选窗/索引映射在真实桌面里回归。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

# 只放已经验证过、能稳定复现的短语；新增用例先在本地跑通再加入。
CASES=(
    "nihao|你好"
    "shijian|时间"
    "zhongguo|中国"
    "beifang|北方"
    "xianzai|现在"
    "zhongguorenmin|中国人民"
    "jintiantianqihenhao|今天天气很好"
    "qinghuadaxue|清华大学"
)

PASS=0
FAIL=0
SKIP=0
for item in "${CASES[@]}"; do
    input="${item%%|*}"
    expect="${item#*|}"
    echo "--- $input → $expect ---"
    log="$(mktemp /tmp/pinyin-addon-case.XXXXXX)"
    if PINYINIME_TEST_INPUT="$input" PINYINIME_TEST_EXPECT="$expect" \
       bash scripts/test_runtime_fcitx5.sh >"$log" 2>&1; then
        if grep -q "运行时冒烟：6 通过，0 失败，0 跳过" "$log"; then
            echo "  ✅ $input + 空格 → $expect"
            PASS=$((PASS + 1))
        else
            echo "  ❌ 未完成真实上屏（可能缺 fcitx5/Xvfb/GTK 依赖）"
            tail -8 "$log" | sed 's/^/     /'
            SKIP=$((SKIP + 1))
        fi
    else
        echo "  ❌ $input 真实上屏失败"
        grep -E "FAIL|未得到|未加载|未注册" "$log" | tail -4 | sed 's/^/     /'
        FAIL=$((FAIL + 1))
    fi
    rm -f "$log"
done

echo ""
echo "addon 多短语验收：$PASS 通过，$FAIL 失败，$SKIP 跳过"
if [ "$FAIL" -ne 0 ]; then
    exit 1
fi
if [ "$SKIP" -ne 0 ]; then
    echo "缺少运行时依赖，未形成有效验收（CI 里应安装 fcitx5/Xvfb/xdotool/GTK3）"
    exit 1
fi
echo "全部通过 ✅"
