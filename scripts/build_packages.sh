#!/bin/bash
# build_packages.sh — 构建 pinyin-ime 的 .rpm / .deb 安装包
#
# 用法:
#   bash scripts/build_packages.sh          # 构建 rpm + deb 并校验
#   bash scripts/build_packages.sh rpm      # 只构建 rpm
#   bash scripts/build_packages.sh deb      # 只构建 deb
#
# 产物（dist/ 目录，不入库）:
#   pinyin-ime-<version>-1.aarch64.rpm
#   pinyin-ime_<version>_arm64.deb
#
# 打包内容:
#   /usr/bin/pinyin_ime_cli                  CLI 测试工具
#   /usr/lib64/fcitx5/libpinyinime.so        fcitx5 addon（deb 布局为 /usr/lib/aarch64-linux-gnu/fcitx5/）
#   /usr/share/fcitx5/addon/pinyinime.conf   addon 描述（版本与 VERSION 同步）
#   /usr/share/fcitx5/conf/pinyinime.conf    用户配置模板
#   /usr/share/doc/pinyin-ime/README.md      说明文档
#
# 说明:
#   - 数据文件（zh_CN.lm/sc.dict）不随包分发：addon 自动搜索 libime 数据包的
#     标准安装路径，或由 conf 里的 DataDir 指定（见 spec 内说明）。
#   - 沙箱适配：/usr 只读无法安装 rpm-build 时，自动使用 $NB/rpmtools 下解出的
#     rpmbuild + rpm 脚本资源（scripts/setup_rpmtools.sh 准备）。
#   - deb 由 scripts/make_deb.py（纯 Python）组装；沙箱无 dpkg 也可构建/自检。
set -euo pipefail

MODE="${1:-all}"
PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$PROJECT_ROOT"

NB="${NB:-/workspace/native-build}"
VERSION=$(cat VERSION)
ARCH_RPM=aarch64
ARCH_DEB=arm64
RELEASE=1
DIST="$PROJECT_ROOT/dist"
WORK="$PROJECT_ROOT/build/packages"

log(){ echo -e "\033[36m[package]\033[0m $*"; }

ADDON_SO="fcitx5-addon/build/libpinyinime.so"
ADDON_CONF="fcitx5-addon/build/pinyinime.conf"
CONF_TPL="fcitx5-addon/conf/pinyinime.conf"
CLI="cpp/build/pinyin_ime_cli"
README_FILE="README.md"

for f in "$ADDON_SO" "$ADDON_CONF" "$CONF_TPL" "$CLI" "$README_FILE"; do
    [ -f "$f" ] || { echo "!! 缺文件: $f（先编译 addon；CLI 见 cpp/build）"; exit 1; }
done
mkdir -p "$DIST"

# 组装安装树 rootfs：$1=目标目录 $2=addon 安装目录（rpm/deb 布局不同）
stage_rootfs() {
    local root="$1" addon_dir="$2"
    rm -rf "$root"
    install -D -m755 "$ADDON_SO"   "$root$addon_dir/libpinyinime.so"
    install -D -m644 "$ADDON_CONF" "$root/usr/share/fcitx5/addon/pinyinime.conf"
    install -D -m644 "$CONF_TPL"   "$root/usr/share/fcitx5/conf/pinyinime.conf"
    install -D -m644 "$README_FILE" "$root/usr/share/doc/pinyin-ime/README.md"
    install -D -m755 "$CLI"        "$root/usr/bin/pinyin_ime_cli"
}

build_rpm() {
    log "构建 RPM v$VERSION ..."
    local top="$WORK/rpmbuild"
    rm -rf "$top"; mkdir -p "$top"/{SPECS,SOURCES,BUILD,RPMS,SRPMS,BUILDROOT}
    local stage="$WORK/stage-rpm/pinyin-ime-$VERSION"
    stage_rootfs "$stage" "/usr/lib64/fcitx5"
    tar czf "$top/SOURCES/pinyin-ime-$VERSION.tar.gz" \
        -C "$WORK/stage-rpm" "pinyin-ime-$VERSION"
    sed "s/@VERSION@/$VERSION/g" packaging/pinyin-ime.spec > "$top/SPECS/pinyin-ime.spec"

    local RB
    # 通用构建参数：不分发 debuginfo、不 strip——保证包内二进制与 S3 发布产物
    # 字节一致（便于 md5/哈希校验）；如需发行版风格 strip 可去掉这几行。
    RBFLAGS=(
        --define "debug_package %{nil}"
        --define "__brp_strip %{nil}"
        --define "__brp_strip_comment_note %{nil}"
        --define "__brp_strip_static_archive %{nil}"
    )
    if command -v rpmbuild >/dev/null 2>&1; then
        RB="$(command -v rpmbuild)"
        log "使用系统 rpmbuild: $RB"
    else
        RB="$NB/rpmtools/usr/bin/rpmbuild"
        [ -x "$RB" ] || { echo "!! 无 rpmbuild；请先运行 scripts/setup_rpmtools.sh"; exit 1; }
        local MRG="$NB/rpmtools/usr/lib/rpm"
        log "使用沙箱 rpmbuild: $RB"
        RBFLAGS+=(
            --define "_rpmconfigdir $MRG"
            --define "_use_internal_dependency_generator 0"
            --define "__arch_install_post $MRG/check-buildroot"
            --define "__brp_ldconfig %{nil}"
            --define "__brp_compress $MRG/brp-compress"
            --define "__brp_python_bytecompile $MRG/brp-python-bytecompile %{__python}"
            --define "__brp_python_hardlink $MRG/brp-python-hardlink"
        )
    fi
    "$RB" "${RBFLAGS[@]}" --define "_topdir $top" -bb "$top/SPECS/pinyin-ime.spec" \
        > "$WORK/rpmbuild.log" 2>&1 || { tail -25 "$WORK/rpmbuild.log"; exit 1; }
    local rpm_out
    rpm_out="$(find "$top/RPMS" -name '*.rpm' | head -1)"
    cp -f "$rpm_out" "$DIST/pinyin-ime-${VERSION}-${RELEASE}.${ARCH_RPM}.rpm"
}

build_deb() {
    log "构建 DEB v$VERSION ..."
    local stage="$WORK/stage-deb"
    stage_rootfs "$stage" "/usr/lib/aarch64-linux-gnu/fcitx5"
    python3 scripts/make_deb.py \
        --rootfs "$stage" \
        --output "$DIST/pinyin-ime_${VERSION}_${ARCH_DEB}.deb" \
        --name pinyin-ime --version "$VERSION" --arch "$ARCH_DEB" \
        --depends "fcitx5" --recommends "libime-data" \
        --maintainer "laotongtian <laotongtian@gitcode.com>" \
        --short-desc "Pinyin input method for Linux based on libime (fcitx5 addon)" \
        --long-desc "pinyin-ime 是基于 libime 引擎的 Linux 拼音输入法（fcitx5 addon）。
 功能：全拼/简拼/整句解码、拼写纠错、模糊音按对开关、双拼、用户词典与自学习。
 数据文件由发行版 libime 数据包提供（标准路径自动搜索），或设置 DataDir 指定。"
}

# ---------- 校验 ----------
verify_rpm() {
    local f="$DIST/pinyin-ime-${VERSION}-${RELEASE}.${ARCH_RPM}.rpm"
    log "校验 RPM: $(basename "$f")"
    rpm -qp --info "$f" | sed -n '1,8p'
    echo "--- 文件列表:"; rpm -qpl "$f"
    echo "--- 自动依赖:"; rpm -qp --requires "$f" | grep -v "^rpmlib" || true
}

verify_deb() {
    local f="$DIST/pinyin-ime_${VERSION}_${ARCH_DEB}.deb"
    log "校验 DEB: $(basename "$f")"
    python3 - "$f" <<'PYEOF'
import sys, io, tarfile, hashlib
path = sys.argv[1]
data = open(path, 'rb').read()
assert data[:8] == b'!<arch>\n', "不是 ar 归档"
off, members = 8, {}
while off + 60 <= len(data):
    name = data[off:off+16].decode().rstrip()
    size = int(data[off+48:off+58].decode().rstrip())
    members[name] = data[off+60:off+60+size]
    off += 60 + size + (size % 2)
print("ar 成员:", list(members), "| debian-binary =", members['debian-binary'])
ctrl = tarfile.open(fileobj=io.BytesIO(members['control.tar.gz']), mode='r:gz')
print("control.tar.gz 成员:", ctrl.getnames())
print("---- control ----"); print(ctrl.extractfile('./control').read().decode())
dt = tarfile.open(fileobj=io.BytesIO(members['data.tar.gz']), mode='r:gz')
for n in dt.getnames(): print("  条目:", n)
# md5sums 与实际内容抽验
md5f = ctrl.extractfile('./md5sums').read().decode().splitlines()
print(f"md5sums 行数: {len(md5f)}")
import tarfile as _tf
for line in md5f:
    want, rel = line.split(None, 1)
    m = dt.extractfile('./' + rel)
    got = hashlib.md5(m.read()).hexdigest()
    assert got == want, f"md5 不一致: {rel}"
print("所有文件 md5sums 校验通过 ✓")
PYEOF
}

case "$MODE" in
    rpm)  build_rpm; verify_rpm ;;
    deb)  build_deb; verify_deb ;;
    all)  build_rpm; build_deb; verify_rpm; verify_deb ;;
    *)    echo "用法: bash scripts/build_packages.sh [all|rpm|deb]"; exit 2 ;;
esac

log "完成。产物："
ls -la "$DIST"
