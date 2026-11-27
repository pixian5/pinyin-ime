# pinyin-ime 发布说明

## v0.9.210 (2026-11-27)

### 软门限与 Top5 提升
- `CharLM::kRerankBasePenalty = 1.0f`：低于 `kRerankBaseGate=1.0` 的候选从 `-100` 硬淘汰改为 `-1` 软降权，保留原分差约束但不把 Top5 可用候选压死。
- addon 门限判断改用过滤后的首个整句候选 `items[0].text`，与 CLI 一致，避免原始 `full[0]` 为短候选时误跳过门限。
- addon 1200 句：Top1 **30.92%**（持平）、Top5 **46.17%**（+1.00pp）、Top10 **49.75%**（+0.58pp）、MRR **0.3764**、字级 **86.45%**；P95 603.9ms。
- CLI nbest=45：Top1 **31.92%**（+0.25pp）、Top5 **45.58%**（+0.42pp）。
- 失败方向已记录：NBest=100、局部同音词替换、LCCC/新闻/词边界字符模型均未超过 v0.9.209；原始 `bench/corpus` 训练的 47.08% 已确认是训练集泄漏。

## v0.9.209 (2026-11-27)

### addon 保守重排门限
- 新增 `CharLM::kRerankBaseGate = 1.0f`：4-gram 只在 libime 原始分差不超过 1.0 的完整句候选间换位；低于门限的候选降权保留，避免高置信 top1 被无条件换掉。
- 短候选（UTF-8 < 18 字节，约 6 个汉字）不套门限，42 例 CLI 快照仅版本头变化。
- CLI 1200 句 Top1 **31.67%**（v0.9.207 31.58%）；addon 1200 句 **30.92% / 86.37%**（v0.9.207 30.17%，+0.75pp；v0.9.204 30.25%）。
- 分域 addon：baike 31.0、news 40.33、recent 33.33、wiki 19.0；Top5 45.17% 略低于 v0.9.207 的 45.92%，属于首屏正确率与翻页率的取舍。
- `ci_build.sh` / `ci_static.sh` / 安装矩阵 30/30 全部通过；三件套 SHA256 校验 OK。

## v0.9.208 (2026-11-27)

### GitHub Actions 依赖安装修复
- GitHub runner 自带 `packages.microsoft.com/ubuntu/24.04/prod` 源返回 403，`build-eval` 的 `apt-get update` 直接退出 100；这不是代码失败。
- workflow 在 `build-eval` / `package` 的依赖安装前移除 `microsoft-prod.list` / `.sources`，避免无关源阻断 Ubuntu 官方包。
- v0.9.207 的 `package`、`install-matrix`、`addon-acceptance` 已通过；v0.9.208 修复后 run `45962224342` 五 job 全绿，三件套校验 OK。

## v0.9.207 (2026-11-27)

### 字符 4-gram 重排落地
- `CharLM` 模型格式升级到 v2：`PICL` + version=2，保留 v1 兼容；新增 `quadCtx_` / `quadEntry_` 与 `logQuadProb()`，`score()` 按 stupid backoff 计算，v1 文件自动走原 trigram。
- `scripts/train_char_lm.py` 同步训练并写出 4-gram；边界 `<s>` 三连上下文、句末 `</s>` 计数与 Python 离线实验一致。
- `data/char_trigram.bin` 重训为 v2：词表 3929、bigram 107229、trigram 203674；4-gram 只保留 count>=2，上下文 10,734、条目 13,842，体积 2.9MB→3.17MB（仅 +0.2MB）。
- CLI nbest=45 全量 1200 句 Top1 **31.58%**（v0.9.206 为 30.33%），字级 86.40%；分域 baike 31.33、news 41.00、recent 34.67、wiki 19.33。
- addon 1200 句：未剪枝 30.08% / 86.39%，剪枝 **30.17% / 86.42%**；相对 v0.9.204 的 30.25% 为 -0.08pp（1 句，噪声级），Top5 45.92%、MRR 0.3707 略升。CLI 增益未完整传导到逐键路径。

## v0.9.206 (2026-11-27)

### 夜间 CI GTK 启动超时修复
- v0.9.205 的 `addon-full-eval` 在 GitHub runner 上等待 GTK harness 首行 5s 超时；本地 chroot 首行通常 1s 内到。
- `scripts/eval_addon_batch.py` 首次等待放宽到 20s，后续 9 次仍 5s，总上限约 65s。
- 2026-11-27 手动 `workflow_dispatch` 运行 `45953471461` 成功：1200 句耗时 8m49s，Top1 30.25%、Top5 45.17%、Top10 49.83%、MRR 0.3689；报告 artifact 已生成。

## v0.9.205 (2026-11-27)

### addon 1200 句夜间/手动全量 CI
- `.github/workflows/ci.yml` 新增 `addon-full-eval` job：`schedule`（每周一 18:00 UTC）和 `workflow_dispatch` 时，在 Ubuntu 24.04 + fcitx5 + Xvfb + GTK3 环境跑 1200 句 addon 真实上屏评测。
- 复用 `package` 产物，安装 deb 后执行 `scripts/eval_addon_batch.sh`，上传 `report.md` / `summary.json`；不阻塞 push 的四个常规 job。
- 现有 push 仍只跑 8 句 addon-acceptance；全量 job 需手动触发一次确认耗时和稳定性。

## v0.9.204 (2026-11-27)

### addon 候选池 Top-k/MRR 评测
- 新增 `PINYINIME_EVAL_CAND` 评测转储：仅评测环境变量存在时，空格提交前按实际显示顺序导出最多 20 条候选（含重排、简拼 boost、云候选），正常输入零 I/O。
- `scripts/eval_addon_batch.py` 新增 Top1/Top5/Top10 句级命中率、MRR、候选池均值，并写入报告。
- 1200 句 addon：Top1 **30.25%**、Top5 **45.17%**、Top10 **49.83%**、MRR **0.3689**、字级 86.24%；P50 503.8ms、P95 646.6ms、P99 711.8ms。
- 关键结论：Top5 比 Top1 高 14.92pp，主要问题是“正确答案在池里但排名不够”，下一阶段优先做重排，不是扩召回。
- 撤销“5–8 字母提交前消歧”实验：`xianyoud` 无改善，全量无提升，P50/P95 反而变差。

## v0.9.203 (2026-11-27)

### addon 逐键消歧修复：长句首选 28.00%→30.25%
- 根因：addon 每输入一个字母都对 `rawPreedit` 调 `disambiguate_if_needed`；长句前缀只是半截拼音时，`xian` 被拆成 `xi'an`，再 `ctx.clear()+重打` 永久锁死。baike 长句实际输出从“文献多见”变成“文戏安多”，字级 60.61%。
- 修复：逐键消歧只保留 `rawPreedit.size() <= 4` 的极短缩写；长输入交给 libime 增量切分。实测 1200 句 addon **30.25% / 86.24%**（原 28.00/80.31），错误句 839（原 864），p50 489.7ms、p95 599.1ms、p99 647.5ms（原 505/754/1757）。
- 回归：addon 八句验收 8/8、单测 721/721；`xianyoud` 这类 5–8 字母的非全拼缩写不再走逐键消歧，需后续用提交前消歧或专用 boost 补回。
- CLI 新增 `--incremental`（逐键刷新候选）与 `bench/eval.py --incremental`，用于比较 batch/逐键口径；当前 batch nbest45 为 30.33/86.24。

## v0.9.202 (2026-11-27)

### addon 全量真机上屏评测
- 新增 `scripts/eval_addon_batch.py` / `.sh`：独立 Xvfb + dbus + fcitx5 + GTK3，逐键敲 1200 句 holdout，空格提交后从 Entry 读整句。
- 结果：addon 真实上屏 **28.00% / 80.31%**；baike 26.33/71.22、news 36.33/88.86、recent 33.33/85.69、wiki 16.00/75.46；p50 505.4ms、p95 754.3ms、p99 1757.2ms。
- 发现 CLI `--batch` 与逐键输入不是同一路径：同句 batch 首选“文献多见”，CLI 交互与 addon 都首选“文戏安多”；28/80 是真实逐键基线，30/86 仅作 batch 回归基线。
- 保留 `PINYINIME_EVAL_KEEP=1` 现场调试开关；本轮不改产品解码/排序代码。
## v0.9.201 (2026-11-27)

### 修 deb 不可满足依赖
- GitHub Actions 的 `addon-acceptance` 首次运行发现：deb 声明 `Depends: fcitx5, libime, libime-data`，但 Ubuntu 24.04 没有名为 `libime` 的包，正常 `dpkg -i` 无法配置；旧安装矩阵一直用 `--ignore-depends` 掩盖了这个问题。
- 改为 `fcitx5, libimecore0, libimepinyin0, libime-data`，与 `packaging/debian/control` 一致；`libime-bin` 会拉起 core/pinyin 库。
- `ci_static.sh` 增加守卫：禁止再出现 `fcitx5, libime, libime-data`，并要求 `libimecore0, libimepinyin0` 存在。
- CI `addon-acceptance` job 保留正常 `dpkg -i` 路径，后续不再用 ignore-deps 掩盖依赖错误。

## v0.9.200 (2026-11-27)

### addon 多短语真实上屏验收
- `scripts/test_runtime_fcitx5.sh` 支持 `PINYINIME_TEST_INPUT` / `PINYINIME_TEST_EXPECT`，默认行为仍是 `nihao→你好`。
- 新增 `scripts/test_addon_phrases.sh`：真实 Xvfb + fcitx5 + GTK3，用 xdotool 逐个输入 8 个短语并按空格上屏，检查整句结果。
- 覆盖 `nihao→你好`、`shijian→时间`、`zhongguo→中国`、`beifang→北方`、`xianzai→现在`、`zhongguorenmin→中国人民`、`jintiantianqihenhao→今天天气很好`、`qinghuadaxue→清华大学`；本地 chroot **8/8**。
- GitHub Actions 新增 `addon-acceptance` job：下载 deb、安装 fcitx5/Xvfb/GTK3/xdotool，再跑上述验收；真实用户入口不再只有“编译通过”。
- 已知限制：仍是 8 句抽样，不是 1200 句 addon 全量评测；addon 与 CLI 的候选窗/翻页键差异仍未做全量对齐。

## v0.9.199 (2026-11-27)

### Top-k 评测与交互成本
- `bench/eval.py` 改用 `--top-k 10`，一次批量拿到候选列表，除句级/字级外新增 Top1/Top3/Top5/Top10、MRR、命中平均排名、命中后平均按键与翻页率（按 CLI 每页 5 个候选保守估算）。
- 新增 `--nbest`（默认 10）；报告和 `summary.json` 同步记录排名指标；自检覆盖空输入、MRR/按键估算。
- clean 模型 nbest=10：**30.00% / 86.04%**，Top3 40.25%、Top5 43.58%、Top10 46.08%、MRR 0.358、命中率 46.08%、命中后平均 1.05 键、翻页率 5.42%。
- clean 模型 nbest=45（贴近 addon 默认）：**30.33% / 86.24%**，Top10 48.92%、MRR 0.371、命中后平均 1.39 键、翻页率 39.04%；首选只 +0.33pp，说明扩池主要改善深排名，Top1 仍是瓶颈。
- 已知限制：仍是 CLI 口径，addon 真实候选窗未单独测；按键/翻页是估算；尚未输出逐句 P95/P99 和 2–6 字词级指标。

## v0.9.198 (2026-11-27)

### 字符模型去泄漏与 clean 过渡基线
- 确认 v0.9.197 的 42.67% / 88.88% 为训练集泄漏：1200 句 holdout 原文 100% 在 `bench/corpus`，字符 trigram 又用同一语料训练；`check_leakage.py` 未检查训练侧。
- 新增 `bench/clean_corpus.py`：以 holdout 为固定 test，剔除逐字相同句子、与任一 test 共享 >=2 个字符 5-gram 的训练句，并排除 `lccc_eval/daily/dialogue`；clean 训练集 12,130 句 / 257,455 字。
- 新增 `bench/check_train_leakage.py`：exact=0、near_dup=0、eval_only=0 才通过；`ci_build.sh` 每次从 clean 语料确定性重训，并与 `data/char_trigram.bin` `cmp`。
- `scripts/train_char_lm.py` 增加 `--meta-out` / `--alpha`，`data/char_trigram.meta.json` 记录语料 sha256、alpha、模型 sha256；`cpp/char_lm.h` 的 `kAlpha` 从 2.0 降到 0.25。
- 新模型：词表 3,929，bigram 107,229，trigram 203,674，2,945,931 字节；clean holdout **30.00% 句级 / 86.05% 字级**，无重排 27.83% / 84.81%。
- `bench/baseline.json` 前移到 v0.9.198 clean 过渡基线；42.67 标为历史泄漏复现值，不再作为门禁。
- 快照 42 例通过（4 例因模型变更有意更新：`womenzaiyonglibimezuoceshi`、`qi'an`、`e`、`riqi`）；完整 `ci_build.sh` 通过。
- 修 `build_packages.sh` 分模式打包时 `SHA256SUMS_v<VERSION>` 只剩当前模式产物的问题；现在最后一次会把同版本 rpm/deb/runtime 全部纳入清单。v0.9.198 安装矩阵 30/30。
- GitHub Actions 新增 `package` 与 `install-matrix` job；`scripts/ci_package.sh` 编译 addon/CLI 后调用 `build_packages.sh all`，上传 deb/rpm/runtime/SHA256，安装矩阵在干净 runner 上跑 30 项断言。
- 已知限制：仍是句子级 + 5-gram 近似去重，不是文档级 split；LCCC/新闻/百科/维基的再分发许可仍未清，模型暂不应进入公开 Release。

## v0.9.197 (2026-11-26)

### 字符 trigram 重排序上线（CLI + fcitx5 addon）
- 新增 `scripts/train_char_lm.py`、`cpp/char_lm.{h,cpp}`、`data/char_trigram.bin`（3.4MB）：用 `bench/corpus` 324,490 字训练字符 trigram（词表 4,212、bigram 125,391、trigram 245,842），整句分数按 `0.55*p3 + 0.30*p2 + 0.15*p1` 加权，重排分数为 `libime score + 2.0 * 字符 log 概率`。
- CLI 1200 句 holdout：**42.67% 句级 / 88.88% 字级**，较 v0.9.196 的 27.83% / 84.81% 提升 **+14.84pp / +4.07pp**；四域 baike 43.0/90.62、news 50.67/91.74、recent 47.33/90.62、wiki 29.67/82.55。新增 `--no-rerank` 开关，模型缺失时自动退回旧行为。
- fcitx5 addon 新增 `displayToRaw_` 显示→原始候选索引映射，渲染、选词、简拼 boost、云候选、翻页统一口径；重排池为 `max(10, nbest)`。真实 fcitx5 5.1.7 + GTK3 敲 `nihao`+空格上屏「你好」，运行时冒烟 **6/6**。
- **修重排引入的真 bug**：`context_.candidates()` 混有只覆盖首音节的短候选（如「你」），首版重排把它排到首位，空格只选一个字并留下 `hao`。现用 `candidateFullPinyin` 与当前 preedit 的紧凑拼音长度比较，只重排覆盖整段拼音的候选。
- 修正 C++ 字符模型「上下文存在但目标字计数为 0」时错误回退 unigram 的问题，改为按 add-0.1 平滑，C++ 与 Python 分数逐字一致。
- 门禁：42 例 CLI 快照重写并通过；`bench/baseline.json` 前移；`ci_build.sh` 全绿（编译、单测、警告 0、fuzz、快照、评测）。顺手修 `pinyin_ime_cli.cpp` 的 `system()` 返回值警告。
- 打包：模型安装到 `/usr/share/pinyin-ime/char_trigram.bin`；deb/rpm 约 2.7MB，runtime 约 24MB。安装矩阵 **30/30**，并修掉 runtime 卸载清单缺 `usr` 前缀、误删 `/usr/share/libime/sc.dict` 两个 bug。
- 文档：`docs/20261126-2349-字符trigram重排序上线-豆包.md`、`docs/20261126-2349-当前开发进度-豆包.md`、`docs/20261126-2349-下一步待实现-豆包.md`。


## v0.9.196 (2026-11-26)

### 移除不可用的远端 CI，保留本地门禁
- 实测 GitCode 与 AtomGit 是同一平台、两个网址；`https://gitcode.com/laotongtian/pinyin-ime/actions` 与 `https://atomgit.com/laotongtian/pinyin-ime/actions` 都不存在。此前 5 条记录只是后端 YAML 校验失败（`VALIDATION_ERROR`），没有 runner 执行；修 YAML 字段无法改变这一事实。
- 删除 `.gitcode/workflows/ci.yml`，不再维护永远不跑的远端配置；`scripts/ci_build.sh` 保留为本地 / 任意 Ubuntu 24.04 环境的一条命令门禁（编译、单测、警告、快照、fuzz、1200 句评测）。
- `scripts/ci_static.sh` 移除 workflow 存在性断言，改为校验本地 `ci_build.sh` 可执行、包含真实编译与评测，并确认不再残留 `.gitcode/workflows/ci.yml`。
- 已知限制：项目当前**没有远端 CI**。发版前必须手动执行 `bash scripts/ci_build.sh`；若以后平台开放 Actions 入口，可从提交 `0cabf54` 取回已修正的 workflow 再启用。

## v0.9.195 (2026-11-26)

### 把 v0.9.194 的「剩余缺口」一次补完
- **真实按键上屏**：`test_runtime_fcitx5.sh` 增加 GTK3 输入框 + fcitx5 GTK im module 路径，用 `xdotool` 敲 `nihao`+空格，断言输入框内容为「你好」。实测 **6/6 通过**（原 XIM 客户端在无窗口管理器的 Xvfb 下拿不到 commit，已改用 GTK im module）。
- **rpm 升级矩阵**：`test_upgrade_matrix.sh` 增加 rpm 段，覆盖 `rpm -i` → `-Uvh` 升级 → `-Uvh --oldpackage` 降级 → 再升级 → `-e` 卸载，校验 CLI 版本、解码、`sc.dict`/LM 指纹、addon 二进制与卸载后数据。deb+rpm 合计 **30/30 通过**。
- **CI 真实编译 + 评测**：新增 `scripts/ci_build.sh` 与 `.gitcode/workflows/ci.yml` 的 `build-eval` job（Ubuntu 24.04 装 libime/fcitx5 开发包），真实编译 CLI/addon，跑单测、警告、快捷词、快照、fuzz、1200 句 holdout 评测与基线门禁。本地 chroot 整条实测通过（27.83% / 84.81%）。
- `ci_static.sh` 增加 8 条守卫（真实上屏断言、rpm 降级路径、CI 脚本/workflow 存在且被调用）。
- 已知限制：`build-eval` 用 `ubuntu-latest`，远端 runner 是否提供该标签需首次推送后确认。

## v0.9.194 (2026-11-26)

### 运行时冒烟与 deb 升级矩阵（测试基建收尾）
- 新增 `scripts/test_runtime_fcitx5.sh`：私有 Xvfb + 私有 HOME + `dbus-run-session` 启动**真实 fcitx5 5.1.7**，断言 addon 被加载、打印 `loaded lm=... dict=...`、注册为输入法、无加载错误，并用 D-Bus 控制器确认 `pinyinime` 可切换。实测 **5/5 通过**；没有 fcitx5 的环境自动 SKIP。
- 新增 `scripts/test_upgrade_matrix.sh`：旧版 deb 安装 → 升级当前版 → 降级回滚 → 再升级 → 卸载，逐步校验 CLI 版本、`nihao→你好`、`sc.dict`/语言模型 md5 指纹、addon 版本与二进制、卸载后数据仍在。实测 **16/16 通过**（0.9.193→0.9.194）。
- `release_check.sh --full` 接入两项（运行时 SKIP 记为提醒、升级矩阵为阻断项）；`ci_static.sh` 增加 6 条守卫。
- **修复 runtime 包 install.sh/uninstall.sh 在无显示环境挂死**：`fcitx5 -r` 在容器/CI（无 DISPLAY）会卡住整个安装/卸载流程；两处都改为有 `DISPLAY`/`WAYLAND_DISPLAY` 时才调用并加 5 秒超时。该问题正是安装矩阵测试跑出来的。另修 `ci_static.sh` 在无 `.git` 的 chroot 里跑 `git diff --check` 直接失败的问题（改为非 git 仓库跳过）。
- 已知限制：XIM 客户端在无窗口管理器的 Xvfb 下拿不到 commit，真实按键上屏留待后续验证。

## v0.9.193 (2026-11-26)

### 模糊测试与两处性能/稳定性缺陷
- 新增 `cpp/fuzz_segment_test.cpp` + `scripts/fuzz_test.sh`：固定种子随机串 + 对抗输入（10 万字符、非法 UTF-8、控制字符），带 ASan/UBSan。
- **修复栈溢出**：`smart_segment` 递归深度等于输入长度，10 万字符直接 SIGSEGV；改为迭代 DP（`cnt/pick` 后正向重建），100k 输入 19ms。
- **修复 O(n²)**：`typo_correction_lookup` 对每个位置拷贝整串做相邻交换，10 万字符 430ms；加 64 字符上限后 0ms。`quick_candidates_v5/v11` 从 464/411ms 降到 27/24ms。
- 仓库卫生：删除 v0.9.192 误入库的 71 个 `.gcov`，`.gitignore` 增加 `*.gcov/*.gcda/*.gcno`，`ci_static.sh` 增 5 条守卫。
- 验证：单测 721 passed（快照 42 例输出不变）、警告 0、fuzz 2000 次（普通）/1000 次（ASan）通过。

## v0.9.192 (2026-11-26)

### 警告清零、覆盖率与内存检查
- **修功能缺陷**：`--phrase-add/--phrase-del/--phrase-list` 之前只解析不执行（帮助有、功能无），现落到 `custom_phrases.txt` 并可用；删除纯空壳 `--clipboard`。
- **修云候选 UI 不刷新**：`addPostEvent` 返回的 `EventSource` 被丢弃导致回调注销，改由引擎持锁持有。
- 清零 `-Wall -Wextra` 警告（单测/CLI/addon 各 0）：运算符优先级、未使用参数、`std::system` 返回值、`%04d%02d%02d` 缓冲区大小、nodiscard。
- 新增 `scripts/warnings_check.sh`、`coverage.sh`（单测门槛 90%，实测 92.7%）、`sanitizer_test.sh`（ASan/UBSan，无 proc 环境自动降级 UBSan）、`cli_features_test.sh`（9 条快捷词行为）。
- `release_check.sh` 增补上述四项；`ci_static.sh` 增补 4 条守卫。

## v0.9.191 (2026-11-26)

### 测试门禁与安装矩阵脚本化
- 新增 `scripts/test_install.sh`：deb/rpm/runtime 三条安装路径 25 项断言（状态/版本/依赖/不含数据文件/无参数解码/卸载干净/sc.dict 不误删）。
- 新增 `scripts/cli_snapshot_test.sh` + `bench/snapshots/`：42 例固定输入逐条比对 top1，文件头记录 LM/dict md5，支持 `--update`。
- 新增 `scripts/release_check.sh`：VERSION↔CHANGELOG↔README↔文档索引↔二进制↔addon 配置一致性 + 静态检查 + 单测 + 快照，`--full` 加安装矩阵与准确率。
- 新增方案文档 `docs/20261126-1920-测试开发升级方案-豆包.md`（L0–L4 分层、P0–P4 阶段计划）。
- 实测：安装矩阵 25/25；快照 42/42 且篡改期望值可按预期失败。

## v0.9.190 (2026-11-26)

### 数据文件归属重构 + 可卸载 runtime
- **rpm/deb 不再自带 LM/词典**：`Requires/Depends: libime-data`，避免与发行版 `/usr/share/libime/sc.dict` 撞文件；包体积 22.7MB → 约 1MB。runtime 包仍自包含（容器场景），可用 `BUNDLE_DATA=1` 强制 rpm/deb 自带。
- runtime 包新增 `uninstall.sh`：按 `installed-files.txt` 清单卸载，并跳过 dpkg/rpm 管理的文件（不会误删 `libime-data` 的数据）。
- `build_packages.sh` 增加打包前 CLI 版本校验（`$CLI --version` 必须等于 `VERSION`；交叉打包自动跳过），修掉「包新、二进制旧」。
- 实测：deb 正常 `dpkg -i`（无 force）通过、`Depends` 含 libime-data；rpm `rpm -i`/`rpm -e` 往返干净；runtime 装→卸往返后 `sc.dict` 保留；三项 SHA256 全 OK。
- **升级风险**：0.9.189 → 0.9.190 升级会删掉 `libime-data` 的 `sc.dict`（旧包曾覆盖它），需 `apt-get install --reinstall libime-data` 修复。

## v0.9.189 (2026-11-26)

### 便携运行时包 + 端到端安装验证
- `build_packages.sh` 新增 `runtime` 模式（`all` 默认包含）：产出 `pinyin-ime-runtime-<ver>-<arch>.tar.gz`，内含完整 `usr/` 安装树、`install.sh`、`BUILD-INFO.txt`；`SHA256SUMS` 同步含三条。
- `build_release.sh` 标注为历史脚本（依赖旧 `/workspace` 沙箱与 aarch64），生产入口统一为 `build_packages.sh`。
- 实测：runtime 包解压执行 `install.sh` 后，CLI/addon/LM 就位，`pinyinime.conf` 的 `Version=0.9.189`；**不传 `--lm/--dict`** 直接解码 `nihao→你好`、`woaibeijing→我爱北京`；`dpkg -i` 安装后 `pinyin_ime_cli --version` 输出 0.9.189。`sha256sum -c` 三项全 OK。
- **发现待决策问题**：包内自带 `sc.dict` 与发行版 `libime-data` 撞文件（dpkg 需 `--force-overwrite`）。建议改为 `Depends: libime, libime-data`、不再自带数据（体积 22.7MB→约 1.4MB），或保留自包含但补 `Conflicts/Replaces: libime-data`。

## v0.9.188 (2026-11-26)

### 发布产物校验清单
- `build_packages.sh` 新增 `write_checksums()`：打包自检后生成 `dist/SHA256SUMS_v<VERSION>`，只列本次打出的 rpm/deb，旧版本残留不进清单；无产物则返回非 0。
- `ci_static.sh` 增第 5 条发布工程守卫（必须包含 `SHA256SUMS_v$VERSION` 逻辑）。
- 实测 0.9.188：清单两条记录，`sha256sum -c` 两项均 OK；rpm 22,743,929 B、deb 22,734,334 B。
- 遗留：`build_release.sh` 仍是旧沙箱脚本（写死 aarch64/`/workspace`），未接入新清单。

## v0.9.187 (2026-11-26)

### 发布工程：架构自适应 + x86_64 deb/rpm 实测
- `build_packages.sh` 不再写死 aarch64/arm64：改用 `uname -m` 推导 rpm/deb 架构与 addon 安装目录，`ARCH_RPM`/`ARCH_DEB`/`FCITX5_LIBDIR` 可覆盖；CLI/addon 产物路径与 `LM`/`DICT` 也可覆盖（支持 out-of-tree 构建）。
- `pinyin-ime.spec` 的 `BuildArch: aarch64` 改为 `@ARCH@` 占位；rpmbuild 增加 `--target "$ARCH_RPM"`。旧写法在 x86_64 上直接报 “No compatible architectures found for build”。
- 实测产出 `pinyin-ime_0.9.187_amd64.deb`（22,734,250 B）与 `pinyin-ime-0.9.187-1.x86_64.rpm`（22,743,733 B），deb md5sums 8 条全通过、rpm 元数据 Arch/License 正确，addon 落在 `/usr/lib/x86_64-linux-gnu/fcitx5/`。
- `ci_static.sh` 新增 4 条发布工程守卫（禁止写死 aarch64、spec 占位符、架构推导、安装目录可覆盖）。
- 未验证：真机 fcitx5 安装加载、aarch64 原生重测、依赖粒度细化、S3 发布链路（仍停 v0.9.121）。

## v0.9.186 (2026-11-26)

### LICENSE 落地 + 准确率口径分层
- 新增 `LICENSE`：LGPL-2.1-or-later（与 libime/fcitx5 同族，允许闭源程序动态链接），README 增「许可证」小节。
- README 顶部补准确率口径说明：原始目标 91%/68% 标注为「需换 LM」，当前可达口径 88%/45%，门禁按可达口径执行。
- 删掉两处过期描述（addon 消歧已于 v0.9.183 修复、LICENSE 已补）。
- 仅文档与许可证改动；721 单测通过、ci_static 全绿、准确率维持 27.83% / 84.81%。

## v0.9.185 (2026-11-26)

### 文档索引补齐（体检 P1-8 关闭）
- `文档索引.md` 由「表格式 + 文末混入 3 条 bullet」统一成**纯表格**，并补齐 15 篇此前漏收的文档（2026-10 的 X11/GUI/云桌面/打包系列 + `wayland_evaluation.md` + `RELEASE_v0.9.125.md`）。
- 校验脚本口径：`comm -23 <(ls docs/*.md) <(索引里的链接)` 结果从 15 篇降到 **0 篇**，48 篇文档全部可检索。
- 说明：这 15 篇的摘要按「标题 + 版本号」提炼，只写能确认的事实，未逐篇重读。

## v0.9.184 (2026-11-26)

### 新增 `scripts/eval_chroot.sh`：一键真实评测
- 一条命令完成「同步源码 → chroot 内编译 CLI → 1200 句 holdout 评测 → 与基线比对 → 报告回写」，支持 `--skip-build` 复用产物。
- 前置检查 chroot 与 `zh_CN.lm` 是否存在；评测低于阈值时沿用 eval.py 的退出码 2。
- 实跑两次（完整编译 / `--skip-build`）结果逐位一致：句级 27.83% / 字级 84.81%，门禁通过。
- 说明：GitCode runner（euleros-2.10.1）缺 libime 开发包，评测暂未进 CI，仍需手工一条命令跑。

## v0.9.183 (2026-11-26)

### addon 与 CLI 统一歧义拆分入口
- `cpp/abbr_presets.h` 新增 `disambiguate_if_needed()`：含空格/标点走原逻辑；纯全拼（可完整切音节）原样返回；只有切不开的简拼/混输才调 `disambiguate_pinyin`。
- CLI 删除 v0.9.182 的本地副本改用共享实现；addon 两处调用点（撤回恢复预编辑 793 行、每键消歧 1729 行）同步切换——GUI 里 `xianzai` 不再被拆成「西安在」。
- 准确率不变（CLI 仍 27.83% / 84.81%）；单测 721 passed（新增 5 条）；CLI 与 addon 均编译通过。
- 遗留：addon 每键仍是「清空 + 重打预编辑」，长句下是白花开销，待改为增量重建。

## v0.9.182 (2026-11-26)

### CLI 与纯库解码偏差归零：句级 +1.16pp / 字级 +0.55pp
- **现象**：同机同 LM/词典、同 holdout，`libime_bench` 27.83% / 84.81%，CLI 一直 26.67% / 84.26%。
- **三处真因**（逐个用探针+逐行 diff 定位）：
  1. `disambiguate_pinyin` 对所有输入无条件拆音节（`xian→xi'an`、`nian→ni'an`），连写全拼被拆成「西安/尼庵」——CLI 新增 `disambiguate_if_needed()`，纯全拼直接交给 libime。
  2. 简拼启发式（后缀/前缀/元音尾 boost）作用于纯全拼，把正确首选顶掉（「早期→早起」）——`abbr_presets.h` 新增 `is_full_pinyin_input()` 守卫，启发式只对简拼/混输生效。
  3. v11 预解码捷径在解码前就把 `xianzai` 换成日期时间并 `continue`，中文候选全被吞掉——捷径加 `v11_shortcut_allowed()`（`xianzai/shijian/riqi/xingqi` 放行正常解码），`boost_v11` 改为纯全拼时插到第二位。
- **结果**：CLI **27.83% / 84.81%**，与纯库参考系逐句一致（1200/1200），四域同步上涨（wiki 字级 76.38→77.11）。
- 单测 716 passed（新增 9 条 v182 用例）；CI 单测标准 `c++17 → c++20` 与 CMake 对齐；addon 编译通过。

## v0.9.181 (2026-11-26)

### 新增 `--extra-dict` + 词库扩充实验（结论：此路不通）
- **新功能** `--extra-dict F`：加载 libime 文本词典到 `UserDict` 索引，格式 `词 wang'ju'chuan 0.0`（空格分隔、音节用单引号连接——`词<TAB>拼音 拼音` 和连写拼音都会被解析器跳过，实测确认）。
- **实验方法**：把 rime-ice 的 base/ext/others 转成上述格式（2–4 字词，按权重取前 5 万），分别做"全量"与"只补系统词典没有的词"两组。
- **结果（同机同 holdout）**：
  - 基线（v0.9.179）：句级 26.67% / 字级 84.26%，68s。
  - 全量 5 万词：**84.26% → 83.80%（−0.46pp）**，耗时 **68s → 138s**（翻倍）。盲目扩词引入同音噪声，反而变差。
  - 仅补缺失（5 万里 37,685 条已在系统词典，占 75.4%，真正新增 12,315 条）：**84.29%（+0.03pp）**，67s。等于没动。
- **结论**：通用词库扩充对准确率无实质作用——系统词典已有 20.9 万条、覆盖了常见词；剩余错误是**同音歧义的排序**问题，不是"词不在库里"。准确率路线从 B 转 C（重排序）/A（换 LM）。

## v0.9.180 (2026-11-26)

### 候选召回上限分析：91%/68% 在当前数据下结构性不可达（仅工具+文档）
- **参考系**：同库同数据的 `libime_bench` 朴素整句解码 = 句级 27.83% / 字级 84.81%，CLI 为 26.67% / 84.26%，**字级仅差 0.55pp**——代码已贴近库天花板，靠改解码逻辑涨不到 1pp。（句级差 1.16pp 疑来自 CLI 首选覆盖逻辑，列为待查项。）
- **召回上限**（`libime_bench nbest=10`）：recall@1 27.83% / @5 41.42% / @10 46.08%；前 10 候选内字级上限 **90.21%**。即完美重排序最多把句级抬到 46.08%，字级顶部 90.21%，两者都够不到 68% / 91%。
- 新工具 `bench/recall.py`（纯 stdlib）：从候选 dump 算 recall@K、候选内字级上限，行数不符直接报错；字级上限只取前 `max(ks)` 列（`libime_bench` 的追加单词级候选会让数字虚高 90.38% vs 90.21%）。
- 路线判定：**B 扩词典 → C 重排序 → A 换/训 LM**；并明确"不得用 `bench/corpus` 造词典"（holdout 同源，属返祖泄漏）。

## v0.9.179 (2026-11-26)

### 首次产出真实引擎基线 + 修掉"合法拼音被判英文原样回显"
- **可在 Linux 上真编译了**：Ubuntu 24.04（`ubuntu-base-24.04.3` rootfs）+ libime 1.1.5 + fcitx5utils 5.1.7 + Boost 1.83，`cmake -S cpp` 直接编译出 `pinyin_ime_cli`；此前"libime 1.1.x 编译不出来"只是 22.04 软件源版本太旧，不是代码问题。**addon 也首次编译通过**（`libpinyinime.so` 1.2MB，libfcitx5core-dev 5.1.7 + ECM），同时反证 v0.9.178 的头部拆分对 addon 无损。
- **真实基线（1200 句零泄漏 holdout，`--batch --top1`）**：句级 **26.33%**、字级 **83.32%**，各域 52–82ms/句。与 README 宣称的"字级 91%/句级 68%"差距巨大，此前 77%→100%、189 条 100% 属语料自证。
- **长度分层诊断**：6–10 字短句字级仅 78.0%、16–20 字 85.4%、31–40 字 85.8%——准确率不随句长下降，说明瓶颈是词典/LM 覆盖与同音歧义，不是长句解码。
- **修复英文误判**：`is_valid_pinyin` 原为"最长音节贪心"，`guangxuershiwuniannianyinbingqushi` 切成 `guang|xue|[r]|shi…`，孤立 `r` 使其被判英文、整句原样回显；holdout 1200 句命中 13 句（1.1%）。改为 `quick_input_rank.h` 的 `pinyin_fully_segmentable()`（完整覆盖可达性 DP，零残字），CLI 只保留薄封装。
- **效果（同机器同语料）**：句级 26.33% → **26.67%**（+0.34pp），字级 83.32% → **84.26%**（+0.94pp），wiki 字级 74.4% → **76.38%**（+2.0pp）；基线门禁通过。
- `bench/baseline.json` 前移为 v0.9.179，`_历史` 保留 v0.9.178 数字；新增单测 8 条（长句覆盖、`nv` 写法、英文反例、与 DP 分词的分工）。

## v0.9.178 (2026-11-26)

### 结构：把 6991 行万能头 quick_input.h 拆成 10 个模块片段
- `cpp/quick_input.h` 由 **6991 行降到 104 行**，只保留公共 include、皮肤（Theme）和 9 个片段 include；CLI 与 addon 的 include 路径、调用代码一行未改。
- 新增 9 个片段头（`cpp/quick_input_*.h`）：`core` 602（v 模式/日期/计算器/数字大写/货币/单位/百分比）、`modes` 913（u/Tab/i/联想/自定义/中英混输/错音/成语/繁简/网址）、`typing` 666（分词/词频/断句/句式/最近输入/选词）、`cloud` 280（云接口/模糊音）、`clipboard` 100、`sentence` 246（长句切分/短语管理）、`stats` 966、`rank` 637（纠错/上下文/打分/DP 分词）、`ui` 837（表情/多 provider/同步/候选窗）、`boost` 1662（v0.9.164+ 增强入口）。
- 片段不是独立头：必须在 `quick_input.h` 的 `namespace pinyin_ime` 内 include（片段内自带 namespace 会嵌套成 `pinyin_ime::pinyin_ime`，本轮已踩坑并写成 CI 检查）。
- 等价性验证：新旧 `quick_input_test.cpp` 的 `g++ -E -P` 预处理输出**逐行完全一致**（65561 行、差异 0 行），即对 CLI/addon 而言语义零变化；g++ 编译通过，单测 699 passed / 0 failed。
- `scripts/ci_static.sh` 新增拆分约束：`quick_input.h` ≤ 400 行、片段 ≥ 6 个、片段不得声明 namespace、umbrella 必须包含每个片段、CLI/addon 不得直接 include 片段。

## v0.9.177 (2026-11-26)

### 评测可信化（D1 第一步）：零泄漏 holdout + 唯一评测入口 + 门禁
- `bench/make_holdout.py`：从 `bench/corpus` 生成 **1200 句**（news / wiki / baike / recent 各 300），逐句过滤后转连续拼音，产物 `bench/holdout/*.tsv` + `manifest.json`（含 sha256、过滤统计、已知局限）。
- `bench/leakcheck.py`：泄漏判定的**唯一实现**——扫描 `cpp/*.h`、`cpp/*.cpp` 的 1982 条中文字面量、1025 条模板、390 条预设、625 条旧评测语料文本；生成与复核共用同一套规则，避免自证。
- `bench/check_leakage.py`：独立读盘复核 holdout，零泄漏 + 格式自检（纯汉字 / 纯 ASCII 拼音 / 跨域不重复），CI 每次跑。
- `bench/eval.py`：唯一评测入口，只测 `pinyin_ime_cli --batch --top1`（用户实际得到的首选）；输出 `summary.json` + `report.md`（含错例）；`--baseline` 门禁：句级下降 >1.0pp 或字级 >0.5pp 退出码 2；`--selftest` 无需编译产物即可验证指标与门禁逻辑。
- CI `unit` job 增加两步：`python3 bench/check_leakage.py`、`python3 bench/eval.py --selftest`。
- 已知局限（写入 manifest）：未对 LM 训练语料去重、多音字按 pypinyin 默认读音，故只用于版本间回归对比，不对外宣称绝对准确率。

## v0.9.176 (2026-11-24)

### 可信化修复：版本单一来源 + 仓库卫生 + CI 门禁
- 版本单一来源：新增 `cmake/version.h.in`，`cpp/` 与 `fcitx5-addon/` 均由 CMake 从仓库根 `VERSION` 生成 `version.h`；删除 CLI `--version` 里写死的 `0.9.86`；删除 addon CMake 中把 `PINYINIME_VERSION` 覆盖回 `project()` 旧值（0.9.126）的两行。
- 仓库卫生：`git rm --cached` 移除 8 个误入库二进制（`cpp/quick_input_test`、`cpp/quick_input_test_latest`、`tools/{qt_ime_test_client,visual_ime_test,xterm_ime_test}`、`pinyin-ime/opt/fcitx5-x11/lib/libFcitx5Utils.so{,.2,.5.1.23}`）与 `ip.txt`；`.gitignore` 补对应规则，撤销 `!ip.txt` 反忽略。
- CI 门禁：`.gitcode/workflows/ci.yml` 新增 `unit` job（g++ 编译并运行 `quick_input_test`）；`scripts/ci_static.sh` 新增版本字面量、单一“当前版本”标题、CHANGELOG 版本、禁止二进制入库、`ip.txt` 未跟踪等检查。
- 文档更正：README 的模糊音“硬性禁令”（称 `--fuzzy` 永远无效、相关代码是死代码）改为“默认关闭、显式启用”，与代码实际行为一致；5 个重复的“当前版本：”标题改为“历史版本：”。

## v0.9.121 (2026-10-21)

### 发布工程：可安装包 + GitCode Release + 文档/测试收敛
- 修复 addon 打包配置：`Library=libpinyinime`、`0=core:<fcitx5 版本>`，并在打包前硬校验，避免再次产出 v0.9.118 那类无法加载的 RPM。
- addon 的 `Version` 改为读取仓库根 `VERSION`，不再写死 0.9.115。
- 新增 `pinyinime.inputmethod`，RPM/DEB 同时安装输入法描述、addon 配置、CLI、LM/dict，做到开箱即用。
- 新增 `scripts/build_release.sh`：构建 RPM/DEB/自包含运行时 tarball，生成 SHA256SUMS 和发布说明。
- 新增 GitCode Actions 静态检查 workflow（`.gitcode/workflows/ci.yml` → `scripts/ci_static.sh`）与本地运行时冒烟 `scripts/ci_runtime.sh`。
- 收敛 README/CHANGELOG/PROJECT_STATUS 的当前版本口径：README 顶部新增 v0.9.121 当前版本与产物章节，PROJECT_STATUS 头部、打包与发布章节更新到 v0.9.121（历史内容原地保留）。
- 发布 GitCode Release `v0.9.121`，附 RPM、DEB、运行时 tarball、校验和；仓库转为公开，4 个资产匿名下载 + SHA256 校验实测通过。
- 产物同步到 S3 `cst:xbz/pinyin-ime/v0.9.121/`（`rclone check` 0 差异），Release 与 S3 双通道分发。


## v0.9.120 (2026-10-21)

### 无 Docker 一键复现预构建运行时 — CLI 9/9 + XIM 8/8 + DBus 7/7
- 新增 `scripts/setup_sandbox_deps.sh`：在 `/usr`、`/etc` 只读的 openEuler aarch64 上把 Xvfb/DBus/X11/中文字体安装到 `/workspace/sysroot`，生成排除 glibc 的 `/workspace/xvfb-libs`，并编译 `xkbcomp` 重定向 shim。
- 新增 `scripts/run_prebuilt.sh`：幂等完成 S3 产物下载/校验、addon 配置修正、数据路径准备、Xvfb + DBus + fcitx5 启动，以及 CLI/XIM/DBus/GUI 验证。
- 运行时组合：fcitx5 5.1.23（v0.9.83 bundle）+ libIME 1.1.17（v0.9.84a）+ CLI/addon v0.9.118 + LM/dict v0.9.116。
- 验证：CLI 9/9、XIM 8/8、DBus InputContext 7/7（按键 65/65），并提交真实候选窗/上屏截图。
- 修复：RPM 内 `Library=libpinyinime.so` 配置错误、sysroot loader 符号链接探测、addon 启动竞态、glibc 混用、Xvfb 硬编码 xkbcomp、`status` 读取旧日志误报。
- 已知差异：`nihaozg` 在 CLI 输出“你好这个”，在 XIM 交互式分词下输出“你好中国”。


## v0.9.93 (2026-10-15)

### 第六台机器复现：只读 /usr 沙箱（140.245.88.50）——输入法真正跑起来
- 从 S3 预构建产物（v0.9.84a + v0.9.83 桌面 bundle + v0.9.84 supplement）复现，**不写 /usr、无 conda、无 docker**。
- CLI 引擎冒烟 **24/24**（全拼/简拼/模糊音/双拼/CLI 功能）。
- fcitx5 以 **DBus+守护进程**方式加载 pinyinime addon + zh_CN.lm + sc.dict，注册输入法 `pinyinime`，`org.fcitx.Fcitx5` 接管 DBus。
- **真实输入链路 e2e 7/7**：经 fcitx5 DBus InputContext 协议（同 GTK/Qt 底层路）发真实按键，上屏 `你好/中国/输入法/北京/上海/拼音/中文`，按键 65/65 —— 证明是真输入法，非 CLI（scripts/e2e_run.sh、e2e_input_test.py）。
- 新增：`scripts/setup_local_runtime.sh`、`scripts/start_im_env_ro.sh`（只读 /usr 启动器）、`scripts/repro_verify.sh`（三层验证，6/6）、`/usr/local/bin/pinyin-ime`。
- 关键坑：CLI 需 `deps-lib/libstdc++.so.6`(GLIBCXX_3.4.32)；`LD_LIBRARY_PATH` 不可带 `sysroot/lib64`（glibc/ld 不匹配）；数据放默认 DataDir `/root/.local/pinyin-ime/share/libime`。
- 环境限制：Xvfb XKB 硬编码 `/usr/bin/xkbcomp` 且本机只读无 CAP_SYS_ADMIN，图形 XIM 端到端本机不可用（非代码缺陷）。


## v0.9.83 (2026-10-10)

### 新增：X11 桌面链路全通 + 免 CLI「搜狗式」体验
- **XIM 端到端 21/21**（`scripts/xvfb_ime_test.sh`）：全拼/简拼/M3纠错/翻页/焦点/撤回
- **GTK3 真实应用 10/10**（`scripts/xvfb_desktop_ime_test.sh`）：GTK_IM_MODULE=fcitx → DBus → fcitx5
- **Qt5 真实应用 ✅**：`tools/qt_ime_test_client` + fcitx5-qt 5.1.4 platforminputcontext 插件
- **候选窗中文渲染 ✅**：classicui + 文泉驿微米黑（截图 `docs/assets/candidate-window.png`）
- **免 CLI 安装 `scripts/install_desktop.sh`**：登录自启（.desktop）、environment.d/xprofile 免配环境变量、
  默认输入法 = pinyinime、中文字体自动安装、自检 8 项；装完重登即可打字；
  **自包含 deps-lib**（conda 运行时库随前缀分发，目标机无需 conda）
- **S3 产物 `cst:xbz/pinyin-ime/v0.9.83/`**：新增 91MB 桌面自包含 bundle
  `pinyin-ime-desktop-v0.9.83.tar.gz`（解压 `$HOME/.local` 即用），+ 常规 CLI/addon/LM/dict，
  部署用法见 README「S3 产物与部署到其它容器」

### 新增脚本/工具
- `scripts/build_fcitx5_qt.sh`：fcitx5-qt 5.1.4（conda gcc + conda Qt5）
- `scripts/fix_sysroot_pc.sh`：只读 /usr 沙箱下批量改写 RPM 前缀 .pc 的硬编码路径
- `scripts/sandbox-bin/{Xvfb,dbus-daemon}`：沙箱 wrapper（库路径 + session.conf 指路）
- `tools/qt_ime_test_client.cpp`：Qt5 端到端输入验证客户端

### 修复/改进
- `build_fcitx5_x11.sh`：自动探测 SYSROOT，系统头与 xkbfile/libxml2/shared-mime-info .pc 兜底
- `build_fcitx5_gtk.sh`：FindX11 指路 SYSROOT；只读 /usr 时 immodules.cache 降级前缀 + GTK_IM_MODULE_FILE
- `xvfb_test_env.sh`：dbus 会话地址从文件恢复（子进程 export 无法传回父进程）

## v0.9.67 (2026-10-09)

### 改进
- **后纠错规则扩展**: 从语料评测错误中提取新的同音字混淆模式
  - 像/想: "如何想保护" → "如何像保护"
  - 片/骗: "好看的骗子" → "好看的片子"
  - 哪/那: "放在那里了" → "放在哪里了"
  - 只/指: "可以只出" → "可以指出"
  - 带/怠: "笃行不带" → "笃行不怠"
  - 舅/久: "舅舅围攻" → "久久为功"
  - 是/试: "电话是听" → "电话试听"
  - 修复在/再规则误修: "在做什么"/"在看" 是正确用法，不再误改
- **smart.py 统一入口**: CLI预测 → 后纠错 → 频率boost → 输出
  - `scripts/pinyin_ime_smart.py`
  - 支持 --batch/--top1/--top-k/--record-freq/--correct-only
- **Web Demo v2 API 验证**: 8个端点全部通过
  - /api?q=, /api?top1=, /api?predict=, /api?freq=1, /api?learn=, /api?correct=, /api?bench=1

### 评测结果 (v0.9.67)
| 语料 | v0.9.66 | v0.9.67 | 提升 |
|------|---------|---------|------|
| daily | 98.0% | 100.0% | +2.0% |
| dialogue | 72.6% | 73.5% | +0.9% |
| news | 39.0% | 40.5% | +1.5% |
| baike | 34.5% | 35.5% | +1.0% |

### CI
- 41/41 CLI, 40/40 daily, 315/315 E2E, 5 句/秒


## v0.9.66 (2026-10-09)

### 新功能
- **YAML 配置系统**: 统一 CLI/addon/daemon 所有设置（模糊音、纠错、boost、双拼、频率、后纠错等），支持热重载
  - `config/pinyin-ime.yaml` 配置文件
  - `scripts/config_loader.py` 加载模块
- **用户频率追踪**: 记录词使用频率，高频词自动提升候选排序
  - `scripts/user_freq.py` 频率追踪模块
  - 频率衰减（30天未使用 × 0.95）、LRU 淘汰（最大 50000 词）
  - Daemon 新增 FREQ/FREQ_TOP/FREQ_STATS 命令
- **句子后纠错**: 初始预测后用规则引擎修正常见错误模式
  - 同音字混淆: 在/再、的/得/地、做/作、反应/反映
  - 量词搭配: 一只猫(非一支)、一支笔(非一只)
  - 常见错别字: 40+ 条成语/固定搭配纠正
  - `scripts/post_corrector.py` 后纠错引擎
  - Hard sentence 评测: 93.8% → 100%
  - Daemon TOP1 自动应用后纠错，新增 CORRECT 命令
- **Web Demo v2**: 改进 UI，添加 REST API 文档
  - 后纠错开关、频率统计面板
  - API 端点: /api?q=, /api?top1=, /api?predict=, /api?freq=1, /api?learn=, /api?correct=, /api?bench=1
  - `scripts/web_demo_v2.py`

### 文档
- 新增 `ARCHITECTURE.md`: 引擎架构、数据流、扩展点、性能特征
- 新增 `CONTRIBUTING.md`: 开发环境、代码规范、提交规范、发布流程

### 测试
- 后纠错测试: 14/14 全通过
- CLI 综合: 41/41
- Daily 评测: 40/40 = 100%
- E2E 端到端: 315/315 = 100%


## v0.9.50 (2026-10-06)

### 修复
- **安全**: git rm --cached secrets.txt ip.txt，凭据不再入库（P0）
- **代码质量**: 删除 abbr_presets.h 中 10 个重复 key（263→253 条）
- **代码质量**: 修正 dasuju→dashuju 等 3 个错误拼音条目
- **架构**: disambiguate_pinyin 合并到 abbr_presets.h 共享头文件，消除 CLI/addon 205 行重复代码
- **消歧**: 扩展音节歧义处理：新增 nian→ni'an、qian→qi'an（原有 xian→xi'an）

### 文档
- 统一版本号到 0.9.50（VERSION/CMakeLists/PROJECT_RULES/CLI）
- 更新过期文档：源码行数（CLI 1276/addon 1015）、数据大小（sc.dict 4.2MB/zh_CN.lm 33.1MB）
- 标记 lccc_eval.txt 指标为"已作废"（训练集泄漏）
- 更新 PROJECT_STATUS/EVALUATION/ARCHITECTURE 中过期版本号和数字

### 未完成（需构建环境）
- P3-9: 提供 374MB 剪枝 LM 作为可选高精度包（需 openEuler 实测）
- P3-10: 训练混合语料 LM（需 openEuler 构建机）
- P3-11: 用 LM-based 消歧替代硬编码 boost 表（需 openEuler 实测）

## v0.9.16 (2026-09-30)

### 新功能
- **fcitx5 addon 拼音消歧**：addon 现在支持 xian→xi'an 自动拆分
  - 逐字输入时检测歧义音节，根据上下文自动拆分
  - 撤回(Ctrl-Z)也应用消歧
  - 新增6个E2E消歧测试用例
- **Web演示服务器**：`scripts/web_demo.py` 浏览器界面
  - 实时候选展示、模糊音/纠错/双拼切换
  - 性能统计（候选数、耗时、总查询数）

### 测试结果
- CLI综合测试: **41/41 (100%)**
- E2E端到端测试: **106/106 (100%)**
- daily 40句准确率: **40/40 (100%)**
- 性能: **181句/秒**, P50=32ms, P99=36ms

## v0.9.15 (2026-09-30)

### 测试结果
- CLI综合测试: **41/41 (100%)**
- E2E端到端测试: **100/100 (100%)**
- daily 40句准确率: **40/40 (100%)**
- 性能: **175句/秒**, P50=32ms, P99=36ms
- 健康检查: **15/15 全通过**

### 功能清单
- ✅ 全拼输入（标准拼音）
- ✅ 简拼输入（首字母缩写 bj→北京）
- ✅ 模糊音（平翘舌/前后鼻音/n-l）
- ✅ 拼音纠错（M3模型 senme→什么）
- ✅ 双拼（8种方案：小鹤/自然码/微软/紫光等）
- ✅ 整句输入（长拼音自动分词）
- ✅ 拼音消歧（xian→xi'an 地名消歧）
- ✅ 用户学习（持久化 user.history + user.dict）
- ✅ 联想预测（上屏后联想）
- ✅ JSON输出（结构化候选）
- ✅ 表情输入（Ctrl-E）
- ✅ 误上屏撤回（Ctrl-Z）
- ✅ fcitx5集成（addon可加载）

### 工具链
| 工具 | 说明 |
|------|------|
| `scripts/demo.sh` | 功能演示（10类） |
| `scripts/ci_test.sh` | CI/CD全量测试 |
| `scripts/healthcheck.sh` | 安装健康检查（6类15项） |
| `scripts/generate_report.sh` | 测试报告生成器 |
| `scripts/warmup.sh` | 预置学习数据加载 |
| `scripts/user_dict.sh` | 用户词典管理 |
| `scripts/benchmark_compare.sh` | 性能对比 |

### 文档
| 文档 | 说明 |
|------|------|
| `QUICKSTART.md` | 快速入门（30秒上手） |
| `INSTALL.md` | 安装指南 |
| `docs/ARCHITECTURE.md` | 架构文档 |
| `test_report.md` | 自动生成测试报告 |

## 版本历史

| 版本 | 日期 | 主要改进 |
|------|------|---------|
| v0.9.16 | 09-30 | addon消歧 + Web演示 |
| v0.9.15 | 09-30 | 测试报告生成器 + 快速入门 |
| v0.9.14 | 09-30 | E2E扩展至100用例 |
| v0.9.13 | 09-30 | 安装指南 + 词典管理 + 健康检查 + 性能对比 |
| v0.9.12 | 09-30 | 消歧测试 + 演示脚本 + python路径修复 |
| v0.9.11 | 09-30 | 消歧增强(地名上下文) + 预热脚本 + 架构文档 |
| v0.9.10 | 09-30 | 全量100% + CI脚本 + 用户学习增强 |
| v0.9.9 | 09-30 | E2E扩展至68用例 + 综合测试报告 |
| v0.9.8 | 09-30 | E2E 40/40 + 用户学习共享 |
| v0.9.7 | 09-30 | E2E 39/40 + fcitx5 server构建 |

## 项目统计
- 源码: 2,354行 (CLI 1276 + addon 1015 + header 153)
- 脚本: 32个
- 文档: 22个
- 测试: 187项 (CLI 41 + E2E 106 + daily 40)
- 语言模型: 34MB (zh_CN.lm)
- 系统词典: 4.3MB (sc.dict)

## v0.9.68 (2026-10-09)

### 新增
- **长句切分**: 拼音音节解析器 + 边界空格插入，提升长句准确率
  - `scripts/pinyin_syllable.py`: 拼音音节解析器，基于声母韵母表 + 动态规划最优切分
  - `scripts/sentence_splitter.py`: 在自然语言边界音节（的/了/在/和/与等）后插入空格，给 LM 分词提示
  - 移除 shi/you/bu 避免拆散复合词（实现/有效/不断）
- **频率 boost 增强**: pinyin→word 精确映射
  - `user_freq.py` v2: 新增 pinyin_map 字段，记录哪个拼音选了哪个词
  - boost 时拼音精确匹配权重 ×3，优先提升用户历史选择
  - top1 模式内部取 top-k → 频率 boost → 返回 top1
- `pinyin_ime_smart.py` 重构: 集成长句切分 + 频率 boost + 后纠错统一流程
  - 新增 `--no-split` 禁用长句切分
  - `--record-freq` 记录 pinyin→word 映射

### 评测
- news: 46% → 47% (+1)
- wiki: 19% → 20% (+1)
- daily/dialogue/baike/recent: 无变化，零回退
- 频率 boost 验证: 重复选择正确提升至首屏
