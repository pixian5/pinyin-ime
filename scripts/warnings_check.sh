#!/bin/bash
# warnings_check.sh — 编译警告门禁（v0.9.192）
#
# 为什么：单元测试能过不代表代码干净。项目长期在 -Wall 之外构建，
# 隐式括号优先级、未使用参数、nodiscard 返回值被丢弃这类问题没人拦。
# 这里对三块代码做 -Wall -Wextra 检查，出现任何 warning 即失败。
#
# 用法:
#   bash scripts/warnings_check.sh          # 单测（不需要 libime）
#   bash scripts/warnings_check.sh --all    # 单测 + CLI + addon（需要 libime/fcitx5 头文件）
#
# 退出码: 0=无警告, 1=有警告或编译失败, 2=环境缺头文件（--all 时降级为只测单测）
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
CXX="${CXX:-g++}"
ALL=0
[ "${1:-}" = "--all" ] && ALL=1
FAIL=0
LOG=/tmp/warnings_check.log

compile_quiet() {   # $1=名字 $2...=编译命令
    local name="$1"; shift
    if ! "$@" >"$LOG" 2>&1; then
        echo "  ❌ $name 编译失败"; head -6 "$LOG" | sed 's/^/     /'; FAIL=1; return
    fi
    local w
    w=$(grep -c 'warning:' "$LOG" || true)
    if [ "$w" -gt 0 ]; then
        echo "  ❌ $name 有 $w 条警告"
        grep 'warning:' "$LOG" | sed 's/.*warning: /     /' | sort -u | head -8
        FAIL=1
    else
        echo "  ✅ $name 无警告"
    fi
}

echo "=== 单元测试源码（-Wall -Wextra）==="
compile_quiet "quick_input_test" "$CXX" -std=c++20 -O1 -Icpp -Wall -Wextra \
    -fsyntax-only cpp/quick_input_test.cpp

if [ "$ALL" = 1 ]; then
    LIBIME_INC=""
    for d in /usr/include/LibIME /usr/local/include/LibIME; do
        [ -d "$d" ] && { LIBIME_INC="$d"; break; }
    done
    FCITX_INC=""
    for d in /usr/include/Fcitx5/Utils /usr/local/include/Fcitx5/Utils; do
        [ -d "$d" ] && { FCITX_INC="-I$d"; break; }
    done
    if [ -z "$LIBIME_INC" ]; then
        echo "  ⚠ 找不到 libime 头文件，跳过 CLI/addon（请在装有 libime 开发包的机器上跑 --all）"
        exit "$FAIL"
    fi
    echo "=== CLI（-Wall -Wextra）==="
    INCS=(-Icpp -Icpp/build -I"$LIBIME_INC")
    [ -n "$FCITX_INC" ] && INCS+=("$FCITX_INC")
    [ -d /usr/include/Fcitx5/Config ] && INCS+=(-I/usr/include/Fcitx5/Config)
    [ -d /usr/include/Fcitx5/Core ] && INCS+=(-I/usr/include/Fcitx5/Core)
    compile_quiet "pinyin_ime_cli" "$CXX" -std=c++20 -O1 "${INCS[@]}" -Wall -Wextra \
        -fsyntax-only cpp/pinyin_ime_cli.cpp

    if [ -d /usr/include/Fcitx5/Config ]; then
        echo "=== addon（-Wall -Wextra）==="
        compile_quiet "fcitx5-addon" "$CXX" -std=c++20 -O1 -Icpp -Ifcitx5-addon \
            "${INCS[@]}" -I/usr/include -fPIC -Wall -Wextra \
            -fsyntax-only fcitx5-addon/pinyinime.cpp
    else
        echo "  ⚠ 缺 fcitx5 开发头文件，跳过 addon"
    fi
fi

[ "$FAIL" -eq 0 ] && echo "警告门禁通过 ✅" || echo "警告门禁失败 ❌"
exit "$FAIL"
