# 候选焦点视觉高亮落地（setCursorIndex）—— 第 15 轮「焦点选字」补完

> 结论：第 15 轮实现了「候选窗内 Ctrl+←/→ 移动焦点、空格上屏焦点候选」的**上屏语义**
> （`candFocus_` 推进 + `selectCandidate(page*pageSize + candFocus_)`），但候选窗里
> classicui 的「高亮光标」一直**固定在第 1 个候选**（`setCursorIndex(0)` 写死），
> 视觉与语义脱节。本轮把高亮对齐到 `candFocus_`，并用截图验证高亮条随焦点水平移动。

## 实现（`fcitx5-addon/pinyinime.cpp` `updateUI`）

主候选列表（表情列表不受影响，emoji 无焦点移动）把：

```cpp
candidateList->setCursorIndex(0);
```

改为：

```cpp
candidateList->setCursorIndex(static_cast<int>(candFocus_));
```

- `candFocus_` 是页内索引（0..8），在 `keyEvent` 处已钳制到 `< end - begin`（第 535 行），
  且 `updateUI` 里 `end > begin` 才构造候选列表（避免空列表 + cursorIndex 触发 classicui
  `candidateFromIndex(0)` abort），故索引必有效。
- 翻页 / 新拼音 / 退格 / 纠错重输均已在各自分支把 `candFocus_` 归零，焦点回到页首时高亮同步回首位。

## 验证：截图像素比对（headless 沙箱做「视觉」验证的可行手法）

headless 无物理显示器，但 classicui 会把候选窗画进 Xvfb 的 X11 帧缓冲，可用 `XGetImage`
抓屏比对。本轮补了一个临时抓屏小工具（`/tmp/x11capture.c`，不入库），在 GTK3 客户端里逐帧：

| 状态 | 高亮条 X 区间（候选窗 Y=[149,177]，槽宽≈45px） |
|---|---|
| 焦点 0（默认） | [11, 55] —— 「一」 |
| 焦点 1（Ctrl+→ 一次） | [55, 100] —— 「以」 |
| 焦点 2（Ctrl+→ 两次） | [100, 145] —— 「已」 |

高亮背景条随 `candFocus_` 精确左/右移一个槽位，证明视觉高亮已对齐到语义焦点。

## 顺带修复：keysend_target.c 缺 Ctrl+←/→ 转义

`tools/keysend_target.c`（DBus+GTK 端到端测试的按键注入器）此前没有 `\l`/`\r` 转义，
遇到就会 `default: return` 提前终止脚本 —— 与 `tools/xim_test_client.c`（XIM 路径，早已支持
`\l`/`\r`）词汇不一致。首次截图验证时焦点按键根本没发出（shot1==shot2），正是这个坑。已补：

```cpp
case 'l': send_key(XK_Left, ControlMask); break;   /* Ctrl+Left（候选焦点左移） */
case 'r': send_key(XK_Right, ControlMask); break;  /* Ctrl+Right（候选焦点右移） */
```

两兄弟测试工具现在共语义一致，焦点用例可跨 XIM / DBus+GTK 双通道复现。

## 回归

`smoke_test.sh` 20/20、`test_interactive.py` 25/25、纠错对拍 exit 0、`xvfb_ime_test.sh` 20/20、
`xvfb_desktop_ime_test.sh` 10/10 —— 全绿（视觉高亮是纯 UI 层，不触碰 libime 解码管线）。

## 版本

`0.6.9 → 0.7.0`（VERSION + fcitx5-addon/CMakeLists.txt 同步）。