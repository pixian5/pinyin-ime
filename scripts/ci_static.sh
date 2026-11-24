#!/bin/bash
# ci_static.sh — 无图形环境的静态回归：语法、版本、打包配置、文档链接。
# 运行时 e2e 需要 Xvfb/DBus，见 scripts/ci_runtime.sh。
set -euo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"
fail=0

check() {
    local name="$1"; shift
    if "$@"; then
        printf '  ✅ %s\n' "$name"
    else
        printf '  ❌ %s\n' "$name"; fail=1
    fi
}

echo "=== shell 语法 ==="
for f in scripts/run_prebuilt.sh scripts/setup_sandbox_deps.sh \
         scripts/build_release.sh scripts/build_packages.sh scripts/ci_static.sh \
         packaging/runtime-start.sh packaging/runtime-stop.sh; do
    check "bash -n $f" bash -n "$f"
done

echo "=== 版本一致性 ==="
version="$(cat VERSION)"
check "VERSION 非空" test -n "$version"
check "addon CMake 读取 VERSION" grep -q 'file(STRINGS.*VERSION' fcitx5-addon/CMakeLists.txt
check "addon 模板 Library 正确" grep -qx 'Library=libpinyinime' fcitx5-addon/pinyinime.conf.in
check "inputmethod 存在" test -f fcitx5-addon/pinyinime.inputmethod

# 版本号唯一来源：源码里不允许再出现写死的 CLI 版本，CMake 不允许写死 project 版本
check "CLI 无写死版本号" bash -c "! grep -q 'pinyin_ime_cli 0\\.9\\.' cpp/pinyin_ime_cli.cpp"
check "CLI 使用生成的 PINYINIME_VERSION" grep -q 'PINYINIME_VERSION' cpp/pinyin_ime_cli.cpp
check "CMake 无写死 project 版本" bash -c "! grep -qE 'project\\(\\S+ VERSION 0\\.' fcitx5-addon/CMakeLists.txt"
check "CMake 生成了 version.h 模板" test -s cmake/version.h.in

# README 只允许一个「当前版本」标题（历史版本一律用「历史版本：」），且顶部版本与 VERSION 一致
cur_count=$(grep -c '^## 当前版本' README.md || true)
check "README 只有一个当前版本标题（当前 ${cur_count} 个）" test "$cur_count" -le 1
check "README 顶部版本与 VERSION 一致" grep -q "\`${version}\`" README.md
check "CHANGELOG 含当前版本" grep -q "v${version}" CHANGELOG.md

echo "=== 仓库卫生 ==="
elf_count=$(git ls-files | xargs -r file 2>/dev/null | grep -Ec 'ELF|PE32|Mach-O' || true)
check "无二进制入库（当前 ${elf_count} 个 ELF/PE/Mach-O）" test "$elf_count" -eq 0
check "ip.txt 未入库" bash -c '! git ls-files --error-unmatch ip.txt >/dev/null 2>&1'

echo "=== 打包配置 ==="
check "spec 包含 inputmethod" grep -q 'pinyinime.inputmethod' packaging/pinyin-ime.spec
check "spec 包含 LM" grep -q 'zh_CN.lm' packaging/pinyin-ime.spec
check "build_packages 校验 Library" grep -q 'Library=libpinyinime' scripts/build_packages.sh
check "build_packages 校验 Version" grep -q 'Version=\$VERSION' scripts/build_packages.sh

echo "=== 文档 ==="
check "README 存在" test -s README.md
check "CHANGELOG 存在" test -s CHANGELOG.md
check "文档索引存在" test -s 文档索引.md

echo "=== git 空白检查 ==="
check "git diff --check" git diff --check

if [ "$fail" != 0 ]; then
    echo "静态检查失败" >&2
    exit 1
fi
echo "静态检查全部通过"
