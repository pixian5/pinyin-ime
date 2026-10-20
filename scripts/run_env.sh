#!/bin/bash
# pinyin-ime 环境启动脚本 (v0.9.118)
# 在 openEuler 22.03 aarch64 上从源码构建的完整输入法环境
#
# 用法:
#   source scripts/run_env.sh          # 设置环境变量
#   bash scripts/run_env.sh start      # 启动 Xvfb + dbus + fcitx5
#   bash scripts/run_env.sh stop       # 停止服务
#   bash scripts/run_env.sh test       # 运行所有测试
#   bash scripts/run_env.sh status     # 查看状态

PROJECT_ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
NB="${NB:-/root/workspace/native-build}"
ENV="$NB/mamba-env"
OPT="$NB/opt"

log() { echo -e "\033[32m[pinyin-ime]\033[0m $*"; }

export_pinyin_env() {
    export MAMBA_ROOT_PREFIX="$NB/mamba-root"
    export PATH="$OPT/fcitx5-x11/bin:$ENV/bin:${PATH:-}"
    export LD_LIBRARY_PATH="$OPT/fcitx5-x11/lib:$ENV/lib:$OPT/libime/lib:$OPT/xcb-imdkit/lib64${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
    export FCITX_ADDON_DIRS="$OPT/fcitx5-x11/lib/fcitx5"
    export FCITX_ADDON_CONFIG_DIRS="$OPT/fcitx5-x11/share/fcitx5/addon"
    export FCITX_DATA_DIR="$OPT/fcitx5-x11/share/fcitx5"
    export XDG_DATA_DIRS="$OPT/fcitx5-x11/share:${XDG_DATA_DIRS:-}"
    export GTK_IM_MODULE=fcitx
    export QT_IM_MODULE=fcitx
    export XMODIFIERS=@im=fcitx
    export LANG=zh_CN.utf8
    export LC_ALL=zh_CN.utf8
    if [ -f /tmp/dbus_addr ]; then
        export DBUS_SESSION_BUS_ADDRESS="$(cat /tmp/dbus_addr 2>/dev/null || true)"
    fi
    # CLI paths
    export PINYIN_IME_CLI="$PROJECT_ROOT/cpp/build/pinyin_ime_cli"
    export PINYIN_IME_LM="$OPT/libime/share/libime/zh_CN.lm"
    export PINYIN_IME_DICT="$OPT/libime/share/libime/sc.dict"
}

do_start() {
    export_pinyin_env
    if ! pgrep -x Xvfb > /dev/null; then
        rm -f /tmp/.X11-unix/X99 /tmp/.X99-lock 2>/dev/null
        Xvfb :99 -screen 0 1280x1024x24 > /dev/null 2>&1 &
        sleep 2; log "Xvfb 已启动 (:99)"
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
    log "输入法已就绪！Ctrl+Space 切换中英文"
    log "CLI: $PINYIN_IME_CLI"
    log "LM:  $PINYIN_IME_LM"
    log "Dict: $PINYIN_IME_DICT"
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
        export_pinyin_env
        export DISPLAY=:99
        echo "XIM:    $(xprop -root XIM_SERVERS 2>/dev/null | grep -q . && echo '✅ registered' || echo '❌ not registered')"
    fi
}

do_test() {
    export_pinyin_env
    export DISPLAY=:99
    cd "$PROJECT_ROOT"

    # CLI tests
    echo "== CLI 测试 =="
    local cli_pass=0 cli_fail=0
    for test in "nihao:你好" "shijie:世界" "zhongguo:中国" "pinyin:拼音" "shurufa:输入法" "beijingdaxue:北京大学" "zhongwen:中文" "xuesheng:学生" "zg:中国" "zgr:中国人" "woshizg:我是中国" "hello:hello"; do
        py=${test%%:*}; exp=${test##*:}
        result=$(echo "$py" | "$PINYIN_IME_CLI" "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 2>/dev/null | head -1 | cut -f1)
        if [ "$result" = "$exp" ]; then
            echo "  ✅ $py → $result"; cli_pass=$((cli_pass+1))
        else
            echo "  ❌ $py → '$result' (expected: $exp)"; cli_fail=$((cli_fail+1))
        fi
    done
    echo "CLI: $cli_pass/$((cli_pass+cli_fail))"

    # XIM tests
    echo ""
    echo "== XIM 端到端测试 =="
    local xim_pass=0 xim_fail=0
    check_xim() {
        local desc="$1" script="$2" want="$3" out
        out=$(timeout 15 ./tools/xim_test_client "$script" 2>/dev/null | grep '^COMMIT' | tr '\n' ' ')
        if printf '%s' "$out" | grep -qF "$want"; then
            echo "  ✅ $desc -> $out"; xim_pass=$((xim_pass+1))
        else
            echo "  ❌ $desc -> $out (期望含 $want)"; xim_fail=$((xim_fail+1))
        fi
    }
    check_xim "nihao"     '\Snihao\s'     "你好"
    check_xim "zhongguo"  '\Szhongguo\s'  "中国"
    check_xim "zg(简拼)"  '\Szg\s'        "中国"
    check_xim "bjdx"      '\Sbjdx\s'      "北京大学"
    check_xim "woshiyige" '\Swoshiyige\s' "我是一个"
    check_xim "中英切换"  '\S\thello\n'   "hello"
    check_xim "纠错"      '\Szongguo\s'   "中国"
    echo "XIM: $xim_pass/$((xim_pass+xim_fail))"

    echo ""
    local total_pass=$((cli_pass+xim_pass)) total_fail=$((cli_fail+xim_fail))
    echo "========================================"
    echo "  总计: $total_pass/$((total_pass+total_fail)) 通过"
    echo "========================================"
}

case "${1:-status}" in
    start)  do_start ;;
    stop)   do_stop ;;
    test)   do_test ;;
    status) do_status ;;
    env)    export_pinyin_env; echo "Environment set" ;;
    *)      echo "用法: $0 {start|stop|test|status|env}"; exit 1 ;;
esac
