# 真桌面复现缺口：keysend_target 未纳入构建脚本（第 17 轮首发现象）

> 结论：`xvfb_desktop_ime_test.sh`（DBus + GTK3 端到端）依赖独立按键注入器 `tools/keysend_target`，
> 但没有任何构建脚本自动编译它（`build_fcitx5_gtk.sh` 只编译了 `gtk_ime_test_client`）。曾在
> 第 8 / 13 轮用**手工 `cc`** 编过它，第 15 轮起 README 的合并复现命令块漏掉了这条手工命令，
> 导致全新沙箱从零复现时真桌面测试「0/10」。

## 现象

- 全新沙箱从零复现，`bash scripts/xvfb_desktop_ime_test.sh` → **0 通过 / 10 失败**，每个用例输出 `[]`。
- 而 XIM 通道 `xvfb_ime_test.sh` 是 **20/20 全绿**。

## 排查

1. fcitx5 启动日志正常：`pinyinime` 已加载 LM/dict、`dbusfrontend` 已加载、DBus 服务名
   `org.fcitx.Fcitx5 :1.0` 已注册、默认组含 `pinyinime` —— 引擎侧无误。
2. GTK3 immodule 注册正确：`/usr/lib64/gtk-3.0/3.0.0/immodules.cache` 指向 `im-fcitx5.so`
   绝对路径，`im-fcitx5.so` 依赖 `libFcitx5GClient.so.2` 可解析、无 `not found`。
3. 手动重跑一例，`keysend_target` 报 **`No such file or directory`** —— 二进制从未编译。
   按键从未送达 GTK 客户端，`inserted-text` 信号自然一次都不触发，输出恒为空。

## 复现路径差异（根因）

- `xvfb_ime_test.sh`（XIM）：`tools/xim_test_client` **自带**按键脚本 + XTest 发键，不依赖外部注入器。
- `xvfb_desktop_ime_test.sh`（DBus+GTK）：`gtk_ime_test_client` 只建窗口、收上屏；按键由**独立进程**
  `tools/keysend_target` 注入（`XSetInputFocus` + `XTestFakeKeyEvent`）—— 这是硬依赖。

`build_fcitx5_gtk.sh` 只编译了 `gtk_ime_test_client`，没编 `keysend_target`；
README 第 16 轮合并复现命令块把第 8/13 轮的手工 `cc tools/keysend_target.c ...` 丢了。

## 修复

- `scripts/build_fcitx5_gtk.sh` 尾部补 `gcc -O2 -o tools/keysend_target tools/keysend_target.c -lX11 -lXtst`，
  与 `gtk_ime_test_client` 一起固化，并加中文注释说明它是 `xvfb_desktop_ime_test.sh` 的硬依赖。
- 修完重跑 `xvfb_desktop_ime_test.sh` → **10/10 全绿**。

## 教训

- 「从零复现命令块」合并时要**逐条核对依赖二进制**是否仍在某个构建脚本里产出；
  手工 `cc` 过的辅助工具（keysend_target / xim_test_client / gtk_ime_test_client）最易漏。
- "0/N 且输出为空" 优先怀疑「输入根本没送达」（注入器缺失/未编译/未起），再排查 im module、DBus、
  引擎解码 —— 与第 12 轮「先查数据是否隔离再归因算法」同类的排查顺序问题。

## 版本

- `0.6.8 → 0.6.9`（VERSION + fcitx5-addon/CMakeLists.txt 同步）。