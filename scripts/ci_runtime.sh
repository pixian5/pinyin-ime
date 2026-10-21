#!/bin/bash
# ci_runtime.sh — 在具备 Xvfb/DBus 的 aarch64 机器上跑完整运行时冒烟。
# 需要已配置 rclone 的 cst 远程，或本地已缓存 S3 产物。
set -euo pipefail
ROOT="$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)"
cd "$ROOT"

bash scripts/setup_sandbox_deps.sh
bash scripts/run_prebuilt.sh start
trap 'bash scripts/run_prebuilt.sh stop || true' EXIT
bash scripts/run_prebuilt.sh test
bash scripts/run_prebuilt.sh gui
