#!/bin/bash
# build_packages.sh — 构建 pinyin-ime 的 .rpm / .deb 安装包
#
# 用法:
#   bash scripts/build_packages.sh          # 构建 rpm + deb 并校验
#   bash scripts/build_packages.sh rpm      # 只构建 rpm
#   bash scripts/build_packages.sh deb      # 只构建 deb
#   bash scripts/build_packages.sh runtime  # 只构建便携运行时 tar.gz（安装到 /usr 的目录树）
#   bash scripts/build_packages.sh all      # rpm + deb + runtime（默认）
#   ARCH_RPM=x86_64 ARCH_DEB=amd64 FCITX5_LIBDIR=/usr/lib/x86_64-linux-gnu/fcitx5 bash scripts/build_packages.sh
#                                           # 覆盖架构/安装目录（默认按 uname -m 自动判断）
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
# v0.9.187：架构自适应。以前写死 aarch64/arm64，导致 x86_64 机器上打出来的包名
# 与 addon 安装目录全错（deb 会把 .so 放进 aarch64-linux-gnu，fcitx5 根本找不到）。
# 需要交叉打包时用环境变量覆盖：ARCH_RPM=... ARCH_DEB=... FCITX5_LIBDIR=...
case "$(uname -m)" in
    x86_64|amd64)  DEFAULT_RPM_ARCH=x86_64; DEFAULT_DEB_ARCH=amd64; DEFAULT_FCITX5_LIBDIR=/usr/lib/x86_64-linux-gnu/fcitx5 ;;
    aarch64|arm64) DEFAULT_RPM_ARCH=aarch64; DEFAULT_DEB_ARCH=arm64; DEFAULT_FCITX5_LIBDIR=/usr/lib/aarch64-linux-gnu/fcitx5 ;;
    *)             DEFAULT_RPM_ARCH=$(uname -m); DEFAULT_DEB_ARCH=$(uname -m); DEFAULT_FCITX5_LIBDIR="/usr/lib/$(uname -m)-linux-gnu/fcitx5" ;;
esac
ARCH_RPM="${ARCH_RPM:-$DEFAULT_RPM_ARCH}"
ARCH_DEB="${ARCH_DEB:-$DEFAULT_DEB_ARCH}"
FCITX5_LIBDIR="${FCITX5_LIBDIR:-$DEFAULT_FCITX5_LIBDIR}"
RELEASE=1
DIST="$PROJECT_ROOT/dist"
WORK="$PROJECT_ROOT/build/packages"

log(){ echo -e "\033[36m[package]\033[0m $*"; }

# v0.9.187：构建产物与数据文件路径都可覆盖，支持 out-of-tree 构建（如 chroot /tmp/build）。
ADDON_SO="${ADDON_SO:-fcitx5-addon/build/libpinyinime.so}"
ADDON_CONF="${ADDON_CONF:-fcitx5-addon/build/pinyinime.conf}"
CONF_TPL="fcitx5-addon/conf/pinyinime.conf"
INPUTMETHOD="fcitx5-addon/pinyinime.inputmethod"
CLI="${CLI:-cpp/build/pinyin_ime_cli}"
README_FILE="README.md"

# 数据文件（zh_CN.lm / sc.dict）：优先 DATA_DIR，其次系统 libime 安装路径。
# 以前默认指向仓外快照 pinyin-ime-s3/v0.9.116，换机器就必然缺文件。
if [ -n "${DATA_DIR:-}" ]; then
    LM="${LM:-$DATA_DIR/zh_CN.lm}"
    DICT="${DICT:-$DATA_DIR/sc.dict}"
else
    LM="${LM:-}"
    DICT="${DICT:-/usr/share/libime/sc.dict}"
fi
if [ -z "$LM" ]; then
    for cand in /usr/share/libime/zh_CN.lm /usr/lib/*/libime/zh_CN.lm; do
        [ -f "$cand" ] && { LM="$cand"; break; }
    done
fi

for f in "$ADDON_SO" "$ADDON_CONF" "$CONF_TPL" "$INPUTMETHOD" "$CLI" "$README_FILE" "$LM" "$DICT"; do
    [ -f "$f" ] || { echo "!! 缺文件: $f（先编译 addon/CLI；数据文件用 DATA_DIR 或 LM/DICT 指定）"; exit 1; }
done
mkdir -p "$DIST"

# 发布前硬校验：v0.9.118 的 RPM 曾把 Library 写成 libpinyinime.so，fcitx5 会再追加
# 一次后缀导致 addon 加载失败。这里直接拒绝错误产物，避免再次发布坏包。
validate_addon_conf() {
    grep -qx 'Library=libpinyinime' "$ADDON_CONF" ||
        { echo "!! $ADDON_CONF 的 Library 必须是 libpinyinime（不能带 .so）"; exit 1; }
    grep -q '^0=core:' "$ADDON_CONF" ||
        { echo "!! $ADDON_CONF 缺少 [Addon/Dependencies] core 版本"; exit 1; }
    grep -qx "Version=$VERSION" "$ADDON_CONF" ||
        { echo "!! $ADDON_CONF 的 Version 与 VERSION=$VERSION 不一致"; exit 1; }
}
validate_addon_conf

# 组装安装树 rootfs：$1=目标目录 $2=addon 安装目录（rpm/deb 布局不同）
stage_rootfs() {
    local root="$1" addon_dir="$2"
    rm -rf "$root"
    install -D -m755 "$ADDON_SO"   "$root$addon_dir/libpinyinime.so"
    install -D -m644 "$ADDON_CONF" "$root/usr/share/fcitx5/addon/pinyinime.conf"
    install -D -m644 "$CONF_TPL"   "$root/usr/share/fcitx5/conf/pinyinime.conf"
    install -D -m644 "$INPUTMETHOD" "$root/usr/share/fcitx5/inputmethod/pinyinime.inputmethod"
    install -D -m644 "$LM"         "$root/usr/share/libime/zh_CN.lm"
    install -D -m644 "$DICT"       "$root/usr/share/libime/sc.dict"
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
    # v0.9.187：spec 里的架构改成占位符，按本机（或 ARCH_RPM 覆盖）替换。
    # 旧 spec 写死 BuildArch: aarch64，x86_64 上 rpmbuild 直接报
    # "No compatible architectures found for build"。
    sed -e "s/@VERSION@/$VERSION/g" -e "s/@ARCH@/$ARCH_RPM/g" \
        packaging/pinyin-ime.spec > "$top/SPECS/pinyin-ime.spec"

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
    "$RB" "${RBFLAGS[@]}" --define "_topdir $top" --target "$ARCH_RPM" \
        -bb "$top/SPECS/pinyin-ime.spec" \
        > "$WORK/rpmbuild.log" 2>&1 || { tail -25 "$WORK/rpmbuild.log"; exit 1; }
    local rpm_out
    rpm_out="$(find "$top/RPMS" -name '*.rpm' | head -1)"
    cp -f "$rpm_out" "$DIST/pinyin-ime-${VERSION}-${RELEASE}.${ARCH_RPM}.rpm"
}

build_deb() {
    log "构建 DEB v$VERSION ..."
    local stage="$WORK/stage-deb"
    stage_rootfs "$stage" "$FCITX5_LIBDIR"
    python3 scripts/make_deb.py \
        --rootfs "$stage" \
        --output "$DIST/pinyin-ime_${VERSION}_${ARCH_DEB}.deb" \
        --name pinyin-ime --version "$VERSION" --arch "$ARCH_DEB" \
        --depends "fcitx5, libime" \
        --maintainer "laotongtian <laotongtian@gitcode.com>" \
        --short-desc "Pinyin input method for Linux based on libime (fcitx5 addon)" \
        --long-desc "pinyin-ime 是基于 libime 引擎的 Linux 拼音输入法（fcitx5 addon）。
 功能：全拼/简拼/整句解码、拼写纠错、模糊音按对开关、双拼、用户词典与自学习。
 数据文件 zh_CN.lm / sc.dict 随包安装到 /usr/share/libime，开箱即用。"
}

# v0.9.189：便携运行时包。给「不能装 rpm/deb，但能解压到 /usr 或 $prefix」的场景
# （容器、云桌面、只读 rootfs 外挂）用。内容与 deb 完全同一棵安装树，附带 install.sh。
build_runtime() {
    log "构建 runtime tar.gz v$VERSION ..."
    local name="pinyin-ime-runtime-$VERSION-$ARCH_RPM"
    local root="$WORK/runtime/$name"
    stage_rootfs "$root" "$FCITX5_LIBDIR"

    cat > "$root/install.sh" <<'EOF'
#!/bin/sh
# 把本目录的 usr/ 树覆盖安装到 /usr（需要 root）。卸载：按清单反向删除即可。
set -e
cd "$(dirname "$0")"
[ "$(id -u)" = 0 ] || { echo "请用 root 运行：sudo sh install.sh"; exit 1; }
tar cf - usr | tar xf - -C /
fcitx5 -r 2>/dev/null || true
echo "安装完成。若 fcitx5 正在运行，请重启输入法（fcitx5 -r）后添加 pinyin-ime。"
EOF
    chmod +x "$root/install.sh"

    cat > "$root/BUILD-INFO.txt" <<EOF
version=$VERSION
arch=$ARCH_RPM
built_at=$(date -u +%Y-%m-%dT%H:%M:%SZ)
addon_dir=$FCITX5_LIBDIR
data=usr/share/libime/{zh_CN.lm,sc.dict}
EOF

    tar czf "$DIST/$name.tar.gz" -C "$WORK/runtime" "$name"
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

# v0.9.188：产物校验清单。发布时用户需要能核对下载物，不能只靠文件名。
# 只覆盖本次真正打出来的包，旧版本残留不进清单。
write_checksums() {
    local out="$DIST/SHA256SUMS_v$VERSION"
    local names=()
    local rpmf="$DIST/pinyin-ime-${VERSION}-${RELEASE}.${ARCH_RPM}.rpm"
    local debf="$DIST/pinyin-ime_${VERSION}_${ARCH_DEB}.deb"
    if [ "$MODE" = "rpm" ] || [ "$MODE" = "all" ]; then
        if [ -f "$rpmf" ]; then names+=("$(basename "$rpmf")"); fi
    fi
    if [ "$MODE" = "deb" ] || [ "$MODE" = "all" ]; then
        if [ -f "$debf" ]; then names+=("$(basename "$debf")"); fi
    fi
    local rtf="$DIST/pinyin-ime-runtime-$VERSION-$ARCH_RPM.tar.gz"
    if [ "$MODE" = "runtime" ] || [ "$MODE" = "all" ]; then
        if [ -f "$rtf" ]; then names+=("$(basename "$rtf")"); fi
    fi
    if [ "${#names[@]}" -eq 0 ]; then
        log "!! 没有可写校验清单的产物"; return 1
    fi
    ( cd "$DIST" && sha256sum "${names[@]}" > "$(basename "$out")" )
    log "校验清单: $(basename "$out")"
    cat "$out"
}

case "$MODE" in
    rpm)     build_rpm; verify_rpm; write_checksums ;;
    deb)     build_deb; verify_deb; write_checksums ;;
    runtime) build_runtime; write_checksums ;;
    all)     build_rpm; build_deb; verify_rpm; verify_deb; build_runtime; write_checksums ;;
    *)       echo "用法: bash scripts/build_packages.sh [all|rpm|deb|runtime]"; exit 2 ;;
esac

log "完成。产物："
ls -la "$DIST"
