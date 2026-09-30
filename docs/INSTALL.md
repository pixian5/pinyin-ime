# pinyin-ime 桌面安装指南

本文档指导在真实 Linux 桌面环境安装和使用 pinyin-ime 输入法。

## 前提条件

- Linux 桌面（X11 或 Wayland）
- fcitx5 已安装并运行
- libime 已安装（`libimecore` + `libimepinyin` 包）
- C++20 编译器（gcc 13+ 或 clang 16+）
- CMake 3.16+, Ninja

### 各发行版安装依赖

**Ubuntu/Debian:**
```bash
sudo apt install fcitx5 libimecore-dev libimepinyin-dev \
  cmake ninja-build g++ pkg-config libfcitx5core-dev \
  libfcitx5config-dev libfcitx5utils-dev
```

**Fedora/RHEL/openEuler:**
```bash
sudo dnf install fcitx5 libime-devel cmake ninja-build gcc-c++ \
  fcitx5-devel pkg-config
```

**Arch Linux:**
```bash
sudo pacman -S fcitx5 libime cmake ninja gcc pkgconf
```

## 安装步骤

### 1. 克隆仓库

```bash
git clone https://gitcode.com/laotongtian/pinyin-ime.git
cd pinyin-ime
```

### 2. 构建 CLI

```bash
mkdir -p cpp/build && cd cpp/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
ninja
sudo ninja install
cd ../..
```

验证：
```bash
echo nihao | pinyin_ime_cli --top1
# 应输出: 你好
```

### 3. 构建 fcitx5 addon

```bash
mkdir -p fcitx5-addon/build && cd fcitx5-addon/build
cmake .. -DCMAKE_BUILD_TYPE=Release -DCMAKE_INSTALL_PREFIX=/usr
ninja
sudo ninja install
cd ../..
```

这会安装：
- `/usr/lib/fcitx5/libpinyinime.so` — addon 共享库
- `/usr/share/fcitx5/addon/pinyinime.conf` — addon 元数据

### 4. 配置 fcitx5

**方法 A：命令行**
```bash
# 添加 pinyinime 到输入法列表
fcitx5-configtool
# 在界面中：添加输入法 → 搜索 "pinyin" → 选择 "Pinyin IME" → 添加
```

**方法 B：手动编辑 profile**
```bash
cat >> ~/.config/fcitx5/profile << 'EOF'

[Groups/0/Items/2]
Name=pinyinime
Layout=
EOF
```

### 5. 重启 fcitx5

```bash
fcitx5 -r
```

### 6. 验证

在任意文本输入框中：
- 按 `Ctrl+Space` 切换到 pinyinime
- 输入 `nihao` → 应显示候选 `你好`
- 按空格上屏

## 配置

配置文件位于 `~/.config/fcitx5/conf/pinyinime.conf`：

```ini
# 候选词数量（越大越准但越慢）
NBest=45

# 每页显示候选数
PageSize=9

# libime 数据目录
DataDir=/usr/share/libime

# 用户数据目录（自学习历史）
UserDataDir=~/.local/share/pinyin-ime

# 模糊音（平翘舌/前后鼻音/n-l）
EnableFuzzy=False

# 拼写纠错
EnableCorrect=False

# 双拼
EnableShuangpin=False
ShuangpinScheme=xiaohe
```

修改后重启 fcitx5 或在配置界面重新加载。

## 快捷键

| 快捷键 | 功能 |
|---|---|
| `Ctrl+Space` | 切换输入法（fcitx5 全局） |
| `Tab` | 中英文切换 |
| `Space` / `Enter` | 上屏首选候选 |
| `1`-`9` | 选择对应候选 |
| `,` / `.` | 候选上一页/下一页 |
| `PageUp` / `PageDown` | 翻页 |
| `←` / `→` | 逐词移动光标 |
| `Ctrl+←` / `Ctrl+→` | 移动候选焦点 |
| `Home` / `End` | 跳句首/句尾 |
| `Ctrl+E` | 表情选择模式 |
| `Ctrl+Z` | 撤回最近上屏 |
| `Ctrl+Shift+S` | 切换双拼 |
| `Esc` | 清空当前输入 |

## CLI 独立使用

无需 fcitx5，CLI 可独立使用：

```bash
# 行模式（管道）
echo nihao | pinyin_ime_cli --top1

# 交互模式（TTY）
pinyin_ime_cli

# 模糊音 + 纠错
pinyin_ime_cli --fuzzy --correct

# 双拼（小鹤）
pinyin_ime_cli --shuangpin

# 自学习
echo -e "woaizhongguo\t我爱中国" | pinyin_ime_cli --learn-file /dev/stdin

# 从中文文本训练用户LM
pinyin_ime_cli --train-text my_corpus.txt

# 联想预测
echo 你好 | pinyin_ime_cli --predict
```

## 故障排除

**addon 不加载：**
```bash
fcitx5-diagnose  # 检查 fcitx5 环境
# 确认 libpinyinime.so 在 /usr/lib/fcitx5/
# 确认 pinyinime.conf 在 /usr/share/fcitx5/addon/
```

**候选为空：**
```bash
# 检查 libime 数据
ls /usr/share/libime/zh_CN.lm /usr/share/libime/sc.dict
# 如果缺失，安装 libime-data 包
```

**准确率低：**
```bash
# 训练用户LM
pinyin_ime_cli --train-text ~/my_documents.txt
# 学习特定短语
echo -e "shanghaishi\t上海是" | pinyin_ime_cli --learn-file /dev/stdin
```
