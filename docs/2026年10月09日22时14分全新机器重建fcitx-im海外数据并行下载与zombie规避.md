# 全新机器重建：fcitx-im 海外数据用代理并行下载 + 不可杀 zombie 规避

> 时间：2026-10-09 22:14
> 场景：在一台**全新** openEuler 22.03 aarch64 沙箱（无 conda/mamba、无 fcitx5/libime、无历史构建产物）
> 从零把 pinyin-ime 重建到「24/24 冒烟全通过」。记录三个关键坑与解法。

## 背景

上一轮的构建产物在别的机器上，本机是干净环境。核心依赖（fcitx5 5.1.23 + libime 1.1.17）
需从源码编译；libime 的 `ENABLE_DATA=On` 会**额外从 fcitx 官方海外服务器
`download.fcitx-im.org` 下载三个数据包**（无国内镜像）：

| 文件 | 大小 | SHA256（前16位） |
|---|---|---|
| lm_sc.arpa-20260629.tar.zst | 74.1MB | `06808333b9173e53` |
| dict-20260907.tar.zst | 2.2MB | `fb75a179065e690d` |
| table-20240108.tar.zst | 4.4MB | `3e9d87b04a393f13` |

## 坑 1：`/` 是 tmpfs 仅 7.2G，构建目录必须落到大盘

沙箱根分区 `/` 是 tmpfs（7.5G 总 / 7.2G 可用），装不下 conda 环境 + 源码 + 构建树。
而 `/workspace` 是 98G overlay（79G 可用）。

**解法**：把默认 `NB=/root/workspace/native-build`（落在小 tmpfs）用**符号链接**指向大盆，
保持所有硬编码该路径的脚本兼容：

```bash
mkdir -p /workspace/native-build /root/workspace
ln -sfn /workspace/native-build /root/workspace/native-build
```

## 坑 2：fcitx-im 海外数据直连 ~20KB/s，用本地代理多连接并行下载

`download.fcitx-im.org` 无国内镜像。实测直连 ~20KB/s（74MB 要 1 小时+）。
本地 7890 端口有 Hysteria2 代理：单连接 ~50KB/s，**4~8 连接并行可聚合到 ~176KB/s**。

**解法**：写 `/tmp/opencode/pget.py` 多连接并行下载器，关键点：
1. 按字节区间切 N 段，N 个连接经代理并行下载到 `.parts/` 分片；
2. **段内断点续传**——每段用「追加式 Range」（`-r (段起+当前已写字节)-段止` + stdout 追加），
   连接中断后从当前字节继续，而非整段从头重下（否则不稳链路上永远不收敛）；
3. 拼接后 `sha256sum` 严格校验，匹配官方 EXPECTED_HASH 才算成功。

把三个数据包预置到 `$NB/src/libime/data/` 后再跑构建，cmake 的 `lm-download` / `dict-download`
脚本会检测到「文件存在且哈希匹配」而**跳过下载**，直接进入 ARPA 提取 + 二进制 LM 转换。

## 坑 3：构建进程在 supervisor 命名空间，`kill` 杀不掉（zombie）

后台 `nohup full_build.sh` 派生的 `ninja` / `cmake -P ...download.cmake` 落在**父级 supervisor
PID 命名空间**。`ps`/`/proc` 能看到，但本命名空间 `kill <pid>` 报 `No such process`
（本命名空间 kill 自己 spawn 的子进程是正常的，已验证）。`pkill -f` 也无效。

**后果**：无法中途停掉卡在慢速下载上的 zombie ninja/cmake；若它占着 `$NB/build/libime`
构建目录，重新 `full_build.sh` 会双 ninja 冲突。

**规避**：
- 方案 A（本次采用）：**增量续建**。zombie 下载到的是我 `rm` 掉目标路径后残留的**幽灵 inode**
  （fd 仍指向已 unlink 的文件），不影响真实路径。等它自行失败退出后，真实构建目录空闲，
  直接 `ninja -C $NB/build/libime` 增量续建——已编译的 .so 复用，只跑数据转换 + 安装，最快。
- 方案 B（备选）：换**全新构建目录**（如 `build/libime_rt`）重编 libime，彻底避开 zombie。

## 结果

增量续建成功：ARPA 提取 + sc.lm 二进制转换 SUCCESS，安装 zh_CN.lm（34.7MB）+ sc.dict（4.5MB）；
CLI `pinyin_ime_cli` 与 addon `libpinyinime.so` 编译通过；**冒烟测试 24/24 全通过**
（全拼/简拼/模糊音/双拼/CLI 功能），真实输入 nihao→你好、zhongguojiaoyu→中国教育、
shurufa→输入法、kexuejishu→科学技术 均正确。

## 复现清单（全新 aarch64 机器）

```bash
# 1. 构建目录落大盘
mkdir -p /workspace/native-build /root/workspace && ln -sfn /workspace/native-build /root/workspace/native-build
# 2. 一键构建（conda 镜像 BFSU + 源码 gh-proxy.com + 数据 fcitx-im 走代理）
bash scripts/full_build.sh          # 卡在数据下载时见「坑2/坑3」
# 3. 数据包用 pget.py 并行预置到 $NB/src/libime/data/ 后：
ninja -C $NB/build/libime && ninja -C $NB/build/libime install   # 增量续建
# 4. 编 CLI + addon（见 full_build.sh §8/§9），跑冒烟
NB=$NB bash scripts/smoke_test.sh
```
