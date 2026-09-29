# 第20轮复现踩坑：conda-forge 通道镜像选择 + NB 默认值 split-brain 修复

> 2026年09月29日19时51分 · 全新 headless 沙箱（openEuler 22.03 aarch64）从零复现记录。

## 1. conda-forge 通道镜像：华为云 anaconda 镜像**不含** conda-forge，只有 tuna 真正镜像

用户要求「下载外国文件优先走镜像源（华为云 > 阿里云 > 腾讯云 > 教育联合 > gh-proxy）」。逐个实测
各镜像的 conda-forge 通道 repodata（`.../anaconda/cloud/conda-forge/linux-aarch64/repodata.json.zst`）：

| 镜像 | 结果 | 说明 |
|---|---|---|
| 华为云 `mirrors.huaweicloud.com/anaconda/cloud/conda-forge` | ❌ 返回 HTML 跳转页 | 华为云 anaconda 镜像只覆盖 `repo.anaconda.com` 的 main 通道，**不覆盖** `conda.anaconda.org/conda-forge` 云通道 |
| 阿里云 `mirrors.aliyun.com/anaconda/cloud/conda-forge` | ❌ 404 | 无此路径 |
| 腾讯云 `mirrors.cloud.tencent.com/anaconda/cloud/conda-forge` | ❌ 404 | 无此路径 |
| 清华 tuna `mirrors.tuna.tsinghua.edu.cn/anaconda/cloud/conda-forge` | ✅ 26MB repodata.json.zst 二进制 | **唯一真正镜像 conda-forge 的国内源** |
| 教育联合 `mirrors.cernet.edu.cn` | ⚠️ 302 | 重定向，未采用 |

**结论与教训**：`conda-forge` 的规范源是 `conda.anaconda.org`，与 Anaconda 官方 `repo.anaconda.com`
是**两个不同主机**；镜像方对两者的覆盖并不一致。误以为「anaconda 镜像」就等于「conda-forge 可用」
是错误假设，刷 repodata 时报 `ZSTD decompression error: Unknown frame descriptor`（实际是
HTML 页面被当成了 zst 解压）。**本项目的 conda-forge 通道固定用 tuna**（仍满足「国内镜像」要求），
已写入 `/root/.pinyin_build_env`。华为云镜像仍是其他有覆盖的场景（如 pkgs/main）的首选。

## 2. NB 默认值 split-brain 修复（本轮真正的代码改动）

各脚本的构建根目录 `NB` 默认值历史上不一致：

- 核心构建链（`setup_build_env.sh` / `build_conda_forge.sh` / `build_cli.sh` / `run_cli.sh` /
  `smoke_test.sh`）默认 `NB=/workspace/native-build`（旧路径）；
- X11/桌面链（`setup_x11_deps.sh` / `build_fcitx5_x11.sh` / `setup_desktop_deps.sh` /
  `build_fcitx5_gtk.sh` / 各 xvfb 测试）默认 `NB=/root/workspace/native-build`（新路径）。

此前靠 README 每轮「`export NB=/root/workspace/native-build`」规避。若照 README 却**漏 export NB**，
核心引擎会落到 `/workspace/native-build`、图形前端落到 `/root/workspace/native-build`，导致
「引擎与 X11/GTK 前端分家」→ 复现失败。这与第 17 轮「keysend_target 未纳入构建脚本 → 真桌面 0/10」
是同一类「脚本级复现缺口」。

**修复**：5 个核心脚本 `NB` 默认值统一为 `/root/workspace/native-build`（活跃链路 17 处全部一致）。
`s/build_native_x86_64.sh` 是 x86_64/Debian 平台遗留脚本（本沙箱不跑），保持原样不动。

**验证**：`unset NB; bash scripts/smoke_test.sh` → 20/20，证明默认值已可直接命中正确产物路径，
无需再依赖手工 export。

**后续建议**：更彻底的做法是让各脚本从自身位置推导 `NB`（`REPO_DIR` 的 sibling），彻底摆脱
硬编码路径；本轮回退为「最小改动统一硬编码默认值」，避免本回合大改动引入新风险，留待后续。