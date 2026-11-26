#!/bin/bash
# =============================================================================
# 安装/升级/回滚矩阵测试（deb）
#
# 目的：把「装完能用」升级为「老版本能平滑升级到当前版本，数据不丢，能回滚」。
#       历史上 0.9.189 曾把自己的数据文件当垃圾删掉，连带删了 libime-data 的
#       sc.dict；这个脚本就是防止这类问题再次发生。
#
# 覆盖路径：
#   旧版安装 → 升级到当前版 → 降级回旧版 → 再升级回当前版 → 卸载 → 数据仍在
#
# 依赖：dpkg、root 权限、dist/ 下已有当前版本与至少一个旧版本的 deb。
# 用法：bash scripts/test_upgrade_matrix.sh
#   退出码：0 = 通过或跳过；1 = 断言失败。
# =============================================================================
set -uo pipefail

PASS=0
FAIL=0
ok()  { PASS=$((PASS + 1)); echo "  [PASS] $*"; }
bad() { FAIL=$((FAIL + 1)); echo "  [FAIL] $*"; }
skip() { echo "  [SKIP] $*"; }

ROOT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
VERSION="$(cat "$ROOT_DIR/VERSION" 2>/dev/null | tr -d '[:space:]')"
DIST="${DIST:-$ROOT_DIR/dist}"
CUR_DEB="$DIST/pinyin-ime_${VERSION}_amd64.deb"

LM="/usr/lib/x86_64-linux-gnu/libime/zh_CN.lm"
DICT="/usr/share/libime/sc.dict"
CLI="/usr/bin/pinyin_ime_cli"
ADDON_CONF="/usr/share/fcitx5/addon/pinyinime.conf"

command -v dpkg >/dev/null 2>&1 || { echo "[SKIP] 没有 dpkg，跳过升级矩阵"; exit 0; }
[ "$(id -u)" = "0" ] || { echo "[SKIP] 需要 root 才能装卸包，跳过升级矩阵"; exit 0; }
[ -f "$CUR_DEB" ] || { echo "[SKIP] 缺少当前版本包 $CUR_DEB（先跑 build_packages.sh）"; exit 0; }
[ -f "$LM" ] && [ -f "$DICT" ] || { echo "[SKIP] 缺少 libime 数据，跳过升级矩阵"; exit 0; }

# 选一个比当前版本低的旧包（sort -V 后倒数第二个）
OLD_DEB=""
for f in $(ls "$DIST"/pinyin-ime_*_amd64.deb 2>/dev/null | sort -V); do
    v="$(basename "$f" | sed -E 's/pinyin-ime_([0-9.]+)_amd64.deb/\1/')"
    [ "$v" = "$VERSION" ] && continue
    if [ "$(printf '%s\n%s\n' "$v" "$VERSION" | sort -V | tail -1)" = "$VERSION" ]; then
        OLD_DEB="$f"
    fi
done
if [ -z "$OLD_DEB" ]; then
    echo "[SKIP] dist/ 里没有旧版本 deb，跳过升级矩阵"
    exit 0
fi
OLD_VER="$(basename "$OLD_DEB" | sed -E 's/pinyin-ime_([0-9.]+)_amd64.deb/\1/')"
echo "[info] 升级路径：$OLD_VER → $VERSION"

md5_of() { md5sum "$1" 2>/dev/null | cut -d' ' -f1; }
data_md5() { echo "$(md5_of "$LM")/$(md5_of "$DICT")"; }

cli_version() { "$CLI" --version 2>/dev/null | awk '{print $2}'; }
decode_nihao() { printf 'nihao\n' | "$CLI" 2>/dev/null | head -1 | cut -f1; }

install_deb() { dpkg -i --force-depends "$1" >/dev/null 2>&1; }

cleanup() {
    # 无论成功失败，最后都回到当前版本
    install_deb "$CUR_DEB" >/dev/null 2>&1
}
trap cleanup EXIT

# --- 0. 基线：先确保当前版本已安装，记录数据指纹 -----------------------------
install_deb "$CUR_DEB" >/dev/null 2>&1 || true
BASE_MD5="$(data_md5)"
echo "[info] 数据指纹：$BASE_MD5"

# --- 1. 安装旧版 --------------------------------------------------------------
install_deb "$OLD_DEB"
[ "$(cli_version)" = "$OLD_VER" ] && ok "旧版 $OLD_VER 安装成功" || bad "旧版安装后版本号不是 $OLD_VER（实际 $(cli_version)）"
[ "$(decode_nihao)" = "你好" ] && ok "旧版解码 nihao → 你好" || bad "旧版解码异常：$(decode_nihao)"
[ "$(data_md5)" = "$BASE_MD5" ] && ok "旧版安装未改动 libime 数据" || bad "旧版安装改动了 libime 数据"

# --- 2. 升级到当前版 ----------------------------------------------------------
install_deb "$CUR_DEB"
[ "$(cli_version)" = "$VERSION" ] && ok "升级后版本 = $VERSION" || bad "升级后版本不对：$(cli_version)"
[ "$(data_md5)" = "$BASE_MD5" ] && ok "升级未改动 libime 数据（sc.dict/LM 指纹一致）" || bad "升级改动了 libime 数据"
[ "$(decode_nihao)" = "你好" ] && ok "升级后解码 nihao → 你好" || bad "升级后解码异常：$(decode_nihao)"
grep -q "^Version=$VERSION$" "$ADDON_CONF" 2>/dev/null \
    && ok "addon 描述版本同步为 $VERSION" \
    || bad "addon 描述版本未同步（$(grep '^Version=' "$ADDON_CONF" 2>/dev/null)）"
[ -f /usr/lib/x86_64-linux-gnu/fcitx5/libpinyinime.so ] \
    && ok "addon 二进制在位" \
    || bad "addon 二进制缺失"

# --- 3. 回滚到旧版 ------------------------------------------------------------
dpkg -i --force-downgrade --force-depends "$OLD_DEB" >/dev/null 2>&1
[ "$(cli_version)" = "$OLD_VER" ] && ok "回滚到 $OLD_VER 成功" || bad "回滚失败（实际 $(cli_version)）"
[ "$(data_md5)" = "$BASE_MD5" ] && ok "回滚未改动 libime 数据" || bad "回滚改动了 libime 数据"
[ "$(decode_nihao)" = "你好" ] && ok "回滚后解码 nihao → 你好" || bad "回滚后解码异常：$(decode_nihao)"

# --- 4. 再升级回当前版 --------------------------------------------------------
install_deb "$CUR_DEB"
[ "$(cli_version)" = "$VERSION" ] && ok "再次升级回 $VERSION" || bad "再次升级失败（实际 $(cli_version)）"

# --- 5. 卸载后数据不能被误删 --------------------------------------------------
dpkg -r pinyin-ime >/dev/null 2>&1
[ -f "$DICT" ] && ok "卸载后 /usr/share/libime/sc.dict 仍在" || bad "卸载后 sc.dict 被删除"
[ -f "$LM" ] && ok "卸载后语言模型仍在" || bad "卸载后语言模型被删除"
[ ! -e "$CLI" ] && ok "卸载后 CLI 已移除" || bad "卸载后 CLI 仍存在"
[ ! -e /usr/lib/x86_64-linux-gnu/fcitx5/libpinyinime.so ] \
    && ok "卸载后 addon 已移除" \
    || bad "卸载后 addon 仍存在"

# --- 6. RPM 路径（有 rpm 且 dist 里有新旧两个 rpm 才跑）-------------------------
CUR_RPM="$(ls "$DIST"/pinyin-ime-"$VERSION"-*.rpm 2>/dev/null | head -1)"
OLD_RPM=""
if command -v rpm >/dev/null 2>&1 && [ -n "$CUR_RPM" ]; then
    for f in $(ls "$DIST"/pinyin-ime-*.rpm 2>/dev/null | sort -V); do
        v="$(basename "$f" | sed -E 's/pinyin-ime-([0-9.]+)-.*\.rpm/\1/')"
        [ "$v" = "$VERSION" ] && continue
        if [ "$(printf '%s\n%s\n' "$v" "$VERSION" | sort -V | tail -1)" = "$VERSION" ]; then
            OLD_RPM="$f"
        fi
    done
fi

if [ -n "$OLD_RPM" ]; then
    OLD_RPM_VER="$(basename "$OLD_RPM" | sed -E 's/pinyin-ime-([0-9.]+)-.*\.rpm/\1/')"
    RPM_ADDON="/usr/lib64/fcitx5/libpinyinime.so"
    rpm_ver() { rpm -q pinyin-ime --qf '%{VERSION}' 2>/dev/null; }
    echo ""
    echo "[info] RPM 升级路径：$OLD_RPM_VER → $VERSION"

    rpm -e --nodeps pinyin-ime >/dev/null 2>&1 || true
    rpm -i --nodeps "$OLD_RPM" >/dev/null 2>&1
    [ "$(rpm_ver)" = "$OLD_RPM_VER" ] && ok "RPM 旧版 $OLD_RPM_VER 安装成功" || bad "RPM 旧版安装失败（实际 $(rpm_ver)）"
    [ "$(decode_nihao)" = "你好" ] && ok "RPM 旧版解码 nihao → 你好" || bad "RPM 旧版解码异常：$(decode_nihao)"
    [ "$(data_md5)" = "$BASE_MD5" ] && ok "RPM 旧版安装未改动 libime 数据" || bad "RPM 旧版安装改动了 libime 数据"

    rpm -Uvh --nodeps "$CUR_RPM" >/dev/null 2>&1
    [ "$(rpm_ver)" = "$VERSION" ] && ok "RPM 升级后版本 = $VERSION" || bad "RPM 升级后版本不对：$(rpm_ver)"
    [ "$(data_md5)" = "$BASE_MD5" ] && ok "RPM 升级未改动 libime 数据" || bad "RPM 升级改动了 libime 数据"
    [ "$(decode_nihao)" = "你好" ] && ok "RPM 升级后解码 nihao → 你好" || bad "RPM 升级后解码异常：$(decode_nihao)"
    [ -f "$RPM_ADDON" ] && ok "RPM addon 二进制在位" || bad "RPM addon 二进制缺失（$RPM_ADDON）"

    rpm -Uvh --oldpackage --nodeps "$OLD_RPM" >/dev/null 2>&1
    [ "$(rpm_ver)" = "$OLD_RPM_VER" ] && ok "RPM 降级回 $OLD_RPM_VER 成功" || bad "RPM 降级失败（实际 $(rpm_ver)）"
    [ "$(decode_nihao)" = "你好" ] && ok "RPM 降级后解码 nihao → 你好" || bad "RPM 降级后解码异常：$(decode_nihao)"
    [ "$(data_md5)" = "$BASE_MD5" ] && ok "RPM 降级未改动 libime 数据" || bad "RPM 降级改动了 libime 数据"

    rpm -Uvh --nodeps "$CUR_RPM" >/dev/null 2>&1
    [ "$(rpm_ver)" = "$VERSION" ] && ok "RPM 再升级回 $VERSION" || bad "RPM 再升级失败（实际 $(rpm_ver)）"

    rpm -e --nodeps pinyin-ime >/dev/null 2>&1
    [ ! -e "$CLI" ] && ok "RPM 卸载后 CLI 已移除" || bad "RPM 卸载后 CLI 仍存在"
    [ ! -e "$RPM_ADDON" ] && ok "RPM 卸载后 addon 已移除" || bad "RPM 卸载后 addon 仍存在"
    [ -f "$DICT" ] && [ -f "$LM" ] && ok "RPM 卸载后 libime 数据仍在" || bad "RPM 卸载后 libime 数据被删除"
else
    skip "缺 rpm 或缺旧版 rpm 包，跳过 RPM 升级矩阵"
fi

echo ""
echo "升级矩阵：$PASS 通过，$FAIL 失败"
[ "$FAIL" -eq 0 ] || exit 1
