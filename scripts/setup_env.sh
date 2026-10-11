#!/bin/bash
# pinyin-ime 环境设置脚本 (v0.9.84b)
# 在 openEuler 22.03 aarch64 上从 S3 预构建产物搭建完整可运行输入法
#
# 用法:
#   source scripts/setup_env.sh          # 设置环境变量
#   bash scripts/setup_env.sh install    # 首次安装
#   bash scripts/setup_env.sh start      # 启动服务
#   bash scripts/setup_env.sh stop       # 停止服务
#   bash scripts/setup_env.sh test       # 运行所有测试
#   bash scripts/setup_env.sh status     # 查看状态

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NB="${NB:-/root/workspace/native-build}"
BUNDLE="${BUNDLE:-/root/.local/pinyin-ime/opt/fcitx5-x11}"
DEPSLIB="${DEPSLIB:-/root/.local/pinyin-ime/opt/deps-lib}"
ENV_DIR="$NB/mamba-env"
DATADIR="/root/.local/pinyin-ime/share/libime"
S3_DIR="$PROJECT_ROOT/pinyin-ime-s3"
VERSION="0.9.83"

log() { echo -e "\033[32m[pinyin-ime]\033[0m $*"; }

export_pinyin_env() {
    export PATH="$BUNDLE/bin:$ENV_DIR/bin:${PATH:-}"
    export LD_LIBRARY_PATH="$DEPSLIB:$DEPSLIB/lib64:$BUNDLE/lib:$NB/opt/libime/lib:$ENV_DIR/lib${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export FCITX_ADDON_DIRS="$BUNDLE/lib/fcitx5"
    export FCITX_ADDON_CONFIG_DIRS="$BUNDLE/share/fcitx5/addon"
    export FCITX_DATA_DIR="$BUNDLE/share/fcitx5"
    export XDG_DATA_DIRS="$BUNDLE/share:${XDG_DATA_DIRS:-}"
    export GTK_IM_MODULE=fcitx
    export QT_IM_MODULE=fcitx
    export XMODIFIERS=@im=fcitx
    export LANG=zh_CN.utf8
    export LC_ALL=zh_CN.utf8
    if [ -f /tmp/dbus_addr ]; then
        export DBUS_SESSION_BUS_ADDRESS="$(cat /tmp/dbus_addr 2>/dev/null || true)"
    fi
}

do_install() {
    log "Step 1: 安装系统依赖..."
    dnf install -y xorg-x11-server-Xvfb dbus dbus-x11 \
        boost-devel fmt-devel zstd-devel libuv-devel ninja-build \
        extra-cmake-modules gettext libuuid-devel python3-dbus \
        libX11-devel libXtst-devel xorg-x11-utils 2>/dev/null
    log "系统依赖已安装"

    log "Step 2: 安装 rclone..."
    if ! which rclone >/dev/null 2>&1; then
        curl -sL "https://gh-proxy.com/https://github.com/rclone/rclone/releases/download/v1.75.1/rclone-v1.75.1-linux-arm64.zip" -o /tmp/rclone.zip
        cd /tmp && unzip -o rclone.zip && cp rclone-v1.75.1-linux-arm64/rclone /usr/local/bin/
        chmod +x /usr/local/bin/rclone
    fi

    log "Step 3: 配置 S3 远程..."
    mkdir -p ~/.config/rclone
    cat > ~/.config/rclone/rclone.conf << 'EOF'
[cst]
type = s3
provider = Other
access_key_id = AKIAHSYF5VQJWQPHJ2WO
secret_access_key = EN3YQRS2SQ=R/YUM/BA+S3JC8YBBG09HKBEMG+LZ
endpoint = https://s3.cstcloud.cn
force_path_style = true
acl = private
EOF

    log "Step 4: 下载 S3 预构建产物..."
    mkdir -p "$S3_DIR/v0.9.83" "$S3_DIR/v0.9.84"
    rclone copy "cst:xbz/pinyin-ime/v0.9.83/" "$S3_DIR/v0.9.83/" --user-agent "rclone/1.75.1 pinyin-ime" 2>/dev/null
    rclone copy "cst:xbz/pinyin-ime/v0.9.84/" "$S3_DIR/v0.9.84/" --user-agent "rclone/1.75.1 pinyin-ime" 2>/dev/null

    log "Step 5: 解压桌面 bundle..."
    mkdir -p /root/.local
    if [ ! -d /root/.local/pinyin-ime ]; then
        tar xzf "$S3_DIR/v0.9.83/pinyin-ime-desktop-v0.9.83.tar.gz" -C /root/.local
    fi

    log "Step 6: 安装 micromamba + conda GCC 13..."
    mkdir -p "$NB"/{bin,src,opt,build,mamba-root}
    export MAMBA_ROOT_PREFIX="$NB/mamba-root"
    if [ ! -x "$NB/bin/micromamba" ]; then
        TMP=$(mktemp -d)
        curl -sL -o "$TMP/mm.tar.bz2" "https://gh-proxy.com/https://github.com/mamba-org/micromamba-releases/releases/download/2.9.0-0/micromamba-linux-aarch64.tar.bz2"
        tar -xjf "$TMP/mm.tar.bz2" -C "$TMP" bin/micromamba
        cp "$TMP/bin/micromamba" "$NB/bin/micromamba" && chmod +x "$NB/bin/micromamba"
        rm -rf "$TMP"
    fi
    if [ ! -d "$ENV_DIR/conda-meta" ]; then
        "$NB/bin/micromamba" create -y -p "$ENV_DIR" -c https://mirrors.bfsu.edu.cn/anaconda/cloud/conda-forge \
            gcc=13 gxx=13 boost-cpp fmt zstd libuv extra-cmake-modules gettext libuuid cmake ninja pkg-config make
    fi

    log "Step 7: 部署 libIME 库..."
    SUPP="$S3_DIR/v0.9.84/supplement"
    cp -f "$SUPP"/libIMEPinyin.so.1.1.17 "$DEPSLIB/"
    cp -f "$SUPP"/libIMECore.so.1.1.17 "$DEPSLIB/"
    cp -f "$SUPP"/libIMETable.so.1.1.17 "$DEPSLIB/"
    cp -f "$SUPP"/libxcb-imdkit.so.1.0.9 "$DEPSLIB/"
    cd "$DEPSLIB"
    ln -sf libIMEPinyin.so.1.1.17 libIMEPinyin.so.0; ln -sf libIMEPinyin.so.0 libIMEPinyin.so
    ln -sf libIMECore.so.1.1.17 libIMECore.so.0; ln -sf libIMECore.so.0 libIMECore.so
    ln -sf libIMETable.so.1.1.17 libIMETable.so.0; ln -sf libIMETable.so.0 libIMETable.so
    ln -sf libxcb-imdkit.so.1.0.9 libxcb-imdkit.so.1; ln -sf libxcb-imdkit.so.1 libxcb-imdkit.so
    cp -f "$SUPP"/libpinyinime_v0.9.84.so "$BUNDLE/lib/fcitx5/libpinyinime.so"

    log "Step 8: 部署数据文件..."
    cp -f "$S3_DIR/v0.9.83/zh_CN.lm" "$DATADIR/libime/zh_CN.lm"
    rm -f "$DATADIR/libime/sc.dict"
    cp -f "$S3_DIR/v0.9.83/sc.dict" "$DATADIR/libime/sc.dict"
    cp -f "$S3_DIR/v0.9.83/sc.dict" "$DATADIR/sc.dict"

    log "Step 9: 修复路径符号链接..."
    ln -sf /root/workspace /workspace 2>/dev/null || true
    mkdir -p /workspace/native-build
    ln -sf "$ENV_DIR" /workspace/native-build/mamba-env
    ln -sf "$NB/opt" /workspace/native-build/opt 2>/dev/null || true
    ln -sf "$BUNDLE" "$NB/opt/fcitx5-x11" 2>/dev/null || true
    mkdir -p "$NB/opt/libime/lib"
    ln -sf "$DATADIR/libime/zh_CN.lm" "$NB/opt/libime/lib/zh_CN.lm"
    ln -sf "$DATADIR/sc.dict" "$NB/opt/libime/lib/sc.dict"
    cp -f "$DEPSLIB"/libIME*.so* "$NB/opt/libime/lib/" 2>/dev/null
    cp -f "$DEPSLIB"/libxcb-imdkit.so* "$NB/opt/libime/lib/" 2>/dev/null
    rm -rf "$ENV_DIR/share/xkeyboard-config-2"
    ln -sf /usr/share/X11/xkb "$ENV_DIR/share/xkeyboard-config-2"
    mkdir -p "$ENV_DIR/share/X11"
    ln -sf /usr/share/X11/xkb "$ENV_DIR/share/X11/xkb" 2>/dev/null || true
    mkdir -p "$ENV_DIR/etc/fonts" "$ENV_DIR/var/cache/fontconfig"
    cat > "$ENV_DIR/etc/fonts/fonts.conf" << 'FONTCFG'
<?xml version="1.0"?>
<!DOCTYPE fontconfig SYSTEM "fonts.dtd">
<fontconfig>
  <include ignore_missing="yes">/etc/fonts/fonts.conf</include>
  <dir>/usr/share/fonts</dir>
  <cachedir>/root/workspace/native-build/mamba-env/var/cache/fontconfig</cachedir>
</fontconfig>
FONTCFG

    log "Step 10: 配置 fcitx5 + CLI..."
    mkdir -p /root/.config/fcitx5/conf ~/.config/pinyin-ime
    cat > /root/.config/fcitx5/profile << 'PROF'
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
PROF
    cat > /root/.config/fcitx5/conf/pinyinime.conf << CONF
DataDir=$NB/opt/libime/lib
UserDataDir=/tmp/pinyin-ime-x11-ud
EnableCorrect=True
EnableFuzzy=True
CONF
    cat > ~/.config/pinyin-ime/config << EOF
lm_path=$DATADIR/libime/zh_CN.lm
dict_path=$DATADIR/sc.dict
nbest=5
EOF

    log "Step 11: 编译 XIM 测试客户端..."
    cd "$PROJECT_ROOT"
    if [ ! -x tools/xim_test_client ]; then
        gcc -o tools/xim_test_client tools/xim_test_client.c $(pkg-config --cflags --libs x11 xtst) 2>/dev/null
    fi

    log "安装完成！运行 'bash scripts/setup_env.sh start' 启动输入法"
}

do_start() {
    export_pinyin_env
    if ! pgrep -x Xvfb > /dev/null; then
        rm -f /tmp/.X11-unix/X99 /tmp/.X99-lock 2>/dev/null
        Xvfb :99 -screen 0 1280x1024x24 > /dev/null 2>&1 &
        sleep 1; log "Xvfb 已启动 (:99)"
    fi
    export DISPLAY=:99
    if ! pgrep -x dbus-daemon > /dev/null; then
        eval $(dbus-launch --sh-syntax 2>/dev/null)
        echo "$DBUS_SESSION_BUS_ADDRESS" > /tmp/dbus_addr
        sleep 1; log "dbus 已启动"
    fi
    if ! pgrep -x fcitx5 > /dev/null; then
        fcitx5 -d 2>&1 | head -5; sleep 3; log "fcitx5 已启动"
    fi
    log "输入法已启动！Ctrl+Space 切换中英文"
}

do_stop() {
    pkill -x fcitx5 2>/dev/null; pkill -x Xvfb 2>/dev/null; pkill -x dbus-daemon 2>/dev/null
    log "已停止"
}

do_status() {
    echo "=== 服务状态 ==="
    echo "Xvfb:   $(pgrep -x Xvfb > /dev/null && echo '✅ running' || echo '❌ stopped')"
    echo "dbus:   $(pgrep -x dbus-daemon > /dev/null && echo '✅ running' || echo '❌ stopped')"
    echo "fcitx5: $(pgrep -x fcitx5 > /dev/null && echo '✅ running' || echo '❌ stopped')"
    if pgrep -x fcitx5 > /dev/null; then
        echo "XIM:    $(DISPLAY=:99 xprop -root XIM_SERVERS 2>/dev/null | grep -q . && echo '✅ registered' || echo '❌ not registered')"
    fi
    echo "=== 数据文件 ==="
    echo "LM:   $(ls /root/.local/pinyin-ime/share/libime/libime/zh_CN.lm 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
    echo "Dict: $(ls /root/.local/pinyin-ime/share/libime/sc.dict 2>/dev/null > /dev/null && echo '✅' || echo '❌')"
}

do_test() {
    export_pinyin_env; export DISPLAY=:99; cd "$PROJECT_ROOT"
    LM="$DATADIR/libime/zh_CN.lm"; DICT="$DATADIR/sc.dict"
    TP=0; TT=0
    echo "========================================"
    echo "  pinyin-ime 全面测试 (v0.9.84b)"
    echo "========================================"

    echo -n "--- CLI 冒烟(10) ---  "
    p=0;t=0
    for i in "nihao:你好" "shijie:世界" "zhongguo:中国" "pinyin:拼音" "shurufa:输入法" "beijingdaxue:北京大学" "zhongwen:中文" "xuesheng:学生" "laoshi:老师" "keji:科技"; do
        py=${i%%:*}; exp=${i##*:}; r=$(echo "$py" | dist/pinyin_ime_cli "$LM" "$DICT" 2>/dev/null | head -1 | cut -f1)
        t=$((t+1)); [ "$r" = "$exp" ] && p=$((p+1))
    done
    echo "$p/$t"; TP=$((TP+p)); TT=$((TT+t))

    echo -n "--- CLI 简拼(5) ---   "
    p=0;t=0
    for i in "zg:中国" "bjdx:北京大学" "sh:上海" "zgr:中国人" "xuesheng:学生"; do
        py=${i%%:*}; exp=${i##*:}; r=$(echo "$py" | dist/pinyin_ime_cli "$LM" "$DICT" 2>/dev/null | head -1 | cut -f1)
        t=$((t+1)); [ "$r" = "$exp" ] && p=$((p+1))
    done
    echo "$p/$t"; TP=$((TP+p)); TT=$((TT+t))

    echo -n "--- CLI 模糊音(3) ---  "
    p=0;t=0
    for i in "zongguo:中国" "senme:什么" "zhidao:知道"; do
        py=${i%%:*}; exp=${i##*:}; r=$(echo "$py" | dist/pinyin_ime_cli -f "$LM" "$DICT" 2>/dev/null | head -1 | cut -f1)
        t=$((t+1)); [ "$r" = "$exp" ] && p=$((p+1))
    done
    echo "$p/$t"; TP=$((TP+p)); TT=$((TT+t))

    echo -n "--- DBus e2e ---       "
    r=$(python3.9 scripts/e2e_input_test.py 2>&1 | grep "Tests passed" | grep -oP '\d+/\d+')
    echo "$r"; p=$(echo $r | cut -d/ -f1); t=$(echo $r | cut -d/ -f2); TP=$((TP+p)); TT=$((TT+t))

    echo -n "--- 交互测试 ---       "
    r=$(python3.9 scripts/interaction_test.py 2>&1 | grep "Tests passed" | grep -oP '\d+/\d+')
    echo "$r"; p=$(echo $r | cut -d/ -f1); t=$(echo $r | cut -d/ -f2); TP=$((TP+p)); TT=$((TT+t))

    echo -n "--- 选词测试 ---       "
    r=$(python3.9 scripts/select_test.py 2>&1 | grep "Tests passed" | grep -oP '\d+/\d+')
    echo "$r"; p=$(echo $r | cut -d/ -f1); t=$(echo $r | cut -d/ -f2); TP=$((TP+p)); TT=$((TT+t))

    echo -n "--- XIM 端到端(5) ---  "
    p=0;t=0
    for i in "nihao:你好" "zhongguo:中国" "zg:中国" "bjdx:北京大学" "woshiyige:我是一个"; do
        py=${i%%:*}; exp=${i##*:}; r=$(./tools/xim_test_client "\S${py}\s" 2>/dev/null | grep "^COMMIT:" | tr '\n' ' ')
        t=$((t+1)); echo "$r" | grep -qF "$exp" && p=$((p+1))
    done
    echo "$p/$t"; TP=$((TP+p)); TT=$((TT+t))

    echo -n "--- XIM 模糊音(5) ---  "
    p=0;t=0
    for i in "zongguo:中国" "senme:什么" "nihao:你好" "zhongguo:中国" "shijie:世界"; do
        py=${i%%:*}; exp=${i##*:}; r=$(./tools/xim_test_client "\S${py}\s" 2>/dev/null | grep "^COMMIT:" | tr '\n' ' ')
        t=$((t+1)); echo "$r" | grep -qF "$exp" && p=$((p+1))
    done
    echo "$p/$t"; TP=$((TP+p)); TT=$((TT+t))

    echo ""
    echo "========================================"
    echo "  总计: $TP/$TT 通过"
    echo "========================================"
}

case "${1:-status}" in
    install) do_install ;;
    start)   do_start ;;
    stop)    do_stop ;;
    test)    do_test ;;
    status)  do_status ;;
    env)     export_pinyin_env; echo "Environment set" ;;
    *)       echo "用法: $0 {install|start|stop|test|status|env}"; exit 1 ;;
esac
