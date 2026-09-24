# 中文输入法项目 · 可行性分析与设计草案

> 状态：**仅分析，未开工**。本文用于决策，不是实施方案。
> 编写日期：2026-09-24

---

## 0. 先回答一个前置问题：你说的"做项目"是哪种？

三条路的目标、难点和投入完全不同，先选清楚再动手，否则很容易做成一个"技术上很酷但没人用"的东西。

| 定位 | 对标 | 核心难点 | 现实回报 |
|---|---|---|---|
| **A. 好用的拼音输入法** | 搜狗 / 微软拼音 / WeType | 词库质量、首词命中率、桌面兼容矩阵 | 高，但要长期运营词库 |
| **B. 可定制的输入法框架** | Rime | 抽象设计、schema DSL、生态建设 | 中，Rime 已经很强，需要明确差异化 |
| **C. 技术验证 / 研究** | golia-pinyin、青简 | 单项算法（切分、排序、新架构） | 取决于目的，适合写博客/简历 |

**个人判断**：如果目标是 A，不要从零写引擎——先把现有轮子接起来跑通，再逐步替换你真正想改的部分。
如果目标是 C，那就别碰桌面协议，只做引擎核心库 + 评测脚本，把精力全压在算法和数据上。

---

## 1. 现有轮子盘点（2026-09 实况）

### 框架层

| 项目 | 语言 / 许可 | 现状 | 结论 |
|---|---|---|---|
| **Fcitx 5** | C++ / LGPL-2.1+ | 活跃，Wayland 支持好，addon 机制成熟 | **首选** |
| **IBus** | C / LGPL | GNOME 默认，稳定但封闭、延迟明显 | 除非目标用户是纯 GNOME，否则不推荐 |
| 直接实现 Wayland `input-method-v1/v2` | — | — | 代价极高，兼容矩阵会吃掉 80% 时间，不建议 |

### 引擎层

| 项目 | 语言 / 许可 | 现状 | 结论 |
|---|---|---|---|
| **libime**（Fcitx 官方自研） | C++20 / **LGPL** | **非常活跃**（2026-09-24 仍有提交，510 commits） | **「好用」路线的最强基线**：trigram 语言模型、多词库、含拼音/双拼/码表，有中文文档 |
| **librime**（中州韵核心） | C++17 / **BSD-3** | 活跃，1.17.0（2026-06），nightly 更新到 8 月，4.6k★ | **可定制框架**路线的首选：许可自由、schema 驱动、全平台前端 |
| **libpinyin** | C / **GPL-3** | 活跃，ibus-libpinyin 的后端 | bigram 模型；**注意 fcitx5-pinyin 并不用它**（见下） |
| **sunpinyin** | C++ / LGPL+CDL | **已停止维护** | 只看不 fork（libime 继承了它的数据与 trigram 算法） |
| 青简 Qingjian | Rust | 1.5k★，偏"语言学中文"场景 | 参考实现 |
| golia-pinyin | Rust / MIT-Apache | 0.1.0 脚手架，FST + PHF，8K SLoC | 参考实现，看架构思路有价值 |

> **易错点（已核实）**：`fcitx5-pinyin` 的后端是 **libime**，不是 libpinyin。
> Fcitx 作者在原帖中说明：libime 数据与 sunpinyin 同源，算法同为 **trigram**（libpinyin 是 bigram），
> 整句预测更好、内存更小、原生支持多词库，因此 fcitx5 明确不打算支持 libpinyin。
> 网上大量教程（含 CSDN 文章）把两者混为一谈，别被误导。

### 数据层

| 数据 | 说明 | 许可 |
|---|---|---|
| **rime-essay（八股文）** | Rime 官方共享词库 + 语言模型 | 自有条款，需单独确认 |
| **雾凇拼音 rime-ice** | 19k★，长期维护的简体词库方案 | LGPL-3.0 |
| **万象拼音** | 2.8k★，Lua 扩展丰富 | 需确认 |
| **pinyin-data** | 单字 → 拼音映射 | CC-BY 4.0 |
| **THUOCL** | 清华开放中文词库 | 需确认 |
| 深蓝词库转换 | 把搜狗/QQ 细胞词库转 Rime 格式 | 工具开源，**词库本身授权存疑，自用可以别分发** |

---

## 2. 一条被低估的资产：`wetype.cpp` 的进程外架构

现有 `fcitx5-wetype/src/wetype.cpp`（896 行）里有几块是**和引擎完全解耦**的：

- `EngineProc`：fork + pipe + Fcitx `EventLoop` IO 事件，主线程零阻塞
- FIFO handler 队列：请求与响应严格配对，不会乱序
- `revision_` 版本号：buffer 变了就丢弃过期响应
- 40ms 防抖 + 4 字符上限：连续输入只发一次批量请求
- 4×5 候选网格、部分选词（spans）、崩溃自动重启并重放 buffer

这套东西换任何后端都能直接用。**换句话说，你已经有一个可用的"输入法前端 + 后端进程管理"骨架，缺的只是一个能打的引擎。**

### 关键设计决策：保留进程外，还是改成进程内？

| | 进程外 + 行协议（现状） | 进程内直连 librime |
|---|---|---|
| 引擎崩溃 | 只死子进程，Fcitx 不受影响 | 拖垮整个 Fcitx |
| 引擎语言 | 任意（C++/Rust/Python 都行） | 必须是能链进 C++ 的 |
| 后端可替换 | 天然可插拔 | 强耦合 |
| 延迟 | 多一次管道 round-trip（微秒级，可忽略） | 略低 |
| 状态同步 | 需要自己维护 revision / 重放 | librime 自己管 session |

**建议：保留进程外 + 行协议。** 理由是你明确想要"后端可替换"，而这正是进程外架构的天然优势。librime 的 `process_key` 是毫秒级，管道开销相对完全可忽略。

---

## 3. 如果接 librime：API 长什么样

librime 提供稳定的 C 接口（`rime_api.h`），接入成本比想象中低：

```c
RimeApi* rime = rime_get_api();

RIME_STRUCT(RimeTraits, traits);
traits.app_name = "pinyin-ime";
traits.shared_data_dir = "/usr/share/rime-data";
traits.user_data_dir   = "/home/u/.local/share/pinyin-ime";
rime->setup(&traits);
rime->initialize(NULL);

RimeSessionId sid = rime->create_session();
rime->process_key(sid, XK_n, 0);            // 逐键喂
rime->process_key(sid, XK_i, 0);

RIME_STRUCT(RimeContext, ctx);
if (rime->get_context(sid, &ctx)) {
    // ctx.composition.preedit     → 拼音串（填 InputPanel::setPreedit）
    // ctx.menu.num_candidates     → 候选数
    // ctx.menu.candidates[i].text → 候选词
    // ctx.menu.page_size / select_keys
    rime->free_context(&ctx);
}

RIME_STRUCT(RimeCommit, commit);
while (rime->get_commit(sid, &commit)) { /* commit.text 上屏 */ rime->free_commit(&commit); }
```

其它常用接口：`RimeCandidateListBegin/Next/FromIndex`、`RimeSetOption`（切简繁/中英）、
`RimeGetSchemaList`/`RimeSelectSchema`（切方案）、`RimeDeployWorkspace`/`RimeStartMaintenance`（部署词库）、
`RimeSetNotificationHandler`（监听 deploy 事件）。

**注意**：`RIME_STRUCT()` / `RIME_STRUCT_INIT` 宏带结构体大小，是为了跨版本 ABI 兼容，必须用宏初始化，
不要直接 memset。

---

## 4. 如果自研引擎：技术分解

### 4.1 音节表与切分

- 403 个合法普通话音节（`pinyin-data` 可取）
- 连续拼音串 → 音节序列：DAG + Viterbi，或 Trie 前向匹配 + 回溯
- 必须处理的边角：
  - 简拼（`zhongguo` → `zg`）
  - 模糊音（z↔zh、l↔n、in↔ing）
  - 隔音符 `'`
  - 歧义切分（`xian` → `xi'an` / `xian`，`fangan` → `fan'gan` / `fang'an`）

### 4.2 词典

- 规模：常用词 10–30 万条才够用
- 结构：Double-array Trie / FST（`marisa-trie` 在 C++，`fst` crate 在 Rust），或 SQLite + 索引
- 热路径预算：**< 1ms**，冷启动加载用 mmap

### 4.3 排序（决定"好不好用"的地方）

- 基础层：unigram 词频
- 整句层：bigram/trigram 语言模型 + Viterbi 找最优路径（libpinyin / sunpinyin 都是这个思路）
- 用户层：自学习词频、用户词
- 小型神经网络：可做，但离线场景下收益/成本比不高，不建议一开始就上

### 4.4 周边（不做就不像个输入法）

简繁转换（OpenCC）、标点符号、符号表情、数字/日期快捷输入、英文模式、双拼、云候选（可选）

### 4.5 性能预算

| 指标 | 目标 |
|---|---|
| 击键 → 候选显示 | < 20ms（本地感知阈值） |
| 常驻内存 | < 100MB |
| 冷启动词典加载 | < 500ms（mmap） |

---

## 5. 工作量估算（诚实版）

| 里程碑 | 接 librime 路线 | 自研引擎路线 |
|---|---|---|
| 命令行能出候选（`nihao` → 你好） | 1–2 天 | 2–3 周 |
| Fcitx5 里能打字 | 1–2 周 | 2–3 个月 |
| 首词命中率 > 85%（"好用"门槛） | 换词库即可 | 6–12 个月 + 持续词库运营 |
| 桌面兼容矩阵（GTK/Qt/Electron/IDE/终端） | 持续投入 | 持续投入 |

**最反直觉的一点**：切分和排序算法只占全部工作量的 15% 左右，**数据的质量和规模才是护城河**。
算法再漂亮，词库只有 5 万条也不会好用。

---

## 6. 风险清单

1. **词库是最大瓶颈** —— 必须提前确定数据来源和授权，不能做到一半才发现词库不能分发
2. **许可传染** —— fork libpinyin 会带 GPL-3；librime 是 BSD-3 更自由；词库数据各有各的授权，要单独核
3. **Wayland 兼容** —— 用 Fcitx 5 可基本规避，但 Electron / JetBrains / 终端仍是个坑
4. **精力分散** —— 现有 WeType 移植项目已经是一个完整的工程，需要先想清楚是暂停还是并行
5. **"没差异化"陷阱** —— 如果只是把 librime 接进 Fcitx5，那和 `fcitx5-rime` 没区别，需要先想清楚你要改什么

---

## 7. 建议的下一步（决策点，按优先级）

1. **先定第 0 节的定位（A / B / C）** —— 这决定后面所有取舍
2. **决定架构**：保留"进程外 + 行协议"还是改进程内（我的建议是保留，见第 2 节）
3. **做最小验证**：用 librime 的 C API 写个 ~200 行 CLI，验证 `nihao → 你好` 全链路跑通
   —— 这一步只需 1–2 天，能一次性验证 API、词库、部署流程，比先写 addon 划算得多
4. **再写 Fcitx5 addon**：把 `wetype.cpp` 的 `EngineProc` 复用过来，只换后端
5. 之后才是逐步自研替换（切分 → 词典 → 排序），每次替换都用同一套 CLI 回归测试对比

---

## 8. 待确认的问题

- [ ] 产品定位：A / B / C？
- [ ] 差异化在哪？凭什么有人用它而不是 `fcitx5-rime` + 雾凇拼音？
- [ ] 目标许可？（决定能不能 fork libpinyin，能不能分发词库）
- [ ] 目标平台：只做 Linux，还是想复用引擎到其它平台？
- [ ] WeType 移植项目：暂停 / 并行 / 放弃？
