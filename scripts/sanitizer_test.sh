#!/bin/bash
# sanitizer_test.sh — ASan/UBSan 检查（v0.9.192）
#
# 项目此前从未跑过内存/未定义行为检查：长句、异常输入、超大输入下的越界
# 只能靠运气。这里用 ASan+UBSan 编译单测并运行；--cli 额外跑 CLI 批处理。
#
# 注意: libime 是第三方动态库，leak 检测会报它的长期缓存，故 detect_leaks=0，
#       只关注越界/悬垂指针（ASan）与 UB（UBSan）。
#
# 用法:
#   bash scripts/sanitizer_test.sh          # 单测
#   bash scripts/sanitizer_test.sh --cli    # 单测 + CLI（需 libime）
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
CXX="${CXX:-g++}"
# 环境能力探测：ASan 的 LeakSanitizer 在「无 /proc 或进程被 ptrace」的沙箱里
# 会直接 fatal（chroot 就是这种环境），此时降级为 UBSan-only，并在输出里标明。
HAS_PROC=0
[ -r /proc/self/status ] && HAS_PROC=1
if [ "$HAS_PROC" = 1 ]; then
    SAN="-fsanitize=address,undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
else
    SAN="-fsanitize=undefined -fno-omit-frame-pointer -fno-sanitize-recover=undefined"
    echo "⚠ 当前环境无可用 /proc（LeakSanitizer 不可用），降级为 UBSan-only"
fi
export ASAN_OPTIONS="detect_leaks=0:abort_on_error=0"
export UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
FAIL=0

echo "=== 单测（ASan + UBSan）==="
if $CXX -std=c++20 -O1 -g $SAN -Icpp cpp/quick_input_test.cpp -o /tmp/san_qt >/tmp/san_build.log 2>&1; then
    if /tmp/san_qt >/tmp/san_run.log 2>&1; then
        echo "  ✅ $(tail -1 /tmp/san_run.log)"
    else
        echo "  ❌ 运行期报错"; tail -12 /tmp/san_run.log; FAIL=1
    fi
else
    echo "  ❌ 构建失败"; tail -5 /tmp/san_build.log; FAIL=1
fi

if [ "${1:-}" = "--cli" ]; then
    echo "=== CLI 批处理（ASan + UBSan）==="
    LIBIME_INC=""
    for d in /usr/include/LibIME /usr/local/include/LibIME; do [ -d "$d" ] && { LIBIME_INC="$d"; break; }; done
    if [ -z "$LIBIME_INC" ]; then
        echo "  ⚠ 缺 libime 头文件，跳过 CLI"; exit "$FAIL"
    fi
    work=/tmp/san_cli
    cmake -S cpp -B "$work" -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="$SAN -O1 -g" \
        >/tmp/san_cli_cfg.log 2>&1 || { echo "  ⚠ 构建配置失败，跳过"; exit "$FAIL"; }
    cmake --build "$work" -j"$(nproc)" --target pinyin_ime_cli >/tmp/san_cli_build.log 2>&1 \
        || { echo "  ❌ CLI 构建失败"; tail -5 /tmp/san_cli_build.log; FAIL=1; exit "$FAIL"; }
    : > /tmp/san_inputs.txt
    for f in bench/holdout/*.tsv; do cut -f1 "$f" | head -60 >> /tmp/san_inputs.txt; done
    cat bench/snapshots/cli_cases.txt >> /tmp/san_inputs.txt
    local_args=(--batch /tmp/san_inputs.txt --top1 --quiet --user-data /tmp/san_ud)
    [ -n "${LM:-}" ] && local_args+=("$LM")
    [ -n "${DICT:-}" ] && local_args+=("$DICT")
    if "$work/pinyin_ime_cli" "${local_args[@]}" >/dev/null 2>/tmp/san_cli_run.log; then
        echo "  ✅ CLI 批处理无 ASan/UBSan 报错"
    else
        echo "  ❌ CLI 运行报错"; tail -15 /tmp/san_cli_run.log; FAIL=1
    fi
fi

[ "$FAIL" -eq 0 ] && echo "sanitizer 检查通过 ✅" || echo "sanitizer 检查失败 ❌"
exit "$FAIL"
