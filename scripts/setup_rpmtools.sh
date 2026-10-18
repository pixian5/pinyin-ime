#!/bin/bash
# setup_rpmtools.sh — 在只读 /usr 的沙箱里准备可用的 rpmbuild（构建 .rpm 用）
#
# 背景：openEuler 22.03 的 rpm 二进制不含构建功能（-bb/-ba 等在 rpm-build 包里，
# 而 /usr 只读无法 dnf install）。本脚本：
#   1) dnf download rpm-build（RPM 包）
#   2) 用 /usr/lib/rpm/rpm2cpio.sh + 内置 Python cpio 解包器解出 /usr/bin/rpmbuild
#      与 /usr/lib/rpm 下的 brp-*/find-*/check-* 等脚本
#   3) 与系统 /usr/lib/rpm（宏、平台定义）合并到 $NB/rpmtools/usr/lib/rpm
#   4) 重写 find-requires/find-provides 为 elfdeps 包装（见下方说明）
#
# 用法: bash scripts/setup_rpmtools.sh
# 之后 scripts/build_packages.sh 会自动使用 $NB/rpmtools 下的 rpmbuild。
set -euo pipefail

NB="${NB:-/workspace/native-build}"
TOOLS="$NB/rpmtools"
TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

log(){ echo -e "\033[36m[rpmtools]\033[0m $*"; }

# 沙箱适配的依赖生成脚本：
# rpm-build 自带的 find-requires/find-provides 调用 /usr/lib/rpm/rpmdeps 的
# “内部生成器”，在沙箱（fileattrs 不全/无 cpio 等）下会静默输出空。
# 这里改为逐文件调用解出的 elfdeps（非 ELF 文件 exit 1，2>/dev/null 后跳过），
# 等价 openEuler __deploop 的思路；配合 rpmbuild 侧
# --define "_use_internal_dependency_generator 0" 生效。
install_wrappers() {
    cat > "$TOOLS/usr/lib/rpm/find-requires" <<'WRAP'
#!/bin/sh
# 沙箱适配：逐文件调用 elfdeps 输出 soname 依赖（非 ELF 文件静默跳过）
while IFS= read -r f; do
    printf '%s\n' "$f" | @RPMDIR@/elfdeps --requires 2>/dev/null || true
done | LC_ALL=C sort -u
WRAP
    cat > "$TOOLS/usr/lib/rpm/find-provides" <<'WRAP'
#!/bin/sh
# 沙箱适配：逐文件调用 elfdeps 输出 provides（非 ELF 文件静默跳过）
while IFS= read -r f; do
    printf '%s\n' "$f" | @RPMDIR@/elfdeps --provides 2>/dev/null || true
done | LC_ALL=C sort -u
WRAP
    sed -i "s#@RPMDIR@#$TOOLS/usr/lib/rpm#g" "$TOOLS/usr/lib/rpm/find-requires" "$TOOLS/usr/lib/rpm/find-provides"
    chmod +x "$TOOLS/usr/lib/rpm/find-requires" "$TOOLS/usr/lib/rpm/find-provides"
}

if [ -x "$TOOLS/usr/bin/rpmbuild" ] && [ -x "$TOOLS/usr/lib/rpm/elfdeps" ]; then
    log "已存在: $TOOLS（跳过下载解包）"
else
    log "dnf 下载 rpm-build ..."
    dnf download --releasever="${RELEASEVER:-22.03}" rpm-build --destdir="$TMP/pkgs" >/dev/null 2>&1 \
      || { echo "!! dnf download 失败（检查仓库网络）"; exit 1; }
    RPM_FILE="$(find "$TMP/pkgs" -name 'rpm-build-*.rpm' | head -1)"
    [ -n "$RPM_FILE" ] || { echo "!! 未下载到 rpm-build"; exit 1; }

    log "解包 $(basename "$RPM_FILE") ..."
    bash /usr/lib/rpm/rpm2cpio.sh "$RPM_FILE" > "$TMP/rpm-build.cpio"
    python3 - "$TMP/rpm-build.cpio" "$TMP/extract" <<'PYEOF'
# 最小 cpio(newc) 解包器（系统无 cpio 命令）
import os, stat, sys

def align4(x): return (4 - (x % 4)) % 4

data = open(sys.argv[1], 'rb').read()
out_dir = sys.argv[2]
off, count = 0, 0
while off + 110 <= len(data):
    if data[off:off+6] not in (b'070701', b'070702'):
        break
    f = [int(data[off+6+i*8:off+6+(i+1)*8], 16) for i in range(13)]
    mode, fsize, namesize = f[1], f[6], f[11]
    name = data[off+110:off+110+namesize-1].decode('utf-8', 'replace')
    if name == 'TRAILER!!!':
        break
    hdr = 110 + namesize
    off += hdr + align4(hdr)
    content = data[off:off+fsize]
    off += fsize + align4(fsize)
    count += 1
    target = os.path.join(out_dir, name.lstrip('/'))
    if stat.S_ISDIR(mode):
        os.makedirs(target, exist_ok=True)
    elif stat.S_ISLNK(mode):
        os.makedirs(os.path.dirname(target), exist_ok=True)
        if os.path.lexists(target):
            os.remove(target)
        os.symlink(content.decode('utf-8', 'replace'), target)
    else:
        os.makedirs(os.path.dirname(target), exist_ok=True)
        with open(target, 'wb') as fp:
            fp.write(content)
        os.chmod(target, mode & 0o7777)
print(f'解包 {count} 个条目 -> {out_dir}')
PYEOF

    log "合并脚本资源到 $TOOLS ..."
    mkdir -p "$TOOLS/usr/bin" "$TOOLS/usr/lib/rpm"
    cp -a /usr/lib/rpm/. "$TOOLS/usr/lib/rpm/" 2>/dev/null || true
    cp -a "$TMP/extract/usr/lib/rpm/." "$TOOLS/usr/lib/rpm/" 2>/dev/null || true
    cp -a "$TMP/extract/usr/bin/rpmbuild" "$TMP/extract/usr/bin/rpmspec" "$TOOLS/usr/bin/" 2>/dev/null || true
    chmod +x "$TOOLS/usr/bin/rpmbuild" 2>/dev/null || true
fi

install_wrappers
"$TOOLS/usr/bin/rpmbuild" --version
log "完成: $TOOLS/usr/bin/rpmbuild（build_packages.sh 将自动使用）"
