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
# 额外文本词典入口（v0.9.181）：选项、帮助、加载点都要在
check "CLI 提供 --extra-dict" grep -q '"--extra-dict"' cpp/pinyin_ime_cli.cpp
check "CLI 帮助列出 --extra-dict" grep -q 'extra-dict F' cpp/pinyin_ime_cli.cpp
check "extra-dict 加载进 UserDict" grep -q 'PinyinDictionary::UserDict, extraDictFile' cpp/pinyin_ime_cli.cpp
check "CMake 无写死 project 版本" bash -c "! grep -qE 'project\\(\\S+ VERSION 0\\.' fcitx5-addon/CMakeLists.txt"
check "CMake 生成了 version.h 模板" test -s cmake/version.h.in

# README 只允许一个「当前版本」标题（历史版本一律用「历史版本：」），且顶部版本与 VERSION 一致
cur_count=$(grep -c '^## 当前版本' README.md || true)
check "README 只有一个当前版本标题（当前 ${cur_count} 个）" test "$cur_count" -le 1
check "README 顶部版本与 VERSION 一致" grep -q "\`${version}\`" README.md
check "CHANGELOG 含当前版本" grep -q "v${version}" CHANGELOG.md

echo "=== quick_input.h 拆分 ==="
qi_lines=$(wc -l < cpp/quick_input.h)
check "quick_input.h <= 400 行（当前 ${qi_lines}）" test "$qi_lines" -le 400
frag_count=$(ls cpp/quick_input_*.h | wc -l)
check "拆分片段 >= 6 个（当前 ${frag_count}）" test "$frag_count" -ge 6
check "片段不含 namespace 声明（防嵌套成 pinyin_ime::pinyin_ime）" bash -c "! grep -qE '^[[:space:]]*namespace[[:space:]]+pinyin_ime' cpp/quick_input_*.h"
for f in cpp/quick_input_*.h; do
    b=$(basename "$f")
    check "quick_input.h 包含 ${b}" grep -q "\"${b}\"" cpp/quick_input.h
done
check "CLI 不直接 include 片段" bash -c "! grep -qE '#include \"quick_input_' cpp/pinyin_ime_cli.cpp"
check "addon 不直接 include 片段" bash -c "! grep -qE '#include \"quick_input_' fcitx5-addon/pinyinime.cpp"

echo "=== 仓库卫生 ==="
elf_count=$(git ls-files | xargs -r file 2>/dev/null | grep -Ec 'ELF|PE32|Mach-O' || true)
check "无二进制入库（当前 ${elf_count} 个 ELF/PE/Mach-O）" test "$elf_count" -eq 0
check "ip.txt 未入库" bash -c '! git ls-files --error-unmatch ip.txt >/dev/null 2>&1'

echo "=== 打包配置 ==="
check "spec 包含 inputmethod" grep -q 'pinyinime.inputmethod' packaging/pinyin-ime.spec
check "spec 包含 LM" grep -q 'zh_CN.lm' packaging/pinyin-ime.spec
check "build_packages 校验 Library" grep -q 'Library=libpinyinime' scripts/build_packages.sh
check "build_packages 校验 Version" grep -q 'Version=\$VERSION' scripts/build_packages.sh

echo "=== 发布工程守卫（v0.9.187）==="
# 架构必须由 uname -m 推导，spec 走 @ARCH@ 占位符：写死 aarch64 会让 x86_64
# 机器上 rpmbuild 直接报 "No compatible architectures found for build"，
# deb 也会把 addon 装进 aarch64-linux-gnu 目录而 fcitx5 找不到。
check "build_packages 用 uname -m 推导架构" grep -q 'uname -m' scripts/build_packages.sh
check "build_packages 无写死 aarch64" bash -c '! grep -q "ARCH_RPM=aarch64" scripts/build_packages.sh'
check "spec 架构用 @ARCH@ 占位" grep -q 'BuildArch:      @ARCH@' packaging/pinyin-ime.spec
check "deb addon 目录可覆盖（FCITX5_LIBDIR）" grep -q 'FCITX5_LIBDIR' scripts/build_packages.sh

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
