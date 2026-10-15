# 只读 /usr 沙箱打通 X 图形层：Xvfb 的 /usr/bin/xkbcomp 用 LD_PRELOAD 重定向

- 时间：2026 年 10 月 15 日 22 时 46 分
- 机器：openEuler 22.03 LTS aarch64（114.116.247.146），/usr、/etc 只读 overlay，root 但无 CAP_SYS_ADMIN，无 conda/docker
- 版本：v0.9.94
- 结论：**第六台机器记录的「只读 /usr 无法验证 X 图形层」并非不可解**。用 LD_PRELOAD 垫片重定向硬编码路径即可。

## 现象

在只读 /usr 沙箱里启动 Xvfb：

```
sh: line 1: /usr/bin/xkbcomp: No such file or directory
XKB: Failed to compile keymap
Keyboard initialization failed. This could be a missing or incorrect setup of xkeyboard-config.
(EE) Fatal server error:
(EE) Failed to activate virtual core keyboard: 2(EE)
```

- 系统未装 xkbcomp，/usr/bin 只读无法放置；无 CAP_SYS_ADMIN 不能 bind-mount/unshare。
- Xvfb 二进制里该路径**编译期硬编码**：`strings Xvfb` 可见 `XKB... /usr/bin` 与 `"%s%sxkbcomp" ...`。

## 根因定位

- X server 的 `ddxLoad.c` 用 **`Popen`** 执行形如
  `"/usr/bin/xkbcomp" -w N ... -xkm ...` 的命令 → 实际是 `/bin/sh -c "..."`。
  故报错来自 `sh`，且 **xkbcomp 以绝对路径调用**，靠 PATH 无法覆盖。
- 因此要拦截的是 **`sh` 进程对 `/usr/bin/xkbcomp` 的 `execve`**。LD_PRELOAD 会被 `sh` 继承，故在 shim 里 hook `execve`（及 `execv/execvp/posix_spawn/p`）即可。

## 方案

新增 `tools/xkbcomp_redirect.c`（LD_PRELOAD 垫片）：仅当被 exec 的路径**精确等于** `/usr/bin/xkbcomp` 时，替换为 `XKBCOMP_REAL`（默认 `/workspace/sysroot/usr/bin/xkbcomp`）。

启动 Xvfb：

```bash
LD_PRELOAD=/workspace/xkbcomp_redirect.so \
XKBCOMP_REAL=/workspace/sysroot/usr/bin/xkbcomp \
LD_LIBRARY_PATH=/workspace/sysroot/usr/lib64 \
/workspace/sysroot/usr/bin/Xvfb :99 -screen 0 1280x800x24 -nolisten tcp \
  -xkbdir /workspace/sysroot/usr/share/X11/xkb
```

`-xkbdir` 让 xkbcomp 在 sysroot 找到键盘数据。

## 另外两个坑（fcitx5 X 模式）

1. **libxkbcommon 数据路径**：桌面 bundle 编译期指向 `/workspace/native-build/mamba-env/share/xkeyboard-config-2`（不存在）→ 需 `XKB_CONFIG_ROOT=<sysroot>/usr/share/X11/xkb`。
2. **classicui 段错误（Signal 11）**：`Fontconfig error: Cannot load default config file` 后崩溃 → 用 `FONTCONFIG_FILE` 指向自生成、包含 sysroot 字体的 `fonts.conf`（`FONTCONFIG_PATH` 一并设置）后稳定。
3. **xim 前端加载失败**：`libxcb-imdkit.so.1: cannot open shared object file` → 在 `RT/lib` 补 `libxcb-imdkit.so.1 -> libxcb-imdkit.so.1.0.9` 符号链接后 `Loaded addon xim`。
4. **addon 配置格式**：`conf/pinyinime.conf` 用**无 section 的裸键**（与 `fcitx5-addon/conf/pinyinime.conf` 一致）才被 `readAsIni` 识别；写 `[PinyinIMEConfig]` section 会导致 `EnableFuzzy/EnableCorrect` 不生效。

## 沙箱进程模型（重要经验）

本沙箱**每次工具调用是独立 bwrap/PID 命名空间**：

- 同一次调用内 `pkill/kill` 正常；
- **跨调用无法 kill 旧 Xvfb/fcitx5**（`kill` 报 `No such process`，但 `/proc` 仍可见）。
- 后果：重复启动会在同一 display 上累积多个 fcitx5 → `Failed to open xim, retrying` → 客户端 XOpenIM 连到死实例、无上屏。
- 对策：X 测试**必须单次调用内**「挑空闲 display → 启动 → 测试 → 收尾」；判活用 `pgrep -f "[X]vfb :$n"`（**括号技巧**），否则 `pgrep -f "Xvfb :$n"` 会**匹配到 pgrep 自身**造成「已在运行」的假阳性，从而漏启动 Xvfb。

## 验证

`scripts/repro_verify.sh` 五层全过（第 5 层 X11/XIM 9/9）。`scripts/xvfb_x11_ime_test.sh` 经 Xvfb + XIM 实测上屏：`nihao→你好`、`zhongguo→中国`、简拼 `zg→中国`、`bjdx→北京大学`、中英切换、中文标点、`zhongguo!→中国！`、`zongguo→中国`。
