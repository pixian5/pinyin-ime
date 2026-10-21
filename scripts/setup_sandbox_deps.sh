#!/bin/bash
# setup_sandbox_deps.sh — 在「/usr、/etc 只读」的 openEuler 沙箱里准备
# Xvfb / DBus / X11 / 中文字体，不写 /usr、不写 /etc、不使用 docker。
#
# 依赖树安装到 $SYSROOT（默认 /workspace/sysroot），运行时通过
# $SYSROOT/lib/ld-linux-*.so.* 或一个只含非 glibc 库的 $XVFB_LIBS 目录加载。
#
# 用法:
#   bash scripts/setup_sandbox_deps.sh
#   SYSROOT=/workspace/sysroot bash scripts/setup_sandbox_deps.sh
set -euo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="${SYSROOT:-/workspace/sysroot}"
XVFB_LIBS="${XVFB_LIBS:-/workspace/xvfb-libs}"
REPO_DIR_DNF="${DNF_REPO_DIR:-/workspace/dnf-repos}"
ARCH="$(uname -m)"

log(){ printf '\033[36m[deps]\033[0m %s\n' "$*"; }
warn(){ printf '\033[33m[deps]\033[0m %s\n' "$*" >&2; }

if [ -x "$SYSROOT/usr/bin/Xvfb" ] && [ -x "$SYSROOT/usr/bin/xkbcomp" ] &&
   [ -x "$SYSROOT/usr/bin/dbus-daemon" ] && [ -x "$SYSROOT/usr/bin/xterm" ]; then
    log "sysroot 运行时已就绪: $SYSROOT"
else
    log "安装 Xvfb/DBus/X11/字体到 $SYSROOT（不写系统 /usr）"
    mkdir -p "$REPO_DIR_DNF"
    cat > "$REPO_DIR_DNF/huawei.repo" <<'EOF'
[OS]
name=openEuler-22.03-LTS-OS
baseurl=https://mirrors.huaweicloud.com/openeuler/openEuler-22.03-LTS/OS/$basearch/
enabled=1
gpgcheck=0

[everything]
name=openEuler-22.03-LTS-everything
baseurl=https://mirrors.huaweicloud.com/openeuler/openEuler-22.03-LTS/everything/$basearch/
enabled=1
gpgcheck=0

[EPOL]
name=openEuler-22.03-LTS-EPOL
baseurl=https://mirrors.huaweicloud.com/openeuler/openEuler-22.03-LTS/EPOL/main/$basearch/
enabled=1
gpgcheck=0

[update]
name=openEuler-22.03-LTS-update
baseurl=https://mirrors.huaweicloud.com/openeuler/openEuler-22.03-LTS/update/$basearch/
enabled=1
gpgcheck=0
EOF
    DNF=(dnf -y --installroot="$SYSROOT" --releasever=22.03-LTS
         --setopt=reposdir="$REPO_DIR_DNF" --setopt=install_weak_deps=False
         --setopt=tsflags=noscripts --nogpgcheck)
    "${DNF[@]}" install \
        xorg-x11-server-Xvfb xorg-x11-server-common xkeyboard-config \
        dbus dbus-x11 dbus-tools xorg-x11-utils xterm xauth \
        dejavu-sans-fonts wqy-microhei-fonts wqy-zenhei-fonts fontconfig \
        libX11-devel libXtst-devel libXext-devel xorg-x11-proto-devel \
        libXft-devel freetype-devel fontconfig-devel libXrender-devel \
        > /tmp/pinyin-ime-dnf-deps.log 2>&1 || warn "dnf 有非致命失败，见 /tmp/pinyin-ime-dnf-deps.log"

    # dbus-daemon 子包在 installroot + noscripts 下偶发解包失败，单独解包兜底。
    if [ ! -x "$SYSROOT/usr/bin/dbus-daemon" ]; then
        log "补装 dbus-daemon（手动解包）"
        TMP="$(mktemp -d)"
        trap 'rm -rf "$TMP"' EXIT
        ( cd "$TMP" && dnf -q --setopt=reposdir="$REPO_DIR_DNF" \
            --releasever=22.03-LTS download dbus-daemon >/dev/null )
        RPM="$(find "$TMP" -name 'dbus-daemon-*.rpm' | head -1)"
        [ -n "$RPM" ] || { echo "找不到 dbus-daemon RPM"; exit 1; }
        ( cd "$TMP" && rpm2archive "$RPM" >/dev/null )
        TGZ="$(find "$TMP" -name '*.rpm.tgz' | head -1)"
        tar xzf "$TGZ" -C "$SYSROOT" \
            ./usr/bin/dbus-daemon ./usr/bin/dbus-run-session \
            ./usr/bin/dbus-cleanup-sockets ./usr/libexec/dbus-1/dbus-daemon-launch-helper
        rm -rf "$TMP"
        trap - EXIT
    fi

    # 可选：截图/演示工具。失败不影响输入法运行。
    "${DNF[@]}" install ImageMagick xorg-x11-apps \
        > /tmp/pinyin-ime-dnf-optional.log 2>&1 || true
fi

[ -x "$SYSROOT/usr/bin/Xvfb" ] || { echo "缺少 $SYSROOT/usr/bin/Xvfb"; exit 1; }
[ -x "$SYSROOT/usr/bin/xkbcomp" ] || { echo "缺少 $SYSROOT/usr/bin/xkbcomp"; exit 1; }
[ -x "$SYSROOT/usr/bin/dbus-daemon" ] || { echo "缺少 $SYSROOT/usr/bin/dbus-daemon"; exit 1; }

# Xvfb 用系统 loader 启动，但需要的 X/GL 库在 sysroot。这里只暴露
# 非 glibc 库，避免 loader 与 sysroot 的 libc 混用。
log "生成 $XVFB_LIBS（排除 glibc/loader）"
mkdir -p "$XVFB_LIBS"
find "$SYSROOT/usr/lib64" -maxdepth 1 \( -type f -o -type l \) 2>/dev/null | while read -r f; do
    b="$(basename "$f")"
    case "$b" in
        ld-linux*|libc.so*|libc-2.*|libm.so*|libm-2.*|libpthread*|libdl*|librt*|libutil*|libresolv*|libnss_*|libanl*|libBrokenLocale*|libthread_db*) continue ;;
    esac
    ln -sfn "$f" "$XVFB_LIBS/$b"
done

# xkbcomp 重定向 shim：Xvfb 硬编码 /usr/bin/xkbcomp，而 /usr 只读。
if [ ! -f /workspace/xkbcomp_redirect.so ]; then
    log "编译 xkbcomp 重定向 shim"
    gcc -shared -fPIC -O2 -o /workspace/xkbcomp_redirect.so \
        "$REPO_DIR/tools/xkbcomp_redirect.c" -ldl
fi

log "完成。现在可运行: bash scripts/run_prebuilt.sh setup && bash scripts/run_prebuilt.sh start"
