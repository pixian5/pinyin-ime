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
check "spec 说明数据由 libime-data 提供" grep -q 'libime-data 提供' packaging/pinyin-ime.spec
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
check "打包产出 SHA256SUMS_v\$VERSION" grep -q 'SHA256SUMS_v$VERSION' scripts/build_packages.sh
check "build_packages 支持 runtime 模式" grep -q 'build_runtime' scripts/build_packages.sh
check "build_release 标为历史脚本" grep -q '历史脚本' scripts/build_release.sh

echo "=== 数据文件归属守卫（v0.9.190）==="
# rpm/deb 自带 sc.dict 会与发行版 libime-data 撞文件，dpkg -i 直接被拒；
# 因此只有 runtime 允许自带数据，rpm/deb 必须走 Requires/Depends: libime-data。
check "spec 依赖 libime-data" grep -q '^Requires:       libime-data' packaging/pinyin-ime.spec
check "spec 不再打包 zh_CN.lm" bash -c '! grep -q "^/usr/share/libime/zh_CN.lm" packaging/pinyin-ime.spec'
check "spec 不再打包 sc.dict" bash -c '! grep -q "^/usr/share/libime/sc.dict" packaging/pinyin-ime.spec'
check "deb 依赖 libime-data" grep -q 'fcitx5, libime, libime-data' scripts/build_packages.sh
check "stage_rootfs 支持不带数据" grep -q 'with_data' scripts/build_packages.sh
check "runtime 包生成 uninstall.sh" grep -q 'uninstall.sh' scripts/build_packages.sh
check "打包前校验 CLI 版本与 VERSION 一致" grep -q 'validate_cli_version' scripts/build_packages.sh

echo "=== 测试基础设施守卫（v0.9.191）==="
check "安装矩阵脚本存在" test -x scripts/test_install.sh
check "发布门禁脚本存在" test -x scripts/release_check.sh
check "CLI 快照脚本存在" test -x scripts/cli_snapshot_test.sh
check "快照用例非空" test -s bench/snapshots/cli_cases.txt
check "快照期望含数据指纹" grep -q '^# lm_md5=' bench/snapshots/cli_expected.tsv
check "快照期望含当前版本" grep -q "^# cli snapshot v$version" bench/snapshots/cli_expected.tsv
check "警告门禁脚本存在" test -x scripts/warnings_check.sh
check "覆盖率脚本存在" test -x scripts/coverage.sh
check "Sanitizer 脚本存在" test -x scripts/sanitizer_test.sh
check "CLI 功能回归脚本存在" test -x scripts/cli_features_test.sh
check "模糊测试脚本存在" test -x scripts/fuzz_test.sh
check "模糊测试源存在" test -s cpp/fuzz_segment_test.cpp
# v0.9.194：运行时冒烟 + 升级矩阵
check "fcitx5 运行时冒烟脚本存在" test -x scripts/test_runtime_fcitx5.sh
check "运行时脚本无 fcitx5 时自动 SKIP" grep -q '[[]SKIP[]] 未安装 fcitx5' scripts/test_runtime_fcitx5.sh
check "升级矩阵脚本存在" test -x scripts/test_upgrade_matrix.sh
check "升级矩阵断言数据指纹" grep -q 'data_md5' scripts/test_upgrade_matrix.sh
check "升级矩阵有退出清理（回到当前版）" grep -q 'trap cleanup EXIT' scripts/test_upgrade_matrix.sh
check "发布门禁接入运行时/升级矩阵" grep -q 'test_runtime_fcitx5.sh' scripts/release_check.sh
# runtime 的 install.sh/uninstall.sh 在 headless 里无 DISPLAY 时调 fcitx5 -r 会挂住（v0.9.194 实测）
check "runtime install/uninstall 的 fcitx5 -r 都有显示环境守卫" bash -c '[ "$(grep -c "WAYLAND_DISPLAY" scripts/build_packages.sh)" -ge 2 ]'
# v0.9.194 追加：真实上屏、rpm 矩阵、CI 真实构建
check "运行时脚本含 GTK 真实上屏断言" grep -q '真实按键上屏' scripts/test_runtime_fcitx5.sh
check "升级矩阵含 rpm 降级路径" grep -q 'rpm -Uvh --oldpackage' scripts/test_upgrade_matrix.sh
check "升级矩阵断言 RPM 数据不被改动" grep -q 'RPM 降级未改动 libime 数据' scripts/test_upgrade_matrix.sh
check "本地 CI 构建脚本存在且可执行" test -x scripts/ci_build.sh
check "本地 CI 构建脚本包含真实编译+评测" grep -q 'bench/eval.py' scripts/ci_build.sh
check "不再保留无法运行的 AtomGit workflow" bash -c '! test -e .gitcode/workflows/ci.yml'
# 覆盖率调试产物绝不能入库（v0.9.192 曾误提交 71 个 .gcov）
check "无 .gcov 入库" bash -c '! git ls-files | grep -q "\.gcov$"'
check ".gitignore 忽略 gcov 产物" grep -q '^\*\.gcov$' .gitignore

echo "=== 文档 ==="
check "README 存在" test -s README.md
check "CHANGELOG 存在" test -s CHANGELOG.md
check "文档索引存在" test -s 文档索引.md

echo "=== git 空白检查 ==="
# chroot/发布源码包可能没有 .git（rsync --exclude .git），此时 git diff 直接 fatal；
# 只在该目录确实是 git 仓库时检查空白。
if git rev-parse --git-dir >/dev/null 2>&1; then
    check "git diff --check" git diff --check
else
    echo "  [SKIP] 当前目录不是 git 仓库，跳过 git diff --check"
fi

if [ "$fail" != 0 ]; then
    echo "静态检查失败" >&2
    exit 1
fi
echo "静态检查全部通过"
