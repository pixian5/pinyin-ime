#!/usr/bin/env bash
# 虚拟桌面（Xvfb）实测 pinyinime addon：一键跑完整用例矩阵并断言。
#
# 覆盖：全拼 / 简拼 / 中英切换 / 中文标点 / 引号配对 / Ctrl-E 表情（含翻页）/ 打拼音中先上屏再标点。
# 用法：bash scripts/xvfb_ime_test.sh
# 环境：NB（native-build 根），需要已构建 fcitx5-x11 + pinyinime addon + tools/xim_test_client。
set -uo pipefail
NB="${NB:-/root/workspace/native-build}"
cd "$(dirname "${BASH_SOURCE[0]}")/.."

bash scripts/xvfb_test_env.sh start > /dev/null 2>&1

export DISPLAY=:99 XMODIFIERS=@im=fcitx LANG=zh_CN.utf8 LC_ALL=zh_CN.utf8
export LD_LIBRARY_PATH="$NB/opt/fcitx5-x11/lib:$NB/opt/libime/lib:$NB/mamba-env/lib"

pass=0
fail=0
check() {
    local desc="$1" script="$2" want="$3"
    local out
    out=$(./tools/xim_test_client "$script" 2>/dev/null | grep "^COMMIT:" | tr '\n' ' ')
    if printf '%s' "$out" | grep -qF "$want"; then
        echo "  PASS  $desc  -> $out"
        pass=$((pass + 1))
    else
        echo "  FAIL  $desc  -> $out (期望含 $want)"
        fail=$((fail + 1))
    fi
}

echo "== pinyinime addon · Xvfb 虚拟桌面实测 =="
check "全拼 nihao"           '\Snihao\s'        "你好"
check "全拼 zhongguo"        '\Szhongguo\s'     "中国"
check "简拼 zg"              '\Szg\s'           "中国"
check "简拼 bjdx"            '\Sbjdx\s'         "北京大学"
check "全拼 woshiyige"       '\Swoshiyige\s'    "我是一个"
check "中英切换 Tab+hello"   '\S\thello\n'      "hello"
check "中文标点 , . ?"       '\S,.?'            "？"
check "引号配对 “ ”"         '\S""'             "”"
check "Ctrl-E 表情 😀"        '\S\x05\1'         "😀"
check "Ctrl-E 翻页 👍"        '\S\x05.\1'        "👍"
check "打拼音中先上屏再标点"  '\Szhongguo!'      "中国！"

echo "==== 结果：$pass 通过 / $fail 失败 ===="
[ "$fail" -eq 0 ]