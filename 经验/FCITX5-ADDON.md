# fcitx5 addon 经验

- **真实上屏测试必须隔离会话**：用 `dbus-run-session` 包住 fcitx5、GTK im module 和测试前台，否则 GTK 与 fcitx5 不在同一 session bus，按键不会进入 addon。再配私有 `HOME`/`XDG_CONFIG_HOME`/`XDG_RUNTIME_DIR`/`DISPLAY`。
- **首句最容易假失败**：窗口 focus 后 fcitx5 还要切输入上下文；不等 0.8s 就把拼音原样回显进 Entry，会被误判成“addon 不工作”。
- **候选索引有三套空间**：libime 原始候选、`displayToRaw_` 重排后的显示索引、云候选/boost 插入槽位。`selectCandidate` 必须按同一映射走；短候选被重排到首位会让空格只上屏一个字（v0.9.197 已修）。
- **空格提交不等于当前显示首选**：重排、简拼 boost、`boostInsertWord_`、云候选都会改变实际索引；调试时打印 `idx/rerankActive/displayToRaw[0]/boostSwap/page/focus`，不要只看候选窗文字。
- **逐键消歧不要对半截拼音做 `ctx.clear()+重打`**：长句前缀 `xian` 会被拆成 `xi'an` 并永久锁死；只对 <=4 字母短缩写逐键消歧，长输入交给 libime 增量切分（v0.9.203 实测 28.00→30.25 / 80.31→86.24）。
- **不要每键都跑全量字符重排**：`refreshUI()` 里 `candidatesToCursor()` + 排序对长 preedit 很贵；长句 P95 会到 1.7s。可先更新 preedit，候选/重排去抖，提交前再补一次。
- **addon 的“真实路径”与 CLI batch 不同**：addon 每键刷新候选，libime 的增量切分可能与整串解码不同；不要拿 batch 结果直接判断 addon 排序 bug。
- **安装依赖要按发行版包名写**：Ubuntu 24.04 没有名为 `libime` 的包；deb 应依赖 `fcitx5, libimecore0, libimepinyin0, libime-data`。`--ignore-depends` 只能验证文件布局，不能证明普通用户能装。
- **配置默认值要核对**：`NBest=45`、`PageSize=9`、`EnableFuzzy=False`、`EnableCorrect=False`、`DataDir=/usr/share/libime`；CLI 评测默认 nbest=10，比较前先固定口径。
- **评测转储要绕开 UI 分页，但不能绕开显示索引**：直接 `ctx.candidatesToCursor()` 是 libime 原始池，空格实际选的是 `displayToRaw_[0]`/boost 后的候选；按显示顺序生成列表，Top1 才能和上屏结果一致。
- **addon 评测必须确认模型真的加载了**：`findDefault()` 会先找当前目录 `data/char_trigram.bin`，再找 `/usr/share/pinyin-ime/`；评测日志出现 `char_trigram.bin not found, rerank disabled` 时，结果只是无重排基线。全量前先 `PINYINIME_CHAR_LM=/usr/share/pinyin-ime/char_trigram.bin` 固定路径，或看启动日志的 `char rerank model=`。
- **夜间 job 要复用 package 产物**：独立 schedule run 没有上一次 push 的 artifact，必须在同一 workflow 里 `needs: package`，再下载 `pinyin-ime-${{ github.sha }}` 安装，否则会误以为缺包。
- **shadow 不能靠单点插入主候选**：把 shadow 首选塞进 `boostInsertWord_` 会打破显示索引/提交时序，v0.9.213 前的两种接法都掉到 0%～2%；必须让 `updateUI` 整段切到独立 `shadowCands_`，并让 `selectCandidate`、翻页、数字键、点击、Ctrl+←/→ 共用同一索引。
- **shadow 解码要持久 context，不能每键整串重放**：每键新建 `PinyinContext` 整串 type 会让单句 P50 ~500ms，连续输入时事件积压、空格超时，400 句 Top1 0.5% 且把上一句文本带入。跨阈值整串一次，之后增量 type，退格/消歧再重建。
- **shadow 候选池不是越大越好**：NBest60→100 在 400 句 Top1 无变化，wiki Top5 只 +1 句，P95 反而到 1.0s；先把 60 作为默认，增大前必须看延迟。
- **长句 shadow 的 Top1 增益不是全域**：v0.9.213 全量 baike +1.67、recent +2.34、news 持平、wiki -0.67；wiki 专名仍需实体词表，候选池/重排救不回来。
