#!/usr/bin/env bash
# 只读 /usr 沙箱：把 RPM 解到本地前缀（SYSROOT）后，RPM 自带的 .pc 文件里
# prefix/includedir/libdir 全是硬编码 /usr/...，pkg-config 会给出系统上并不存在的
# include/lib 路径（CMake 的 imported target 甚至直接报 "includes non-existent path"）。
# 本脚本把 SYSROOT 下所有 .pc 的 /usr、/etc 路径统一改写到前缀内，一次性修好。
#
# 用法：
#   SYSROOT=/workspace/xroot bash scripts/fix_sysroot_pc.sh
#
# 已知副作用（无害）：
#   · systemd*.pc 里的运行时路径也会被改写（构建期用不到）；
#   · URL 里的 http:// 不受影响（已做保护）。
set -euo pipefail

SYSROOT="${SYSROOT:-/workspace/xroot}"
[ -d "$SYSROOT/usr" ] || { echo "!! $SYSROOT 不是 RPM 前缀（缺 usr/）" >&2; exit 1; }

python3 - "$SYSROOT" <<'EOF'
import re, sys, glob, os
root = sys.argv[1]
files = (glob.glob(os.path.join(root, 'usr/lib64/pkgconfig/*.pc')) +
         glob.glob(os.path.join(root, 'usr/share/pkgconfig/*.pc')))
# 1) /usr/xxx → <root>/usr/xxx；行尾 =/usr → =<root>/usr；/etc → <root>/etc（跳过 URL）
urlpat = re.compile(r'https?://\S+')
usrpat = re.compile(r'(?<![\w.-])/usr(?=/)')
endpat = re.compile(r'^(\w+)=/usr$', re.M)
etcpat = re.compile(r'^(/etc)(?=/|$)', re.M)
changed = 0
for f in files:
    txt = open(f).read()
    urls = []
    def keep(m):
        urls.append(m.group(0)); return '\x00%d\x00' % (len(urls) - 1)
    new = urlpat.sub(keep, txt)
    new = usrpat.sub(root + '/usr', new)
    new = endpat.sub(lambda m: f'{m.group(1)}={root}/usr', new)
    new = etcpat.sub(root + '/etc', new)
    if urls:
        new = re.sub(r'\x00(\d+)\x00', lambda m: urls[int(m.group(1))], new)
    if new != txt:
        open(f, 'w').write(new)
        changed += 1
print(f'[fix-sysroot-pc] rewritten {changed}/{len(files)} .pc under {root}')
EOF
