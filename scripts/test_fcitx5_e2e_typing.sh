#!/bin/bash
# =============================================================================
# pinyin-ime fcitx5 End-to-End Typing Test
# 
# This test uses fcitx5's testfrontend addon to directly test the pinyinime
# input method without requiring X11 or DBus. It verifies:
#   1. pinyinime addon loads in fcitx5
#   2. IM switches to pinyinime
#   3. Key events are accepted
#   4. Preedit text is produced
#   5. Candidate lists are generated
#   6. Text is committed on Space/number selection
# =============================================================================

set -e

NB=/workspace/native-build
ENV="$NB/mamba-env"
OPT="$NB/opt"
FCITX5_SRC="$NB/src/fcitx5-5.1.23"

export PATH="$ENV/bin:$OPT/fcitx5-server/bin:$PATH"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5-server/lib:$OPT/fcitx5-server/lib/fcitx5:$OPT/libime/lib"
export XDG_DATA_DIRS="$OPT/fcitx5-server/share:$ENV/share:/usr/share"
export XDG_CONFIG_HOME=/tmp/fcitx5-e2e-config
export XDG_RUNTIME_DIR=/tmp/runtime-root
mkdir -p "$XDG_CONFIG_HOME/fcitx5" "$XDG_RUNTIME_DIR"

# Create profile with pinyinime as default IM
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

# Check if test binary exists
if [ ! -f "$NB/test_pinyin_e2e" ]; then
    echo "❌ Test binary not found. Building..."
    GCC_CXX="$ENV/lib/gcc/aarch64-conda-linux-gnu/16.2.0/include/c++"
    $ENV/bin/g++ -std=c++20 -o "$NB/test_pinyin_e2e" "$NB/test_pinyin_e2e.cpp" \
      -isystem "$GCC_CXX" \
      -isystem "$GCC_CXX/aarch64-conda-linux-gnu" \
      -isystem "$ENV/aarch64-conda-linux-gnu/sysroot/usr/include" \
      -I"$OPT/fcitx5-server/include/Fcitx5/Utils" \
      -I"$OPT/fcitx5-server/include/Fcitx5/Config" \
      -I"$OPT/fcitx5-server/include/Fcitx5/Core" \
      -I"$OPT/fcitx5-server/include/Fcitx5/Module" \
      -I"$OPT/fcitx5-server/include/Fcitx5/Module/fcitx-module/testfrontend" \
      -I"$FCITX5_SRC/testing/testfrontend" \
      -L"$OPT/fcitx5-server/lib" \
      -L"$OPT/fcitx5-server/lib/fcitx5" \
      -L"$ENV/lib" \
      -Wl,-rpath-link,"$OPT/fcitx5-server/lib" \
      -lFcitx5Core -lFcitx5Config -lFcitx5Utils -ltestfrontend \
      -Wl,-rpath,"$OPT/fcitx5-server/lib" \
      -Wl,-rpath,"$OPT/fcitx5-server/lib/fcitx5" \
      -Wl,-rpath,"$ENV/lib" \
      -lpthread
    echo "✅ Built test binary"
fi

echo ""
echo "=============================================="
echo "  pinyin-ime fcitx5 End-to-End Typing Test"
echo "=============================================="
echo ""

# Run the test and capture output
OUTPUT=$(timeout 15 "$NB/test_pinyin_e2e" 2>&1)

# Parse and display results
PASS=0
FAIL=0

check() {
    local desc="$1"
    local pattern="$2"
    if echo "$OUTPUT" | grep -q "$pattern"; then
        echo "  ✅ $desc"
        PASS=$((PASS + 1))
    else
        echo "  ❌ $desc"
        FAIL=$((FAIL + 1))
    fi
}

echo "--- Addon Loading ---"
check "pinyinime addon loaded" "Loaded addon pinyinime"
check "LM and dict loaded" "pinyinime: loaded lm="
check "IM registered" "Found 1 input method"

echo ""
echo "--- IM Activation ---"
check "IM switched to pinyinime" "Current IM: pinyinime"

echo ""
echo "--- Typing 'nihao' ---"
check "Keys accepted (n,i,h,a,o)" "accepted: 1"
check "Preedit 'ni hao'" "Preedit after nihao: 'ni hao'"
check "Candidate 你好" "\[0\] 你好"
check "9 candidates" "Candidates: 9"

echo ""
echo "--- Commit on Space ---"
check "Commit 你好" "Commit: 你好"

echo ""
echo "--- Typing 'shijie' ---"
check "Preedit 'shi jie'" "Preedit after shijie: 'shi jie'"
check "Candidate 世界" "\[0\] 世界"
check "Commit 世界" "Commit: 世界"

echo ""
echo "--- Summary ---"
echo "  Passed: $PASS"
echo "  Failed: $FAIL"
echo ""

if [ $FAIL -eq 0 ]; then
    echo "🎉 All checks passed! pinyin-ime works end-to-end in fcitx5."
    exit 0
else
    echo "⚠️  Some checks failed. See full output below:"
    echo ""
    echo "$OUTPUT"
    exit 1
fi
