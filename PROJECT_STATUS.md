# pinyin-ime 项目状态

> 更新时间：2026-09-30 v0.8.7

## 项目概述

基于 libime 的 Linux 拼音输入法，支持 CLI 和 fcitx5 addon 两种前端。
目标：在 openEuler 22.03 aarch64 上实现真正可用的中文拼音输入法。

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

## 构建依赖

- libime 1.1.17（zh_CN.lm 34.7MB + sc.dict 4.5MB）
- fcitx5 5.1.23（addon）
- conda-forge gcc 16.2.0（openEuler 22.03 aarch64）
- CMake 3.16+ / Ninja
- Python 3: pypinyin, jieba（评测用）

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
│   ├── install.sh          # 一键安装（CLI + addon）
│   └── ...
├── bench/                  # 评测框架
│   └── corpus/             # 语料
├── correction/             # M3 纠错模块
├── data/                   # 预训练数据
│   ├── user_lm_full/       # 全语料用户 LM
│   └── user_lm_news/       # 新闻语料用户 LM
├── Makefile                # 构建入口
├── VERSION                 # 0.8.7
└── README.md
```

## 后续计划

1. **系统语言模型训练**：用 kenlm 从大规模语料训练领域 LM
2. **M3 纠错集成**：接入 libime 解码管线（需引擎重建）
3. **fcitx5 addon 实机验收**：带显示服务器的桌面机实测
4. **候选部首/偏旁检索**：生僻字输入
5. **跨设备同步**：用户词库云同步
6. **安装包**：RPM/DEB 打包
