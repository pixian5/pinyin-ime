#!/bin/bash
# 在独立 dbus 会话里跑真实 fcitx5 addon 批量评测。
# 必须这样包一层：fcitx5、GTK im module 和测试前台要共享同一个 session bus。
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
exec dbus-run-session -- python3 "$ROOT/scripts/eval_addon_batch.py" "$@"
