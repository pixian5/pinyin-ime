# pinyin-ime 安装指南

## 快速安装（从 S3 下载预构建产物）

> 适用于 openEuler 22.03 aarch64，已预构建好的二进制可直接下载使用。
> 最新版本：**v0.9.77**（含 fcitx5 桌面集成 + Web 演示 + 纠错模块）

### 1. 安装 rclone

```bash
curl -O https://downloads.rclone.org/rclone-current-linux-arm64.zip
unzip rclone-current-linux-arm64.zip
cp rclone-*-linux-arm64/rclone /usr/local/bin/
```

### 2. 配置 S3

```bash
mkdir -p ~/.config/rclone
cat > ~/.config/rclone/rclone.conf << 'CONF'
[cst]
type = s3
endpoint = https://s3.cstcloud.cn
access_key_id = <your-access-key-id>
secret_access_key = <your-secret-access-key>
force_path_style = true
acl = private
provider = Other
CONF
```

### 3. 下载构建产物

```bash
# 下载全部文件（源码包 + 二进制包 + CLI + addon + fcitx5 + LM + dict + SHA256）
rclone copy cst:xbz/pinyin-ime/v0.9.77/ ./pinyin-ime/ \
    --user-agent "rclone/1.75.1 pinyin-ime"

# 或使用一键下载脚本
bash scripts/download_from_s3.sh cst v0.9.77
```

### 4. 校验完整性

```bash
cd pinyin-ime
sha256sum -c SHA256SUMS_v0.9.77
```

### 5. 运行

```bash
# 方式一：CLI 命令行（需要 libime 运行时库）
export LD_LIBRARY_PATH=/path/to/libime/lib:/path/to/fcitx5/lib
echo "nihao" | pinyin_ime_cli_v0.9.77 zh_CN.lm sc.dict
# 输出: 你好

# 方式二：解压二进制发行包（含 CLI + addon + LM + dict）
tar xzf pinyin-ime-0.9.77-bin.tar.gz
cd pinyin-ime-0.9.77-bin
echo "zhongguo" | bin/pinyin_ime_cli share/libime/zh_CN.lm share/libime/sc.dict
# 输出: 中国

# 方式三：fcitx5 桌面输入法（需要 fcitx5 运行时 + X11 环境）
# 将 addon 放入 fcitx5 addon 目录，配置 DataDir 指向 LM/dict 所在路径
cp libpinyinime_v0.9.77.so ~/.local/share/fcitx5/addon/  # 或系统 addon 目录
# 在 fcitx5 配置中添加 pinyinime 输入法

# 方式四：Web 演示
tar xzf pinyin-ime-0.9.77.tar.gz
cd pinyin-ime
python3 scripts/web_demo_v2.py --port 8080
# 浏览器访问 http://localhost:8080
```

### 产物清单（v0.9.77）

| 文件 | 大小 | 说明 |
|------|------|------|
| pinyin-ime-0.9.77.tar.gz | 64MB | 源码 tarball（297 文件） |
| pinyin-ime-0.9.77-bin.tar.gz | 33MB | 二进制发行包（CLI + addon + LM + dict） |
| pinyin_ime_cli_v0.9.77 | 750KB | CLI 前端二进制 |
| libpinyinime_v0.9.77.so | 831KB | fcitx5 输入法 addon |
| fcitx5_v0.9.77 | 439KB | fcitx5 服务（X11+DBus+Server 全功能） |
| zh_CN.lm | 34MB | 4-gram 语言模型 |
| sc.dict | 4.3MB | 拼音词典 |
| SHA256SUMS_v0.9.77 | 595B | SHA256 校验文件 |

### S3 路径结构

```
cst:xbz/pinyin-ime/
  ├── v0.9.49/    （历史版本）
  ├── v0.9.65/
  ├── v0.9.66/
  ├── v0.9.68/
  ├── v0.9.71/
  └── v0.9.77/    （最新版本）
      ├── pinyin-ime-0.9.77.tar.gz        # 源码包
      ├── pinyin-ime-0.9.77-bin.tar.gz    # 二进制包
      ├── pinyin_ime_cli_v0.9.77          # CLI
      ├── libpinyinime_v0.9.77.so         # fcitx5 addon
      ├── fcitx5_v0.9.77                  # fcitx5 服务
      ├── zh_CN.lm                        # 语言模型
      ├── sc.dict                         # 词典
      └── SHA256SUMS_v0.9.77             # 校验文件
```

### 上传新版本到 S3

```bash
# 构建完成后上传
bash scripts/upload_to_s3.sh 0.9.77
```

---

## 从源码构建


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
