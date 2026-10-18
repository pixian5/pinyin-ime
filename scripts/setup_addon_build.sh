#!/bin/bash
# setup_addon_build.sh —— 搭建 fcitx5 addon 源码编译环境
#
# 在有 dnf + 代理的 openEuler 22.03 aarch64 机器上，用 S3 预构建的 fcitx5 头文件
# + GitHub 获取的 libime 1.1.17 头文件 + dnf 安装的 Boost，编译出 libpinyinime.so。
#
# 用法: bash scripts/setup_addon_build.sh && cmake --build fcitx5-addon/build
set -euo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPLOY="${DEPLOY:-/workspace/deploy}"
FCITX5_DIR="$DEPLOY/pinyin-ime/opt/fcitx5-x11"
RT_DIR="$DEPLOY/pinyin-ime-v0.9.84a"
LIBIME_INC="$DEPLOY/libime-headers"
CMAKE_DIR="$DEPLOY/libime-cmake"
COMPAT_INC="$DEPLOY/compat-headers"
LIBIME_LIB="$RT_DIR/lib"

log(){ echo -e "\033[36m[addon-build]\033[0m $*"; }

# 1) dnf install boost-devel
log "安装 Boost 头文件…"
dnf install -y boost-devel >/dev/null 2>&1 || true

# 2) 获取 libime 1.1.17 头文件
if [ ! -d "$LIBIME_INC/libime/core" ]; then
    log "获取 libime 1.1.17 头文件…"
    export https_proxy="${PROXY:-http://127.0.0.1:7890}"
    export http_proxy="$https_proxy"
    TMP=$(mktemp -d)
    git clone --depth 1 --filter=blob:none --sparse https://github.com/fcitx/libime.git "$TMP/libime"
    cd "$TMP/libime"
    git checkout 1.1.17 2>/dev/null || true
    git sparse-checkout set src/libime/core src/libime/pinyin
    mkdir -p "$LIBIME_INC/libime"
    cp -r src/libime/core src/libime/pinyin "$LIBIME_INC/libime/"
    cd "$REPO_DIR"
    rm -rf "$TMP"
fi

# 3) 创建 export 头文件（消费者侧：宏为空）
cat > "$LIBIME_INC/libime/core/libimecore_export.h" << 'EOF'
#ifndef LIBIMECORE_EXPORT_H
#define LIBIMECORE_EXPORT_H
#define LIBIMECORE_EXPORT
#define LIBIMECORE_NO_EXPORT
#define LIBIMECORE_DEPRECATED
#define LIBIMECORE_DEPRECATED_EXPORT
#define LIBIMECORE_DECL_EXPORT
#define LIBIMECORE_DECL_IMPORT
#endif
EOF
cat > "$LIBIME_INC/libime/pinyin/libimepinyin_export.h" << 'EOF'
#ifndef LIBIMEPINYIN_EXPORT_H
#define LIBIMEPINYIN_EXPORT_H
#define LIBIMEPINYIN_EXPORT
#define LIBIMEPINYIN_NO_EXPORT
#define LIBIMEPINYIN_DEPRECATED
#define LIBIMEPINYIN_DEPRECATED_EXPORT
#define LIBIMEPINYIN_DECL_EXPORT
#define LIBIMEPINYIN_DECL_IMPORT
#endif
EOF

# 4) 创建 source_location 兼容垫片（GCC 10 不支持 C++20 <source_location>）
mkdir -p "$COMPAT_INC"
cat > "$COMPAT_INC/source_location" << 'EOF'
#ifndef _COMPAT_SOURCE_LOCATION
#define _COMPAT_SOURCE_LOCATION
#include <cstdint>
namespace std {
struct source_location {
    static constexpr source_location current(
        const char* file = __builtin_FILE(),
        const char* func = __builtin_FUNCTION(),
        uint_least32_t line = __builtin_LINE(),
        uint_least32_t col = 0) noexcept {
        source_location loc;
        loc._file = file; loc._line = line; loc._col = col; loc._func = func;
        return loc;
    }
    constexpr const char* file_name() const noexcept { return _file; }
    constexpr const char* function_name() const noexcept { return _func; }
    constexpr uint_least32_t line() const noexcept { return _line; }
    constexpr uint_least32_t column() const noexcept { return _col; }
private:
    const char* _file = ""; const char* _func = "";
    uint_least32_t _line = 0; uint_least32_t _col = 0;
};
}
#endif
EOF

# 5) 创建 libime CMake 配置文件
mkdir -p "$CMAKE_DIR/LibIMECore" "$CMAKE_DIR/LibIMEPinyin"
cat > "$CMAKE_DIR/LibIMEPinyin/LibIMEPinyinConfig.cmake" << EOF
set(LibIMECore_FOUND TRUE)
set(LibIMEPinyin_FOUND TRUE)
if(NOT TARGET LibIME::Core)
    add_library(LibIME::Core SHARED IMPORTED)
    set_target_properties(LibIME::Core PROPERTIES
        IMPORTED_LOCATION "$LIBIME_LIB/libIMECore.so"
        INTERFACE_INCLUDE_DIRECTORIES "$LIBIME_INC")
endif()
if(NOT TARGET LibIME::Pinyin)
    add_library(LibIME::Pinyin SHARED IMPORTED)
    set_target_properties(LibIME::Pinyin PROPERTIES
        IMPORTED_LOCATION "$LIBIME_LIB/libIMEPinyin.so"
        INTERFACE_INCLUDE_DIRECTORIES "$LIBIME_INC"
        INTERFACE_LINK_LIBRARIES "LibIME::Core")
endif()
EOF
cat > "$CMAKE_DIR/LibIMECore/LibIMECoreConfig.cmake" << EOF
set(LibIMECore_FOUND TRUE)
if(NOT TARGET LibIME::Core)
    add_library(LibIME::Core SHARED IMPORTED)
    set_target_properties(LibIME::Core PROPERTIES
        IMPORTED_LOCATION "$LIBIME_LIB/libIMECore.so"
        INTERFACE_INCLUDE_DIRECTORIES "$LIBIME_INC")
endif()
EOF

# 6) 修复 fcitx5 CMake targets 硬编码路径
mkdir -p /workspace/native-build/opt
ln -sf "$FCITX5_DIR" /workspace/native-build/opt/fcitx5-x11 2>/dev/null || true

# 7) cmake configure
log "cmake 配置…"
cmake -S "$REPO_DIR/fcitx5-addon" -B "$REPO_DIR/fcitx5-addon/build" \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_PREFIX_PATH="$FCITX5_DIR;$CMAKE_DIR" \
    -DCMAKE_CXX_FLAGS="-I$COMPAT_INC"

log "完成。编译: cmake --build $REPO_DIR/fcitx5-addon/build"
log "安装: cp $REPO_DIR/fcitx5-addon/build/libpinyinime.so $FCITX5_DIR/lib/fcitx5/"
