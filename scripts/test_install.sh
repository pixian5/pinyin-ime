#!/bin/bash
# test_install.sh — 安装矩阵自动验收（v0.9.191）
#
# 以前 deb/rpm/runtime 三条安装路径全靠人工敲命令、肉眼看输出，0.9.189 出过
# 「dpkg 必须 --force-overwrite 才装得上」「升级把 libime-data 的 sc.dict 删掉」两个坑，
# 都是装完才发现。这个脚本把三条路径变成断言，一条命令给出 PASS/FAIL 矩阵。
#
# 用法（建议在能装包的 Linux 里跑，如 chroot）:
#   bash scripts/test_install.sh              # 打包 + 三条安装路径
#   bash scripts/test_install.sh --skip-build # 复用 dist/ 现有产物
#   LM=... DICT=... bash scripts/test_install.sh
#
# 退出码: 0=全部通过, 1=有失败项, 2=环境不满足（缺 dpkg/rpm 等）
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
VERSION=$(cat VERSION)
ARCH_RPM="${ARCH_RPM:-$(uname -m)}"
ARCH_DEB="${ARCH_DEB:-$([ "$(uname -m)" = x86_64 ] && echo amd64 || echo arm64)}"
DIST="$ROOT/dist"
SKIP_BUILD=0
[ "${1:-}" = "--skip-build" ] && SKIP_BUILD=1

PASS=0; FAIL=0; FAILED_LIST=()
ok(){ PASS=$((PASS+1)); echo "  ✅ $*"; }
ng(){ FAIL=$((FAIL+1)); FAILED_LIST+=("$*"); echo "  ❌ $*"; }
check(){ local name="$1"; shift; if "$@" >/tmp/ti_out.log 2>&1; then ok "$name"; else ng "$name"; sed -n '1,5p' /tmp/ti_out.log | sed 's/^/     /'; fi; }

command -v dpkg >/dev/null 2>&1 || { echo "!! 缺 dpkg，无法测试 deb 路径"; exit 2; }
command -v rpm  >/dev/null 2>&1 || { echo "!! 缺 rpm，无法测试 rpm 路径"; exit 2; }

DEB="$DIST/pinyin-ime_${VERSION}_${ARCH_DEB}.deb"
RPM="$DIST/pinyin-ime-${VERSION}-1.${ARCH_RPM}.rpm"
RT="$DIST/pinyin-ime-runtime-${VERSION}-${ARCH_RPM}.tar.gz"

if [ "$SKIP_BUILD" = 0 ]; then
    echo "=== 打包 v$VERSION ==="
    LM="${LM:-}" DICT="${DICT:-}" bash scripts/build_packages.sh all || { echo "!! 打包失败"; exit 1; }
fi
for f in "$DEB" "$RPM" "$RT"; do
    [ -f "$f" ] || { echo "!! 缺产物 $f"; exit 2; }
done

echo "=== 1. 校验清单 ==="
( cd "$DIST" && sha256sum -c "SHA256SUMS_v$VERSION" ) >/tmp/ti_sha.log 2>&1 \
    && ok "SHA256SUMS_v$VERSION 全部匹配" || ng "SHA256 校验失败（见 /tmp/ti_sha.log）"

# 无参数解码冒烟：验证装完后默认搜索路径可用
smoke_decode() {
    local cli="$1" ud; ud=$(mktemp -d)
    printf 'nihao\nwoaibeijing\n' > "$ud/in.txt"
    local out; out=$("$cli" --batch "$ud/in.txt" --top1 --quiet --user-data "$ud" 2>/dev/null)
    [ "$(echo "$out" | sed -n 1p)" = "你好" ] && [ "$(echo "$out" | sed -n 2p)" = "我爱北京" ]
}

echo "=== 2. deb 路径 ==="
dpkg -r pinyin-ime >/dev/null 2>&1 || true   # 清掉上一次残留状态
if dpkg -i "$DEB" >/tmp/ti_deb.log 2>&1; then
    ok "dpkg -i 无警告安装"
else
    # chroot 里 fcitx5/libime 包名不可满足；真实发行版不会走到这里
    if dpkg -i --ignore-depends=fcitx5,libime,libime-data "$DEB" >/tmp/ti_deb2.log 2>&1; then
        ok "dpkg -i 成功（本次忽略依赖：chroot 内 fcitx5/libime 未安装）"
    else
        ng "dpkg -i 失败"; sed -n '1,6p' /tmp/ti_deb2.log | sed 's/^/     /'
    fi
fi
check "deb 状态为 installed" bash -c 'dpkg -s pinyin-ime 2>/dev/null | grep -q "Status: install ok installed"'
check "deb 版本=$VERSION" bash -c "dpkg -s pinyin-ime 2>/dev/null | grep -q '^Version: $VERSION'"
check "deb 依赖声明含 libime-data" bash -c 'dpkg -s pinyin-ime 2>/dev/null | grep -q "libime-data"'
check "deb 不打包 zh_CN.lm" bash -c '! dpkg -L pinyin-ime 2>/dev/null | grep -q "/usr/share/libime/zh_CN.lm"'
check "deb 不打包 sc.dict" bash -c '! dpkg -L pinyin-ime 2>/dev/null | grep -q "/usr/share/libime/sc.dict"'
check "deb CLI --version 与 VERSION 一致" bash -c "[ \"\$(pinyin_ime_cli --version 2>/dev/null)\" = \"pinyin_ime_cli $VERSION\" ]"
check "deb 安装字符重排模型" test -s /usr/share/pinyin-ime/char_trigram.bin
check "deb 安装后无参数解码正确" smoke_decode "$(command -v pinyin_ime_cli)"
dpkg -r pinyin-ime >/dev/null 2>&1 || true
dpkg --purge --force-all pinyin-ime >/dev/null 2>&1 || true

echo "=== 3. rpm 路径 ==="
rpm -e --nodeps pinyin-ime >/dev/null 2>&1 || true
if rpm -i --nodeps --replacefiles "$RPM" >/tmp/ti_rpm.log 2>&1; then ok "rpm -i 安装"; else ng "rpm -i 失败"; sed -n 1,6p /tmp/ti_rpm.log | sed 's/^/     /'; fi
check "rpm 已注册且版本=$VERSION" bash -c "rpm -q pinyin-ime | grep -q '$VERSION'"
check "rpm 文件列表含 addon .so" bash -c 'rpm -ql pinyin-ime | grep -q "libpinyinime.so"'
check "rpm 文件列表含字符模型" bash -c 'rpm -ql pinyin-ime | grep -q "char_trigram.bin"'
check "rpm 不打包 LM/sc.dict" bash -c '! rpm -ql pinyin-ime | grep -Eq "zh_CN.lm|sc.dict"'
check "rpm 安装后 CLI 可解码" smoke_decode "$(command -v pinyin_ime_cli)"
if rpm -e --nodeps pinyin-ime >/tmp/ti_rpme.log 2>&1 && ! rpm -q pinyin-ime >/dev/null 2>&1; then
    ok "rpm -e 干净卸载"
else
    ng "rpm -e 卸载不干净"
fi
# 卸载后不应残留 addon（rpm 会删自己装的文件）
[ ! -f /usr/lib64/fcitx5/libpinyinime.so ] && ok "rpm 卸载后 addon 已移除" || ng "rpm 卸载后 addon 残留"
[ ! -f /usr/share/pinyin-ime/char_trigram.bin ] && ok "rpm 卸载后字符模型已移除" || ng "rpm 卸载后字符模型残留"

echo "=== 4. runtime 路径 ==="
TMP=$(mktemp -d); trap 'rm -rf "$TMP"' EXIT
tar xzf "$RT" -C "$TMP" || ng "runtime 解压失败"
RTD="$TMP/$(basename "$RT" .tar.gz)"
check "runtime 含 install.sh" test -x "$RTD/install.sh"
check "runtime 含 uninstall.sh" test -x "$RTD/uninstall.sh"
check "runtime 含 BUILD-INFO.txt" test -s "$RTD/BUILD-INFO.txt"
check "runtime 含字符重排模型" test -s "$RTD/usr/share/pinyin-ime/char_trigram.bin"
( cd "$RTD" && sh install.sh >/tmp/ti_rt.log 2>&1 ) && ok "install.sh 执行成功" || { ng "install.sh 失败"; sed -n 1,5p /tmp/ti_rt.log | sed 's/^/     /'; }
check "runtime 安装后 CLI 可解码" smoke_decode /usr/bin/pinyin_ime_cli
SC_SUM_BEFORE=$(md5sum /usr/share/libime/sc.dict 2>/dev/null | cut -d' ' -f1)
( cd "$RTD" && sh uninstall.sh >/tmp/ti_rtu.log 2>&1 ) && ok "uninstall.sh 执行成功" || ng "uninstall.sh 失败"
check "runtime 卸载后 CLI 已移除" bash -c '! test -f /usr/bin/pinyin_ime_cli'
check "runtime 卸载后 addon 已移除" bash -c '! test -f /usr/lib/x86_64-linux-gnu/fcitx5/libpinyinime.so'
check "runtime 卸载后字符模型已移除" bash -c '! test -f /usr/share/pinyin-ime/char_trigram.bin'
if [ -n "$SC_SUM_BEFORE" ]; then
    check "runtime 卸载后 sc.dict 未被误删" test -f /usr/share/libime/sc.dict
fi

echo
echo "===== 安装矩阵结果：通过 $PASS / 失败 $FAIL ====="
if [ "$FAIL" -gt 0 ]; then
    printf '失败项:\n'; printf '  - %s\n' "${FAILED_LIST[@]}"
    exit 1
fi
echo "全部通过 ✅"
