#!/bin/bash
# pinyin-ime 打包脚本 — 生成 RPM / DEB / tarball
# 用法: bash scripts/package.sh [--rpm|--deb|--tarball|--all]
set -euo pipefail

SCRIPT_DIR="$(cd "$(dirname "$0")" && pwd)"
PROJ_DIR="$(dirname "$SCRIPT_DIR")"
VERSION=$(cat "$PROJ_DIR/VERSION")
NB="${NB:-/workspace/native-build}"
OPT="$NB/opt"
ENV_DIR="$NB/mamba-env"

export LD_LIBRARY_PATH="$OPT/libime/lib:$OPT/fcitx5/lib:$ENV_DIR/lib"

BUILD_DIR="$PROJ_DIR/packaging/build"
mkdir -p "$BUILD_DIR"

build_tarball() {
    echo "=== 构建 tarball ==="
    local OUT="$BUILD_DIR/pinyin-ime-$VERSION.tar.gz"
    tar -czf "$OUT" \
        --exclude='.git' --exclude='build' --exclude='native-build' \
        --exclude='cpp/build' --exclude='fcitx5-addon/build' \
        --exclude='packaging/build' \
        -C "$(dirname "$PROJ_DIR")" "$(basename "$PROJ_DIR")"
    echo "  → $OUT ($(du -sh "$OUT" | cut -f1))"
}

build_rpm() {
    echo "=== 构建 RPM ==="
    local SPEC="$PROJ_DIR/packaging/pinyin-ime.spec"
    local RPM_TOP="$BUILD_DIR/rpm"
    mkdir -p "$RPM_TOP"/{BUILD,RPMS,SOURCES,SPECS,SRPMS}

    # Create source tarball
    tar -czf "$RPM_TOP/SOURCES/pinyin-ime-$VERSION.tar.gz" \
        --exclude='.git' --exclude='build' --exclude='native-build' \
        --exclude='cpp/build' --exclude='fcitx5-addon/build' \
        --exclude='packaging/build' \
        -C "$(dirname "$PROJ_DIR")" "$(basename "$PROJ_DIR")"

    # Build RPM
    rpmbuild -bb "$SPEC" \
        --define "_topdir $RPM_TOP" \
        --define "VERSION $VERSION" 2>&1 || {
        echo "  RPM 构建需要完整构建环境（fcitx5/libime 开发包）"
        echo "  在有完整依赖的系统上运行：rpmbuild -bb packaging/pinyin-ime.spec"
    }

    # Copy any built RPMs
    find "$RPM_TOP/RPMS" -name "*.rpm" -exec cp {} "$BUILD_DIR/" \; 2>/dev/null || true
    echo "  RPM spec: $SPEC"
}

build_deb() {
    echo "=== 构建 DEB ==="
    local DEB_DIR="$BUILD_DIR/deb/pinyin-ime_$VERSION"
    mkdir -p "$DEB_DIR"/{DEBIAN,usr/bin,usr/lib/fcitx5,usr/share/fcitx5/addon}

    # Copy control
    cp "$PROJ_DIR/packaging/debian/control" "$DEB_DIR/DEBIAN/control"
    sed -i "s/Version:.*/Version: $VERSION/" "$DEB_DIR/DEBIAN/control" 2>/dev/null || true

    # Copy binaries (if built)
    if [ -f "$PROJ_DIR/cpp/build/pinyin_ime_cli" ]; then
        cp "$PROJ_DIR/cpp/build/pinyin_ime_cli" "$DEB_DIR/usr/bin/"
    fi
    if [ -f "$PROJ_DIR/fcitx5-addon/build/libpinyinime.so" ]; then
        cp "$PROJ_DIR/fcitx5-addon/build/libpinyinime.so" "$DEB_DIR/usr/lib/fcitx5/"
    fi
    if [ -f "$PROJ_DIR/fcitx5-addon/build/pinyinime.conf" ]; then
        cp "$PROJ_DIR/fcitx5-addon/build/pinyinime.conf" "$DEB_DIR/usr/share/fcitx5/addon/"
    fi

    # Calculate installed size
    INSTALLED_SIZE=$(du -sk "$DEB_DIR/usr" 2>/dev/null | cut -f1)
    sed -i "/^Installed-Size:/d" "$DEB_DIR/DEBIAN/control" 2>/dev/null || true
    sed -i "/^Architecture:/a Installed-Size: $INSTALLED_SIZE" "$DEB_DIR/DEBIAN/control" 2>/dev/null || true

    # Build .deb if dpkg-deb available
    if command -v dpkg-deb &>/dev/null; then
        dpkg-deb --build "$DEB_DIR" "$BUILD_DIR/pinyin-ime_$VERSION.deb"
        echo "  → $BUILD_DIR/pinyin-ime_$VERSION.deb"
    else
        echo "  dpkg-deb 不可用，DEB 目录已准备: $DEB_DIR"
        echo "  在 Debian/Ubuntu 上运行: dpkg-deb --build $DEB_DIR"
    fi
}

case "${1:---all}" in
    --rpm)    build_rpm ;;
    --deb)    build_deb ;;
    --tarball) build_tarball ;;
    --all)
        build_tarball
        build_rpm
        build_deb
        ;;
    *) echo "用法: bash scripts/package.sh [--rpm|--deb|--tarball|--all]"; exit 1 ;;
esac

echo "=== 打包完成 ==="
echo "产物目录: $BUILD_DIR"
ls -la "$BUILD_DIR/" 2>/dev/null
