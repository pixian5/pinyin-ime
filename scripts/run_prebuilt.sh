#!/bin/bash
# run_prebuilt.sh — 用 S3 预构建产物在无 docker 的 openEuler aarch64 上
# 启动真正可用的 fcitx5 + pinyinime，并做 CLI / DBus / XIM / GUI 验证。
#
# 用法:
#   bash scripts/run_prebuilt.sh setup    # 下载/解包/生成运行时（幂等）
#   bash scripts/run_prebuilt.sh start    # 启动 Xvfb + DBus + fcitx5
#   bash scripts/run_prebuilt.sh status
#   bash scripts/run_prebuilt.sh test     # CLI + DBus e2e + XIM e2e
#   bash scripts/run_prebuilt.sh gui      # 生成候选窗/上屏截图
#   bash scripts/run_prebuilt.sh stop
#
# 默认路径均可覆盖:
#   DEPLOY=/workspace/deploy RUNDIR=/workspace/fcitx5-runtime
#   SYSROOT=/workspace/sysroot S3_DIR=$REPO_DIR/pinyin-ime-s3
set -uo pipefail

REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
DEPLOY="${DEPLOY:-/workspace/deploy}"
RUNDIR="${RUNDIR:-/workspace/fcitx5-runtime}"
SYSROOT="${SYSROOT:-/workspace/sysroot}"
S3_DIR="${S3_DIR:-$REPO_DIR/pinyin-ime-s3}"
XVFB_LIBS="${XVFB_LIBS:-/workspace/xvfb-libs}"
SHIM="${SHIM:-/workspace/xkbcomp_redirect.so}"
XDISPLAY="${XDISPLAY:-:99}"
DISPNUM="${XDISPLAY#:}"

MAIN_VER="${MAIN_VER:-v0.9.118}"
DATA_VER="${DATA_VER:-v0.9.116}"
DESK_VER="${DESK_VER:-v0.9.83}"
LIBIME_VER="${LIBIME_VER:-v0.9.84a}"
RCLONE_REMOTE="${RCLONE_REMOTE:-cst}"
S3_BUCKET="${S3_BUCKET:-xbz}"
S3_PREFIX="${S3_PREFIX:-pinyin-ime}"
UA="rclone/1.75.2 pinyin-ime"

DESK="$DEPLOY/pinyin-ime"
RT="$DEPLOY/pinyin-ime-$LIBIME_VER"
FCITX_BIN="$DESK/opt/fcitx5-x11/bin/fcitx5"
CLI="$S3_DIR/$MAIN_VER/pinyin_ime_cli_v0.9.118"
ADDON="$S3_DIR/$MAIN_VER/libpinyinime_aarch64_v0.9.118.so"
LM="$S3_DIR/$DATA_VER/zh_CN.lm"
DICT="$S3_DIR/$DATA_VER/sc.dict"
DATADIR="$DESK/share/libime/libime"
CLIENT="$REPO_DIR/tools/xim_test_client"
GUI="$REPO_DIR/tools/xim_gui_demo"

log(){ printf '\033[36m[ime]\033[0m %s\n' "$*"; }
warn(){ printf '\033[33m[ime]\033[0m %s\n' "$*" >&2; }
die(){ printf '\033[31m[ime]\033[0m %s\n' "$*" >&2; exit 1; }

need_deps(){
    [ -x "$SYSROOT/usr/bin/Xvfb" ] || die "缺 $SYSROOT/usr/bin/Xvfb；先运行 bash scripts/setup_sandbox_deps.sh"
    [ -x "$SYSROOT/usr/bin/xkbcomp" ] || die "缺 xkbcomp；先运行 bash scripts/setup_sandbox_deps.sh"
    [ -x "$SYSROOT/usr/bin/dbus-daemon" ] || die "缺 dbus-daemon；先运行 bash scripts/setup_sandbox_deps.sh"
    [ -f "$SHIM" ] || die "缺 $SHIM；先运行 bash scripts/setup_sandbox_deps.sh"
    [ -d "$XVFB_LIBS" ] || die "缺 $XVFB_LIBS；先运行 bash scripts/setup_sandbox_deps.sh"
}

download_artifacts(){
    local missing=0
    for f in "$CLI" "$ADDON" "$LM" "$DICT"; do
        [ -f "$f" ] || missing=1
    done
    [ -x "$DESK/opt/fcitx5-x11/bin/fcitx5" ] || missing=1
    [ -d "$RT" ] || missing=1
    [ "$missing" = 0 ] && return 0
    command -v rclone >/dev/null || die "缺预构建产物且未安装 rclone"
    rclone listremotes 2>/dev/null | grep -q "^${RCLONE_REMOTE}:" ||
        die "rclone 未配置远程 ${RCLONE_REMOTE}（见 README / scripts/download_from_s3.sh）"
    for v in "$MAIN_VER" "$DATA_VER" "$DESK_VER"; do
        [ -d "$S3_DIR/$v" ] && [ -n "$(ls -A "$S3_DIR/$v" 2>/dev/null)" ] && continue
        log "从 S3 下载 $v"
        rclone copy "${RCLONE_REMOTE}:${S3_BUCKET}/${S3_PREFIX}/${v}/" "$S3_DIR/$v/" \
            --user-agent "$UA" --stats-one-line --stats 15s || die "下载 $v 失败"
    done
    if [ ! -d "$RT" ]; then
        log "从 S3 下载 libIME bundle $LIBIME_VER"
        rclone copy "${RCLONE_REMOTE}:${S3_BUCKET}/${S3_PREFIX}/${LIBIME_VER}/" \
            "$S3_DIR/$LIBIME_VER/" --user-agent "$UA" --stats-one-line --stats 15s ||
            die "下载 $LIBIME_VER 失败"
        local tgz
        tgz="$(find "$S3_DIR/$LIBIME_VER" -name 'pinyin-ime-v0.9.84a.tar.gz' | head -1)"
        [ -n "$tgz" ] && tar xzf "$tgz" -C "$DEPLOY"
    fi
}

extract_tree(){
    mkdir -p "$DEPLOY" "$S3_DIR"
    if [ ! -x "$FCITX_BIN" ]; then
        log "解包 fcitx5 桌面运行时 $DESK_VER"
        tar xzf "$S3_DIR/$DESK_VER/pinyin-ime-desktop-v0.9.83.tar.gz" -C "$DEPLOY"
    fi
    if [ ! -d "$RT" ]; then
        if [ -d "$REPO_DIR/pinyin-ime-s3/$LIBIME_VER/pinyin-ime-v0.9.84a" ]; then
            cp -a "$REPO_DIR/pinyin-ime-s3/$LIBIME_VER/pinyin-ime-v0.9.84a" "$RT"
        else
            die "缺 libIME 运行时 $RT"
        fi
    fi
    [ -x "$FCITX_BIN" ] || die "缺 fcitx5: $FCITX_BIN"
    [ -f "$RT/lib/libIMEPinyin.so.1.1.17" ] || die "缺 libIME: $RT/lib"
    [ -f "$CLI" ] || die "缺 CLI: $CLI"
    [ -f "$ADDON" ] || die "缺 addon: $ADDON"
}

check_data(){
    local checksum
    checksum="$(sha256sum "$LM" | cut -d' ' -f1)"
    [ "$checksum" = "3588b3942c8fd62e1a6bd3bae8c7cadc0faf928c175b245224ed475862218387" ] ||
        die "zh_CN.lm 校验失败"
    checksum="$(sha256sum "$DICT" | cut -d' ' -f1)"
    [ "$checksum" = "38b9ca9a350b68fb3c4dd1028ccb9a4f4fb9a14d01a6a71839aa074bdd34ef10" ] ||
        die "sc.dict 校验失败"
}

prepare_files(){
    mkdir -p "$DATADIR" "$RUNDIR/conf/fcitx5/conf" "$RUNDIR/cache" \
             "$RUNDIR/fontcache" "$RUNDIR/userdata" /run/user/0
    rm -f "$DATADIR/sc.dict" "$DESK/share/libime/sc.dict"
    cp -f "$LM" "$DATADIR/zh_CN.lm"
    cp -f "$DICT" "$DATADIR/sc.dict"
    cp -f "$DICT" "$DESK/share/libime/sc.dict"
    cp -f "$ADDON" "$DESK/opt/fcitx5-x11/lib/fcitx5/libpinyinime.so"

    cat > "$DESK/opt/fcitx5-x11/share/fcitx5/addon/pinyinime.conf" <<'EOF'
[Addon]
Name=Pinyin IME
Comment=Pinyin input method built on libime
Type=SharedLibrary
Library=libpinyinime
Category=InputMethod
Version=0.9.118
OnDemand=False
Configurable=True

[Addon/Dependencies]
0=core:5.1.23
EOF

    cat > "$RUNDIR/conf/fcitx5/profile" <<'EOF'
[Groups/0]
Name=Default
Default Layout=us
DefaultIM=pinyinime
[Groups/0/Items/0]
Name=keyboard-us
Layout=
[Groups/0/Items/1]
Name=pinyinime
Layout=
[GroupOrder]
0=Default
EOF

    cat > "$RUNDIR/conf/fcitx5/conf/pinyinime.conf" <<EOF
NBest=45
PageSize=9
DataDir=$DATADIR
UserDataDir=$RUNDIR/userdata
EnableFuzzy=True
EnableCorrect=True
EOF

    cat > "$RUNDIR/fonts.conf" <<EOF
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <dir>$SYSROOT/usr/share/fonts</dir>
  <cachedir>$RUNDIR/fontcache</cachedir>
</fontconfig>
EOF

    cat > "$RUNDIR/dbus.conf" <<'EOF'
<!DOCTYPE busconfig PUBLIC "-//freedesktop//DTD D-Bus Bus Configuration 1.0//EN"
 "http://www.freedesktop.org/standards/dbus/1.0/busconfig.dtd">
<busconfig>
  <type>session</type>
  <listen>unix:tmpdir=/tmp</listen>
  <auth>EXTERNAL</auth>
  <policy context="default">
    <allow send_destination="*"/>
    <allow receive_sender="*"/>
    <allow own="*"/>
  </policy>
</busconfig>
EOF
}

build_helpers(){
    if [ ! -f "$SHIM" ]; then
        log "编译 xkbcomp shim"
        gcc -shared -fPIC -O2 -o "$SHIM" "$REPO_DIR/tools/xkbcomp_redirect.c" -ldl
    fi
    if [ ! -x "$CLIENT" ] || [ "$REPO_DIR/tools/xim_test_client.c" -nt "$CLIENT" ]; then
        log "编译 XIM 测试客户端"
        gcc -O2 -I "$SYSROOT/usr/include" -o "$CLIENT" \
            "$REPO_DIR/tools/xim_test_client.c" -L "$SYSROOT/usr/lib64" \
            -Wl,-rpath-link,"$SYSROOT/usr/lib64" -lX11 -lXtst -lXext -lXi ||
            warn "XIM 客户端编译失败（XIM 测试将跳过）"
    fi
    if [ ! -x "$GUI" ] || [ "$REPO_DIR/tools/xim_gui_demo.c" -nt "$GUI" ]; then
        log "编译 GUI 演示客户端"
        gcc -O2 -I "$SYSROOT/usr/include" -I "$SYSROOT/usr/include/freetype2" \
            -I "$SYSROOT/usr/include/fontconfig" -o "$GUI" \
            "$REPO_DIR/tools/xim_gui_demo.c" -L "$SYSROOT/usr/lib64" \
            -Wl,-rpath-link,"$SYSROOT/usr/lib64" -lX11 -lXtst -lXft \
            -lfontconfig -lfreetype || warn "GUI 客户端编译失败（截图将跳过）"
    fi
}

setup(){
    download_artifacts
    extract_tree
    check_data
    prepare_files
    build_helpers
    log "运行时已就绪: DEPLOY=$DEPLOY"
    log "数据: $DATADIR/zh_CN.lm ($(du -h "$DATADIR/zh_CN.lm" | cut -f1))"
}

kill_pidfile(){
    local f="$1"
    [ -f "$f" ] || return 0
    local pid
    pid="$(cat "$f" 2>/dev/null || true)"
    if [ -n "$pid" ] && kill -0 "$pid" 2>/dev/null; then
        kill "$pid" 2>/dev/null || true
        for _ in $(seq 1 20); do kill -0 "$pid" 2>/dev/null || break; sleep 0.1; done
        kill -9 "$pid" 2>/dev/null || true
    fi
    rm -f "$f"
}

stop_all(){
    kill_pidfile "$RUNDIR/fcitx5.pid"
    kill_pidfile "$RUNDIR/dbus.pid"
    kill_pidfile "$RUNDIR/xvfb.pid"
    rm -f "$RUNDIR/dbus_addr"
}

start_xvfb(){
    rm -f "/tmp/.X${DISPNUM}-lock" "/tmp/.X11-unix/X${DISPNUM}" 2>/dev/null || true
    log "启动 Xvfb $XDISPLAY"
    LD_PRELOAD="$SHIM" XKBCOMP_REAL="$SYSROOT/usr/bin/xkbcomp" \
    LD_LIBRARY_PATH="$XVFB_LIBS" setsid "$SYSROOT/usr/bin/Xvfb" "$XDISPLAY" \
        -screen 0 1280x1024x24 -nolisten tcp \
        -xkbdir "$SYSROOT/usr/share/X11/xkb" \
        > "$RUNDIR/xvfb.log" 2>&1 < /dev/null &
    echo $! > "$RUNDIR/xvfb.pid"
    sleep 2
    pgrep -f "[X]vfb $XDISPLAY" >/dev/null || { tail -20 "$RUNDIR/xvfb.log"; die "Xvfb 启动失败"; }
}

start_dbus(){
    local loader out
    # $SYSROOT/lib 通常是指向 usr/lib 的符号链接，find 默认不跟随起始符号链接，
    # 因此这里用 glob 而不是 find -maxdepth。
    loader="$(ls -1 "$SYSROOT"/lib/ld-linux-*.so.* 2>/dev/null | head -1)"
    [ -n "$loader" ] || die "找不到 sysroot loader（$SYSROOT/lib/ld-linux-*.so.*）"
    log "启动 DBus session"
    out="$("$loader" --library-path "$SYSROOT/usr/lib64:$SYSROOT/lib64" \
        "$SYSROOT/usr/bin/dbus-daemon" --config-file="$RUNDIR/dbus.conf" \
        --fork --print-address=1 --print-pid=1 2>"$RUNDIR/dbus.err")" ||
        { cat "$RUNDIR/dbus.err"; die "DBus 启动失败"; }
    printf '%s\n' "$out" > "$RUNDIR/dbus.out"
    local addr pid
    addr="$(printf '%s\n' "$out" | grep -m1 '^unix:' || true)"
    pid="$(printf '%s\n' "$out" | grep -E '^[0-9]+$' | tail -1 || true)"
    [ -n "$addr" ] || die "DBus 地址为空"
    printf '%s\n' "$addr" > "$RUNDIR/dbus_addr"
    [ -n "$pid" ] && printf '%s\n' "$pid" > "$RUNDIR/dbus.pid"
}

start_fcitx(){
    local db
    db="$(cat "$RUNDIR/dbus_addr")"
    log "启动 fcitx5（X11/XIM/DBus）"
    env DISPLAY="$XDISPLAY" DBUS_SESSION_BUS_ADDRESS="$db" \
        XMODIFIERS=@im=fcitx GTK_IM_MODULE=fcitx QT_IM_MODULE=fcitx \
        XDG_CONFIG_HOME="$RUNDIR/conf" XDG_CACHE_HOME="$RUNDIR/cache" \
        XDG_RUNTIME_DIR=/run/user/0 XKB_CONFIG_ROOT="$SYSROOT/usr/share/X11/xkb" \
        FONTCONFIG_FILE="$RUNDIR/fonts.conf" FONTCONFIG_PATH="$RUNDIR" \
        LD_LIBRARY_PATH="$DESK/opt/deps-lib:$DESK/opt/fcitx5-x11/lib:$RT/lib" \
        PATH="$DESK/opt/fcitx5-x11/bin:/usr/local/bin:/usr/bin:/bin" \
        XDG_DATA_DIRS="$DESK/opt/fcitx5-x11/share:$DESK/share:/usr/share" \
        FCITX_ADDON_DIRS="$DESK/opt/fcitx5-x11/lib/fcitx5" \
        FCITX_DATA_DIR="$DESK/opt/fcitx5-x11/share/fcitx5" \
        setsid "$FCITX_BIN" > "$RUNDIR/fcitx5.log" 2>&1 < /dev/null &
    echo $! > "$RUNDIR/fcitx5.pid"
    # 进程出现 ≠ addon 已注册：必须等日志里同时出现 addon 与 LM/dict 加载行。
    local ready=0
    for _ in $(seq 1 80); do
        if grep -q 'Loaded addon pinyinime' "$RUNDIR/fcitx5.log" 2>/dev/null &&
           grep -q 'loaded lm=' "$RUNDIR/fcitx5.log" 2>/dev/null; then
            ready=1; break
        fi
        kill -0 "$(cat "$RUNDIR/fcitx5.pid")" 2>/dev/null || break
        sleep 0.25
    done
    [ "$ready" = 1 ] || { tail -40 "$RUNDIR/fcitx5.log"; die "pinyinime addon / LM 未加载"; }

    cat > "$RUNDIR/runtime.env" <<EOF
export DISPLAY=$XDISPLAY
export DBUS_SESSION_BUS_ADDRESS='$db'
export XMODIFIERS=@im=fcitx
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XDG_CONFIG_HOME=$RUNDIR/conf
export FONTCONFIG_FILE=$RUNDIR/fonts.conf
export FONTCONFIG_PATH=$RUNDIR
export LD_LIBRARY_PATH=$XVFB_LIBS
export PATH=$SYSROOT/usr/bin:\$PATH
EOF
}

start(){
    need_deps
    setup
    stop_all
    start_xvfb
    start_dbus
    start_fcitx
    log "Xvfb $XDISPLAY ✅ | DBus ✅ | fcitx5 ✅ | pinyinime ✅"
    log "加载行: $(grep -m1 'loaded lm=' "$RUNDIR/fcitx5.log" | sed 's/^.*pinyinime: //')"
    log "进入环境: source $RUNDIR/runtime.env"
}

status_cmd(){
    local ok=0
    pgrep -f "[X]vfb $XDISPLAY" >/dev/null && echo "Xvfb $XDISPLAY: ✅" || { echo "Xvfb $XDISPLAY: ❌"; ok=1; }
    pgrep -x fcitx5 >/dev/null && echo "fcitx5: ✅" || { echo "fcitx5: ❌"; ok=1; }
    if [ -f "$RUNDIR/dbus_addr" ] && [ -n "$(cat "$RUNDIR/dbus_addr")" ] &&
       [ -f "$RUNDIR/dbus.pid" ] && kill -0 "$(cat "$RUNDIR/dbus.pid")" 2>/dev/null; then
        echo "DBus: ✅ $(cat "$RUNDIR/dbus_addr")"
    else
        echo "DBus: ❌"; ok=1
    fi
    # 日志会跨 stop/start 保留，必须同时确认 fcitx5 仍在运行，避免把旧日志当成当前状态。
    if pgrep -x fcitx5 >/dev/null &&
       grep -q 'Loaded addon pinyinime' "$RUNDIR/fcitx5.log" 2>/dev/null; then
        echo "pinyinime addon: ✅"
        grep -m1 'loaded lm=' "$RUNDIR/fcitx5.log" | sed 's/^/  /'
    else
        echo "pinyinime addon: ❌"; ok=1
    fi
    return $ok
}

cli_tests(){
    local cases=(
        "nihao|你好" "zhongguo|中国" "zg|中国" "shurufa|输入法"
        "beijing|北京" "beijingdaxue|北京大学"
        # CLI 按整串最长路径切分，nihaozg 会选“你好这个”；XIM 端有交互式分词，
        # 同样的按键序列得到“你好中国”，因此两套断言不同。
        "nihaozg|你好这个" "woshizhongguoren|我是中国人" "hello|hello"
    )
    local pass=0 fail=0 c input expect out
    local libs="$DESK/opt/deps-lib:$DESK/opt/fcitx5-x11/lib:$RT/lib"
    for c in "${cases[@]}"; do
        input="${c%%|*}"; expect="${c##*|}"
        out="$(printf '%s\n' "$input" | env LD_LIBRARY_PATH="$libs" \
            "$CLI" "$DATADIR/zh_CN.lm" "$DATADIR/sc.dict" 2>/dev/null |
            head -1 | cut -f1)"
        if [ "$out" = "$expect" ]; then
            printf '  ✅ CLI %-20s → %s\n' "$input" "$out"; pass=$((pass+1))
        else
            printf '  ❌ CLI %-20s → %s (期望 %s)\n' "$input" "$out" "$expect"; fail=$((fail+1))
        fi
    done
    echo "CLI: $pass/$((pass+fail))"
    [ "$fail" = 0 ]
}

xim_tests(){
    [ -x "$CLIENT" ] || { warn "跳过 XIM（客户端未编译）"; return 0; }
    local cases=(
        '\Snihao\s|你好' '\Szhongguo\s|中国' '\Sshurufa\s|输入法'
        '\Sbeijing\s|北京' '\Szg\s|中国' '\Snihaozg\s|你好中国'
        '\Swoshizhongguoren\s|我是中国人' '\Sbeijingdaxue\s|北京大学'
    )
    local pass=0 fail=0 c script expect out
    for c in "${cases[@]}"; do
        script="${c%%|*}"; expect="${c##*|}"
        out="$(env DISPLAY="$XDISPLAY" DBUS_SESSION_BUS_ADDRESS="$(cat "$RUNDIR/dbus_addr")" \
            XMODIFIERS=@im=fcitx LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8 \
            LD_LIBRARY_PATH="$XVFB_LIBS" timeout 25 "$CLIENT" "$script" 2>/dev/null |
            sed -n 's/^COMMIT://p' | tr -d '\n')"
        if [ "$out" = "$expect" ]; then
            printf '  ✅ XIM %-22s → %s\n' "$script" "$out"; pass=$((pass+1))
        else
            printf '  ❌ XIM %-22s → %s (期望 %s)\n' "$script" "$out" "$expect"; fail=$((fail+1))
        fi
    done
    echo "XIM: $pass/$((pass+fail))"
    [ "$fail" = 0 ]
}

dbus_tests(){
    [ -f "$REPO_DIR/scripts/e2e_input_test.py" ] || return 0
    log "DBus InputContext e2e"
    ( cd "$REPO_DIR" && env DISPLAY="$XDISPLAY" \
        DBUS_SESSION_BUS_ADDRESS="$(cat "$RUNDIR/dbus_addr")" \
        LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8 LD_LIBRARY_PATH="$XVFB_LIBS" \
        python3 scripts/e2e_input_test.py ) 2>&1 | tail -16
}

test_cmd(){
    need_deps
    [ -f "$RUNDIR/dbus_addr" ] || start
    echo "=== CLI ==="; cli_tests; local c=$?
    echo "=== XIM ==="; xim_tests; local x=$?
    echo "=== DBus ==="; dbus_tests
    [ "$c" = 0 ] && [ "$x" = 0 ] || return 1
}

run_gui_shot(){
    local script="$1" name="$2" shot="$RUNDIR/screenshots"
    mkdir -p "$shot"
    [ -x "$GUI" ] || { warn "跳过 GUI（客户端未编译）"; return 1; }
    [ -x "$SYSROOT/usr/bin/xwd" ] || { warn "跳过 GUI（缺 xwd）"; return 1; }
    local im_config im_coders
    im_config="$(find "$SYSROOT/usr/lib64" -maxdepth 2 -type d -name 'config-Q16HDRI' | head -1)"
    im_coders="$(find "$SYSROOT/usr/lib64" -maxdepth 3 -type d -name coders | head -1)"
    [ -n "$im_config" ] && [ -n "$im_coders" ] || { warn "跳过 GUI（缺 ImageMagick 模块）"; return 1; }

    env DISPLAY="$XDISPLAY" DBUS_SESSION_BUS_ADDRESS="$(cat "$RUNDIR/dbus_addr")" \
        XMODIFIERS=@im=fcitx LANG=zh_CN.UTF-8 LC_ALL=zh_CN.UTF-8 \
        FONTCONFIG_FILE="$RUNDIR/fonts.conf" FONTCONFIG_PATH="$RUNDIR" \
        LD_LIBRARY_PATH="$XVFB_LIBS" setsid "$GUI" "$script" 10 \
        > "$RUNDIR/xim_gui.log" 2>&1 < /dev/null &
    local pid=$!
    sleep 4
    LD_LIBRARY_PATH="$XVFB_LIBS" DISPLAY="$XDISPLAY" \
        "$SYSROOT/usr/bin/xwd" -root -silent -out "$shot/$name.xwd"
    MAGICK_CONFIGURE_PATH="$SYSROOT/etc/ImageMagick-7:$im_config" \
    MAGICK_CODER_MODULE_PATH="$im_coders" \
    LD_LIBRARY_PATH="$XVFB_LIBS" "$SYSROOT/usr/bin/magick" \
        "$shot/$name.xwd" -crop 1000x550+0+0 +repage "$shot/$name.png"
    wait "$pid" 2>/dev/null || true
    rm -f "$shot/$name.xwd"
    log "截图: $shot/$name.png"
}

gui_cmd(){
    need_deps
    [ -f "$RUNDIR/dbus_addr" ] || start
    run_gui_shot '\Snihao' prebuilt-candidate
    run_gui_shot '\Snihao\s' prebuilt-commit
}

case "${1:-start}" in
    setup) setup ;;
    start) start ;;
    stop) stop_all; log "已停止" ;;
    restart) stop_all; start ;;
    status) status_cmd ;;
    test) test_cmd ;;
    gui) gui_cmd ;;
    env) echo "source $RUNDIR/runtime.env" ;;
    *) echo "用法: $0 {setup|start|stop|restart|status|test|gui|env}"; exit 2 ;;
esac
