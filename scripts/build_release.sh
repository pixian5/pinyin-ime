#!/bin/bash
# build_release.sh — 构建可发布的 pinyin-ime 产物：
#   dist/pinyin-ime-<ver>-1.aarch64.rpm
#   dist/pinyin-ime_<ver>_arm64.deb
#   dist/pinyin-ime-runtime-<ver>-aarch64.tar.gz
#   dist/SHA256SUMS_v<ver>
#
# 依赖：
#   - 已准备 fcitx5/libIME 的 CMake 前缀（默认 /workspace/native-build/opt/...）
#   - 已安装 rpmbuild（系统或 $NB/rpmtools）；无 rpmbuild 时可 SKIP_RPM=1
#   - Python 3（DEB 由 scripts/make_deb.py 组装）
#
# 用法:
#   bash scripts/build_release.sh
#   SKIP_BUILD=1 bash scripts/build_release.sh   # 复用已有 build 产物
#   SKIP_RPM=1   bash scripts/build_release.sh   # 只出 DEB + runtime
set -euo pipefail

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

VERSION="$(cat VERSION)"
ARCH="aarch64"
DIST="$PROJECT_ROOT/dist"
WORK="$PROJECT_ROOT/build/release"
PREFIX="${PINYIN_BUILD_PREFIX:-/workspace/native-build/opt/fcitx5-x11;/workspace/native-build/opt/libime}"
DEPLOY="${DEPLOY:-/workspace/deploy/pinyin-ime}"
LIBIME_RT="${LIBIME_RT:-/workspace/deploy/pinyin-ime-v0.9.84a}"
DATA_DIR="${DATA_DIR:-$PROJECT_ROOT/pinyin-ime-s3/v0.9.116}"
FONT="${FONT:-/workspace/sysroot/usr/share/fonts/wqy-microhei/wqy-microhei.ttc}"
DEPS_LIB="${DEPS_LIB:-$DEPLOY/opt/deps-lib}"

# 沙箱里的 fcitx5/libIME 是用较新的 libstdc++ 构建的；系统 GCC 10 链接时会找不到
# GLIBCXX_3.4.3x 符号。显式把 deps-lib 的 libstdc++ 放到链接行，并写入 rpath。
# 正常发行版构建不需要这段（系统 libstdc++ 已足够新）。
LINK_FLAGS="${LINK_FLAGS:-}"
if [ -f "$DEPS_LIB/libstdc++.so.6" ]; then
    LINK_FLAGS="-Wl,--no-as-needed $DEPS_LIB/libstdc++.so.6 $DEPS_LIB/libgcc_s.so.1 -Wl,-rpath,$DEPS_LIB -Wl,-rpath,$DEPLOY/opt/fcitx5-x11/lib -Wl,-rpath,$LIBIME_RT/lib"
fi

log(){ printf '\033[36m[release]\033[0m %s\n' "$*"; }
die(){ printf '\033[31m[release]\033[0m %s\n' "$*" >&2; exit 1; }

[ -n "$VERSION" ] || die "VERSION 为空"
[ -d "$DEPLOY" ] || die "缺 DEPLOY=$DEPLOY"
[ -d "$LIBIME_RT" ] || die "缺 LIBIME_RT=$LIBIME_RT"
[ -f "$DATA_DIR/zh_CN.lm" ] || die "缺 LM: $DATA_DIR/zh_CN.lm"
[ -f "$DATA_DIR/sc.dict" ] || die "缺 dict: $DATA_DIR/sc.dict"
[ -f "$FONT" ] || die "缺中文字体: $FONT"

mkdir -p "$DIST" "$WORK"

if [ "${SKIP_BUILD:-0}" != 1 ]; then
    log "编译 CLI"
    cmake -S cpp -B cpp/build -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_PREFIX_PATH="$PREFIX" \
        -DCMAKE_EXE_LINKER_FLAGS="$LINK_FLAGS"
    cmake --build cpp/build -j"$(nproc)"

    log "编译 fcitx5 addon"
    cmake -S fcitx5-addon -B fcitx5-addon/build -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr -DCMAKE_PREFIX_PATH="$PREFIX" \
        -DCMAKE_MODULE_LINKER_FLAGS="$LINK_FLAGS"
    cmake --build fcitx5-addon/build -j"$(nproc)"
fi

[ -x cpp/build/pinyin_ime_cli ] || die "缺 cpp/build/pinyin_ime_cli"
[ -f fcitx5-addon/build/libpinyinime.so ] || die "缺 fcitx5-addon/build/libpinyinime.so"
[ -f fcitx5-addon/build/pinyinime.conf ] || die "缺 fcitx5-addon/build/pinyinime.conf"

log "校验 addon 配置"
grep -qx 'Library=libpinyinime' fcitx5-addon/build/pinyinime.conf ||
    die "addon conf 的 Library 必须是 libpinyinime"
grep -q '^0=core:' fcitx5-addon/build/pinyinime.conf ||
    die "addon conf 缺少 core 依赖"
grep -qx "Version=$VERSION" fcitx5-addon/build/pinyinime.conf ||
    die "addon conf 的 Version 与 VERSION=$VERSION 不一致"

if [ "${SKIP_RPM:-0}" != 1 ]; then
    log "构建 RPM + DEB"
    bash scripts/build_packages.sh all
else
    log "构建 DEB"
    bash scripts/build_packages.sh deb
fi

log "组装便携运行时"
BUNDLE_NAME="pinyin-ime-runtime-v${VERSION}-aarch64"
BUNDLE="$WORK/$BUNDLE_NAME"
rm -rf "$BUNDLE"
mkdir -p "$BUNDLE"/{bin,lib,share/libime/libime,share/fonts,scripts,opt}
mkdir -p "$BUNDLE/opt/fcitx5-x11/lib/fcitx5" \
         "$BUNDLE/opt/fcitx5-x11/share/fcitx5/addon" \
         "$BUNDLE/opt/fcitx5-x11/share/fcitx5/conf" \
         "$BUNDLE/opt/fcitx5-x11/share/fcitx5/inputmethod"

cp -a "$DEPLOY/opt/fcitx5-x11" "$BUNDLE/opt/"
cp -a "$DEPLOY/opt/deps-lib" "$BUNDLE/opt/"
cp -a "$LIBIME_RT/lib/." "$BUNDLE/lib/"
cp -f cpp/build/pinyin_ime_cli "$BUNDLE/bin/"
cp -f fcitx5-addon/build/libpinyinime.so "$BUNDLE/opt/fcitx5-x11/lib/fcitx5/"
cp -f fcitx5-addon/build/pinyinime.conf "$BUNDLE/opt/fcitx5-x11/share/fcitx5/addon/"
cp -f fcitx5-addon/pinyinime.inputmethod "$BUNDLE/opt/fcitx5-x11/share/fcitx5/inputmethod/"
cp -f fcitx5-addon/conf/pinyinime.conf "$BUNDLE/opt/fcitx5-x11/share/fcitx5/conf/"
cp -f "$DATA_DIR/zh_CN.lm" "$BUNDLE/share/libime/libime/"
cp -f "$DATA_DIR/sc.dict" "$BUNDLE/share/libime/libime/"
cp -f "$FONT" "$BUNDLE/share/fonts/wqy-microhei.ttc"
cp -f packaging/runtime-start.sh "$BUNDLE/scripts/start.sh"
cp -f packaging/runtime-stop.sh "$BUNDLE/scripts/stop.sh"
chmod +x "$BUNDLE/scripts/"*.sh "$BUNDLE/bin/pinyin_ime_cli"
cp -f packaging/runtime-README.md "$BUNDLE/README.md"
cp -f VERSION "$BUNDLE/VERSION"

# 记录 bundle 的构建来源，便于问题定位。
cat > "$BUNDLE/BUILD-INFO.txt" <<EOF
version=$VERSION
commit=$(git rev-parse HEAD 2>/dev/null || echo unknown)
built_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)
fcitx5=5.1.23
libime=1.1.17
data=$DATA_DIR
EOF

tar czf "$DIST/$BUNDLE_NAME.tar.gz" -C "$WORK" "$BUNDLE_NAME"

log "生成校验和"
(
    cd "$DIST"
    sha256sum "pinyin-ime-${VERSION}-1.aarch64.rpm" \
        "pinyin-ime_${VERSION}_arm64.deb" \
        "$BUNDLE_NAME.tar.gz" > "SHA256SUMS_v$VERSION"
)

log "完成"
ls -lh "$DIST" | sed -n '1,20p'
