# pinyin-ime 项目状态

> 更新时间：2026-09-29 v0.8.5

## 项目概述

基于 libime 的 Linux 拼音输入法，支持 CLI 和 fcitx5 addon 两种前端。
目标：在 openEuler 22.03 aarch64 上实现真正可用的中文拼音输入。

## 功能清单

### CLI（pinyin_ime_cli）

| 功能 | 状态 | 说明 |
|---|---|---|
| 全拼输入 | ✅ | 完整拼音解码 |
| 简拼输入 | ✅ | 首字母缩写（bjdx→北京大学） |
| 模糊音 | ✅ | 平翘舌/前后鼻音/n-l/键盘纠错 |
| 双拼 | ✅ | 8种方案：小鹤/自然码/微软/紫光/智能ABC/中文之星/拼音加加/国标 |
| 自学习 | ✅ | 用户语言模型持久化 |
| 中英切换 | ✅ | Ctrl-Space |
| 中文标点 | ✅ | 自动标点 |
| 表情输入 | ✅ | Ctrl-E 进入表情选择 |
| 撤回 | ✅ | Ctrl-Z 撤回最近候选 |
| 候选翻页 | ✅ | [,]/[.] 翻页，PageUp/PageDown |
| 逐词移动 | ✅ | ←/→ 逐词，Ctrl+←/→ 移动焦点 |
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
| 安装脚本 | ✅ | scripts/install.sh |
| Makefile | ✅ | make build/test/bench/eval/train |

### fcitx5 addon

| 功能 | 状态 | 说明 |
|---|---|---|
| 基本输入 | ✅ | 全拼/简拼 |
| 双拼 | ✅ | 8种方案 + Ctrl+Shift+S 切换 |
| 配置 | ✅ | EnableShuangpin + ShuangpinScheme |
| 热更新 | ✅ | reloadConfig 同步 |

## 评测结果

### 准确率（news.txt 500 句）

| 指标 | 基线 | 训练后 | 改进 |
|---|---|---|---|
| 句级准确率 | 36.2% | 53.6% | +17.4pp |
| 字级准确率 | 89.0% | 90.7% | +1.7pp |

### 性能

| 指标 | 值 |
|---|---|
| 逐键延迟（平均） | 0.97ms |
| 逐键延迟（P50） | 0.79ms |
| 逐键延迟（P99） | 2.51ms |

### 冒烟测试

24/24 通过（全拼5 + 简拼3 + 模糊音4 + 双拼8 + CLI功能4）

## 构建依赖

- libime（zh_CN.lm + sc.dict）
- fcitx5（addon）
- conda-forge gcc 16.2.0（openEuler 22.03 aarch64）
- CMake 3.16+

## 文件结构

```
pinyin-ime/
├── cpp/                    # CLI 源码
│   ├── pinyin_ime_cli.cpp  # 主程序
│   ├── pinyin_corrector.cpp # M3 纠错
│   └── CMakeLists.txt
├── fcitx5-addon/           # fcitx5 插件
│   ├── pinyinime.cpp
│   ├── pinyinime.h
│   └── CMakeLists.txt
├── scripts/                # 工具脚本
│   ├── smoke_test.sh       # 冒烟测试
│   ├── install.sh          # 安装脚本
│   ├── train_from_corpus.py # 训练对生成
│   ├── quick_eval.py       # 快速评测
│   └── train_and_eval.sh   # 一键训练+评测
├── bench/                  # 评测框架
│   └── corpus/             # 语料
├── data/                   # 预训练数据
│   └── user_lm_news/       # 新闻语料用户 LM
├── Makefile                # 构建入口
├── VERSION                 # 0.8.5
└── README.md
```

## 后续计划

1. **系统语言模型训练**：用 kenlm 从大规模语料训练领域 LM
2. **fcitx5 addon 增强**：添加配置 UI、更多 CLI 功能
3. **候选部首/偏旁检索**：生僻字输入
4. **跨设备同步**：用户词库云同步
5. **安装包**：RPM/DEB 打包
