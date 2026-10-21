#!/bin/bash
# 停止便携运行时启动的 fcitx5；只匹配 bundle 内的二进制，避免误杀系统输入法。
set -uo pipefail

ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
stopped=0

if [ -f "$ROOT/fcitx5.pid" ]; then
    if kill "$(cat "$ROOT/fcitx5.pid")" 2>/dev/null; then
        stopped=1
    fi
    rm -f "$ROOT/fcitx5.pid"
fi

if command -v pgrep >/dev/null 2>&1; then
    for p in $(pgrep -x fcitx5 2>/dev/null || true); do
        exe="$(readlink "/proc/$p/exe" 2>/dev/null || true)"
        case "$exe" in
            "$ROOT"/*)
                # 沙箱容器 /proc 可能显示宿主 PID，NSpid 才是本命名空间可用的 PID。
                inner="$(awk '/^NSpid:/{print $NF}' "/proc/$p/status" 2>/dev/null || true)"
                if kill "${inner:-$p}" 2>/dev/null || kill "$p" 2>/dev/null; then
                    stopped=1
                fi
                ;;
        esac
    done
fi

if [ "$stopped" = 1 ]; then
    echo "已停止 bundle 内的 fcitx5"
else
    echo "bundle 内没有运行中的 fcitx5"
fi
