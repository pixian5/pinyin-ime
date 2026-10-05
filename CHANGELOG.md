# pinyin-ime 发布说明


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
