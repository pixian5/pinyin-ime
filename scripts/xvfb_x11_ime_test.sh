#!/usr/bin/env bash
# xvfb_x11_ime_test.sh —— 在「只读 /usr」沙箱内，用 Xvfb + XIM 真实验证 pinyinime。
#
# 与 xvfb_ime_test.sh（依赖 conda/native-build 的 Xvfb）不同：本脚本用
#   · sysroot 里的 Xvfb（openEuler RPM 解包，不写 /usr）
#   · LD_PRELOAD 垫片把 /usr/bin/xkbcomp 重定向到 sysroot（见 tools/xkbcomp_redirect.c）
#   · 桌面 bundle 的 fcitx5（X11） + pinyinime addon + libxcb-imdkit（xim 前端）
# 全程不写 /usr、不依赖 conda/docker。
#
# 重要：本沙箱每次 tool 调用运行在独立 bwrap/PID 命名空间，跨调用无法 kill 旧守护
# 进程。因此本脚本在【同一次调用】内选一个空闲 display、启动、测试、收尾。
#
# 用法：bash scripts/xvfb_x11_ime_test.sh
set -uo pipefail
REPO_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
SYSROOT="${SYSROOT:-/workspace/sysroot}"
cd "$REPO_DIR"

# 选一个「没有任何 Xvfb 进程占用」的随机 display。
# 说明：本沙箱每次调用是独立 bwrap/PID 命名空间，跨调用无法 kill 旧 Xvfb/fcitx5；
# 若复用旧 display，残留的 fcitx5 会抢占 XIM，导致 XOpenIM 连到死实例而无上屏。
pick_display() {
    local i n
    for i in $(seq 1 30); do
        n=$(( (RANDOM % 800) + 150 ))
        if ! pgrep -f "[X]vfb :$n" >/dev/null 2>&1; then
            rm -f "/tmp/.X${n}-lock" "/tmp/.X11-unix/X${n}" 2>/dev/null
            XDISPLAY=":$n"; return 0
        fi
    done
    XDISPLAY=":99"; return 0
}
pick_display
export XDISPLAY DISPLAY="$XDISPLAY" XMODIFIERS=@im=fcitx LANG=zh_CN.utf8 LC_ALL=zh_CN.utf8
export LD_LIBRARY_PATH="$SYSROOT/usr/lib64"
echo "[xtest] 使用 DISPLAY=$XDISPLAY"
# 结束清理本次调用启动的实例（同命名空间内可 kill）
trap 'XDISPLAY="$XDISPLAY" bash scripts/start_im_env_x.sh stop >/dev/null 2>&1' EXIT

# 构建 XIM 客户端（源码入库、二进制不入库）
if [ ! -x tools/xim_test_client ]; then
    echo "[xtest] 编译 tools/xim_test_client …"
    gcc -O2 -I"$SYSROOT/usr/include" -o tools/xim_test_client tools/xim_test_client.c \
        -L"$SYSROOT/usr/lib64" -lX11 -lXtst -lXext
fi

# 拉起 X 模式环境（Xvfb + DBus + fcitx5 XIM）
XDISPLAY="$XDISPLAY" bash scripts/start_im_env_x.sh start >/tmp/xvfb_x11_env.log 2>&1
if ! grep -q "xim + pinyinime 已加载" /tmp/xvfb_x11_env.log; then
    echo "❌ 环境未就绪，见 /tmp/xvfb_x11_env.log"; tail -25 /tmp/xvfb_x11_env.log; exit 1
fi

pass=0; fail=0
check() {
    local desc="$1" script="$2" want="$3" out
    out=$(timeout 40 ./tools/xim_test_client "$script" 2>/dev/null | grep '^COMMIT' | tr '\n' ' ')
    if printf '%s' "$out" | grep -qF "$want"; then
        echo "  PASS  $desc  -> $out"; pass=$((pass+1))
    else
        echo "  FAIL  $desc  -> $out (期望含 $want)"; fail=$((fail+1))
    fi
}
# 信息项：只打印结果，不计通过/失败
info() {
    local desc="$1" script="$2" out
    out=$(timeout 40 ./tools/xim_test_client "$script" 2>/dev/null | grep '^COMMIT' | tr '\n' ' ')
    echo "  INFO  $desc  -> $out"
}

echo "== pinyinime · Xvfb + XIM 真实桌面实测（sysroot + LD_PRELOAD xkbcomp）=="
check "全拼 nihao"        '\Snihao\s'     "你好"
check "全拼 zhongguo"     '\Szhongguo\s'  "中国"
check "简拼 zg"           '\Szg\s'        "中国"
check "简拼 bjdx"         '\Sbjdx\s'      "北京大学"
check "全拼 woshiyige"    '\Swoshiyige\s' "我是一个"
check "中英切换 Tab+hello" '\S\thello\n'  "hello"
check "中文标点（全角问号）" '\S,.?'        "？"
check "打拼音中先上屏再标点" '\Szhongguo!'  "中国！"
check "纠错 zongguo→中国"  '\Szongguo\s'   "中国"

# 信息项（不计入通过/失败）：S3 预构建 addon 为 v0.9.84a，其 EnableCorrect 的
# should_correct 门控较当前源码（v0.9.93，含「重错全拼扩展」）保守，故 senme
# （sen/me 均为合法音节）不会整串纠正为「什么」，而是直接上屏「森么」。
# CLI（同一引擎）`echo senme | pinyin-ime --correct` 可正确得「什么」。
# 完整 M3 纠错对齐需用当前源码重编 addon（缺匹配 libime 1.1.17 头，列为待办）。
info "senme 纠错（addon 版本相关）" '\Ssenme\s'

echo "==== 结果：$pass 通过 / $fail 失败 ===="
[ "$fail" -eq 0 ]
