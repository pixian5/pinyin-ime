# 2026年10月19日08时45分-云电脑XFCE桌面部署pinyinime输入法-Hermes

## 本轮目标

把 pinyin-ime fcitx5 输入法部署到**云电脑桌面（:1，XFCE 4）**，让用户在 noVNC 网页里直接可用中文输入。全程不重装 XFCE（云电脑模式已内置）。

## 一、云电脑模式现状（检查结论）

云电脑服务**原本就在运行**，无需再连/装 XFCE：
- X 显示 :1（X.Org 11.0）✅
- x11vnc VNC 服务：5900（密码/共享）、5901（只读）、5903（无密码本机）✅
- noVNC 网页入口：websockify 0.0.0.0:6080，/vnc.html 可访问（HTTP 200）✅
- 桌面会话：`dbus-run-session -- startxfce4`（xfce4-session + xfwm4 + xfce4-panel）✅
- 会话 DBus 地址：从 xfce4-session 进程环境读取 `DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/dbus-iBh7LGTmAr,...`

## 二、部署步骤

1. **确认 addon 已就位**（上一轮已放到用户级目录）：
   - `~/.local/lib/fcitx5/libpinyinime.so`
   - `~/.local/share/fcitx5/addon/pinyinime.conf`
2. **写配置**（`~/.config/fcitx5/conf/pinyinime.conf`）：
   ```
   DataDir=/usr/share/libime
   UserDataDir=/tmp/pinyin-ime-desktop1
   EnableCorrect=True
   ```
3. **写 profile**（`~/.config/fcitx5/profile`，DefaultIM=pinyinime，杀 fcitx5 后再写防覆盖）：
   ```
   [Groups/0] Name=Default  Default Layout=us  DefaultIM=pinyinime
   [Groups/0/Items/0] Name=keyboard-us
   [Groups/0/Items/1] Name=pinyinime
   ```
4. **启动 fcitx5 接入 XFCE 会话 DBus**（关键：不能用测试用的 :93 bus，必须用 XFCE 的）：
   ```bash
   export DISPLAY=:1
   export DBUS_SESSION_BUS_ADDRESS=unix:path=/tmp/dbus-iBh7LGTmAr,guid=...
   export XMODIFIERS=@im=fcitx GTK_IM_MODULE=fcitx QT_IM_MODULE=fcitx
   export FCITX_ADDON_DIRS="$HOME/.local/lib/fcitx5:/usr/lib/x86_64-linux-gnu/fcitx5"
   nohup fcitx5 -d > /tmp/fcitx5-desktop1.log 2>&1 &
   ```
   日志确认：`pinyinime: loaded lm=... dict=...`、`Loaded addon pinyinime`、`Found 1 input method(s)`。
5. **持久化环境变量**（`~/.xprofile` + `~/.profile` 追加 + `~/.config/environment.d/input-method.conf`）：
   ```
   export XMODIFIERS=@im=fcitx
   export GTK_IM_MODULE=fcitx
   export QT_IM_MODULE=fcitx
   export LANG=zh_CN.utf8
   export LC_ALL=zh_CN.utf8
   ```

## 三、桌面实测（:1 + XFCE DBus + GTK 应用）

用 `tools/gtk_ime_test_client` 在 :1 上创建 GtkEntry 窗口，xdotool 注入按键：

| 用例 | 输入 | 上屏 | 结果 |
|---|---|---|---|
| 全拼 | nihao | 你好 | ✅ |
| 全拼 | zhongguo | 中国 | ✅ |
| 简拼 | zg | 中国 | ✅ |
| 简拼 | bjdx | 北京大学 | ✅ |
| M3 纠错 | senme | 什么 | ✅ |
| M3 纠错 | zhing | 中 | ✅ |

**云电脑桌面（:1）GTK e2e：6/6 全过。**

## 四、注意事项

- `~/.xprofile` 原本是个空目录，需先 `rmdir` 再写文件。
- fcitx5 的配置 `~/.config/fcitx5/profile` 在启动时可能被重写为内置 pinyin——务必先杀旧 fcitx5 再写 profile。
- 云电脑已有 fcitx5 测试实例跑在 :93（旧测试显示），不要误以为它是桌面的；桌面实例是 `fcitx5 -d` 且环境里 DISPLAY=:1。

## 五、版本与产物

- 版本 0.9.105 → **0.9.106**（VERSION、fcitx5-addon/CMakeLists.txt）。
- 本轮无新增文件（部署是系统配置 + 启动服务），文档记录部署步骤。
