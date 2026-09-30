# pinyin-ime 安装指南

## 环境要求

- Linux (x86_64 / aarch64)
- C++20 编译器 (GCC 12+ / Clang 15+)
- CMake 3.16+
- Ninja
- conda-forge / micromamba (用于构建 fcitx5 + libime)

## 快速开始

### 1. 克隆仓库

```bash
git clone https://gitcode.com/laotongtian/pinyin-ime.git
cd pinyin-ime
```

### 2. 环境设置

```bash
source ./setup_env.sh
```

### 3. 构建

```bash
# 构建 fcitx5 + libime (首次，约30分钟)
bash scripts/build_native.sh

# 构建 CLI + addon
bash scripts/build_cpp.sh
```

### 4. 预热学习数据

```bash
bash scripts/warmup.sh
```

### 5. 验证

```bash
# 运行演示
bash scripts/demo.sh

# 运行全部测试
bash scripts/ci_test.sh
```

## 使用方法

### CLI 行模式

```bash
# 基本用法
echo "nihao" | ./run.sh
# 输出: 你好

# 多候选
echo "nihao" | "$PINYIN_IME_CLI" --quiet "$PINYIN_IME_LM" "$PINYIN_IME_DICT" 5
# 输出: 你好	你号	你毫	拟好	你耗

# 模糊音
echo "cifan" | "$PINYIN_IME_CLI" --quiet --top1 --fuzzy "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# 输出: 吃饭

# 纠错
echo "senme" | "$PINYIN_IME_CLI" --quiet --top1 --correct "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# 输出: 什么

# 双拼（小鹤方案）
echo "nihc" | "$PINYIN_IME_CLI" --quiet --top1 --shuangpin --sp-scheme xiaohe "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# 输出: 你好

# JSON 输出
echo "nihao" | "$PINYIN_IME_CLI" --json "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
# 输出: ["你好","你号",...]

# 批量学习
echo -e "woaixuexi\t我爱学习" > learn.txt
echo "dummy" | "$PINYIN_IME_CLI" --learn-file learn.txt "$PINYIN_IME_LM" "$PINYIN_IME_DICT"
```

### CLI 交互模式

```bash
./run.sh
# 逐键输入，数字键选候选，空格/回车上屏
# Ctrl-E: 表情选择
# Ctrl-Z: 撤回上屏
# [/]: 翻页
# Home/End: 候选光标跳首/尾
```

### fcitx5 集成

```bash
# 构建 E2E 测试
bash scripts/build_e2e_test.sh

# 运行 E2E 测试
# (需要 fcitx5 server with testfrontend)
```

## 配置

### CLI 配置文件

位置: `~/.config/pinyin-ime/config`

```ini
lm_path=/path/to/zh_CN.lm
dict_path=/path/to/sc.dict
nbest=5
user_data=/home/user/.local/share/pinyin-ime
```

### 用户学习数据

位置: `~/.local/share/pinyin-ime/`

| 文件 | 说明 |
|------|------|
| user.history | 用户语言模型历史 |
| user.dict | 用户词典 |

CLI 和 fcitx5 addon 共享同一份数据。

## 故障排除

### 找不到语言模型

```
pinyin_ime_cli: 找不到语言模型（zh_CN.lm / sc.lm）
```

解决: 确认 `setup_env.sh` 正确设置路径，或手动指定:
```bash
echo "nihao" | "$PINYIN_IME_CLI" --top1 /path/to/zh_CN.lm /path/to/sc.dict
```

### pypinyin 未安装

```bash
pip install pypinyin
```

### 构建失败

```bash
# 检查工具链
which gcc cmake ninja micromamba

# 重新构建
bash scripts/build_native.sh 2>&1 | tee build.log
```

## 性能

- 吞吐量: 166+ 句/秒
- P50 延迟: 33ms/键
- P99 延迟: 38ms/键
- 内存: ~50MB (含34MB语言模型)
