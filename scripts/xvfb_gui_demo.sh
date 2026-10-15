#!/usr/bin/env bash
# xvfb_gui_demo.sh —— 在只读 /usr 沙箱内，用 Xvfb 真实演示 fcitx5 pinyinime 的
# 「打字 → 候选窗 → 上屏」桌面体验（对标搜狗拼音），并输出整屏截图 PNG。
#
# 这不是 CLI：一个真实 X 应用窗口（白底，libXft 走 fontconfig/文泉驿渲染中文），
# 按键经 XTest 注入，经 XIM 交给 fcitx5 pinyinime addon 出候选、上屏。
#
# 依赖（均由本仓库脚本准备）：
#   · sysroot 的 Xvfb（+ tools/xkbcomp_redirect.c 垫片）
#   · sysroot 的中文字体（wqy-microhei/zenhei）与 libXft（走 fontconfig）
#   · deploy 的桌面 bundle fcitx5-x11 + pinyinime addon
#
# 用法：bash scripts/xvfb_gui_demo.sh [输出目录=./screenshots]
set -uo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="${SYSROOT:-/workspace/sysroot}"
OUTDIR="${1:-$REPO_DIR/screenshots}"
cd "$REPO_DIR"
mkdir -p "$OUTDIR"

# 选一个无 Xvfb 占用的随机 display（跨 bwrap 命名空间无法 kill 旧实例）
for i in $(seq 1 30); do
    n=$(( (RANDOM % 800) + 150 ))
    if ! pgrep -f "[X]vfb :$n" >/dev/null 2>&1; then export XDISPLAY=":$n"; break; fi
done
: "${XDISPLAY:=:99}"

LOC="$(locale -a 2>/dev/null | grep -iE 'zh_CN\.utf-?8' | head -1)"
[ -z "$LOC" ] && LOC="$(locale -a 2>/dev/null | grep -iE 'C\.utf-?8' | head -1)"
[ -z "$LOC" ] && LOC="$(locale -a 2>/dev/null | grep -iE 'en_US\.utf-?8' | head -1)"

export DISPLAY="$XDISPLAY" XMODIFIERS=@im=fcitx LD_LIBRARY_PATH="$SYSROOT/usr/lib64"
export LANG="$LOC" LC_ALL="$LOC"
echo "[demo] DISPLAY=$XDISPLAY LANG=$LOC"

# 编译 GUI 演示程序（源码入库，二进制不入库）
if [ ! -x tools/ime_gui_demo ]; then
    echo "[demo] 编译 tools/ime_gui_demo …"
    gcc -O2 -I"$SYSROOT/usr/include" -I"$SYSROOT/usr/include/freetype2" \
        -o tools/ime_gui_demo tools/ime_gui_demo.c \
        -L"$SYSROOT/usr/lib64" -lX11 -lXtst -lXft -lfontconfig -lfreetype || exit 1
fi

# 起 X 模式环境（Xvfb + DBus + fcitx5 XIM）
XDISPLAY="$XDISPLAY" bash scripts/start_im_env_x.sh start >/tmp/gui_demo_env.log 2>&1
if ! grep -q "xim + pinyinime 已加载" /tmp/gui_demo_env.log; then
    echo "❌ 环境未就绪"; tail -20 /tmp/gui_demo_env.log; exit 1
fi
trap 'XDISPLAY="$XDISPLAY" bash scripts/start_im_env_x.sh stop >/dev/null 2>&1' EXIT

# 预热 fontconfig 缓存（含文泉驿 CJK 字体），确保候选窗与应用窗中文正常渲染
RUN="${RUNDIR:-/workspace/fcitx5-runtime}"
export FONTCONFIG_FILE="$RUN/fonts.conf" FONTCONFIG_PATH="$RUN"
fc-cache -f >/dev/null 2>&1 || true

PRE="$OUTDIR/ime-preedit.ppm"
C1="$OUTDIR/ime-commit1.ppm"
C2="$OUTDIR/ime-commit2.ppm"
./tools/ime_gui_demo "$PRE" "$C1" "$C2" || { echo "❌ 演示程序失败"; exit 1; }

# PPM → PNG（python3 标准库 zlib，自写最小 PNG 编码器）
python3 - "$PRE" "$OUTDIR/gui-candidate-preedit.png" \
           "$C1"  "$OUTDIR/gui-committed-nihao.png" \
           "$C2"  "$OUTDIR/gui-committed-nihaoshijie.png" <<'PY'
import sys, zlib, struct
def read_ppm(p):
    d = open(p, 'rb').read(); parts = []; i = 0
    while len(parts) < 4:
        while d[i:i+1].isspace(): i += 1
        if d[i:i+1] == b'#':
            while d[i:i+1] != b'\n': i += 1
            continue
        j = i
        while not d[j:j+1].isspace(): j += 1
        parts.append(d[i:j]); i = j
    assert parts[0] == b'P6', parts[0]
    w, h = int(parts[1]), int(parts[2]); i += 1
    return w, h, d[i:i+w*h*3]
def write_png(path, w, h, rgb):
    raw = b''.join(b'\x00' + rgb[y*w*3:(y+1)*w*3] for y in range(h))
    def chunk(t, data):
        c = t + data
        return struct.pack('>I', len(data)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    png = b'\x89PNG\r\n\x1a\n'
    png += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    png += chunk(b'IDAT', zlib.compress(raw, 9)) + chunk(b'IEND', b'')
    open(path, 'wb').write(png); print("wrote", path)
a = sys.argv[1:]
for i in range(0, len(a), 2):
    w, h, rgb = read_ppm(a[i]); write_png(a[i+1], w, h, rgb)
PY

rm -f "$PRE" "$C1" "$C2"
echo "== 截图输出 =="
ls -l "$OUTDIR"/gui-candidate-preedit.png "$OUTDIR"/gui-committed-*.png
