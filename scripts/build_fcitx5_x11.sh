#!/usr/bin/env bash
# 补编 fcitx5-X11（XIM 前端 + 主程序 server + keyboard），用于在 Xvfb 虚拟桌面下
# 实机运行 pinyinime addon（scripts/xvfb_ime_test.sh 的前提）。
#
# 与 build_conda_forge.sh（引擎版 fcitx5，X11/Wayland/DBus/Server 全关）的区别：
#   开 ENABLE_X11=On / ENABLE_KEYBOARD=On / ENABLE_SERVER=On，其余保持一致。
#
# 关键坑（openEuler 22.03 aarch64 实测，见 README「第8轮」）：
#   1. conda-forge gcc 16 是交叉式编译器，默认 include 搜索只有 conda 前缀的 sysroot，
#      不搜宿主机 /usr/include → 用 -idirafter 兜底系统头，LIBRARY_PATH 兜底系统库；
#   2. conda 的 pkg-config 默认只搜 conda 前缀 → 显式把 /usr/lib64/pkgconfig 前置，
#      让 cairo/pango/gdk-pixbuf/librsvg/xcb 等系统 .pc 能被找到；
#   3. xcb-imdkit 1.0.3 conda-forge 无此包 → 需先源码编入 $NB/opt/xcb-imdkit
#      （见 scripts/setup_x11_deps.sh）；
#   4. fcitx5 5.1.23 的 GitHub archive 缺 yoga 子模块 → 需先补齐
#      third_party/yoga（facebook/yoga @042f5013152e），否则 add_subdirectory 失败；
#   5. openEuler librsvg 2.50 移除 rsvg_handle_get_intrinsic_size_in_pixels →
#      需先跑 scripts/setup_x11_deps.sh 打 librsvg 兼容补丁。
set -euo pipefail

NB=${NB:-/root/workspace/native-build}
ENV="$NB/mamba-env"
OPT="$NB/opt"
BUILD="$NB/build"
SRC="$NB/src"

export PATH="$ENV/bin:$PATH"
export CC="$ENV/bin/aarch64-conda-linux-gnu-cc"
export CXX="$ENV/bin/aarch64-conda-linux-gnu-c++"
# CMake config 搜索：conda 工具链 + 源码编译的 xcb-imdkit
export CMAKE_PREFIX_PATH="$ENV:$OPT/xcb-imdkit"
# pkg-config：系统 X11/cairo/pango 等 .pc 在前，conda 的在后（避免 conda pkg-config 只搜自己）
export PKG_CONFIG_PATH="/usr/lib64/pkgconfig:/usr/share/pkgconfig:$ENV/lib/pkgconfig:$ENV/share/pkgconfig:$OPT/xcb-imdkit/lib64/pkgconfig"
# 系统头/库兜底：conda gcc 不搜 /usr/include、/usr/lib64
export CFLAGS="-idirafter /usr/include"
export CXXFLAGS="-idirafter /usr/include"
export LIBRARY_PATH="/usr/lib64:$ENV/lib"
export LD_LIBRARY_PATH="$ENV/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"

FCITX_BUILD="$BUILD/fcitx5-x11"
mkdir -p "$FCITX_BUILD"

echo "=== configure fcitx5-x11 ==="
cmake -S "$SRC/fcitx5-5.1.23" -B "$FCITX_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$OPT/fcitx5-x11" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DENABLE_TESTING_ADDONS=Off \
    -DENABLE_TEST=Off \
    -DENABLE_COVERAGE=Off \
    -DENABLE_ENCHANT=Off \
    -DENABLE_X11=On \
    -DENABLE_WAYLAND=Off \
    -DENABLE_DBUS=Off \
    -DENABLE_DOC=Off \
    -DENABLE_SERVER=On \
    -DENABLE_KEYBOARD=On \
    -DUSE_SYSTEMD=Off \
    -DENABLE_XDGAUTOSTART=Off \
    -DENABLE_EMOJI=Off \
    -DBUILD_SPELL_DICT=Off \
    -DENABLE_LIBUUID=On \
    -DENABLE_DL=On \
    -DUSE_SYSTEM_YOGA=Off \
    -DISOCODES_ISO639_JSON=/usr/share/iso-codes/json/iso_639-3.json \
    > "$BUILD/fcitx5-x11-configure.log" 2>&1
echo "=== build fcitx5-x11 ==="
ninja -C "$FCITX_BUILD" > "$BUILD/fcitx5-x11-build.log" 2>&1
echo "=== install fcitx5-x11 ==="
ninja -C "$FCITX_BUILD" install > "$BUILD/fcitx5-x11-install.log" 2>&1

# ---- 数据路径适配（addon 能在此 prefix 下找到 LM/dict）----
# libime 的「LM 在 lib/libime、dict 在 share/libime」区分布局，addon 的 DataDir 候选
# 无法同时命中二者，用符号链接在 lib/libime/ 下补齐 sc.dict，让 DataDir=$OPT/libime/lib 两者都能找到。
ln -sf "$OPT/libime/share/libime/sc.dict" "$OPT/libime/lib/libime/sc.dict"

# ---- 默认输入法（zh_CN 语言默认配置）----
# fcitx5 首次启动（无 profile）会按 data/default/<lang> 决定默认输入法（官方写的是 pinyin/rime）。
# 本 addon 注册名是 pinyinime，覆盖成它，让 XIM 实测里 Ctrl+Space 能直接切到本输入法。
cat > "$OPT/fcitx5-x11/share/fcitx5/default/zh_CN" <<'EOF'
[DefaultInputMethod]
0=pinyinime
EOF

echo "FCITX5-X11 DONE -> $OPT/fcitx5-x11"