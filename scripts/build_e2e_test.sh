#!/bin/bash
# Build and run the fcitx5 end-to-end typing test
# This tests pinyinime through the real fcitx5 API: key press → preedit → candidate → commit

set -e
NB=${NB:-/root/workspace/native-build}
ENV="$NB/mamba-env"
OPT="$NB/opt"
FCITX5_SRC="$NB/src/fcitx5-5.1.23"
BUILD="$NB/build"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

export PATH="$ENV/bin:$PATH"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5-server/lib:$OPT/fcitx5-server/lib/fcitx5:$OPT/libime/lib"

GCC_CXX="$ENV/lib/gcc/aarch64-conda-linux-gnu/16.2.0/include/c++"

echo "=== Building E2E test ==="
$ENV/bin/g++ -std=c++20 -o "$NB/test_pinyin_e2e_ext" "$PROJECT_ROOT/test/e2e_typing_test.cpp" \
  -isystem "$GCC_CXX" -isystem "$GCC_CXX/aarch64-conda-linux-gnu" \
  -isystem "$ENV/aarch64-conda-linux-gnu/sysroot/usr/include" \
  -I"$OPT/fcitx5/include/Fcitx5/Core" -I"$OPT/fcitx5/include/Fcitx5/Utils" \
  -I"$OPT/fcitx5/include/Fcitx5/Config" -I"$BUILD/fcitx5-full/src/lib" \
  -I"$FCITX5_SRC/src/lib" -I"$FCITX5_SRC/testing/testfrontend" \
  -L"$OPT/fcitx5-server/lib" -L"$OPT/fcitx5-server/lib/fcitx5" -L"$ENV/lib" \
  -Wl,-rpath-link,"$OPT/fcitx5-server/lib" \
  -lFcitx5Core -lFcitx5Config -lFcitx5Utils -ltestfrontend \
  -Wl,-rpath,"$OPT/fcitx5-server/lib" -Wl,-rpath,"$OPT/fcitx5-server/lib/fcitx5" \
  -Wl,-rpath,"$ENV/lib" -lpthread

echo "=== Running E2E test ==="
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

timeout 15 "$NB/test_pinyin_e2e_ext" 2>&1 | grep -v "^I\|^E\|^W"
