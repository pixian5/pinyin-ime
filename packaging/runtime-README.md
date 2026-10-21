# pinyin-ime 便携运行时

本目录包含 fcitx5 5.1.23、libIME 1.1.17、pinyinime addon、CLI、LM/dict 和中文字体，
不写系统 `/usr`、`/etc`，适合只读系统、无 Docker 的 aarch64 Linux。

## 启动

```bash
./scripts/start.sh
```

启动器会：

- 使用 bundle 内的 fcitx5 / libIME / 依赖库
- 在 `./config` 生成隔离的 fcitx5 配置（不覆盖系统配置）
- 使用 `./share/libime/libime/zh_CN.lm` 和 `sc.dict`
- 使用 `./share/fonts` 的中文字体
- 启动 fcitx5 并激活输入法

进入桌面后按 `Ctrl+Space` 切换中英文。停止：

```bash
./scripts/stop.sh
```

## 无桌面环境

需要先有 X11 + DBus。可以用 Xvfb：

```bash
Xvfb :99 -screen 0 1280x1024x24 &
export DISPLAY=:99
eval "$(dbus-launch --sh-syntax)"
./scripts/start.sh
```

## CLI

```bash
LD_LIBRARY_PATH="$PWD/opt/deps-lib:$PWD/opt/fcitx5-x11/lib:$PWD/lib" \
  ./bin/pinyin_ime_cli ./share/libime/libime/zh_CN.lm ./share/libime/libime/sc.dict
```

## 版本与校验

- 版本：见 `VERSION`
- 校验：发布页的 `SHA256SUMS`

## 已知限制

- 当前只验证 X11/XIM；Wayland 未验证。
- 便携运行时的配置在 bundle 内，和系统 fcitx5 配置隔离。
- 需要目标机提供基础 X11/DBus 会话；bundle 不包含 Xvfb/DBus。
