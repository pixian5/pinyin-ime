#!/bin/bash
# CI/CD test script for pinyin-ime
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"
NB=${NB:-/root/workspace/native-build}
PYTHON3=/usr/local/bin/python3
FAIL=0

echo "================================================"
echo "  pinyin-ime CI/CD 测试 $(date '+%Y-%m-%d %H:%M:%S')"
echo "================================================"

cd "$PROJECT_ROOT"

# 1. CLI Tests
printf "【1】CLI综合测试: "
CLI=$(timeout 120 bash scripts/test_all.sh 2>&1)
CP=$(echo "$CLI" | grep -c "✅")
CF=$(echo "$CLI" | grep -c "❌")
if [ "$CF" -eq 0 ]; then printf "✅ %d/%d\n" "$CP" "$((CP+CF))"; else printf "❌ %d/%d\n" "$CP" "$((CP+CF))"; FAIL=1; fi

# 2. Daily Accuracy
printf "【2】daily 40句准确率: "
DAILY=$($PYTHON3 scripts/eval_daily.py 2>&1)
DS=$(echo "$DAILY" | grep "Sentence" | grep -oP '\d+/\d+')
DNUM=$(echo "$DS" | grep -oP "^\d+"); if [ "$DNUM" -ge 38 ] 2>/dev/null; then printf "✅ %s\n" "$DS"; else printf "❌ %s\n" "$DS"; FAIL=1; fi

# 3. Performance
printf "【3】性能基准: "
BENCH=$($PYTHON3 scripts/benchmark.py 2>&1)
TP=$(echo "$BENCH" | grep -oP '\d+ 句/秒')
if [ -n "$TP" ]; then printf "✅ %s\n" "$TP"; else printf "❌\n"; FAIL=1; fi

# 4. E2E Test
printf "【4】E2E端到端测试: "
if [ -f "$NB/test_pinyin_e2e" ]; then
    export LD_LIBRARY_PATH="$NB/mamba-env/lib:$NB/opt/fcitx5-server/lib:$NB/opt/fcitx5-server/lib/fcitx5:$NB/opt/libime/lib"
    export XDG_DATA_HOME="$NB/opt/fcitx5-server/share"
    export XDG_CONFIG_HOME="/tmp/fcitx5-test-config"
    export HOME="/tmp/fcitx5-test-home"
    mkdir -p "$XDG_CONFIG_HOME/fcitx5/conf" "$HOME"
    cat > "$XDG_CONFIG_HOME/fcitx5/profile" << 'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=pinyinime
[Groups/0/Items/0]
Name=keyboard-us
Layout=
[Groups/0/Items/1]
Name=pinyinime
Layout=
[GroupOrder]
0=Default
EOF
    E2E=$(timeout 60 "$NB/test_pinyin_e2e" 2>&1 | grep "Total:")
    EP=$(echo "$E2E" | grep -oP 'Pass: \K\d+')
    ET=$(echo "$E2E" | grep -oP 'Total: \K\d+')
    EF=$(echo "$E2E" | grep -oP 'Fail: \K\d+')
    if [ "$EF" = "0" ]; then printf "✅ %d/%d\n" "$EP" "$ET"; else printf "❌ %d/%d\n" "$EP" "$ET"; FAIL=1; fi
else
    printf "⚠️ 未构建\n"
fi

echo "================================================"
if [ "$FAIL" -eq 0 ]; then echo "  ✅ 全部通过"; exit 0; else echo "  ❌ 有失败项"; exit 1; fi
