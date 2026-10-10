#!/usr/bin/env bash
# 真桌面端到端实测：DBus + fcitx5-gtk im module + 真实 GTK3 应用（GtkEntry）。
#
# 与 scripts/xvfb_ime_test.sh（走 XIM 协议）不同，本脚本走「真实桌面应用」的输入链路：
#   GTK3 应用 --GTK_IM_MODULE=fcitx--> im-fcitx5.so --DBus--> fcitx5 守护进程 --pinyinime addon-->
#   引擎解码上屏。是用浏览器/编辑器打字的同一条路（XIM 之外的主流通道）。
#
# 覆盖：全拼 / 简拼 / 整句 / M3 纠错 / 中英切换 / 中文标点 / 打拼音中先上屏再标点。
# 用法：bash scripts/xvfb_desktop_ime_test.sh
# 环境：NB，需已构建 fcitx5-x11（Dbus=On）+ fcitx5-gtk im module（含 immodules.cache）+ addon。
set -uo pipefail
NB="${NB:-/root/workspace/native-build}"
cd "$(dirname "${BASH_SOURCE[0]}")/.."

FCITX_BIN="$NB/opt/fcitx5-x11/bin/fcitx5"
export DISPLAY=:99 XMODIFIERS=@im=fcitx LANG=zh_CN.utf8 LC_ALL=zh_CN.utf8
export LD_LIBRARY_PATH="$NB/opt/fcitx5-x11/lib:$NB/opt/libime/lib"
export FCITX_ADDON_DIRS="$NB/opt/fcitx5-x11/lib/fcitx5"
export FCITX_ADDON_CONFIG_DIRS="$NB/opt/fcitx5-x11/share/fcitx5/addon"
export FCITX_DATA_DIR="$NB/opt/fcitx5-x11/share/fcitx5"
# GTK3 靠 immodules.cache 发现 im module；真实机器写在 /usr/lib64/gtk-3.0/...
# 只读 /usr 沙箱里缓存落在前缀内，用 GTK_IM_MODULE_FILE 指过去（文件不存在时此变量无害）。
IMCACHE="$NB/opt/fcitx5-x11/lib/gtk-3.0/3.0.0/immodules.cache"
[ -f "$IMCACHE" ] && export GTK_IM_MODULE_FILE="$IMCACHE"

# ---------- 用户配置（数据路径 + 隔离用户目录 + 开纠错）----------
mkdir -p ~/.config/fcitx5/conf
cat > ~/.config/fcitx5/conf/pinyinime.conf <<EOF
DataDir=$NB/opt/libime/lib
UserDataDir=/tmp/pinyin-ime-desk-ud
EnableCorrect=True
EOF

# ---------- 环境：Xvfb + 独立 DBus 会话 + fcitx5（DBus）----------
if ! pgrep -x Xvfb > /dev/null 2>&1; then
    rm -f /tmp/.X11-unix/X99 /tmp/.X99-lock
    setsid Xvfb :99 -screen 0 1280x800x24 -nolisten tcp >/tmp/xvfb.log 2>&1 < /dev/null &
    sleep 2
fi
# 独立 DBus 会话（避免与 XIM 测试的 dbus 串线）
dbus-daemon --session --fork --print-address=1 --print-pid=1 > /tmp/desk-dbus-info 2>/dev/null
export DBUS_SESSION_BUS_ADDRESS="$(grep -m1 '^unix:' /tmp/desk-dbus-info)"

pkill -9 -x fcitx5 2>/dev/null
sleep 1
rm -rf /tmp/pinyin-ime-desk-ud ~/.local/share/pinyin-ime   # 隔离 + 防御性清默认目录（历史污染教训）
nohup "$FCITX_BIN" > /tmp/desk-fcitx5.log 2>&1 &
sleep 5

# 校验：DBus 前端与 dbus addon 都加载、总线名在
if ! grep -q "Loaded addon dbusfrontend" /tmp/desk-fcitx5.log; then
    echo "!! fcitx5 未加载 dbusfrontend，退出"; tail -8 /tmp/desk-fcitx5.log; exit 1
fi

pass=0; fail=0
# run_case <desc> <按键脚本> <期望子串>
run_case() {
    local desc="$1" script="$2" want="$3" out wid
    : > /tmp/desk-client.log
    GTK_IM_MODULE=fcitx ./tools/gtk_ime_test_client 12 > /tmp/desk-client.log 2>&1 &
    local gpid=$!
    local i
    for i in $(seq 1 40); do grep -q WINDOW_ID /tmp/desk-client.log && break; sleep 0.25; done
    wid=$(grep -oE 'WINDOW_ID=0x[0-9a-f]+' /tmp/desk-client.log | cut -d= -f2)
    ./tools/keysend_target "$wid" "$script" > /dev/null 2>&1
    sleep 2
    out=$(grep '^GTK_COMMIT:' /tmp/desk-client.log | sed 's/^GTK_COMMIT://' | tr -d '\n' | tr ' ' '_')
    kill "$gpid" 2>/dev/null
    if grep '^GTK_COMMIT:' /tmp/desk-client.log | grep -qF "$want"; then
        echo "  PASS  $desc  -> $(grep '^GTK_COMMIT:' /tmp/desk-client.log | tr '\n' ' ')"
        pass=$((pass + 1))
    else
        echo "  FAIL  $desc  -> [$out] (期望含 $want)"
        fail=$((fail + 1))
    fi
}

echo "== pinyinime addon · 真桌面（DBus + GTK3 im module）端到端实测 =="
run_case "全拼 nihao"       '\Snihao\s'      "你好"
run_case "全拼 zhongguo"    '\Szhongguo\s'   "中国"
run_case "简拼 zg"          '\Szg\s'         "中国"
run_case "简拼 bjdx"        '\Sbjdx\s'       "北京大学"
run_case "整句 woshiyige"   '\Swoshiyige\s'  "我是一个"
run_case "M3 纠错 senme"    '\Ssenme\s'      "什么"
run_case "M3 纠错 zongguo"  '\Szongguo\s'    "中国"
run_case "中英切换 Tab"     '\S\thello\n'    "hello"
run_case "中文标点 ,"       '\S,'            "，"
run_case "先上屏再标点"     '\Szhongguo!'    "中国"

echo "==== 结果：$pass 通过 / $fail 失败 ===="
[ "$fail" -eq 0 ]