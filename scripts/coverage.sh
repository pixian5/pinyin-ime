#!/bin/bash
# coverage.sh — 行覆盖率报告（v0.9.192）
#
# 单位：项目自身的行（cpp/*.h、cpp/*.cpp），不含标准库/系统头。
# 两条腿：
#   --unit  纯函数单测覆盖率（不需要 libime），门槛 90%；
#   --cli   CLI 批量路径覆盖率（需要 libime，构建带 --coverage），只做报告，
#           因为 CLI 大量代码是交互式菜单/向导，批处理永远碰不到。
#
# 用法:
#   bash scripts/coverage.sh --unit
#   bash scripts/coverage.sh --cli        # 需 LM/DICT 与 libime
#   bash scripts/coverage.sh              # 两条腿都跑（缺 libime 时自动只跑 unit）
#
# 输出: bench/results/coverage/v$VERSION/summary.txt（不入库）
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
VERSION=$(cat VERSION)
OUTDIR="$ROOT/bench/results/coverage/v$VERSION"
mkdir -p "$OUTDIR"
MODE="${1:-all}"

SUMMARY="$OUTDIR/summary.txt"
: > "$SUMMARY"
echo "# coverage v$VERSION ($(date -u +%FT%TZ))" >> "$SUMMARY"

# 解析 gcov 文本：只统计 Source:cpp/* 的行
parse_gcov() {  # $1=目录（*.gcov 所在） $2=报告标签 $3=门槛百分比（0=不设门槛）
    python3 - "$1" "$2" "$3" <<'PY' >> "$SUMMARY"
import glob, os, sys
d, label, threshold = sys.argv[1], sys.argv[2], float(sys.argv[3])
files = {}
for path in glob.glob(os.path.join(d, '*.gcov')):
    src = None
    hit = tot = 0
    for line in open(path, encoding='utf-8', errors='ignore'):
        if line.startswith('        -:    0:Source:'):
            src = line.split('Source:', 1)[1].strip()
            continue
        parts = line.split(':', 2)
        if len(parts) != 3 or src is None:
            continue
        c = parts[0].strip()
        if c in ('-', '=====') or not c:
            continue
        # gcov 对「从未执行」的行用的是 #####（不是 0），必须计入分母
        if c.startswith('#####'):
            tot += 1
            continue
        try:
            n = int(c.split('*')[0])
        except ValueError:
            continue
        tot += 1
        if n > 0:
            hit += 1
    # CMake 构建时源码可能记录成绝对路径（/src/cpp/...），统一归一化为 cpp/...
    if src and '/cpp/' in src:
        src = 'cpp/' + src.split('/cpp/', 1)[1]
    if src and src.startswith('cpp/') and tot:
        files[src] = (hit, tot)
total_hit = sum(v[0] for v in files.values())
total_tot = sum(v[1] for v in files.values())
pct = (total_hit / total_tot * 100) if total_tot else 0
print(f"[{label}] 总行 {total_tot}，已执行 {total_hit}，覆盖率 {pct:.1f}%")
for k in sorted(files, key=lambda x: -files[x][1])[:12]:
    h, t = files[k]
    print(f"  {k:34s} {h:5d}/{t:5d} {h/t*100:5.1f}%")
if threshold > 0:
    print(f"[{label}] 门槛 {threshold:.0f}% -> " + ("通过" if pct >= threshold else "未达标"))
    sys.exit(0 if pct >= threshold else 1)
PY
}

FAIL=0
run_unit() {
    local work=/tmp/cov_unit
    rm -rf "$work"; mkdir -p "$work"
    echo "=== 单测覆盖率 ==="
    g++ -std=c++20 -O0 --coverage -Icpp cpp/quick_input_test.cpp -o "$work/qt" >/tmp/cov_build.log 2>&1 \
        || { echo "❌ 单测构建失败"; tail -5 /tmp/cov_build.log; FAIL=1; return; }
    "$work/qt" >/tmp/cov_run.log 2>&1 || { echo "❌ 单测运行失败"; FAIL=1; return; }
    # gcov 需要能按源码路径（cpp/...）打开源文件，否则只写 4 行头信息、统计为 0
    ln -sfn "$ROOT/cpp" "$work/cpp"
    ( cd "$work" && gcov -o . qt-quick_input_test.gcda >/tmp/cov_gcov.log 2>&1 ) || true
    parse_gcov "$work" "unit" 90 || FAIL=1
}

run_cli() {
    local work=/tmp/cov_cli
    echo "=== CLI 批量路径覆盖率 ==="
    LM="${LM:-}"
    DICT="${DICT:-}"
    if [ ! -f "${LM:-/usr/share/libime/zh_CN.lm}" ] && [ -z "${LM:-}" ]; then
        echo "⚠ 找不到系统 LM，跳过 CLI 覆盖率"; return
    fi
    rm -rf "$work"
    cmake -S cpp -B "$work" -DCMAKE_BUILD_TYPE=Debug \
        -DCMAKE_CXX_FLAGS="--coverage -O0 -g" >/tmp/cov_cli_cfg.log 2>&1 \
        || { echo "⚠ CLI 覆盖率构建配置失败（可能缺 libime），跳过"; tail -3 /tmp/cov_cli_cfg.log; return; }
    cmake --build "$work" -j"$(nproc)" --target pinyin_ime_cli >/tmp/cov_cli_build.log 2>&1 \
        || { echo "⚠ CLI 覆盖率构建失败，跳过"; tail -5 /tmp/cov_cli_build.log; return; }
    local inputs=/tmp/cov_cli_inputs.txt
    cat bench/snapshots/cli_cases.txt > "$inputs"
    for f in bench/holdout/*.tsv; do cut -f1 "$f" | head -50; done >> "$inputs"
    local args=(--batch "$inputs" --top1 --quiet --user-data /tmp/cov_cli_ud)
    [ -n "$LM" ] && args+=("$LM")
    [ -n "$DICT" ] && args+=("$DICT")
    "$work/pinyin_ime_cli" "${args[@]}" >/dev/null 2>/tmp/cov_cli_run.log \
        || { echo "⚠ CLI 运行失败，跳过"; tail -3 /tmp/cov_cli_run.log; return; }
    ln -sfn "$ROOT/cpp" "$work/cpp"
    ( cd "$work" && gcov -o CMakeFiles/pinyin_ime_cli.dir \
        CMakeFiles/pinyin_ime_cli.dir/pinyin_ime_cli.cpp.gcda >/tmp/cov_cli_gcov.log 2>&1 ) || true
    parse_gcov "$work" "cli" 0 || true
}

case "$MODE" in
    --unit) run_unit ;;
    --cli)  run_cli ;;
    all)    run_unit; run_cli ;;
    *)      echo "用法: bash scripts/coverage.sh [--unit|--cli]"; exit 2 ;;
esac

echo "报告: $SUMMARY"
exit "$FAIL"
