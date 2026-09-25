#!/usr/bin/env python3
"""首词命中率评测：给定词表，喂整词全拼，看目标词是否排在候选第 1 / 前 5。

「首词命中率」衡量的是体感最直接的东西：想要的词在不在第一个位置、要不要翻页。
整句准确率测的是「整句一次上屏」，这里测的是「词级首选」。

两个词源：
  - essay : rime-essay 八股文词表（词 + 词频），真实输入法词表，词级测试集
  - jieba : 用 jieba 对新闻语料分词，得到语料里真实出现过的词

关键坑（已踩过）：essay 词表默认是繁体，必须先 OpenCC 繁转简再生成拼音，
否则「一個」的 gold 是繁体、引擎却输出简体「一个」，整词被误判为未命中，
top1 会从 ~88% 虚降到 ~51%。本脚本默认走 t2s。

用法:
    # essay 词源（词表可从本地读，缺省自动下载）
    python first_word.py --source essay --essay corpus/essay.txt --cli /path/to/libime_bench
    # jieba 新闻分词词源
    python first_word.py --source jieba --corpus corpus/news.txt --cli /path/to/libime_bench
    # 自检：echo 应 ≈0%，oracle 应 ≈100%
    python first_word.py --source jieba --corpus corpus/news.txt --engine echo --limit 200
"""
from __future__ import annotations

import argparse
import re
import sys
import time
import urllib.request
from pathlib import Path

from opencc import OpenCC

sys.path.insert(0, str(Path(__file__).resolve().parent))

from pinyinbench.core import (                       # noqa: E402
    Case,
    dump_json,
    hanzi_to_jianpin,
    hanzi_to_pinyin,
    load_sentences,
)
from pinyinbench.engines import CliEngine, ENGINES   # noqa: E402

CJK_RANGE = "\u4e00-\u9fff"
NON_CJK = re.compile(rf"[^{CJK_RANGE}]")

# 目标词长度：单字（的/是/了）首词命中过于简单，无法区分方案好坏，
# 这里只看 2–6 字的多字词，这才是「首词要不要翻页」真正见功力的地方。
DEFAULT_MIN_LEN = 2
DEFAULT_MAX_LEN = 6

UA = "pinyin-bench/0.1 (input-method accuracy research)"

# essay.txt 下载源：GitHub raw 是官方，jsdelivr 是 CDN 镜像（热点/弱网兜底）。
ESSAY_URLS = [
    "https://raw.githubusercontent.com/rime/rime-essay/master/essay.txt",
    "https://cdn.jsdelivr.net/gh/rime/rime-essay@master/essay.txt",
]


def ensure_essay(path: str, urls: list[str] = ESSAY_URLS) -> str:
    """essay.txt 已存在就返回路径，否则按候选源逐个下载（与 fetch 脚本同款策略）。"""
    p = Path(path)
    if p.exists():
        return str(p)
    p.parent.mkdir(parents=True, exist_ok=True)
    for url in urls:
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=60) as r:
                data = r.read().decode("utf-8", errors="ignore")
            p.write_text(data, encoding="utf-8")
            print(f"已下载 essay 词表 → {p}（{len(data.splitlines())} 行）", file=sys.stderr)
            return str(p)
        except Exception as exc:
            print(f"  下载失败 {url}: {exc}", file=sys.stderr)
            time.sleep(1)
    raise SystemExit("essay 词表下载失败，请用 --essay 指定本地文件")


def load_essay_words(path: str, min_len: int, max_len: int, t2s: OpenCC) -> list[str]:
    """解析 rime-essay essay.txt（"词\\t词频"），繁转简后按词频降序返回多字词。"""
    freq: dict[str, int] = {}
    with open(path, encoding="utf-8") as f:
        for line in f:
            raw = line.rstrip("\n")
            if not raw:
                continue
            parts = raw.split("\t")
            if not parts or not parts[0]:
                continue
            # 关键：繁体 → 简体，否则繁体 gold 与简体候选永远对不上
            w = NON_CJK.sub("", t2s.convert(parts[0])).strip()
            if not (min_len <= len(w) <= max_len):
                continue
            try:
                f_num = int(parts[1]) if len(parts) > 1 else 0
            except ValueError:
                f_num = 0
            # 同一简体词可能来自多个繁体原形（如「一個」「一个」都 → 一个），取最大词频
            if f_num > freq.get(w, 0):
                freq[w] = f_num
    return [w for w, _ in sorted(freq.items(), key=lambda kv: -kv[1])]


def load_jieba_words(corpus: str, min_len: int, max_len: int) -> list[str]:
    """对新闻语料 jieba 分词，摘出语料里真实出现过的 2–6 字词（去重、保序）。"""
    import jieba

    words: list[str] = []
    seen: set[str] = set()
    for sent in load_sentences(corpus):
        for tok in jieba.cut(sent):
            w = NON_CJK.sub("", tok).strip()
            if not (min_len <= len(w) <= max_len):
                continue
            if w in seen:
                continue
            seen.add(w)
            words.append(w)
    return words


def dedup_by_pinyin(cases: list[Case]) -> list[Case]:
    """按拼音去重（保留先出现的）。仅用于 oracle 自检，避免同音词碰撞导致 <100%。"""
    seen: set[str] = set()
    out: list[Case] = []
    for c in cases:
        if c.pinyin in seen:
            continue
        seen.add(c.pinyin)
        out.append(c)
    return out


def run(engine, cases: list[Case]) -> tuple[list[dict], dict]:
    rows: list[dict] = []
    hit1 = hit5 = 0
    for c in cases:
        cands = engine.predict(c.pinyin)
        top1 = bool(cands) and cands[0] == c.gold
        top5 = c.gold in cands[:5]
        hit1 += int(top1)
        hit5 += int(top5)
        rows.append({
            "word": c.gold,
            "pinyin": c.pinyin,
            "pred_top1": cands[0] if cands else "",
            "top1": int(top1),
            "top5": int(top5),
        })
    n = len(cases)
    metrics = {
        "n": n,
        "top1": round(hit1 / n, 4) if n else 0.0,
        "top5": round(hit5 / n, 4) if n else 0.0,
    }
    return rows, metrics


def build_engine(args, cases: list[Case]):
    if args.cli:
        return CliEngine([args.cli] + args.cli_arg,
                         name=args.cli_name or Path(args.cli).stem)
    cls = ENGINES.get(args.engine)
    if cls is None:
        raise SystemExit(f"未知引擎: {args.engine}（可选: {', '.join(ENGINES)}，或用 --cli）")
    eng = cls()
    eng.prepare(cases)
    return eng


def main() -> int:
    p = argparse.ArgumentParser(description="拼音输入法首词命中率评测")
    p.add_argument("--source", choices=["essay", "jieba"], required=True, help="词源")
    p.add_argument("--essay", default="corpus/essay.txt", help="essay 词表路径（缺省自动下载）")
    p.add_argument("--corpus", default="corpus/news.txt", help="jieba 词源用的语料")
    p.add_argument("--limit", type=int, help="只取前 N 个词（按词频/出现顺序）")
    p.add_argument("--min-len", type=int, default=DEFAULT_MIN_LEN)
    p.add_argument("--max-len", type=int, default=DEFAULT_MAX_LEN)
    p.add_argument("--jianpin", action="store_true", help="用首字母简拼作为输入（每音节取首字母）")
    p.add_argument("--engine", default="oracle", help=f"内置引擎: {', '.join(ENGINES)}")
    p.add_argument("--cli", help="外部引擎可执行文件路径")
    p.add_argument("--cli-arg", action="append", default=[], help="传给外部引擎的额外参数")
    p.add_argument("--cli-name", help="外部引擎在报告中的名字")
    p.add_argument("--out", default="results", help="结果输出目录")
    args = p.parse_args()

    # 1) 装词表
    if args.source == "essay":
        essay_path = ensure_essay(args.essay)
        words = load_essay_words(essay_path, args.min_len, args.max_len, OpenCC("t2s"))
    else:
        words = load_jieba_words(args.corpus, args.min_len, args.max_len)
    if args.limit:
        words = words[: args.limit]
    if not words:
        raise SystemExit("词表为空，请检查语料/词表路径与长度过滤参数")

    to_pinyin = hanzi_to_jianpin if args.jianpin else hanzi_to_pinyin
    cases = [Case(gold=w, pinyin=to_pinyin(w)) for w in words]
    # oracle 自检：按拼音去重，避免同音词（如「公式/公示」）让 oracle 只能返回一个而 <100%。
    # 真实引擎（--cli）用完整词表——同音词选词正是「首词命中率」要测的能力。
    run_cases = dedup_by_pinyin(cases) if (not args.cli and args.engine == "oracle") else cases
    engine = build_engine(args, run_cases)
    try:
        rows, metrics = run(engine, run_cases)
    finally:
        engine.close()

    eng_name = args.cli_name or Path(args.cli).stem if args.cli else args.engine
    print(f"引擎: {eng_name}    词源: {args.source}    词数: {metrics['n']}")
    print(f"  首词命中率 top1    {metrics['top1'] * 100:6.2f}%")
    print(f"  前五命中率 top5    {metrics['top5'] * 100:6.2f}%")

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    summary = {"engine": eng_name, "source": args.source, "metrics": metrics}
    json_path = out / f"first_word_{args.source}_{eng_name}_summary.json"
    dump_json(json_path, summary)
    print(f"  汇总              {json_path}")

    # 逐词明细 CSV（便于 diff：哪个词掉了、是不是同音词误判）
    import csv
    csv_path = out / f"first_word_{args.source}_{eng_name}_cases.csv"
    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=["word", "pinyin", "pred_top1", "top1", "top5"])
        w.writeheader()
        w.writerows(rows)
    print(f"  逐词明细          {csv_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())