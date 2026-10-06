# pinyin-ime 快速入门

## 30秒上手

```bash
git clone https://gitcode.com/laotongtian/pinyin-ime.git
cd pinyin-ime
source ./setup_env.sh
echo "nihao" | ./run.sh
# 输出: 你好
```

## 常用命令

### 基本输入
```bash
echo "nihao" | ./run.sh              # → 你好
echo "zhongguo" | ./run.sh           # → 中国
echo "beijingdaxue" | ./run.sh       # → 北京大学
```

### 多候选
```bash
echo "nihao" | "$PINYIN_IME_CLI" --quiet "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 5
# 你好	你号	你毫	拟好	你耗
```

### 模糊音（平翘舌不区分）
```bash
echo "cifan" | "$PINYIN_IME_CLI" --quiet --top1 --fuzzy "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# → 吃饭 (ci→chi)
```

### 纠错（打错了也能出）
```bash
echo "senme" | "$PINYIN_IME_CLI" --quiet --top1 --correct "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# → 什么 (senme→shenme)
```

### 双拼（小鹤方案）
```bash
echo "nihc" | "$PINYIN_IME_CLI" --quiet --top1 --shuangpin --sp-scheme xiaohe "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# → 你好
```

### 交互模式
```bash
./run.sh
# 逐键输入，数字选候选，空格上屏
# Ctrl-E 表情 | Ctrl-Z 撤回 | [/] 翻页
```

## 工具一览

| 命令 | 说明 |
|------|------|
| `bash scripts/demo.sh` | 功能演示（10类） |
| `bash scripts/healthcheck.sh` | 安装健康检查 |
| `bash scripts/ci_test.sh` | 全量测试 |
| `bash scripts/generate_report.sh` | 生成测试报告 |
| `bash scripts/warmup.sh` | 预热学习数据 |
| `bash scripts/user_dict.sh list` | 查看用户词典 |
| `bash scripts/user_dict.sh add <拼音> <中文>` | 添加词条 |

## 学习新词

```bash
# 方法1: 交互模式中选词自动学习
./run.sh
# 输入拼音 → 选择候选 → 自动学习

# 方法2: 批量学习
echo -e "woaixuexi\t我爱学习" > learn.txt
echo "dummy" | "$PINYIN_IME_CLI" --learn-file learn.txt "$PINYIN_IME_LM" "$PINYIN_IME_DICT"

# 方法3: 词典管理工具
bash scripts/user_dict.sh add woaixuexi 我爱学习
```

## 更多文档

- [安装指南](INSTALL.md) — 完整安装步骤
- [架构文档](docs/ARCHITECTURE.md) — 系统设计
- [测试报告](test_report.md) — 自动生成

## 用户词典

自定义短语，优先级最高：

```bash
# 添加
"$PINYIN_IME_CLI" --add-user "myname=张三" "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
"$PINYIN_IME_CLI" --add-user "gongsi=华为技术" "$PINYIN_IME_LM" "$PINYIN_IME_DICT"

# 列出
"$PINYIN_IME_CLI" --list-user "$PINYIN_IME_LM" "$PINYIN_IME_DICT"

# 删除
"$PINYIN_IME_CLI" --del-user "myname" "$PINYIN_IME_LM" "$PINYIN_IME_DICT"

# 使用（自动生效）
echo "myname" | "$PINYIN_IME_CLI" --quiet --top1 "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# → 张三
```

词典文件：`~/.config/pinyin-ime/abbr.txt`，格式 `pinyin=chinese`，每行一条。

## 联想预测

输入中文文本，预测下一个词：

```bash
echo "你好" | "$PINYIN_IME_CLI" --predict "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# → 世界	吗	的	不	了	。	，	呢	一	、
```

## 批量学习

从文件批量训练用户语言模型：

```bash
# 每行：拼音<TAB>中文
"$PINYIN_IME_CLI" --learn-file data/preset_learning.txt "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
```

## 从文本训练

从中文文本训练用户 LM：

```bash
"$PINYIN_IME_CLI" --train-text corpus.txt "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
```

## boost 机制

三层 boost 确保高频短语正确：

1. **完整匹配**：`shanghaishi → 上海市`（预设表直接命中）
2. **后缀 boost**：`woxiangshanghaishi → 我想上海市`（输入后缀匹配预设 key）
3. **前缀 boost**：`woxiangqubeijing → 我想去北京`（输入前缀匹配预设 key）

## 测试

```bash
bash scripts/smoke_test.sh       # 冒烟 24 项
bash scripts/test_edge_cases.sh  # 边界 39 项
bash scripts/test_stress.sh      # 压力 25 项
bash scripts/test_correction.sh  # 纠错 14 项
bash scripts/test_boost.sh       # boost 30 项
bash scripts/benchmark.sh        # 性能基准
```

## 性能

| 模式 | 延迟 | 吞吐 |
|------|------|------|
| 批量 | 15ms/条 | 65 条/秒 |
| 单次 | 43ms/条 | — |
| CLI 启动开销 | 38ms | — |
