# pinyin-ime 发布说明


## v0.9.120 (2026-10-21)

### 无 Docker 一键复现预构建运行时 — CLI 9/9 + XIM 8/8 + DBus 7/7
- 新增 `scripts/setup_sandbox_deps.sh`：在 `/usr`、`/etc` 只读的 openEuler aarch64 上把 Xvfb/DBus/X11/中文字体安装到 `/workspace/sysroot`，生成排除 glibc 的 `/workspace/xvfb-libs`，并编译 `xkbcomp` 重定向 shim。
- 新增 `scripts/run_prebuilt.sh`：幂等完成 S3 产物下载/校验、addon 配置修正、数据路径准备、Xvfb + DBus + fcitx5 启动，以及 CLI/XIM/DBus/GUI 验证。
- 运行时组合：fcitx5 5.1.23（v0.9.83 bundle）+ libIME 1.1.17（v0.9.84a）+ CLI/addon v0.9.118 + LM/dict v0.9.116。
- 验证：CLI 9/9、XIM 8/8、DBus InputContext 7/7（按键 65/65），并提交真实候选窗/上屏截图。
- 修复：RPM 内 `Library=libpinyinime.so` 配置错误、sysroot loader 符号链接探测、addon 启动竞态、glibc 混用、Xvfb 硬编码 xkbcomp、`status` 读取旧日志误报。
- 已知差异：`nihaozg` 在 CLI 输出“你好这个”，在 XIM 交互式分词下输出“你好中国”。


## v0.9.93 (2026-10-15)

### 第六台机器复现：只读 /usr 沙箱（140.245.88.50）——输入法真正跑起来
- 从 S3 预构建产物（v0.9.84a + v0.9.83 桌面 bundle + v0.9.84 supplement）复现，**不写 /usr、无 conda、无 docker**。
- CLI 引擎冒烟 **24/24**（全拼/简拼/模糊音/双拼/CLI 功能）。
- fcitx5 以 **DBus+守护进程**方式加载 pinyinime addon + zh_CN.lm + sc.dict，注册输入法 `pinyinime`，`org.fcitx.Fcitx5` 接管 DBus。
- **真实输入链路 e2e 7/7**：经 fcitx5 DBus InputContext 协议（同 GTK/Qt 底层路）发真实按键，上屏 `你好/中国/输入法/北京/上海/拼音/中文`，按键 65/65 —— 证明是真输入法，非 CLI（scripts/e2e_run.sh、e2e_input_test.py）。
- 新增：`scripts/setup_local_runtime.sh`、`scripts/start_im_env_ro.sh`（只读 /usr 启动器）、`scripts/repro_verify.sh`（三层验证，6/6）、`/usr/local/bin/pinyin-ime`。
- 关键坑：CLI 需 `deps-lib/libstdc++.so.6`(GLIBCXX_3.4.32)；`LD_LIBRARY_PATH` 不可带 `sysroot/lib64`（glibc/ld 不匹配）；数据放默认 DataDir `/root/.local/pinyin-ime/share/libime`。
- 环境限制：Xvfb XKB 硬编码 `/usr/bin/xkbcomp` 且本机只读无 CAP_SYS_ADMIN，图形 XIM 端到端本机不可用（非代码缺陷）。


## v0.9.83 (2026-10-10)

### 新增：X11 桌面链路全通 + 免 CLI「搜狗式」体验
- **XIM 端到端 21/21**（`scripts/xvfb_ime_test.sh`）：全拼/简拼/M3纠错/翻页/焦点/撤回
- **GTK3 真实应用 10/10**（`scripts/xvfb_desktop_ime_test.sh`）：GTK_IM_MODULE=fcitx → DBus → fcitx5
- **Qt5 真实应用 ✅**：`tools/qt_ime_test_client` + fcitx5-qt 5.1.4 platforminputcontext 插件
- **候选窗中文渲染 ✅**：classicui + 文泉驿微米黑（截图 `docs/assets/candidate-window.png`）
- **免 CLI 安装 `scripts/install_desktop.sh`**：登录自启（.desktop）、environment.d/xprofile 免配环境变量、
  默认输入法 = pinyinime、中文字体自动安装、自检 8 项；装完重登即可打字；
  **自包含 deps-lib**（conda 运行时库随前缀分发，目标机无需 conda）
- **S3 产物 `cst:xbz/pinyin-ime/v0.9.83/`**：新增 91MB 桌面自包含 bundle
  `pinyin-ime-desktop-v0.9.83.tar.gz`（解压 `$HOME/.local` 即用），+ 常规 CLI/addon/LM/dict，
  部署用法见 README「S3 产物与部署到其它容器」

### 新增脚本/工具
- `scripts/build_fcitx5_qt.sh`：fcitx5-qt 5.1.4（conda gcc + conda Qt5）
- `scripts/fix_sysroot_pc.sh`：只读 /usr 沙箱下批量改写 RPM 前缀 .pc 的硬编码路径
- `scripts/sandbox-bin/{Xvfb,dbus-daemon}`：沙箱 wrapper（库路径 + session.conf 指路）
- `tools/qt_ime_test_client.cpp`：Qt5 端到端输入验证客户端

### 修复/改进
- `build_fcitx5_x11.sh`：自动探测 SYSROOT，系统头与 xkbfile/libxml2/shared-mime-info .pc 兜底
- `build_fcitx5_gtk.sh`：FindX11 指路 SYSROOT；只读 /usr 时 immodules.cache 降级前缀 + GTK_IM_MODULE_FILE
- `xvfb_test_env.sh`：dbus 会话地址从文件恢复（子进程 export 无法传回父进程）

## v0.9.67 (2026-10-09)

### 改进
- **后纠错规则扩展**: 从语料评测错误中提取新的同音字混淆模式
  - 像/想: "如何想保护" → "如何像保护"
  - 片/骗: "好看的骗子" → "好看的片子"
  - 哪/那: "放在那里了" → "放在哪里了"
  - 只/指: "可以只出" → "可以指出"
  - 带/怠: "笃行不带" → "笃行不怠"
  - 舅/久: "舅舅围攻" → "久久为功"
  - 是/试: "电话是听" → "电话试听"
  - 修复在/再规则误修: "在做什么"/"在看" 是正确用法，不再误改
- **smart.py 统一入口**: CLI预测 → 后纠错 → 频率boost → 输出
  - `scripts/pinyin_ime_smart.py`
  - 支持 --batch/--top1/--top-k/--record-freq/--correct-only
- **Web Demo v2 API 验证**: 8个端点全部通过
  - /api?q=, /api?top1=, /api?predict=, /api?freq=1, /api?learn=, /api?correct=, /api?bench=1

### 评测结果 (v0.9.67)
| 语料 | v0.9.66 | v0.9.67 | 提升 |
|------|---------|---------|------|
| daily | 98.0% | 100.0% | +2.0% |
| dialogue | 72.6% | 73.5% | +0.9% |
| news | 39.0% | 40.5% | +1.5% |
| baike | 34.5% | 35.5% | +1.0% |

### CI
- 41/41 CLI, 40/40 daily, 315/315 E2E, 5 句/秒


## v0.9.66 (2026-10-09)

### 新功能
- **YAML 配置系统**: 统一 CLI/addon/daemon 所有设置（模糊音、纠错、boost、双拼、频率、后纠错等），支持热重载
  - `config/pinyin-ime.yaml` 配置文件
  - `scripts/config_loader.py` 加载模块
- **用户频率追踪**: 记录词使用频率，高频词自动提升候选排序
  - `scripts/user_freq.py` 频率追踪模块
  - 频率衰减（30天未使用 × 0.95）、LRU 淘汰（最大 50000 词）
  - Daemon 新增 FREQ/FREQ_TOP/FREQ_STATS 命令
- **句子后纠错**: 初始预测后用规则引擎修正常见错误模式
  - 同音字混淆: 在/再、的/得/地、做/作、反应/反映
  - 量词搭配: 一只猫(非一支)、一支笔(非一只)
  - 常见错别字: 40+ 条成语/固定搭配纠正
  - `scripts/post_corrector.py` 后纠错引擎
  - Hard sentence 评测: 93.8% → 100%
  - Daemon TOP1 自动应用后纠错，新增 CORRECT 命令
- **Web Demo v2**: 改进 UI，添加 REST API 文档
  - 后纠错开关、频率统计面板
  - API 端点: /api?q=, /api?top1=, /api?predict=, /api?freq=1, /api?learn=, /api?correct=, /api?bench=1
  - `scripts/web_demo_v2.py`

### 文档
- 新增 `ARCHITECTURE.md`: 引擎架构、数据流、扩展点、性能特征
- 新增 `CONTRIBUTING.md`: 开发环境、代码规范、提交规范、发布流程

### 测试
- 后纠错测试: 14/14 全通过
- CLI 综合: 41/41
- Daily 评测: 40/40 = 100%
- E2E 端到端: 315/315 = 100%


## v0.9.50 (2026-10-06)

### 修复
- **安全**: git rm --cached secrets.txt ip.txt，凭据不再入库（P0）
- **代码质量**: 删除 abbr_presets.h 中 10 个重复 key（263→253 条）
- **代码质量**: 修正 dasuju→dashuju 等 3 个错误拼音条目
- **架构**: disambiguate_pinyin 合并到 abbr_presets.h 共享头文件，消除 CLI/addon 205 行重复代码
- **消歧**: 扩展音节歧义处理：新增 nian→ni'an、qian→qi'an（原有 xian→xi'an）

### 文档
- 统一版本号到 0.9.50（VERSION/CMakeLists/PROJECT_RULES/CLI）
- 更新过期文档：源码行数（CLI 1276/addon 1015）、数据大小（sc.dict 4.2MB/zh_CN.lm 33.1MB）
- 标记 lccc_eval.txt 指标为"已作废"（训练集泄漏）
- 更新 PROJECT_STATUS/EVALUATION/ARCHITECTURE 中过期版本号和数字

### 未完成（需构建环境）
- P3-9: 提供 374MB 剪枝 LM 作为可选高精度包（需 openEuler 实测）
- P3-10: 训练混合语料 LM（需 openEuler 构建机）
- P3-11: 用 LM-based 消歧替代硬编码 boost 表（需 openEuler 实测）

## v0.9.16 (2026-09-30)

### 新功能
- **fcitx5 addon 拼音消歧**：addon 现在支持 xian→xi'an 自动拆分
  - 逐字输入时检测歧义音节，根据上下文自动拆分
  - 撤回(Ctrl-Z)也应用消歧
  - 新增6个E2E消歧测试用例
- **Web演示服务器**：`scripts/web_demo.py` 浏览器界面
  - 实时候选展示、模糊音/纠错/双拼切换
  - 性能统计（候选数、耗时、总查询数）

### 测试结果
- CLI综合测试: **41/41 (100%)**
- E2E端到端测试: **106/106 (100%)**
- daily 40句准确率: **40/40 (100%)**
- 性能: **181句/秒**, P50=32ms, P99=36ms

## v0.9.15 (2026-09-30)

### 测试结果
- CLI综合测试: **41/41 (100%)**
- E2E端到端测试: **100/100 (100%)**
- daily 40句准确率: **40/40 (100%)**
- 性能: **175句/秒**, P50=32ms, P99=36ms
- 健康检查: **15/15 全通过**

### 功能清单
- ✅ 全拼输入（标准拼音）
- ✅ 简拼输入（首字母缩写 bj→北京）
- ✅ 模糊音（平翘舌/前后鼻音/n-l）
- ✅ 拼音纠错（M3模型 senme→什么）
- ✅ 双拼（8种方案：小鹤/自然码/微软/紫光等）
- ✅ 整句输入（长拼音自动分词）
- ✅ 拼音消歧（xian→xi'an 地名消歧）
- ✅ 用户学习（持久化 user.history + user.dict）
- ✅ 联想预测（上屏后联想）
- ✅ JSON输出（结构化候选）
- ✅ 表情输入（Ctrl-E）
- ✅ 误上屏撤回（Ctrl-Z）
- ✅ fcitx5集成（addon可加载）

### 工具链
| 工具 | 说明 |
|------|------|
| `scripts/demo.sh` | 功能演示（10类） |
| `scripts/ci_test.sh` | CI/CD全量测试 |
| `scripts/healthcheck.sh` | 安装健康检查（6类15项） |
| `scripts/generate_report.sh` | 测试报告生成器 |
| `scripts/warmup.sh` | 预置学习数据加载 |
| `scripts/user_dict.sh` | 用户词典管理 |
| `scripts/benchmark_compare.sh` | 性能对比 |

### 文档
| 文档 | 说明 |
|------|------|
| `QUICKSTART.md` | 快速入门（30秒上手） |
| `INSTALL.md` | 安装指南 |
| `docs/ARCHITECTURE.md` | 架构文档 |
| `test_report.md` | 自动生成测试报告 |

## 版本历史

| 版本 | 日期 | 主要改进 |
|------|------|---------|
| v0.9.16 | 09-30 | addon消歧 + Web演示 |
| v0.9.15 | 09-30 | 测试报告生成器 + 快速入门 |
| v0.9.14 | 09-30 | E2E扩展至100用例 |
| v0.9.13 | 09-30 | 安装指南 + 词典管理 + 健康检查 + 性能对比 |
| v0.9.12 | 09-30 | 消歧测试 + 演示脚本 + python路径修复 |
| v0.9.11 | 09-30 | 消歧增强(地名上下文) + 预热脚本 + 架构文档 |
| v0.9.10 | 09-30 | 全量100% + CI脚本 + 用户学习增强 |
| v0.9.9 | 09-30 | E2E扩展至68用例 + 综合测试报告 |
| v0.9.8 | 09-30 | E2E 40/40 + 用户学习共享 |
| v0.9.7 | 09-30 | E2E 39/40 + fcitx5 server构建 |

## 项目统计
- 源码: 2,354行 (CLI 1276 + addon 1015 + header 153)
- 脚本: 32个
- 文档: 22个
- 测试: 187项 (CLI 41 + E2E 106 + daily 40)
- 语言模型: 34MB (zh_CN.lm)
- 系统词典: 4.3MB (sc.dict)

## v0.9.68 (2026-10-09)

### 新增
- **长句切分**: 拼音音节解析器 + 边界空格插入，提升长句准确率
  - `scripts/pinyin_syllable.py`: 拼音音节解析器，基于声母韵母表 + 动态规划最优切分
  - `scripts/sentence_splitter.py`: 在自然语言边界音节（的/了/在/和/与等）后插入空格，给 LM 分词提示
  - 移除 shi/you/bu 避免拆散复合词（实现/有效/不断）
- **频率 boost 增强**: pinyin→word 精确映射
  - `user_freq.py` v2: 新增 pinyin_map 字段，记录哪个拼音选了哪个词
  - boost 时拼音精确匹配权重 ×3，优先提升用户历史选择
  - top1 模式内部取 top-k → 频率 boost → 返回 top1
- `pinyin_ime_smart.py` 重构: 集成长句切分 + 频率 boost + 后纠错统一流程
  - 新增 `--no-split` 禁用长句切分
  - `--record-freq` 记录 pinyin→word 映射

### 评测
- news: 46% → 47% (+1)
- wiki: 19% → 20% (+1)
- daily/dialogue/baike/recent: 无变化，零回退
- 频率 boost 验证: 重复选择正确提升至首屏
