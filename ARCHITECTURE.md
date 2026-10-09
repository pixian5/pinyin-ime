# pinyin-ime 架构文档

## 系统概览

pinyin-ime 是基于 libime 引擎的 Linux 拼音输入法，支持 CLI、fcitx5 addon、Web Demo 三种前端。

```
┌─────────────────────────────────────────────────────────┐
│                      前端层                              │
│  ┌──────────┐  ┌──────────────┐  ┌──────────────────┐  │
│  │ CLI      │  │ fcitx5 addon │  │ Web Demo (v2)   │  │
│  │ (C++)    │  │ (C++)        │  │ (Python+HTTP)   │  │
│  └────┬─────┘  └──────┬───────┘  └────────┬─────────┘  │
│       │               │                    │             │
├───────┼───────────────┼────────────────────┼─────────────┤
│       │          中间件层                   │             │
│  ┌────▼────────────────────────────────────▼─────────┐  │
│  │  Daemon (Unix Socket)  │  Config Loader (YAML)   │  │
│  │  Post-Corrector        │  User Freq Tracker     │  │
│  └────┬─────────────────────────────────────────────┘  │
│       │                                                 │
├───────┼─────────────────────────────────────────────────┤
│       │              引擎层                              │
│  ┌────▼─────────────────────────────────────────────┐  │
│  │  libime PinyinEngine                              │  │
│  │  ├── 4-gram 语言模型 (kenlm zh_CN.lm 34MB)       │  │
│  │  ├── 系统词典 (sc.dict 4.3MB)                     │  │
│  │  ├── 用户语言模型 (UserLanguageModel)             │  │
│  │  ├── 模糊音 / 纠错 / 双拼 / 简拼                 │  │
│  │  └── 消歧 Boost (abbr_presets.h 150+ 条)         │  │
│  └──────────────────────────────────────────────────┘  │
│                                                         │
├─────────────────────────────────────────────────────────┤
│                     数据层                              │
│  /usr/share/libime/       → 系统LM + 词典             │
│  ~/.local/share/pinyin-ime/ → 用户数据                 │
│    ├── user.dict          → 用户词典                   │
│    ├── user.lm            → 用户语言模型               │
│    ├── user.freq          → 频率追踪数据 (JSON)       │
│    └── history            → 输入历史                   │
│  ~/.config/pinyin-ime/    → 配置                       │
│    └── config.yaml        → YAML 统一配置              │
└─────────────────────────────────────────────────────────┘
```

## 数据流

### 1. 基本输入流程

```
用户输入拼音 → PinyinEngine.candidates() → 候选词列表
                                              ↓
                                    Post-Corrector.correct()
                                              ↓
                                    User-Freq.record()
                                              ↓
                                         上屏输出
```

### 2. CLI 模式

```
stdin → pinyin_ime_cli → libime → stdout
         ├── --batch: 批量模式
         ├── --top1: 仅返回最佳候选
         ├── --top-k N: 返回前N个候选
         ├── --predict: 联想预测
         ├── --learn-file: 自学习
         ├── --add-user: 添加用户词
         └── --fuzzy/--correct/--shuangpin: 开关
```

### 3. Daemon 模式

```
Client → Unix Socket → PinyinIMEDaemon
                          ├── PING/PONG
                          ├── TOP1/TOPK/INPUT/BATCH
                          ├── PREDICT
                          ├── LEARN
                          ├── CORRECT (后纠错)
                          ├── FREQ/FREQ_TOP/FREQ_STATS
                          └── QUIT
```

### 4. fcitx5 Addon 模式

```
按键事件 → fcitx5 → PinyinIMEAddon → libime → 候选窗
                     ├── 模糊音/纠错/双拼配置
                     └── 选词上屏 → UserLanguageModel
```

## 核心组件

### PinyinEngine (libime)

- **语言模型**: 4-gram kenlm，zh_CN.lm (34MB)，覆盖 300K+ 词
- **词典**: sc.dict (4.3MB)，系统词库
- **用户模型**: UserLanguageModel，记录用户选词历史，持久化到 user.lm
- **N-best**: 默认 45 候选，page_size 9

### 消歧 Boost (abbr_presets.h)

预置 150+ 条消歧规则，解决高频同音歧义：

| 拼音 | 默认 → 纠正 | 场景 |
|------|-------------|------|
| xianbeijing | 先北京 → 西安北京 | 地名消歧 |
| shanghaishi | 上海是 → 上海市 | "市"后缀 |
| chengdushi | 成都时 → 成都是美食之都 | 上下文 |
| xianshi | 现实 → 显示 | 常用词优先 |

### Post-Corrector (v0.9.66)

三阶段后纠错引擎：

1. **同音字混淆**: 在/再、的/得/地、做/作、反应/反映
2. **量词搭配**: 一只猫(非一支)、一支笔(非一只)
3. **常见错别字**: 40+ 条成语/固定搭配纠正规则

### User-Freq Tracker (v0.9.66)

- 记录词使用频率到 `user.freq` (JSON)
- 高频词自动提升候选排序（boost_weight 可配置）
- 频率衰减：30天未使用 × 0.95 衰减因子
- 最大 50000 词，LRU 淘汰

### Config Loader (v0.9.66)

YAML 统一配置，热重载支持：

```yaml
fuzzy:     { enabled, z_zh, c_ch, s_sh, an_ang, en_eng, in_ing, ... }
correction:{ enabled, keyboard_adjacent, common_typos }
shuangpin: { enabled, scheme }
learning:  { enabled, history_weight, auto_learn_threshold }
frequency: { enabled, decay_factor, boost_weight }
post_correction: { enabled, homophone_fix, measure_word_fix, common_mistake_fix }
```

## 扩展点

### 添加新的消歧规则

编辑 `cpp/abbr_presets.h`，在 `BOOST_ENTRIES` 数组中添加：
```cpp
{"pinyin", "正确中文"},
```
重新编译 CLI 即可。

### 添加后纠错规则

编辑 `scripts/post_corrector.py`：
- 同音字: `self.zai_zai_rules` 列表
- 量词: `self.measure_word_rules` 字典
- 成语: `self.idiom_rules` 列表

### 添加用户词典

```bash
pinyin_ime_cli --add-user "py=中文" zh_CN.lm sc.dict
```

或通过 daemon:
```
LEARN py\t中文
```

### 训练领域语言模型

```bash
python3 scripts/train_user_lm.py --corpus domain.txt --output user.lm
```

## 性能特征

| 指标 | 值 |
|------|-----|
| 冷启动 | ~200ms (加载34MB LM) |
| 单次预测 | ~5ms |
| 批量吞吐 | ~5 句/秒 |
| Daemon 模式 | 消除冷启动，常驻内存 |
| 内存占用 | ~150MB (LM + 词典) |

## 测试体系

| 层级 | 范围 | 数量 |
|------|------|------|
| 冒烟 | 基本功能 | 24 |
| CLI综合 | 边界/Boost/纠错/压力 | 41 |
| Daily | 日常40句 | 40 |
| 后纠错 | 同音字/量词/成语 | 14 |
| E2E | fcitx5端到端 | 315 |
| **总计** | | **434+** |
