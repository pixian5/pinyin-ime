#!/bin/bash
# Complete evaluation suite for pinyin-ime
echo "=========================================="
echo "  pinyin-ime 完整评测报告"
echo "=========================================="
echo ""

echo "--- 1. fcitx5 端到端打字测试 ---"
NB=${NB:-/workspace/native-build}
OPT="$NB/opt"
export PATH="$NB/mamba-env/bin:$PATH"
export LD_LIBRARY_PATH="$NB/mamba-env/lib:$OPT/fcitx5-server/lib:$OPT/fcitx5-server/lib/fcitx5:$OPT/libime/lib"
export XDG_DATA_HOME="$OPT/fcitx5-server/share"
export XDG_CONFIG_HOME="/tmp/fcitx5-test-config"
export HOME="/tmp/fcitx5-test-home"
export FCITX_DATA_DIRS="$OPT/fcitx5-server/share/fcitx5"
mkdir -p "$XDG_CONFIG_HOME/fcitx5/conf" "$HOME"
cat > "$XDG_CONFIG_HOME/fcitx5/profile" << 'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=pinyinime
[Groups/0/Items/0]
Name=pinyinime
Layout=
[GroupOrder]
0=Default
EOF
timeout 15 "$NB/test_pinyin_e2e_ext" 2>&1 | grep -E "PASS|FAIL|Total|Accuracy|Summary"

echo ""
echo "--- 2. 日常句子准确率 ---"
python3 scripts/eval_daily.py 2>&1 | grep -v "^$"

echo ""
echo "--- 3. 新闻/百科准确率 ---"
python3 scripts/eval_engine2.py 2>&1 | grep -v "^$"

echo ""
echo "=========================================="
echo "  评测完成"
echo "=========================================="
