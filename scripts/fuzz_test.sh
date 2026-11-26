#!/bin/bash
# fuzz_test.sh — 拼音管线变异模糊测试（v0.9.193）
#
# 编译 cpp/fuzz_segment_test.cpp 并运行；优先带 ASan+UBSan，
# 无 /proc 的沙箱（chroot）自动降级 UBSan。
#
# 用法: bash scripts/fuzz_test.sh [迭代次数，默认 2000]
#       FUZZ_SEED=12345 bash scripts/fuzz_test.sh  # 换种子复现
set -uo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
CXX="${CXX:-g++}"
ITER="${1:-2000}"
HAS_PROC=0; [ -r /proc/self/status ] && HAS_PROC=1
if [ "$HAS_PROC" = 1 ]; then SAN="-fsanitize=address,undefined"; else SAN="-fsanitize=undefined"; fi
SAN="$SAN -fno-omit-frame-pointer -fno-sanitize-recover=undefined -O1 -g"
export ASAN_OPTIONS="detect_leaks=0" UBSAN_OPTIONS="print_stacktrace=1:halt_on_error=1"
out=/tmp/fuzz_segment_test
$CXX -std=c++20 $SAN -Icpp cpp/fuzz_segment_test.cpp -o "$out" || { echo "❌ 编译失败"; exit 1; }
"$out" "$ITER"
