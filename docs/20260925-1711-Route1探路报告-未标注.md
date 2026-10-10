# Route 1 探路报告：本机原生构建 libime（2026-09-25）

**结论：探路成功，可以继续。** fcitx5 5.1.23 + libime 1.1.17 已在本机（Debian 12,
x86_64, 无 docker）源码编译通过，基线评测复现成功。

## 环境

| 项 | 值 |
|---|---|
| OS | Debian 12 (bookworm), x86_64 |
| CPU / 内存 | 2 核 / 3.7 GB（**无 swap**） |
| 磁盘 | 27 GB 可用 |
| docker | 不可用（用户要求不使用） |
| 系统 gcc | 12.2.0（**不足以编 fcitx5 5.1.23**） |
| 工具链 | conda-forge gcc 13.4.0（micromamba 装在 `/workspace/native-build/mamba-env`） |

## 摸过的 6 个坑（对应 build_conda_forge.sh 的前置检查）

1. **GCC 版本**：fcitx5 5.1.23 源码 `#include <format>`，需 GCC 13+；Debian 12 只有
   gcc-12，apt 无 gcc-13 → **必须用 conda-forge 的 gcc-13**（micromamba + conda-forge 可达，已验证）。
2. **事件循环后端**：`USE_SYSTEMD=Off` 后 fcitx5 configure 报
   `Failed to find a valid event loop backend. Backends checked: libuv` → 补装 `libuv1-dev`。
3. **kenlm submodule**：libime 用 tarball 下载会缺 `src/libime/core/kenlm/`（空目录），
   configure 报 `Cannot find source file: ../src/libime/core/kenlm/lm/build_binary_main.cc`
   → **必须 `git clone --recurse-submodules`**。
4. **产物路径**：libime install 后 LM 被 CMake `RENAME` 成 `zh_CN.lm`（在 `lib/libime/` 下），
   不是 `share/libime/sc.lm`；dict 在 `share/libime/sc.dict`。README 里的
   `<prefix>/lib64/libime/zh_CN.lm` 是 lib64 布局，本机用 `lib` 布局。
5. **conda cmake 4.x vs 系统 cmake 3.25**：两者共存；configure 必须显式
   `-DCMAKE_C_COMPILER/-DCMAKE_CXX_COMPILER` 指向 conda 的 `x86_64-conda-linux-gnu-cc/c++`，
   否则会被系统 gcc 12 抢走（踩过一次：configure 显示 `GNU 12.2.0`）。
6. **内存**：3.7 GB 无 swap；fcitx5/libime 编译本身没问题（各 114/106 个目标，几分钟），
   但 **lmplz 训大语料 LM 会 OOM**（lm/README.md 已标注需 `-S 5G`，本机更紧，需调小或加 swap）。

## 依赖清单（apt，Debian 12）

构建基座：`cmake ninja-build pkg-config libuv1-dev`
fcitx5 头文件：`extra-cmake-modules nlohmann-json3-dev libxcb-imdkit-dev libxkbcommon-dev
libxcb-keysyms1-dev libxcb-xkb-dev libxkbcommon-x11-dev libudev-dev libxml2-dev libsqlite3-dev
gettext libdbus-1-dev libgpgme-dev libx11-dev libxtst-dev uuid-dev zlib1g-dev iso-codes
libxcb-icccm4-dev libxcb-image0-dev libxcb-render-util0-dev libxcb-xinerama0-dev libxkbfile-dev
libx11-xcb-dev`
conda-forge（mamba-env）：`gcc_linux-64=13 gxx_linux-64=13 cmake ninja pkg-config
boost-cpp icu fmt zstd libuuid libuv`

## 验证结果（决定性证据）

```
cd bench && .venv/bin/python run_bench.py --corpus corpus/news.txt \
  --cli ../cpp/build/libime_bench \
  --cli-arg /workspace/native-build/opt/libime/lib/libime/zh_CN.lm \
  --cli-arg /workspace/native-build/opt/libime/share/libime/sc.dict \
  --cli-arg 5

引擎: libime_native    语料: corpus/news.txt
  用例数          2000
  完全准确率(句级)  39.60%     ← README 基线 39.60% ✅
  综合准确率(字级)  88.81%     ← README 基线 88.82%（差 0.01 pct 属噪声）✅
  平均码长        3.099 键/字  ← README 基线 3.099 ✅
  延迟 P50 / P99  47.15 / 110.09 ms
```

冒烟测试：`nihao → 你好`、`zhongguo → 中国`、`woshishui → 我是谁`、
`jintiantianqihao → 今天天气好` 全部首选正确。

## 产物位置

| 产物 | 路径 |
|---|---|
| 构建脚本（本报告对应） | `scripts/build_native_x86_64.sh` |
| fcitx5 安装 | `/workspace/native-build/opt/fcitx5/` |
| libime 安装 | `/workspace/native-build/opt/libime/` |
| LM | `/workspace/native-build/opt/libime/lib/libime/zh_CN.lm` (34,736,327 B) |
| 词典 | `/workspace/native-build/opt/libime/share/libime/sc.dict` (4,489,669 B) |
| 评测适配器 | `<repo>/cpp/build/libime_bench` |
| 构建日志 | `/workspace/native-build/logs/*.log` |

> 注：`/workspace/native-build/` 不在仓库内，属本机构建环境；仓库内只沉淀源码与脚本
> （与 `.gitignore` 里 `native-build/` 不入库的约定一致）。

## 对后续路线的含义

- **Route 1（能构建）→ 通了**。fork libime + Fcitx5 addon（M1）的编译环境已就绪，
  下一步可直接在 `libime-1.1.17` 源码上改。
- **Route A（换 LM）→ 地基可用**。lmplz（KenLM）已随 libime 一起编进
  `build/libime`（`ENABLE_TOOLS=On`），训 LM 的工具链完整；唯一约束是内存，
  大语料训练需控制 `-S` 或找 swap。
- **纠错模块集成进 libime（M3-a）**：现在有真实引擎可对接，Python 纠错模块可通过
  libime_bench 的行协议先行验证，再进 addon。
