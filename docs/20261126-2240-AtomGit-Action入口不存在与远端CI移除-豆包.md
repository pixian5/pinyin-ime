# AtomGit Action 入口不存在与远端 CI 移除（2026-11-26 22:40）

## 结论先行

GitCode 与 AtomGit 是**同一个平台的两个网址/前端**，不是两个产品；账号与仓库互通。这个仓库在 `gitcode.com` 和 `atomgit.com` 下都没有可用的 Actions 页面：

- `https://gitcode.com/laotongtian/pinyin-ime/actions` → 「页面找不到或无权限」
- `https://atomgit.com/laotongtian/pinyin-ime/actions` → 同样不存在

因此 `.gitcode/workflows/ci.yml` 是一个**永远不会被执行**的配置。继续保留它，会让 README 和发布说明误以为项目有远端 CI。v0.9.196 已删除该文件，远端 CI 正式取消，`scripts/ci_build.sh` 仅作为本地/任意 Ubuntu 24.04 环境的一条命令门禁。

## 之前看到的 5 条记录是什么

后端 `workflow-runs` 接口确实返回 5 条记录，全部 `VALIDATION_ERROR`：

```text
POST https://web-api.gitcode.com/api/v2/projects/laotongtian%2Fpinyin-ime/actions/workflow-runs/list
{"page":1,"per_page":20,"record_type":"physical"}
```

诊断包括：

1. `jobs[build-eval].name` 含 `+`，只允许字母、数字、空格及 `- _ , ; : . / ( )`。
2. `runs-on` 使用 `euleros-2.10.1`，单字符串只允许 `default`、`ubuntu-latest`、`euler-latest`、`ubuntu-24`、`ubuntu-22`。
3. checkout 步骤只有 `uses`、没有 `name`。
4. `uses: checkout-action@0.0.1` 插件不存在，官方写法是 `uses: checkout`。

这些错误当时已全部修好，并加了 `workflow_dispatch`；但之后多次 push、甚至加一个只执行 `echo` 的最小工作流，都只注册成 `active`，不产生任何运行记录。仓库 `PIPELINE=1`、push 事件能查到、`actions/metrics/summary` 的 `total_runs=0`。这说明后端只做了 YAML 校验，没有为这个仓库调度执行。

## 为什么不再修 YAML

Actions 页面本身不存在，修 YAML 字段没有意义。即使把校验错误全部清零，也不会有 runner 接单。继续在 `.gitcode/workflows/` 里维护配置，只会制造「远端 CI 已接入」的假象。

## v0.9.196 的处置

- 删除 `.gitcode/workflows/ci.yml`。
- `scripts/ci_static.sh` 移除 workflow 存在性/调用断言，改为：
  - `scripts/ci_build.sh` 存在且可执行；
  - `ci_build.sh` 包含真实编译与 `bench/eval.py` 评测；
  - 仓库不再保留 `.gitcode/workflows/ci.yml`。
- `scripts/ci_build.sh` 头部改为「本地 / 任意 Ubuntu 24.04 环境」，不再写 GitCode Actions。
- README、CHANGELOG、PROJECT_STATUS 同步改为「无远端 CI，只有本地门禁」。
- 版本 `0.9.195 → 0.9.196`。

## 以后要恢复远端 CI 怎么办

不要直接重建空配置。先确认平台真的开放了 Actions 入口（能看到 Actions 页、能点 Run workflow），再应用历史提交里的配置：

```bash
git show 0cabf54:.gitcode/workflows/ci.yml > .gitcode/workflows/ci.yml
```

历史配置已修好 job 名、`runs-on`、checkout 插件与空 step 名，并带 `workflow_dispatch`。恢复前先确认平台要求（是否需要内测名额、是否只支持组织仓库、runner 标签是否可用），否则会重新回到「只有 VALIDATION_ERROR、没有执行」的状态。

## 本地门禁现状

`bash scripts/ci_build.sh` 仍然完整可用：编译 CLI/addon、721 单测、警告检查、快捷词 9 条、fuzz 500、42 例快照、1200 句 holdout 评测与基线门禁。本地 chroot 实测 27.83% / 84.81%，基线通过。没有远端 CI 之后，发版前必须手动跑这一条命令。
