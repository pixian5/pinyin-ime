#!/bin/bash
# Build and run the fcitx5 end-to-end typing test
# This tests pinyinime through the real fcitx5 API: key press → preedit → candidate → commit

set -e
NB=${NB:-/workspace/native-build}
ENV="$NB/mamba-env"
OPT="$NB/opt"
FCITX5_SRC="$NB/src/fcitx5-5.1.23"
BUILD="$NB/build"
SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJECT_ROOT="$(dirname "$SCRIPT_DIR")"

export PATH="$ENV/bin:$PATH"
export LD_LIBRARY_PATH="$ENV/lib:$OPT/fcitx5-server/lib:$OPT/fcitx5-server/lib/fcitx5:$OPT/libime/lib"

GCC_CXX="$ENV/lib/gcc/aarch64-conda-linux-gnu/16.2.0/include/c++"

# Step 1: Build fcitx5-full with testing addons (if not already built)
if [ ! -f "$OPT/fcitx5-server/lib/fcitx5/libtestfrontend.so" ]; then
    echo "=== Building fcitx5-full with testing addons ==="
    # Install required deps
    MAMBA_BIN="$NB/bin/micromamba"
    export MAMBA_ROOT_PREFIX="$NB/mamba-root"
    MIRROR="https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge"
    "$MAMBA_BIN" install -y -p "$ENV" -c "$MIRROR" expat jsoncpp iso-codes nlohmann_json 2>&1 | tail -3

    cmake -S "$FCITX5_SRC" -B "$BUILD/fcitx5-full" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$OPT/fcitx5-server" \
        -DCMAKE_INSTALL_LIBDIR=lib \
        -DENABLE_TESTING_ADDONS=On -DENABLE_TEST=Off -DENABLE_COVERAGE=Off \
        -DENABLE_ENCHANT=Off -DENABLE_X11=Off -DENABLE_WAYLAND=Off -DENABLE_DBUS=Off \
        -DENABLE_DOC=Off -DENABLE_SERVER=On -DENABLE_KEYBOARD=On \
        -DUSE_SYSTEMD=Off -DENABLE_XDGAUTOSTART=Off -DENABLE_EMOJI=Off \
        -DBUILD_SPELL_DICT=Off -DENABLE_LIB_UUID=On -DENABLE_DL=On \
        -DISOCODES_ISO639_JSON="$ENV/share/iso-codes/json/iso_639-3.json"

    ninja -C "$BUILD/fcitx5-full"
    ninja -C "$BUILD/fcitx5-full" install

    # Copy testing addon configs to main addon directory
    cp "$OPT/fcitx5-server/share/fcitx5/testing/addon/"*.conf "$OPT/fcitx5-server/share/fcitx5/addon/"
fi

# Step 2: Install pinyinime addon
echo "=== Installing pinyinime addon ==="
cp "$PROJECT_ROOT/fcitx5-addon/build/libpinyinime.so" "$OPT/fcitx5-server/lib/fcitx5/"
cp "$PROJECT_ROOT/fcitx5-addon/build/pinyinime.conf" "$OPT/fcitx5-server/share/fcitx5/addon/"

# Step 3: Set up data directory
DATA_DIR="/tmp/libime_data"
mkdir -p "$DATA_DIR/libime"
ln -sf "$OPT/libime/lib/libime/zh_CN.lm" "$DATA_DIR/libime/zh_CN.lm"
ln -sf "$OPT/libime/share/libime/sc.dict" "$DATA_DIR/sc.dict"

# Step 4: Build E2E test
echo "=== Building E2E test ==="
$ENV/bin/g++ -std=c++20 -o "$NB/test_pinyin_e2e" "$PROJECT_ROOT/test/e2e_typing_test.cpp" \
  -isystem "$GCC_CXX" -isystem "$GCC_CXX/aarch64-conda-linux-gnu" \
  -isystem "$ENV/aarch64-conda-linux-gnu/sysroot/usr/include" \
  -I"$OPT/fcitx5-server/include/Fcitx5/Core" -I"$OPT/fcitx5-server/include/Fcitx5/Utils" \
  -I"$OPT/fcitx5-server/include/Fcitx5/Config" \
  -I"$BUILD/fcitx5-full/src/lib" \
  -I"$FCITX5_SRC/src/lib" -I"$FCITX5_SRC/testing/testfrontend" \
  -L"$OPT/fcitx5-server/lib" -L"$OPT/fcitx5-server/lib/fcitx5" -L"$ENV/lib" \
  -L"$BUILD/fcitx5-full/lib" \
  -Wl,-rpath-link,"$OPT/fcitx5-server/lib" \
  -Wl,-rpath,"$OPT/fcitx5-server/lib" -Wl,-rpath,"$OPT/fcitx5-server/lib/fcitx5" \
  -Wl,-rpath,"$ENV/lib" \
  -lFcitx5Core -lFcitx5Config -lFcitx5Utils -ltestfrontend -lkeyboard \
  -lxkbcommon -lexpat -lpthread

echo "=== E2E test binary built ==="

# Step 5: Run E2E test
echo "=== Running E2E test ==="
export XDG_DATA_HOME="$OPT/fcitx5-server/share"
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

timeout 30 "$NB/test_pinyin_e2e" 2>&1 | grep -v "^W\|^I"
