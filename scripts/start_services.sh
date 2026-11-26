#!/bin/bash
# pinyin-ime 服务启动脚本
# 用法: bash scripts/start_services.sh [start|stop|status|test]

NB=/root/workspace/native-build
REPO=/root/workspace/pinyin-ime

export PATH="$NB/opt/fcitx5/bin:${PATH:-}"
export LD_LIBRARY_PATH="$NB/mamba-env/lib:$NB/opt/fcitx5/lib:$NB/opt/libime/lib:${LD_LIBRARY_PATH:-}"
export FCITX_ADDON_DIRS="$NB/opt/fcitx5/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$NB/opt/fcitx5/share/fcitx5/addon"
export FCITX_DATA_DIR="$NB/opt/fcitx5/share/fcitx5"
export XDG_DATA_DIRS="$NB/opt/fcitx5/share:${XDG_DATA_DIRS:-}"
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export XMODIFIERS=@im=fcitx
export LANG=zh_CN.utf8
export LC_ALL=zh_CN.utf8
export DISPLAY=:99

CMD="${1:-start}"

case "$CMD" in
  start)
    # Kill existing
    pkill -x fcitx5 2>/dev/null
    pkill -x Xvfb 2>/dev/null
    pkill -x dbus-daemon 2>/dev/null
    sleep 1

    # Start Xvfb
    rm -f /tmp/.X11-unix/X99 /tmp/.X99-lock 2>/dev/null
    setsid Xvfb :99 -screen 0 1280x1024x24 > /tmp/xvfb.log 2>&1 &
    sleep 2

    # Start dbus
    eval $(dbus-launch --sh-syntax 2>/dev/null)
    echo "$DBUS_SESSION_BUS_ADDRESS" > /tmp/dbus_addr
    sleep 1

    # Start fcitx5
    setsid fcitx5 -d > /tmp/fcitx5.log 2>&1 &
    sleep 3

    echo "=== Services started ==="
    echo "Xvfb: $(pgrep -x Xvfb > /dev/null && echo '✅' || echo '❌')"
    echo "dbus: $(pgrep -x dbus-daemon > /dev/null && echo '✅' || echo '❌')"
    echo "fcitx5: $(pgrep -x fcitx5 > /dev/null && echo '✅' || echo '❌')"
    echo "=== fcitx5 log ==="
    cat /tmp/fcitx5.log
    ;;

  stop)
    pkill -x fcitx5 2>/dev/null
    pkill -x Xvfb 2>/dev/null
    pkill -x dbus-daemon 2>/dev/null
    echo "Services stopped"
    ;;

  status)
    echo "Xvfb: $(pgrep -x Xvfb > /dev/null && echo '✅' || echo '❌')"
    echo "dbus: $(pgrep -x dbus-daemon > /dev/null && echo '✅' || echo '❌')"
    echo "fcitx5: $(pgrep -x fcitx5 > /dev/null && echo '✅' || echo '❌')"
    ;;

  test)
    # Run CLI tests
    LM=$NB/opt/libime/lib/libime/zh_CN.lm
    DICT=$NB/opt/libime/share/libime/sc.dict
    CLI=$NB/cpp/build/pinyin_ime_cli

    echo "=== CLI Tests ==="
    pass=0; fail=0
    declare -A tests=(
      ["nihao"]="你好"
      ["zhongguo"]="中国"
      ["zg"]="中国"
      ["nihaozg"]="你好中国"
      ["woshizg"]="我是中国"
      ["beijingdaxue"]="北京大学"
      ["hello"]="hello"
      ["shurufa"]="输入法"
      ["woshizhongguoren"]="我是中国人"
      ["nihaozhongguo"]="你好中国"
    )
    for input in "${!tests[@]}"; do
      expected="${tests[$input]}"
      result=$(echo "$input" | $CLI "$LM" "$DICT" 2>/dev/null | head -1 | cut -f1)
      if [ "$result" = "$expected" ]; then
        echo "  ✅ $input → $result"
        ((pass++))
      else
        echo "  ❌ $input → $result (expected: $expected)"
        ((fail++))
      fi
    done
    echo "=== Results: $pass passed, $fail failed ==="
    ;;
esac
