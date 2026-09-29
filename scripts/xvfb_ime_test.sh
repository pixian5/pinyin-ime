#!/usr/bin/env bash
# 虚拟桌面（Xvfb）实测 pinyinime addon：一键跑完整用例矩阵并断言。
#
# 覆盖：全拼 / 简拼 / 中英切换 / 中文标点 / 引号配对 / Ctrl-E 表情（含翻页）
#       打拼音中先上屏再标点 / M3 纠错（EnableCorrect）/ 简拼不误伤。
# 用法：bash scripts/xvfb_ime_test.sh
# 环境：NB（native-build 根），需要已构建 fcitx5-x11 + pinyinime addon + tools/xim_test_client。
set -uo pipefail
NB="${NB:-/root/workspace/native-build}"
cd "$(dirname "${BASH_SOURCE[0]}")/.."

# 用户配置：指向编译产物数据路径，并开启 M3 纠错（实测代理 eng 态下验证）。
mkdir -p ~/.config/fcitx5/conf
cat > ~/.config/fcitx5/conf/pinyinime.conf <<EOF
DataDir=$NB/opt/libime/lib
UserDataDir=/tmp/pinyin-ime-x11-ud
EnableCorrect=True
EOF

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
# 误上屏撤回：上屏「你好」→ Ctrl-Z（恢复拼音）→ 空格重新上屏「你好」。
# 断言用正则（中间会有 forwardKey 退格产生的空 COMMIT，固定串匹配不过）。
out=$(./tools/xim_test_client '\Snihao\s\x1a\s' 2>/dev/null | grep "^COMMIT:" | tr '\n' ' ')
if printf '%s' "$out" | grep -qE '你好.*你好'; then
    echo "  PASS  误上屏撤回 Ctrl-Z  -> $out"
    pass=$((pass + 1))
else
    echo "  FAIL  误上屏撤回 Ctrl-Z  -> $out (期望两次「你好」)"
    fail=$((fail + 1))
fi
check "M3 纠错 senme→什么"    '\Ssenme\s'        "什么"
check "纠错边界 zhing→中"    '\Szhing\s'        "中"
check "M3 纠错 zongguo→中国"  '\Szongguo\s'      "中国"
check "纠错不误伤简拼 zg"     '\Szg\s'           "中国"

# ---- 候选翻页：需干净用户模型（同一会话内 learn() 会污染候选排序）----
# 先重启 fcitx5 + 清空用户数据；每个翻页用例用不同拼音，避免互相学习污染。
pkill -9 -x fcitx5 2>/dev/null   # SIGKILL：避免旧进程在 SIGTERM 退出回调里重新落盘用户数据
sleep 1
rm -rf /tmp/pinyin-ime-x11-ud ~/.local/share/pinyin-ime   # 清隔离目录 + 防御性清默认目录（历史污染教训）
bash scripts/xvfb_test_env.sh start > /dev/null 2>&1
# 候选翻页：第 12 轮确认根因是「默认用户目录污染」而非竞态；隔离用户数据后页 2 首字完全确定
# （yi 页2首=亿、ji 页2首=其）。每个用例用不同拼音，避免同会话 learn() 互相污染。
check "= 翻页选字 (yi)"        '\Syi=\1'    "亿"
check "PageDown 翻页选字 (ji)" '\Sji\d\1'   "其"
check "= 翻页后 - 翻回 (de)"   '\Sde=-\1'    "的"

echo "==== 结果：$pass 通过 / $fail 失败 ===="
[ "$fail" -eq 0 ]