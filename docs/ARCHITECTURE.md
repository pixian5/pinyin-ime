# pinyin-ime 架构文档

## 系统概览

pinyin-ime 是一个基于 libime（fcitx5 拼音引擎）的 Linux 拼音输入法，提供 CLI 和 fcitx5 addon 两种使用方式。

```
┌─────────────────────────────────────────────────┐
│                   用户输入                        │
│         (键盘按键 / stdin 拼音串)                  │
└──────────┬──────────────────┬────────────────────┘
           │                  │
    ┌──────▼──────┐   ┌──────▼──────┐
    │  CLI 接口    │   │ fcitx5 addon │
    │ pinyin_ime  │   │ pinyinime.so │
    │   _cli      │   │ + .conf      │
    └──────┬──────┘   └──────┬──────┘
           │                  │
           └────────┬─────────┘
                    │
           ┌────────▼────────┐
           │   libime 引擎    │
           │  PinyinIME +    │
           │  PinyinContext  │
           └────────┬────────┘
                    │
     ┌──────────────┼──────────────┐
     │              │              │
┌────▼────┐  ┌──────▼──────┐  ┌───▼────┐
│ 语言模型 │  │  系统词典    │  │用户数据 │
│zh_CN.lm │  │  sc.dict    │  │user.dict│
│ (34MB)  │  │  (4.5MB)    │  │user.hist│
└─────────┘  └─────────────┘  └─────────┘
```

## 核心组件

### 1. CLI (pinyin_ime_cli)

**文件**: `cpp/pinyin_ime_cli.cpp` (~1255行)

功能：
- 行模式：stdin 每行一段拼音 → 输出候选
- 交互模式：逐键输入，数字/空格/回车上屏
- 简拼/全拼/模糊音/纠错/双拼（8种方案）
- 用户学习（持久化 user.history + user.dict）
- JSON 输出、批量处理、预测
- 表情选择（Ctrl-E）、误上屏撤回（Ctrl-Z）

关键函数：
- `disambiguate_pinyin()` — 拼音消歧（xian→xi'an）
- `collect_candidates()` — 候选生成
- `select_and_commit()` — 选词上屏 + 学习
- `save_user_data()` — 用户数据持久化

### 2. fcitx5 addon (pinyinime)

**文件**: `fcitx5-addon/` (pinyinime.cpp ~938行, .h, .conf)

功能：
- 标准 fcitx5 输入法接口
- 配置选项：DataDir, UserDataDir, EnableFuzzy, EnableCorrection, NBest, PageSize
- 用户学习与 CLI 共享同一份数据
- 候选分页、翻页

### 3. libime 引擎

提供拼音解码核心能力：
- `PinyinIME` — 引擎主类
- `PinyinContext` — 输入上下文（type/select/commit）
- `PinyinDictionary` — 词典管理（系统 + 用户）
- `UserLanguageModel` — 用户语言模型
- `ShuangpinProfile` — 双拼方案

### 4. 数据文件

| 文件 | 大小 | 说明 |
|------|------|------|
| zh_CN.lm | 34MB | 系统语言模型（N-gram） |
| sc.dict | 4.5MB | 系统词典 |
| user.history | ~1KB | 用户学习历史 |
| user.dict | ~300B | 用户词典 |

## 数据流

### CLI 行模式
```
stdin → disambiguate_pinyin() → PinyinContext.type() → 
collect_candidates() → 输出候选列表
```

### CLI 交互模式
```
按键 → PinyinContext.type() → preedit 更新 → 
候选列表更新 → 用户选择 → select_and_commit() → 
learn() → save_user_data()
```

### fcitx5 addon
```
fcitx5 按键事件 → pinyinime.cpp keyEvent() → 
PinyinContext.type() → preedit/candidate 更新 → 
fcitx5 commitString() → learn()
```

## 测试体系

| 测试 | 数量 | 说明 |
|------|------|------|
| CLI 综合 | 35 | 全拼/简拼/模糊/纠错/双拼/JSON |
| E2E 端到端 | 68 | fcitx5 testfrontend 模拟真实按键 |
| daily 准确率 | 40 | 日常用语句级准确率 |
| 性能基准 | - | 句/秒, P50/P99 延迟 |

## 构建系统

```
scripts/
├── build_native.sh    # 构建 fcitx5 + libime (conda-forge 工具链)
├── build_cpp.sh       # 构建 CLI + addon
├── build_e2e_test.sh  # 构建 E2E 测试二进制
├── ci_test.sh         # CI/CD 全量测试
├── warmup.sh          # 预置学习数据加载
├── test_all.sh        # CLI 综合测试
├── test_report.sh     # 综合测试报告
├── eval_daily.py      # daily 40句准确率评测
└── benchmark.py       # 性能基准
```

## 消歧策略

`disambiguate_pinyin()` 处理 "xian" 歧义：
- "xian" = 先/现/显 (单音节) vs xi'an = 西安 (双音节)
- 根据上下文决定是否拆分
- 拆分上下文：地名 (beijing, nanjing...) + 常见句式
- 不拆上下文：常见词 (xiandai=现代, xianzai=现在...)
- 独立 "xian" 不拆分（先/现 更常见）
