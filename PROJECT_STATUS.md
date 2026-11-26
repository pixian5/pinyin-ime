# pinyin-ime 项目状态

> 更新时间：2026-11-26 **v0.9.190**（发布工程：rpm/deb 依赖 libime-data（体积约 1MB）、runtime 自包含且可卸载、打包前校验 CLI 版本；LICENSE 落地（LGPL-2.1-or-later）并把准确率目标分层（原始 91%/68% vs 可达 88%/45%）；文档索引 48 篇全部收录，P1-8 关闭；真实评测收敛为 `scripts/eval_chroot.sh` 一条命令（27.83% / 84.81%，门禁通过）；前序 v0.9.183 把歧义拆分收敛为 CLI/addon 共享入口；功能矩阵仍以 v0.9.121 发布工程为基线，v0.9.122–v0.9.176 的功能见 README 与 CHANGELOG）

## 项目概述

基于 libime 的 Linux 拼音输入法，支持 CLI 和 fcitx5 addon 两种前端。
目标：在 openEuler 22.03 aarch64 上实现真正可用的中文拼音输入法。

## 当前发布状态（v0.9.121，2026-10-21）

**运行时组合**（全部来自 S3 `cst:xbz/pinyin-ime`，无 Docker）：

| 组件 | 版本 | 来源 |
|---|---|---|
| fcitx5 | 5.1.23 | `v0.9.83` desktop bundle（X11/XIM/XCB/DBus/classicui） |
| libIME | 1.1.17 | `v0.9.84a` |
| LM / dict | `zh_CN.lm` 34M + `sc.dict` 4.3M | `v0.9.116` |
| CLI / addon | 本机源码编译（GCC 10.3 + libime 1.1.17 头） | `cpp/` + `fcitx5-addon/` |

**发布产物**（[GitCode Release v0.9.121](https://gitcode.com/laotongtian/pinyin-ime/releases/tag/v0.9.121)，S3 `v0.9.121/` 同步）：

- `pinyin-ime-0.9.121-1.aarch64.rpm` / `pinyin-ime_0.9.121_arm64.deb`（addon + CLI + inputmethod + LM/dict）
- `pinyin-ime-runtime-v0.9.121-aarch64.tar.gz`（自包含便携运行时，82M，`scripts/start.sh` 一键启动）
- `SHA256SUMS_v0.9.121`

**验证矩阵（v0.9.121 实测）**：

| 项 | 结果 |
|---|---|
| `scripts/build_release.sh` 端到端（编译→RPM/DEB→bundle→校验和） | ✅ |
| bundle CLI 9/9、XIM 8/8、DBus e2e 7/7（65/65 按键） | ✅ |
| bundle fcitx5：17 addon 全加载（classicui/dbus/xcb/pinyinime + LM/dict） | ✅ |
| RPM 全新 root 安装/核对/卸载（`rpm -ivh --nodeps` → `rpm -e`） | ✅ |
| addon conf 硬校验（`Library=libpinyinime`、`Version=0.9.121`、`0=core:5.1.23`） | ✅ |
| GitCode Actions 静态检查 `.gitcode/workflows/ci.yml` | ✅ 脚本已备 |

**入口脚本**：

- `scripts/build_release.sh` — 构建全部发布产物（沙箱 rpmbuild 自动使用 `$NB/rpmtools`）
- `scripts/run_prebuilt.sh` — S3 预构建运行时的 start/stop/status/test/gui
- `scripts/ci_static.sh` / `scripts/ci_runtime.sh` — 静态检查 / 运行时冒烟

**已知限制**：仅验证 X11/XIM（Wayland 未验证）；CLI 与 XIM 断言不同（`nihaozg` → CLI“你好这个”/XIM“你好中国”，分词策略差异）；本机 `/usr` `/etc` 只读，便携运行时是推荐分发方式。

## 功能清单

### CLI（pinyin_ime_cli）

| 功能 | 状态 | 说明 |
|---|---|---|
| 全拼输入 | ✅ | 完整拼音解码 |
| 简拼输入 | ✅ | 首字母缩写（bjdx→北京大学） |
| 模糊音 | ✅ | 平翘舌/前后鼻音/n-l/键盘纠错 |
| 双拼 | ✅ | 8种方案：小鹤/自然码/微软/紫光/智能ABC/中文之星/拼音加加/国标 |
| 自学习 | ✅ | 用户语言模型持久化（上屏即落盘） |
| 中英切换 | ✅ | Tab 切换 |
| 中文标点 | ✅ | 半角→全角，含引号智能配对 |
| 表情输入 | ✅ | Ctrl-E 进入表情选择 |
| 撤回 | ✅ | Ctrl-Z 撤回最近候选 |
| 候选翻页 | ✅ | [,]/[.] 翻页，PageUp/PageDown |
| 逐词移动 | ✅ | ←/→ 逐词，Ctrl+←/→ 移动焦点 |
| Home/End | ✅ | 候选光标跳句首/句尾 |
| 联想预测 | ✅ | --predict 基于 libime::Prediction |
| 纠错 | ✅ | --correct M3 拼写纠错 |
| 批量处理 | ✅ | --batch F 从文件处理 |
| 批量学习 | ✅ | --learn-file F 训练用户 LM |
| 文本训练 | ✅ | --train-text F 从中文文本训练 |
| 导出历史 | ✅ | --dump-user 导出学习记录 |
| 配置文件 | ✅ | ~/.config/pinyin-ime/config |
| JSON 输出 | ✅ | --json 程序化调用 |
| 统计信息 | ✅ | --stats 引擎统计 |
| 只输出首选 | ✅ | --top1 管道友好 |
| 健康检查 | ✅ | --self-test 验证安装 |
| LM 权重调节 | ✅ | --history-weight F |
| 安装脚本 | ✅ | scripts/install.sh（CLI + addon 一键安装） |
| Makefile | ✅ | make build/test/bench/eval/train |

### fcitx5 addon

| 功能 | 状态 | 说明 |
|---|---|---|
| 基本输入 | ✅ | 全拼/简拼 |
| 双拼 | ✅ | 8种方案 + Ctrl+Shift+S 切换 |
| 中英切换 | ✅ | Tab 切换 |
| 中文标点 | ✅ | 半角→全角，含引号配对 |
| 光标移动 | ✅ | ←/→ 逐词，Home/End 跳句首尾 |
| 配置 | ✅ | EnableShuangpin + ShuangpinScheme |
| 热更新 | ✅ | reloadConfig 同步 |
| 编译验证 | ✅ | libpinyinime.so + 符号导出通过 |
| 实机加载 | ✅ | fcitx5 server 成功加载 addon + LM + dict |
| 输入法注册 | ✅ | fcitx5 注册 1 个输入法（pinyinime） |
| **XIM 端到端** | ✅ 21/21 | Xvfb + 自研 XIM 客户端（xvfb_ime_test.sh） |
| **GTK3 应用端到端** | ✅ 10/10 | im-fcitx5.so + DBus（xvfb_desktop_ime_test.sh） |
| **Qt5 应用端到端** | ✅ | fcitx5-qt platforminputcontext 插件 + qt_ime_test_client |
| **候选窗渲染** | ✅ | classicui + 中文字体（docs/assets/candidate-window.png） |
| **免 CLI 桌面集成** | ✅ | install_desktop.sh（autostart + environment.d + launcher） |

### 领域 LM 训练

| 功能 | 状态 | 说明 |
|---|---|---|
| 训练脚本 | ✅ | scripts/train_domain_lm.py（语料→ARPA→libime 二进制） |
| 多语料合并 | ✅ | 支持多个语料文件合并训练 |
| 可配 N-gram 阶数 | ✅ | --order 2/3/4 |
| 自定义 LM | ✅ | data/custom_news.lm（4-gram，16K 句训练） |
| 对话领域 LM | ✅ | 3-gram，183 句，20KB |

### 用户词库同步

| 功能 | 状态 | 说明 |
|---|---|---|
| 导出 | ✅ | sync_user_data.py export（二进制安全） |
| 导入 | ✅ | sync_user_data.py import（覆盖/合并模式） |
| 合并 | ✅ | sync_user_data.py merge（两设备词库合并） |
| 远程同步 | ✅ | push/pull（rclone 到 S3/OBS 等） |

### 打包

| 功能 | 状态 | 说明 |
|---|---|---|
| 一键发布 | ✅ | `scripts/build_release.sh`（编译 + RPM + DEB + 便携运行时 + SHA256SUMS，v0.9.121 起） |
| RPM | ✅ | `scripts/build_packages.sh rpm`（addon + CLI + inputmethod + LM/dict；spec 含硬校验） |
| DEB | ✅ | `scripts/build_packages.sh deb`（纯 Python `make_deb.py`，无需 dpkg） |
| 便携运行时 | ✅ | `pinyin-ime-runtime-v<ver>-aarch64.tar.gz`（fcitx5+libIME+数据+字体，不写 /usr） |
| 旧 tarball | ⚠️ | `scripts/package.sh`（写死 /root/workspace，仅历史保留，勿用） |

## 评测结果

### 准确率（news.txt 100 句快速评测）

| 指标 | 基线 | 训练后(200句) | 改进 |
|---|---|---|---|
| 句级准确率 | 47.0% | 53.0% | +6.0pp |
| 字级准确率 | 90.8% | 92.7% | +1.9pp |

### 纠错模块（correction/metric.py）

| 指标 | top1 | top3 |
|---|---|---|
| delete | 12.14% | 45.65% |
| insert | 78.10% | 98.81% |
| substitute | 27.62% | 59.05% |
| 混淆排序 top2 | 86.05% | - |

### 性能

| 指标 | 值 |
|---|---|
| 逐键延迟（平均） | 0.97ms |
| 逐键延迟（P50） | 0.79ms |
| 逐键延迟（P99） | 2.51ms |

### 冒烟测试

24/24 通过（全拼5 + 简拼3 + 模糊音4 + 双拼8 + CLI功能4）
交互回归：26/26 通过

### fcitx5 addon 实机验收

14 项检查全部通过：
- addon 二进制存在 + 符号导出
- 配置文件正确（[Addon] 段 + Library 指定）
- libime 数据（zh_CN.lm 33.1MB + sc.dict 4.2MB）加载成功
- Xvfb 虚拟显示 + DBus session 启动
- fcitx5 server 加载 pinyinime addon
- 语言模型和字典加载成功
- 输入法注册成功（Found 1 input method）
- DBus 服务名注册（org.fcitx.Fcitx5）
- 无致命错误

### fcitx5 端到端打字验证（testfrontend API）

12/12 项检查全部通过（scripts/test_fcitx5_e2e_typing.sh）：
- pinyinime addon 加载 + LM/dict 加载 + IM 注册
- IM 成功切换到 pinyinime（setCurrentInputMethod）
- 按键全部被接受（accepted=1）
- 输入 "nihao" → 预编辑 "ni hao" → 9 个候选 [你好, 拟好, 你毫, ...]
- 空格选首选 → 提交 "你好"
- 输入 "shijie" → 预编辑 "shi jie" → 9 个候选 [世界, 时节, 师姐, ...]
- 空格选首选 → 提交 "世界"
- 输入 "nihao" + 数字选词 → 提交 "你好"

验证方式：通过 fcitx5 内置 testfrontend addon 直接调用 C++ API，
绕过 DBus/X11 前端限制，在无头环境中完成完整的按键→候选→提交流程。

## 构建依赖

- libime 1.1.17（zh_CN.lm 33.1MB + sc.dict 4.2MB）
- fcitx5 5.1.23（addon + server）
- conda-forge gcc 16.2.0（openEuler 22.03 aarch64）
- CMake 3.16+ / Ninja
- Python 3: pypinyin, jieba（评测用）
- Xvfb + DBus（addon 实机验收用）

## 文件结构

```
pinyin-ime/
├── cpp/                    # CLI 源码
│   ├── pinyin_ime_cli.cpp  # 主程序（1135行）
│   ├── pinyin_corrector.*  # M3 纠错（C++ 原生）
│   ├── emoji_list.h        # 表情数据
│   └── CMakeLists.txt
├── fcitx5-addon/           # fcitx5 插件
│   ├── pinyinime.cpp       # addon 引擎（927行）
│   ├── pinyinime.h         # addon 头文件
│   └── CMakeLists.txt
├── scripts/                # 工具脚本
│   ├── setup_build_env.sh  # 环境搭建
│   ├── build_conda_forge.sh # 引擎编译
│   ├── build_cli.sh        # CLI 编译
│   ├── run_cli.sh          # 一键运行
│   ├── smoke_test.sh       # 冒烟测试（24例）
│   ├── test_interactive.py # 交互测试（26例）
│   ├── test_fcitx5_addon.sh # fcitx5 addon 实机验收（14项）
│   ├── install.sh          # 一键安装（CLI + addon）
│   ├── train_domain_lm.py  # 领域 LM 训练
│   ├── sync_user_data.py   # 用户词库同步
│   ├── package.sh          # RPM/DEB/tarball 打包
│   └── ...
├── packaging/              # 打包文件
│   ├── pinyin-ime.spec     # RPM spec
│   └── debian/control      # DEB control
├── bench/                  # 评测框架
│   └── corpus/             # 语料
├── correction/             # M3 纠错模块
├── data/                   # 预训练数据
│   ├── custom_news.lm      # 自定义新闻 LM
│   ├── user_lm_full/       # 全语料用户 LM
│   └── user_lm_news/       # 新闻语料用户 LM
├── Makefile                # 构建入口
├── VERSION                 # 0.9.50
└── README.md
```

## 后续计划

1. **更大规模 LM 训练**：当前 16K 句训练的 LM 在 holdout 上不如原始 libime LM（35% vs 75%），需要更大语料或插值策略
2. **M3 纠错深度集成**：接入 libime 解码管线（需引擎重建）
3. **候选部首/偏旁检索**：生僻字输入
4. **~~桌面 GUI 实测~~**：✅ 已通过 testfrontend API 完成端到端打字验证（12/12 通过）
5. **双拼方案自定义**：用户可编辑双拼键位映射

## v0.8.8 构建验证（2026-09-30）

- **环境**: openEuler 22.03 LTS aarch64, conda-forge gcc 16.2.0
- **fcitx5**: 5.1.23 从源码编译（禁用 X11/Wayland/DBus/Server，仅核心库）
- **libime**: 1.1.17 从源码编译（4-gram, KENLM_MAX_ORDER=4）
- **CLI**: pinyin_ime_cli 编译通过，24/24 冒烟 + 26/26 交互测试全通过
- **Addon**: libpinyinime.so 编译通过
- **lmplz**: kenlm lmplz 编译通过（用于自定义 LM 训练）
- **数据**: zh_CN.lm 34MB + sc.dict 4.3MB（libime 自带）
