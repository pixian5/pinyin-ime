# pinyin-ime 架构设计

## 整体架构

```
┌─────────────────────────────────────────────────────┐
│                    用户输入                          │
│              (键盘事件 / 管道文本)                    │
└──────────┬──────────────────────┬───────────────────┘
           │                      │
           ▼                      ▼
┌──────────────────┐   ┌──────────────────────┐
│   fcitx5 addon   │   │      CLI 工具         │
│ (pinyinime.cpp)  │   │ (pinyin_ime_cli.cpp) │
│  DBus + Wayland  │   │   stdin/stdout       │
└────────┬─────────┘   └──────────┬───────────┘
         │                        │
         ▼                        ▼
┌─────────────────────────────────────────────────────┐
│                   核心引擎层                         │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │ PinyinIME   │  │ PinyinContext│  │  用户LM    │ │
│  │ (解码引擎)  │  │  (输入上下文) │  │ (自学习)   │ │
│  └─────────────┘  └──────────────┘  └────────────┘ │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │ 拼音分割    │  │  候选排序    │  │  模糊音    │ │
│  └─────────────┘  └──────────────┘  └────────────┘ │
└────────────────────────┬────────────────────────────┘
                         │
                         ▼
┌─────────────────────────────────────────────────────┐
│                   libime 底层                        │
│  ┌─────────────┐  ┌──────────────┐  ┌────────────┐ │
│  │ LanguageModel│  │  Dictionary  │  │  PinyinIME │ │
│  │ (zh_CN.lm)  │  │  (sc.dict)   │  │  (C++ API) │ │
│  └─────────────┘  └──────────────┘  └────────────┘ │
└─────────────────────────────────────────────────────┘
```

## 模块说明

### 1. CLI 工具 (`cpp/pinyin_ime_cli.cpp`)

独立的命令行拼音输入工具，不依赖 fcitx5。

**职责：**
- 解析命令行参数（--shuangpin, --fuzzy, --correct 等）
- 交互模式：逐键输入，实时显示候选
- 批量模式：从文件读取拼音，输出中文
- 管道模式：stdin → stdout
- 用户学习：--learn-file, --train-text
- 输出格式：文本/JSON/Top-1

**数据流：**
```
stdin → 拼音字符串 → PinyinIME::decode() → 候选列表 → stdout
```

### 2. fcitx5 Addon (`fcitx5-addon/pinyinime.cpp`)

系统级输入法模块，通过 fcitx5 框架接收键盘事件。

**职责：**
- 注册为 fcitx5 输入法工厂
- 处理键盘事件（数字键、空格、退格、方向键等）
- 管理输入上下文（preedit、候选窗口）
- DBus 配置热加载
- 延迟加载数据（首次按键时加载 LM/Dict）

**关键设计：**
- `ensureDataLoaded()`: 延迟加载，避免启动卡顿
- `reloadConfig()`: 独立于 DBus 的配置读取
- `PinyinIMEConfig`: 声明式配置，fcitx5-configtool 自动生成 UI

### 3. 核心引擎（libime 封装）

**PinyinIME**：解码引擎，将拼音字符串转换为汉字候选
- 全拼解码：`nihao` → [你好, 你毫, 拟好, ...]
- 简拼解码：`bjdx` → [北京大学, ...]
- 双拼解码：按方案映射后解码

**PinyinContext**：输入上下文，管理状态
- 拼音缓冲区
- 候选列表
- 光标位置
- 分割信息

**LanguageModel**：语言模型，给候选打分排序
- 系统LM：zh_CN.lm（34.7MB，基于大规模语料训练）
- 用户LM：自学习 bigram 模型
- 混合排序：系统LM分数 + 用户LM加分

### 4. 功能模块

#### 模糊音 (`--fuzzy`)
```
输入: cifan (ci→chi 模糊)
映射: ci → {ci, chi}
解码: {ci, chi} × {fan} → {吃饭, 刺番, ...}
排序: 精确匹配优先，模糊匹配按代价排序
```

#### 拼写纠错 (`--correct`)
```
输入: senme (漏了h)
M3模型: senme → shenme (P=0.85)
解码: shenme → [什么, 甚末, ...]
```

#### 双拼 (`--shuangpin`)
```
输入: vsgo (小鹤双拼)
映射: v→zh, s→ong, g→g, o→uo
解码: zh+ong+g+uo → zhongguo → [中国, ...]
```

#### 用户学习
```
训练: --learn-file phrases.txt
存储: ~/.local/share/pinyin-ime/user.dict (二进制)
效果: 已学习短语在候选中排名提升
```

## 数据文件

```
/usr/share/libime/          # 系统数据 (符号链接)
├── zh_CN.lm                # 系统语言模型 (34.7MB)
├── zh_CN.lm.pred           # 预测模型
└── sc.dict                 # 系统词典

~/.local/share/pinyin-ime/  # 用户数据
├── user.dict               # 用户词典 (二进制)
└── user.history            # 输入历史 (二进制)

~/.config/fcitx5/conf/
└── pinyinime.conf          # addon 配置
```

## 构建系统

```
pinyin-ime/
├── cpp/CMakeLists.txt      # CLI 构建
├── fcitx5-addon/CMakeLists.txt  # addon 构建
├── packaging/
│   ├── pinyin-ime.spec     # RPM 打包
│   └── debian/             # Debian 打包
│       ├── control
│       ├── rules
│       ├── changelog
│       └── copyright
└── scripts/
    ├── install.sh          # 一键安装
    ├── ci_test.sh          # CI 测试
    ├── test_all.sh         # 综合测试
    ├── benchmark.py        # 性能基准
    ├── eval_daily.py       # 准确率评测
    └── user_dict_import.py # 词典导入
```

## 性能优化

1. **延迟加载**：LM/Dict 在首次按键时加载，不阻塞启动
2. **NBest 限制**：默认 45 个候选，平衡准确率和性能
3. **缓存友好**：PinyinContext 复用，避免重复分配
4. **批量模式**：单进程处理多句，均摊启动开销

## 扩展方向

1. **预处理分割**：在发送给解码器前，用规则处理已知的分割歧义（xian→xi'an）
2. **更大LM**：用更多日常对话语料训练，提升泛化准确率
3. **在线学习**：实时更新用户LM，无需手动训练
4. **云同步**：用户词典跨设备同步
5. **繁体输出**：集成 OpenCC，支持简繁切换
