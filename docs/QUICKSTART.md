# pinyin-ime 快速开始

## 30秒上手

```bash
# 交互模式：直接打字
pinyin_ime_cli

# 行模式：管道使用
echo nihao | pinyin_ime_cli --top1
```

## 常用功能

### 1. 基本输入
```bash
echo nihao       | pinyin_ime_cli --top1   # 你好
echo zhongguo    | pinyin_ime_cli --top1   # 中国
echo beijingdaxue | pinyin_ime_cli --top1  # 北京大学
```

### 2. 简拼（首字母缩写）
```bash
echo bjdx | pinyin_ime_cli --top1   # 北京大学
echo zg   | pinyin_ime_cli --top1   # 中国
```

### 3. 模糊音（平翘舌、前后鼻音、n/l）
```bash
echo cifan    | pinyin_ime_cli --fuzzy --top1    # 吃饭 (ci→chi)
echo lanfang  | pinyin_ime_cli --fuzzy --top1    # 南方 (l→n)
```

### 4. 拼写纠错
```bash
echo senme   | pinyin_ime_cli --correct --top1   # 什么 (senme→shenme)
echo zongguo | pinyin_ime_cli --correct --top1   # 中国 (zong→zhong)
```

### 5. 双拼（8种方案）
```bash
# 小鹤双拼
echo vsgo | pinyin_ime_cli --shuangpin --sp-scheme xiaohe --top1  # 中国

# 自然码双拼
echo vsgo | pinyin_ime_cli --shuangpin --sp-scheme ziranma --top1  # 中国

# 可选方案：xiaohe ziranma ms ziguang abc zhongwenzhixing pinyinjiajia gb
```

### 6. 用户自学习
```bash
# 学习特定短语
echo -e "woaizhongguo\t我爱中国" | pinyin_ime_cli --learn-file /dev/stdin

# 从中文文本训练
pinyin_ime_cli --train-text my_article.txt

# 查看已学习内容
pinyin_ime_cli --dump-user
```

### 7. 联想预测
```bash
echo 你好 | pinyin_ime_cli --predict
# 输出联想词：吗、啊、世界、中国...
```

### 8. 批量处理
```bash
# 创建输入文件
cat > input.txt << 'EOF'
nihao
shijie
zhongguo
EOF

# 批量转换
pinyin_ime_cli --batch input.txt --top1
```

### 9. JSON 输出（供程序调用）
```bash
echo nihao | pinyin_ime_cli --json
# ["你好","你毫","你号","拟好","你耗"]
```

### 10. 表情输入
```bash
# 交互模式中按 Ctrl+E 进入表情选择
# 1-9 选表情，逗号/句号翻页，Esc 退出
```

## fcitx5 桌面使用

### 安装
```bash
./scripts/install.sh
```

### 配置
```bash
fcitx5-configtool
# 添加输入法 → 搜索 "pinyin" → 选择 "Pinyin IME" → 添加
```

### 快捷键
| 键 | 功能 |
|---|---|
| Ctrl+Space | 切换输入法 |
| Tab | 中英文切换 |
| Space/Enter | 上屏首选 |
| 1-9 | 选候选 |
| ,/. | 翻页 |
| Ctrl+E | 表情 |
| Ctrl+Z | 撤回 |

## 配置文件

`~/.config/fcitx5/conf/pinyinime.conf`:
```ini
NBest=45           # 候选数
PageSize=9         # 每页显示
EnableFuzzy=False  # 模糊音
EnableCorrect=False # 纠错
EnableShuangpin=False # 双拼
ShuangpinScheme=xiaohe  # 双拼方案
```

## 性能

| 指标 | 值 |
|---|---|
| 吞吐量 | 168 句/秒 |
| 延迟 P50 | 32 ms/键 |
| 准确率 | 97.5% (日常40句) |
