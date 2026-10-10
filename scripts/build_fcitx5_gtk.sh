#!/usr/bin/env bash
# fcitx5-gtk 5.1.7 —— GTK3 im module（GTK_IM_MODULE=fcitx 的入口），支撑「真桌面」端到端打字。
#
# 关键点（openEuler 22.03 aarch64 实测，第16轮）：
#   1. fcitx5-gtk 是纯客户端库（自带 Fcitx5GClient，经 DBus 与 fcitx5 守护进程通信），
#      不链接 fcitx5 核心库 → 与 fcitx5 二进制无 ABI 耦合，故用【系统 gcc 10.3】编译，
#      避免 conda-gcc 的 libstdc++ 与系统 GTK3 应用的 libstdc++ 冲突；
#   2. 只编 GTK3 im module（关 GTK2/GTK4/GIR/SNOOPER，少拉依赖);
#   3. ECM（Extra CMake Modules）来自 conda env（$ENV/share/ECM/cmake），经 -DECM_DIR 指入，
#      ECM 是纯 cmake 模块、与编译器架构无关；
#   4. openEuler GTK3 的 immodule 用「immodules.cache 缓存文件」发现（非运行时扫描），
#      需用 gtk-query-immodules-3.0-64 生成并写入系统路径（需 root）。
set -euo pipefail

NB=${NB:-/root/workspace/native-build}
ENV="$NB/mamba-env"
OPT="$NB/opt"
BUILD="$NB/build"
SRC="$NB/src"

# 只读 /usr 沙箱：X11 头/库在 RPM 本地前缀里，FindX11 需要显式指路（同 build_fcitx5_x11.sh）
SYSROOT="${SYSROOT:-}"
if [ -z "$SYSROOT" ]; then
    for _r in "$NB/xroot" /workspace/xroot; do
        [ -d "$_r/usr/include" ] && SYSROOT="$_r" && break
    done
fi

GTK_SRC="$SRC/fcitx5-gtk-5.1.7"
GTK_BUILD="$BUILD/fcitx5-gtk"
mkdir -p "$GTK_BUILD"

echo "=== configure fcitx5-gtk（系统 gcc + 仅 GTK3）==="
# 真实机器上 X11 在 /usr（FindX11 自己找得到），只在沙箱前缀场景才注入提示
X11_ARGS=()
if [ -n "$SYSROOT" ]; then
    X11_ARGS=(-DX11_X11_INCLUDE_PATH="$SYSROOT/usr/include"
              -DX11_X11_LIB="$SYSROOT/usr/lib64/libX11.so")
fi
cmake -S "$GTK_SRC" -B "$GTK_BUILD" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX="$OPT/fcitx5-x11" \
    -DCMAKE_INSTALL_LIBDIR=lib \
    -DECM_DIR="$ENV/share/ECM/cmake" \
    ${X11_ARGS[@]+"${X11_ARGS[@]}"} \
    -DENABLE_GTK2_IM_MODULE=Off \
    -DENABLE_GTK3_IM_MODULE=On \
    -DENABLE_GTK4_IM_MODULE=Off \
    -DENABLE_GIR=Off \
    -DENABLE_SNOOPER=Off \
    > "$BUILD/fcitx5-gtk-configure.log" 2>&1
echo "=== build fcitx5-gtk ==="
ninja -C "$GTK_BUILD" > "$BUILD/fcitx5-gtk-build.log" 2>&1
echo "=== install fcitx5-gtk（im-fcitx5.so 进 fcitx5 前缀）==="
ninja -C "$GTK_BUILD" install > "$BUILD/fcitx5-gtk-install.log" 2>&1

# ---- 注册 GTK3 immodule（openEuler GTK3 靠 immodules.cache 发现，非运行时扫描）----
# 生成缓存指向本前缀下的 im-fcitx5.so；写入 GTK3 编译期 libdir（需 root）。
MODDIR="$OPT/fcitx5-x11/lib/gtk-3.0/3.0.0/immodules"
if [ -f "$MODDIR/im-fcitx5.so" ]; then
    echo "=== 生成 GTK3 immodules.cache ==="
    GTK_QUERY="$(command -v gtk-query-immodules-3.0-64 || command -v gtk-query-immodules-3.0)"
    # 沙箱里 query 工具来自 RPM 前缀，libgtk-3 不在系统路径 → 补 LD_LIBRARY_PATH
    # （真实机器上工具用系统 libgtk，此变量为空即无副作用）
    LD_LIBRARY_PATH="${SYSROOT:+$SYSROOT/usr/lib64}${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}" \
        "$GTK_QUERY" "$MODDIR/im-fcitx5.so" > /tmp/immodules.cache 2>/dev/null
    # 首选写系统 libdir（真实机器上普通桌面安装即生效）；
    # 只读 /usr 的沙箱写不进去 → 降级到前缀内缓存，由 GTK_IM_MODULE_FILE 指过去。
    if mkdir -p /usr/lib64/gtk-3.0/3.0.0 2>/dev/null && \
       cp /tmp/immodules.cache /usr/lib64/gtk-3.0/3.0.0/immodules.cache 2>/dev/null; then
        echo "    缓存条目：$(grep -c '"fcitx"' /usr/lib64/gtk-3.0/3.0.0/immodules.cache) 处 fcitx 模块（系统路径）"
    else
        cp /tmp/immodules.cache "$OPT/fcitx5-x11/lib/gtk-3.0/3.0.0/immodules.cache"
        echo "    /usr 只读：缓存已写 $OPT/fcitx5-x11/lib/gtk-3.0/3.0.0/immodules.cache"
        echo "    沙箱运行需 export GTK_IM_MODULE_FILE=$OPT/fcitx5-x11/lib/gtk-3.0/3.0.0/immodules.cache"
    fi
fi

# ---- 编译 GTK3 实测客户端（系统 gcc + xroot 里的 gtk3）----
# 沙箱把 xroot 写进 RPATH，这样调用方不必自带 LD_LIBRARY_PATH 也能找到 libgtk-3
GTK_RPATH=()
[ -n "${SYSROOT:-}" ] && GTK_RPATH=(-Wl,-rpath,"$SYSROOT/usr/lib64")
echo "=== 编译 tools/gtk_ime_test_client（真实 GTK3 应用，供端到端断言 GTK_COMMIT）==="
gcc -O2 -o tools/gtk_ime_test_client tools/gtk_ime_test_client.c \
    ${GTK_RPATH[@]+"${GTK_RPATH[@]}"} \
    $(pkg-config --cflags --libs gtk+-3.0 gdk-x11-3.0) 2>&1

# keysend_target：向 GTK 客户端窗口注入按键（XTest）。它是 xvfb_desktop_ime_test.sh 的硬依赖，
# 此前漏编译导致真桌面测试「0/10」（按键从未送达）。与 gtk_ime_test_client 一起固化在此，不再手工 cc。
echo "=== 编译 tools/keysend_target（按键注入器，xvfb_desktop_ime_test.sh 依赖）==="
gcc -O2 -o tools/keysend_target tools/keysend_target.c \
    ${GTK_RPATH[@]+"${GTK_RPATH[@]}"} \
    $(pkg-config --cflags --libs x11 xtst) 2>&1

echo "FCITX5-GTK DONE -> $OPT/fcitx5-x11  （immodule 已注册到系统 immodules.cache）"