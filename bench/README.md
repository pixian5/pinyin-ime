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
- 尚未实现"首词命中率"（需要分词，计划接 jieba 后按词评测）
- 评测不启用模糊音，也不加载用户词库——保证结果可复现
