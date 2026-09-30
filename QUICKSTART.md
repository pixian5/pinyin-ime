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
