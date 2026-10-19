# 2026年10月19日08时30分-GTK-immodule-真实桌面GUI候选窗截图验证-Hermes

## 本轮目标

在 Ubuntu 24.04 x86_64 上完成「真实桌面 GUI」的完整视觉验证：GTK3 应用通过 fcitx5 IM module 通道输入拼音、弹出候选窗、上屏中文，并截图确认候选窗和上屏文字真实渲染。

## 一、GTK immodule e2e（真实桌面输入链路）

与上一轮 XIM 不同，本轮走 GTK3 的 IM module 通道：`GTK_IM_MODULE=fcitx` → `im-fcitx5.so`（系统 fcitx5-frontend-gtk3 自带）→ DBus → fcitx5 守护进程 → 自研 pinyinime addon。这是浏览器/编辑器等真实桌面应用的输入路径。

- 编译 `tools/gtk_ime_test_client.c`（GtkEntry 应用，每次上屏打印 `GTK_COMMIT:<text>`）→ 用 `tools/keysend_target` / xdotool 注入按键。
- **踩坑 1：DBus 地址**。`/tmp/dbus-93.env` 必须存 `DBUS_SESSION_BUS_ADDRESS=`（不是 `DBUS=`），否则 GTK 客户端连不上 fcitx5。
- **踩坑 2：GTK 客户端默认 30 秒超时**（`g_timeout_add_seconds`），连续测试要传更长参数（如 `gtk_ime_test_client 180`）。
- **踩坑 3：xdotool type 注入不经过 IM 键处理**（标点会输出半角），改用 `xdotool key comma/period/question` 直接按物理键才能触发中文标点转换。
- **踩坑 4：每个用例必须独立启动 GTK 客户端**，否则 `\e` 清空后首条输入偶发丢失、用例间状态互相污染。
- **踩坑 5：`\S`（Ctrl+Space）在 GTK 里不可靠**（GtkEntry 可能拦截），改用 `xdotool key ctrl+space` 切中文，之后所有用例不再切换。

最终结果（xdotool 注入，每用例独立客户端）：

| 用例 | 输入 | 上屏 | 结果 |
|---|---|---|---|
| 全拼 | nihao | 你好 | ✅ |
| 全拼 | zhongguo | 中国 | ✅ |
| 简拼 | zg | 中国 | ✅ |
| 简拼 | bjdx | 北京大学 | ✅ |
| 长句 | woshiyige | 我是一个 | ✅ |
| M3 纠错 | senme | 什么 | ✅ |
| M3 纠错 | zhing | 中 | ✅ |
| M3 纠错 | zongguo | 中国 | ✅ |
| 标点 | comma | ， | ✅ |
| 标点 | period | 。 | ✅ |
| 标点 | question | ？ | ✅ |

**GTK immodule e2e：11/11 全过。**

## 二、GUI 候选窗视觉验证（截图）

编译并运行 `tools/ime_gui_demo.c`（真实 X 应用窗口，libXft + fontconfig 渲染中文，走 XIM 通道）：
- 编译命令：`gcc -O2 -o ime_gui_demo ime_gui_demo.c -lX11 -lXtst -lXft -lfontconfig -lfreetype $(pkg-config --cflags xft fontconfig freetype2)`（本机有完整 X 开发库，无需沙箱 sysroot）。
- fcitx5 classicui 已加载（日志 `Loaded addon classicui`），Noto CJK / wqy 中文字体可用。

程序自动完成：`Ctrl+Space 激活 → 输入 nihao → 候选窗出现 → 截图 → 空格上屏「你好」→ 截图 → 再输入 shijie 上屏「世界」→ 窗口显示「你好世界」→ 截图`，退出码 0。

截图（`screenshots/gui-104-*.png`）经 tesseract OCR 验证：
- `gui-104-preedit-nihao.png`：候选窗 OCR 识别出 `ni hao` 预编辑 + 9 个中文候选（`1.你好 2.拟好 3.你号 4.拟号 5.泥好 ...`）
- `gui-104-commit-nihao.png`：窗口上屏 `你好`
- `gui-104-commit-nihaoshijie.png`：窗口上屏 `你好世界`
- `gui-104-candidate-zone-diff.png` / `gui-104-candidate-zoom.png`：候选窗差异区域放大图

**结论：候选窗真实渲染、拼音预编辑正确、候选列表正确、空格上屏正确、连续上屏正确。**

## 三、版本与产物

- 版本 0.9.104 → **0.9.105**（VERSION、fcitx5-addon/CMakeLists.txt）。
- 新增截图 5 张（screenshots/gui-104-*.png）。
- 本轮无源码修改（GTK 客户端和 ime_gui_demo 均为既有工具在本机的首次成功编译运行）。

## 四、待办与优化方向

- [ ] 把本机 GTK e2e 固化为 `scripts/gtk_ime_e2e_ubuntu.sh`（DBus 地址、超时、xdotool 注入、独立客户端等踩坑都写进去）。
- [ ] 把 GUI 截图验证固化为脚本（可选：接入 CI 截图存档）。
- [ ] x86_64 的 .deb/.rpm 打包（S3 目前是 aarch64 包）。
- [ ] libime 升级 1.1.17 恢复 L_R/Correction/GB/setCursorIndex 完整能力。
