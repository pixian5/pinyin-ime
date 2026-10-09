# 贡献指南

感谢你对 pinyin-ime 的关注！本文档介绍如何参与开发。

## 开发环境

### 前置依赖

- openEuler 22.03+ / 任何 Linux (aarch64 / x86_64)
- micromamba (conda-forge 工具链)
- GCC 12+ (conda-forge 提供 16.2.0)
- CMake 3.16+ / ninja
- Python 3.8+ (pypinyin, jieba, PyYAML)

### 一键搭建

```bash
bash scripts/setup_build_env.sh   # 安装 micromamba + 依赖
bash scripts/build_cli.sh         # 编译 CLI + kenlm
```

## 项目结构

```
pinyin-ime/
├── cpp/                    # C++ 核心
│   ├── pinyin_ime_cli.cpp  # CLI 主程序
│   ├── abbr_presets.h      # 消歧 Boost 规则
│   └── CMakeLists.txt
├── fcitx5-addon/           # fcitx5 输入法插件
│   ├── pinyinime.cpp
│   ├── pinyinime.h
│   └── CMakeLists.txt
├── scripts/                # Python 工具链
│   ├── config_loader.py    # YAML 配置加载
│   ├── post_corrector.py   # 后纠错引擎
│   ├── user_freq.py        # 频率追踪
│   ├── pinyin_daemon.py    # Unix Socket 守护进程
│   ├── pinyin_client.py    # Daemon 客户端
│   ├── web_demo_v2.py      # Web 演示
│   ├── eval_daily.py       # 日常评测
│   ├── eval_corpus.py      # 大语料评测
│   ├── auto_learn_v2.py    # 自学习
│   ├── train_user_lm.py    # 领域LM训练
│   ├── benchmark.py        # 性能基准
│   ├── ci_test.sh          # CI 编排
│   └── test_all.sh         # 综合测试
├── config/                 # 配置
│   └── pinyin-ime.yaml     # 统一 YAML 配置
├── bench/corpus/           # 评测语料
├── packaging/              # RPM 打包
├── ARCHITECTURE.md         # 架构文档
└── README.md
```

## 开发流程

### 1. 修改代码

- **C++ 代码**: 编辑 `cpp/` 下的文件，重新 `bash scripts/build_cli.sh`
- **Python 脚本**: 编辑 `scripts/` 下的文件，直接运行即可
- **消歧规则**: 编辑 `cpp/abbr_presets.h`，重新编译

### 2. 运行测试

```bash
# 快速验证
bash scripts/smoke_test.sh

# 完整测试
bash scripts/test_all.sh

# CI 全流程
bash scripts/ci_test.sh
```

### 3. 评测

```bash
# 日常40句
python3 scripts/eval_daily.py

# 大语料 (news/baike/wiki/recent)
python3 scripts/eval_corpus.py --corpus news --max-lines 200
```

## 代码规范

### C++

- 缩进: 4 空格
- 命名: snake_case 函数/变量, CamelCase 类
- 注释: 中文注释说明业务逻辑
- 错误处理: 检查所有 libime API 返回值

### Python

- 缩进: 4 空格
- 命名: snake_case
- 类型提示: 公共函数建议添加
- 文档字符串: 模块级 + 公共类/函数

## 提交规范

### Commit Message

```
<type>(<scope>): <description>

类型: feat/fix/docs/refactor/test/chore
范围: cli/addon/daemon/config/corrector/freq/eval
```

示例:
```
feat(corrector): 添加在/再同音字混淆纠正规则
fix(cli): 修复batch模式top1输出缺少换行
docs(arch): 添加ARCHITECTURE.md架构文档
```

### 版本号

遵循语义化版本 (SemVer):
- 主版本: 架构变更
- 次版本: 新功能 (0.9.x)
- 修订: Bug修复

更新 `VERSION` 文件和 `CHANGELOG.md`。

## 添加新功能

### 添加消歧规则

1. 编辑 `cpp/abbr_presets.h`，在 `BOOST_ENTRIES` 数组末尾添加
2. 重新编译: `bash scripts/build_cli.sh`
3. 添加测试到 `scripts/test_all.sh`
4. 运行 CI 验证

### 添加后纠错规则

1. 编辑 `scripts/post_corrector.py`
2. 在对应的 rules 列表中添加
3. 在 `__main__` 测试中添加验证用例
4. 更新 `scripts/test_all.sh` 后纠错部分

### 添加评测语料

1. 将语料文件放入 `bench/corpus/`
2. 运行: `python3 scripts/eval_corpus.py --corpus <name> --max-lines 200`
3. 更新 `scripts/eval_baseline.json`

## 发布流程

1. 更新 `VERSION` 和 `CHANGELOG.md`
2. 运行完整 CI: `bash scripts/ci_test.sh`
3. 打包: `tar czf dist/pinyin-ime-$(cat VERSION).tar.gz ...`
4. 上传到 S3/OBS
5. 推送到 GitCode

## 常见问题

### Q: 编译失败，找不到 libime 头文件

A: 确保已运行 `bash scripts/setup_build_env.sh`，libime 安装在 conda-forge 环境中。

### Q: eval_corpus.py 准确率很低

A: 这是正常的。大语料（news/baike）包含大量专有名词和长句，单字级 LM 难以完美处理。日常对话场景 100% 准确率。

### Q: 如何调试 fcitx5 addon？

A: 使用 `fcitx5-diagnose` 检查环境，`testfrontend` addon 做端到端测试。
