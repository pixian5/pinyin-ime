#!/usr/bin/env bash
# start_fcitx5_dev.sh — 在本机（用户级安装）启动带 pinyin-ime addon 的 fcitx5
#
# 为什么需要这个脚本：
#   1. fcitx5 不会自动搜索 ~/.local/lib/fcitx5，必须显式给 FCITX_ADDON_DIRS，
#      否则日志只有 "Could not locate library libpinyinime.so for addon pinyinime"。
#   2. fcitx5-remote / 配置热加载都走 DBus，脚本会从桌面会话进程里取
#      DBUS_SESSION_BUS_ADDRESS，避免在非登录 shell 里操作不到那个实例。
#   3. addon 必须在输入法组里，否则 fcitx5 退出时会改写 profile 把 pinyinime 丢掉。
#
# 用法：
#   bash scripts/start_fcitx5_dev.sh            # 启动（若已在跑则先停）
#   bash scripts/start_fcitx5_dev.sh --restart  # 强制重启
set -u

ADDON_DIR="${HOME}/.local/lib/fcitx5"
SYS_ADDON_DIR="/usr/lib/x86_64-linux-gnu/fcitx5"
DISPLAY_NUM="${DISPLAY_NUM:-:1}"
PROFILE="${HOME}/.config/fcitx5/profile"

# 1) 从桌面会话里捞 DBus 地址（fcitx5 自己的环境里就有）
pick_dbus() {
    if [ -n "${DBUS_SESSION_BUS_ADDRESS:-}" ] && fcitx5-remote >/dev/null 2>&1; then
        return 0
    fi
    for pid in $(pgrep -u "$(id -u)" -f "xfce4-session|gnome-session|fcitx5" 2>/dev/null); do
        addr=$(tr '\0' '\n' < "/proc/${pid}/environ" 2>/dev/null | sed -n 's/^DBUS_SESSION_BUS_ADDRESS=//p' | head -1)
        if [ -n "${addr}" ]; then
            export DBUS_SESSION_BUS_ADDRESS="${addr}"
            return 0
        fi
    done
    return 1
}

pick_dbus || echo "警告：没找到会话 DBus 地址，fcitx5-remote 可能不可用"

export DISPLAY="${DISPLAY_NUM}"
export XMODIFIERS="@im=fcitx"
export GTK_IM_MODULE=fcitx
export QT_IM_MODULE=fcitx
export FCITX_ADDON_DIRS="${ADDON_DIR}:${SYS_ADDON_DIR}"
export LANG="${LANG:-zh_CN.utf8}"

# 2) 停掉旧实例
for p in $(pgrep -x fcitx5); do kill "$p" 2>/dev/null; done
sleep 1

# 3) 保证 profile 里有 pinyinime（fcitx5 退出会重写 profile 把不可用 addon 丢掉）。
#    这里直接按固定模板重写：落在 [GroupOrder] 之前的正确位置，避免手动追加到文末。
mkdir -p "$(dirname "${PROFILE}")"
cat > "${PROFILE}" <<'EOF'
[Groups/0]
# Group Name
Name=Default
# Layout
Default Layout=us
# Default Input Method
DefaultIM=pinyinime

[Groups/0/Items/0]
# Name
Name=keyboard-us
# Layout
Layout=

[Groups/0/Items/1]
# Name
Name=pinyinime
# Layout
Layout=

[GroupOrder]
0=Default
EOF

# 4) 启动（必须重定向：fcitx5 -d fork 后仍持有 stdout，脚本接管道时会被挂住）
fcitx5 -d >/dev/null 2>&1
sleep 2
fcitx5-remote -o >/dev/null 2>&1
fcitx5-remote -s pinyinime >/dev/null 2>&1

echo "fcitx5 已启动：state=$(fcitx5-remote 2>/dev/null) im=$(fcitx5-remote -n 2>/dev/null)"
echo "addon 目录：${FCITX_ADDON_DIRS}"
