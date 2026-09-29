#!/usr/bin/env bash
# fcitx5-X11（XIM 前端）补编前置依赖：X11/Xvfb 系统库 + xcb-imdkit + yoga 子模块 + rsvg 补丁。
#
# 这是 scripts/build_fcitx5_x11.sh 的前置步骤（一次性）。openEuler 22.03 aarch64 实测：
#   - openEuler fcitx5 包仓库没有，需 conda-forge 工具链 + 系统 X11 开发库混合编译；
#   - xcb-imdkit（XIM 协议库）conda-forge 无此包，必须源码编入 $NB/opt/xcb-imdkit；
#   - fcitx5 5.1.23 的 GitHub archive 缺 yoga 子模块，按钉死的 commit 补齐到 third_party/yoga；
#   - openEuler librsvg 2.50 移除 rsvg_handle_get_intrinsic_size_in_pixels，需本地补丁。
set -euo pipefail

NB=${NB:-/root/workspace/native-build}
ENV="$NB/mamba-env"
SRC="$NB/src"
OPT="$NB/opt"
PROXY="${PROXY:-http://127.0.0.1:7890}"

# ---------- 1. 系统 X11/Xvfb 依赖（dnf）----------
echo "=== dnf 安装 X11 依赖 ==="
dnf install -y --setopt=install_weak_deps=False \
    xorg-x11-server-Xvfb xterm dbus-x11 dbus-tools \
    libxcb-devel xcb-util-devel xcb-util-keysyms-devel xcb-util-wm-devel \
    xcb-util-image-devel xcb-util-renderutil-devel xcb-util-cursor-devel \
    libX11-devel libXtst-devel libxkbcommon-devel libxkbcommon-x11-devel \
    xorg-x11-proto-devel xorg-x11-util-macros libXinerama-devel \
    expat-devel fontconfig-devel freetype-devel libpng-devel pixman-devel \
    pango-devel gdk-pixbuf2-devel librsvg2-devel libxkbfile-devel iso-codes

# ---------- 2. nlohmann-json（fcitx5 keyboard 需要，conda 包名是 nlohmann_json）----------
echo "=== conda 安装 nlohmann_json ==="
"$NB/bin/micromamba" install -y -p "$ENV" \
    -c "${MIRROR:-https://mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge}" \
    nlohmann_json

# ---------- 3. xcb-imdkit 1.0.3 源码编译进 $OPT/xcb-imdkit ----------
if [ ! -f "$OPT/xcb-imdkit/lib64/cmake/XCBImdkit/XCBImdkitConfig.cmake" ]; then
    echo "=== 编译 xcb-imdkit 1.0.3 ==="
    mkdir -p "$SRC"
    curl -L --retry 5 -x "$PROXY" -o "$SRC/xcb-imdkit.tar.gz" \
        "https://codeload.github.com/fcitx/xcb-imdkit/tar.gz/refs/tags/1.0.3"
    tar -xzf "$SRC/xcb-imdkit.tar.gz" -C "$SRC"
    cmake -S "$SRC/xcb-imdkit-1.0.3" -B "$SRC/xcb-imdkit-1.0.3/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX="$OPT/xcb-imdkit" \
        -DCMAKE_PREFIX_PATH="$ENV"
    PATH="$ENV/bin:$PATH" ninja -C "$SRC/xcb-imdkit-1.0.3/build"
    PATH="$ENV/bin:$PATH" ninja -C "$SRC/xcb-imdkit-1.0.3/build" install
fi

# ---------- 4. 补 yoga 子模块（fbacebook/yoga @042f5013152e）----------
YOGADIR="$SRC/fcitx5-5.1.23/third_party/yoga"
if [ ! -f "$YOGADIR/yoga/CMakeLists.txt" ]; then
    echo "=== 补 yoga 子模块 ==="
    curl -L --retry 5 -x "$PROXY" -o "$SRC/yoga.tar.gz" \
        "https://codeload.github.com/facebook/yoga/tar.gz/042f5013152e"
    rm -rf "$YOGADIR"
    tar -xzf "$SRC/yoga.tar.gz" -C "$SRC/fcitx5-5.1.23/third_party"
    mv "$SRC/fcitx5-5.1.23/third_party"/yoga-* "$YOGADIR"
fi

# ---------- 5. librsvg 2.50 兼容补丁（本地源码副本，不入库）----------
THEMECPP="$SRC/fcitx5-5.1.23/src/ui/classic/theme.cpp"
if grep -q "rsvg_handle_get_intrinsic_size_in_pixels" "$THEMECPP" 2>/dev/null; then
    echo "=== 打 librsvg 兼容补丁 ==="
    python3 - "$THEMECPP" <<'PY'
import sys
p = sys.argv[1]
s = open(p, encoding='utf-8').read()
old = '''    if (rsvg_handle_get_intrinsic_size_in_pixels(svg, &w, &h)) {
        int width = std::max(1, static_cast<int>(std::ceil(w)));
        int height = std::max(1, static_cast<int>(std::ceil(h)));
        return std::make_pair(width, height);
    }'''
new = '''    RsvgDimensionData dim{};
    rsvg_handle_get_dimensions(svg, &dim);
    if (dim.width > 0 && dim.height > 0) {
        return std::make_pair(std::max(1, dim.width), std::max(1, dim.height));
    }'''
    assert s.count(old) == 1, s.count(old)
    open(p, 'w', encoding='utf-8').write(s.replace(old, new))
PY
fi

echo "=== X11 依赖就绪，下一步：bash scripts/build_fcitx5_x11.sh ==="