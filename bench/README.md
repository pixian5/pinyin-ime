# pinyinbench — 整句准确率评测框架（M0）

做输入法最怕的是"改了一版，感觉变好了"——没有数字就没有判断依据。
这个框架就是先把尺子做出来。

## 快速开始

```sh
python3 -m venv .venv && . .venv/bin/activate
pip install -r requirements.txt          # pypinyin + opencc

# 1. 自检：oracle 引擎必须 100%，否则说明框架本身算错了
python3 run_bench.py --corpus corpus/wiki.txt --engine oracle

# 2. 对照组：echo 引擎把拼音原样返回，分数应接近 0
python3 run_bench.py --corpus corpus/wiki.txt --engine echo --limit 100

# 3. 真实基线：libime（需在容器内编译好适配器）
python3 run_bench.py --corpus corpus/wiki.txt --cli /src/build/libime_bench --show 20
```

## 语料

`corpus/wiki.txt` 来自中文维基百科随机条目，抓下来后就固化在本地，保证可复现。

```sh
python3 fetch_corpus.py --target 3000 --out corpus/wiki.txt
```

- 只保留 4–30 字的**纯汉字**短句
- 中文维基正文繁简混排，抓取时已用 OpenCC 统一转**简体**
- 默认**续抓**（合并已有文件），网络中断后重跑不会丢之前的成果；`--fresh` 才重新来
- 抓取脚本对 API 友好：每批 50 篇，轮间隔 1 秒

现状：`corpus/wiki.txt` 408 句 / 7148 字（热点环境下中断，待补）。
**这个量够验证框架，不够出权威基线**——基线建议 ≥2000 句。

## 指标

| 指标 | 定义 | 备注 |
|---|---|---|
| 完全准确率 | 整句首选 == 标准答案 的比例 | 最严苛 |
| 综合准确率 | LCS 匹配字数 / 标准答案字数 | 容忍长度不等 |
| 平均码长 | 拼音串长度 / 汉字数 | 全拼约 3.03 键/字 |
| 延迟 P50 / P99 | 单次 `predict` 耗时 | 含 IPC，不含进程启动 |

口径对齐 AutoRime / hanzi-chai 的公开做法，便于和它们的数字横向比较。

## 引擎接口

引擎只需实现一件事：

```python
def predict(self, pinyin: str) -> list[str]:
    """给连续全拼，返回候选列表，第一个即首选。"""
```

- `OracleEngine` 作弊引擎，只用于自检框架（必须 100%）
- `EchoEngine` 占位引擎，验证非满分场景统计正确
- `CliEngine` 通过子进程行协议调外部引擎：`写 "<pinyin>\n"` → `读一行，候选 \t 分隔`

外部引擎可以是 C++（libime / 自研）、Rust、任何东西——只要遵守这个行协议。
进程常驻，避免启动开销污染延迟统计。

## 输出

每次运行都在 `results/` 下产出：

- `<engine>_cases.csv` — 逐句明细，可 diff、可回归
- `<engine>_summary.json` — 汇总指标

改动前后跑两次，直接对比 CSV 就知道是变好还是变坏。

## 语料

现有两份，用 `--corpus` 切换：

| 语料 | 来源 | 状态 |
|---|---|---|
| `corpus/news.txt` | 人民网 RSS（时政/国际/社会/财经等 10 频道） | ✅ **当前基线语料** |
| `corpus/wiki.txt` | 中文维基百科 | ⚠️ 保留作对照，**不适合做基线**（专有名词密集） |

```sh
python3 fetch_news_corpus.py --target 2000 --out corpus/news.txt   # 约 4MB
python3 fetch_corpus.py      --target 2000 --out corpus/wiki.txt   # 维基
```

抓取时的清洗规则：**含数字/拉丁字母的句子整句丢弃**。因为过滤非汉字时
"6月5日" 会变成 "月日"，造出残缺句拖低分数。含 `丨` 的编辑署名行同理。

## 当前基线（libime 1.0.16，2000 句人民网新闻）

| 指标 | 实测 | AutoRime 参考（人民日报） |
|---|---|---|
| 完全准确率（句级） | 30.55% | 58.93% |
| 综合准确率（字级） | **85.79%** | 88.67% |
| 平均码长 | 3.099 键/字 | 3.08 |
| 延迟 P50 / P99 | ~26 / ~55 ms | — |

换语料带来的变化：

| 语料 | 句级 | 字级 |
|---|---|---|
| 维基百科 | 23.45% | 79.38% |
| 新闻（未清洗） | 25.05% | 84.66% |
| **新闻（清洗后）** | **30.55%** | **85.79%** |

剩余差距主要来自**句长**（本语料平均 19.1 字 vs 对照 9.0 字）和**词库规模**
（Debian 的 sc.dict 只有 6MB，对照用 370 万条）。
注意 20 字以上长句的字级已达 **88.76%**，与对照的 88.67% 持平——引擎本身不差。
详见 `PLAN-A.md` 第 9、10 节。

## 当前基线（libime 1.1.17 重跑，2026 数据）

在 openEuler 22.03 上源码编译 fcitx5 5.1.23 + libime **1.1.17**（gcc 13 工具链），
数据为 2026 年最新：`zh_CN.lm` **34,736,327 B**（`lm_sc.arpa-20260629`）、
`sc.dict` **4,528,345 B**（`dict-20260907`）。`libime_bench` 启动时把这两个字节数
打到 stderr，是「词库版本」的直接证据。

同一份 `corpus/news.txt` 2000 句：

| 指标 | 1.0.16 | 1.1.17 | 变化 |
|---|---|---|---|
| 完全准确率（句级） | 30.55% | **39.55%** | +9.0 pct |
| 综合准确率（字级） | 85.79% | **88.82%** | +3.0 pct |
| 平均码长 | 3.099 | 3.099 | — |
| 延迟 P50 / P99（整句一次喂完） | ~26 / ~55 ms | 58.28 / 107.94 ms | 跨机器不可比 |

字级 88.82% 越过 88% 阈值 → **Route A 成立**：差距主要来自词库版本，详见 `PLAN-A.md` §12。

```sh
# 依赖：gcc-toolset-13 + 自建 deps（boost / ECM / zstd / gettext 等）
cmake --build <fcitx5-build> --target install -j8     # fcitx5 5.1.23
cmake --build <libime-build> --target install -j8     # libime 1.1.17（自动下载并生成 LM/词典）
python3 bench/run_bench.py --corpus corpus/news.txt \
  --cli cpp/build/libime_bench \
  --cli-arg <prefix>/lib64/libime/zh_CN.lm \
  --cli-arg <prefix>/share/libime/sc.dict --cli-arg 5
```

> libime 把 LM 从 `sc.lm` 重命名安装为 `zh_CN.lm`，在 `lib64/libime/`（openEuler
> 因 libdir=lib64）；`sc.dict` 在 `share/libime/`。适配器默认搜索路径已补 openEuler
> 的 `lib64`。此外 openEuler 的 boost-devel 不带 CMake config，libime 顶层的
> `find_package(Boost ... CONFIG)` 需改为 `MODULE`。

## 逐键延迟评测（perkey）

`run_bench.py` 里的「延迟」是整句一次喂完的 round-trip 耗时（平均 19 字约 26ms），
跟真实输入法「每敲一键、引擎增量更新」不是一回事。为此在 `cpp/libime_bench.cpp`
加了 `perkey` 逐键模式，配合 `bench/latency_perkey.py` 量单键成本：

```sh
python3 latency_perkey.py --corpus corpus/news.txt --cli /path/to/libime_bench --limit 1000
```

| 指标 | 1.0.16（1000 句 / 59803 键） | 1.1.17（2000 句 / 118309 键） |
|---|---|---|
| 平均单键 | 1.15 ms | **0.98 ms** |
| P50 / P99 | 0.92 / 2.86 ms | **0.80 / 2.54 ms** |

结论：之前 ~26ms 是「整句一次喂完」的口径，拆到单键只有 1ms 级——
真实逐键输入的体感延迟远低于整句一次喂完。两者不可比，各测各的。

## 首词命中率评测

整句准确率测「整句一次上屏」，首词命中率测「想要的词在不在第一位、要不要翻页」，
后者体感更直接。`bench/first_word.py` 支持两个词源：

```sh
# jieba 新闻分词词源
python3 first_word.py --source jieba --corpus corpus/news.txt --cli /path/to/libime_bench
# rime-essay 八股文词表（词+词频；词表缺省自动下载到 corpus/essay.txt）
python3 first_word.py --source essay --cli /path/to/libime_bench
```

| 词源 | 1.0.16 top1/top5 | 1.1.17 top1/top5 | 说明 |
|---|---|---|---|
| 新闻分词（jieba） | 89.30% / 97.00% | **89.33% / 96.00%** | @300 词，持平 |
| essay 词表 | 74.28% / 95.18% | **95.80% / 99.90%** | @2000 词，大涨 |

> 取样口径：jieba 旧基线对应约 300 词；essay 旧 74.28% 是旧引擎+旧词库（2022）。
> 新引擎 essay 各词量下均明显更高（@500=98.00%、@2000=95.80%、@10000=89.66%）。

> **坑（已踩）**：essay 词表是繁体，必须先 OpenCC 繁转简再生成拼音，否则
> 「一個」的 gold 是繁体、引擎却输出简体「一个」，整词被误判为未命中
> （500 词从 88% 虚降到 51%）。`first_word.py` 已默认走 t2s。

两套新工具和主评测一样，用 `oracle`（≈100%）/ `echo`（≈0%）自检，改动前后各跑一次即可回归。

## ✅ 已完成：libime 新版本基线重跑

原计划的 libime 1.1.5 基线已落地——本机 openEuler 22.03 无容器运行时，改为
**源码编译 libime 1.1.17**（比 1.1.5 更新），词库/语言模型为 2026 年数据。
结果见上「当前基线（libime 1.1.17 重跑）」：句级 30.55%→**39.55%**、
字级 85.79%→**88.82%**，**Route A 成立**（差距主要在词库版本）。
备选 docker 路径仍在 `docker/Dockerfile.noble`：

```sh
docker build -f docker/Dockerfile.noble -t pinyin-bench-1.1.5 .
docker run --rm -v "$PWD/bench":/bench -w /bench pinyin-bench-1.1.5 bash -c \
  "python3 run_bench.py --corpus corpus/news.txt --cli /src/build/libime_bench"
```

`libime_bench` 启动时会往 stderr 打印 `lm`/`dict` 路径与字节数——对比这一行
即可确认 sc.dict 规模有没有变。详见 `PLAN-A.md` 第 12 节。

## 容器构建踩坑记录

libime 在 Debian 12 上的路径全是"非标准"的，第一次构建会连撞 6 个错误，都记在 `docker/Dockerfile` 里了：

| 坑 | 现象 | 解法 |
|---|---|---|
| Docker Hub 拉不动 | `auth.docker.io` TLS 超时 | `docker build --build-arg BASE_IMAGE=<本地 Debian 镜像>` |
| 没有 `make` | cmake 报 `CMAKE_MAKE_PROGRAM is not set` | 干脆不用 cmake，直接 g++ |
| 头文件不在 `/usr/include` | `libime/core/lattice.h: No such file` | 实际在 `/usr/include/LibIME`，加 `-I` |
| core 头文件缺失 | `historybigram.h` 找不到 | `libimepinyin-dev` 不带 core 头，补装 `libimecore-dev` |
| Boost 头文件缺失 | `boost/range/adaptor/...` | 补装 `libboost-dev` |
| fcitx-utils 头缺失 | `fcitx-utils/macros.h` | 补装 `libfcitx5utils-dev`，路径 `/usr/include/Fcitx5/Utils` |
| 库名大小写 | `cannot find -limepinyin` | 真名是 **`libIMEPinyin` / `libIMECore`**（IME 大写） |
| 构造函数不接受 `std::string` | 重载匹配失败 | `UserLanguageModel(const char*)`、`dict->load(idx, const char*, fmt)` |
| 语言模型文件名 | 找不到 `sc.lm` | Debian 上叫 **`zh_CN.lm`**，在 `/usr/lib/<triplet>/libime/` |

## 已知限制

- 维基语料偏书面、专有名词多，和日常聊天有分布差异；后续应补新闻 / 口语 / 技术三类
- 首词命中率目前只在「完美全拼、无模糊音、无用户词库」条件下测，未覆盖模糊音/简拼场景
- 评测不启用模糊音，也不加载用户词库——保证结果可复现
