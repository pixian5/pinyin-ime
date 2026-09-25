#!/usr/bin/env python3
"""从中文语料统计「音节二元转移」代价，生成 correction/data/bigram_costs.json。

用法:
    python3 -m correction.scripts.calibrate_transition
    python3 -m correction.scripts.calibrate_transition --corpus /path/a.txt /path/b.txt

输出: correction/data/bigram_costs.json —— 供 correction/transition.py 惰性加载。

设计（与 prior.py 同哲学：数据驱动、代价 = -ln 概率、缩放只做「影响力」控制）：

  1. 只统计「相邻两个合法音节」的二元转移。lazy_pinyin 输出非合法音节（如
     语气词 嗯→'n'）当作**序列断点**，不跨断点计二元，避免捏造不存在的转移。
  2. 用 **加一式平滑（Lidstone, s=0.5）** 统一「见过 / 没见过」两种情形：

         P(cur | prev) = (count(prev,cur) + s) / (count(prev) + s·V)

     这样罕见音节不会因为只出现一次就造出 cost≈0 的假高置信（MLE 的过拟合），
     没见过的一元对也会自然落进「比见过更贵」的区间，无需单独设回退惩罚。
  3. JSON 里存「自然对数、未缩放」的原始值 + 每个 prev 的平滑分母 denom[prev]，
     运行时缩放因子 LAMBDA 在 transition.py 顶部（职责同 prior.py 的 ALPHA）。
"""
from __future__ import annotations

import argparse
import json
import math
import re
from collections import Counter
from pathlib import Path


def _get_pypinyin():
    try:
        from pypinyin import lazy_pinyin
        return lazy_pinyin
    except ImportError:
        raise SystemExit("需要 pypinyin: pip install pypinyin")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--corpus", nargs="*", default=None,
                   help="语料文件路径（默认 bench/corpus/ 下 wiki + news + daily + recent + baike）")
    p.add_argument("--smooth", type=float, default=0.5,
                   help="加一式平滑 s（默认 0.5）")
    p.add_argument("--out", default=None, help="输出 JSON 路径")
    args = p.parse_args()

    lazy_pinyin = _get_pypinyin()
    from correction.syllables import SYLLABLES, SYLLABLE_SET

    project_root = Path(__file__).resolve().parents[2]
    if args.corpus:
        corpus_files = [Path(f) for f in args.corpus]
    else:
        corpus_files = [project_root / "bench" / "corpus" / "wiki.txt",
                        project_root / "bench" / "corpus" / "news.txt",
                        project_root / "bench" / "corpus" / "daily.txt",
                        project_root / "bench" / "corpus" / "recent.txt",
                        project_root / "bench" / "corpus" / "baike.txt"]
    if args.out is None:
        out = project_root / "correction" / "data" / "bigram_costs.json"
    else:
        out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    unigram: Counter = Counter()
    bigram: Counter = Counter()
    total_chars = 0
    for f in corpus_files:
        if not f.exists():
            print(f"警告: 语料不存在: {f}")
            continue
        for line in f.read_text(encoding="utf-8").splitlines():
            han = re.sub(r"[^\u4e00-\u9fff]", "", line)
            if not han:
                continue
            total_chars += len(han)
            run: list[str] = []
            for x in lazy_pinyin(han):
                if x in SYLLABLE_SET:
                    run.append(x)
                else:  # 非合法音节 → 序列断点
                    for a, b in zip(run, run[1:]):
                        bigram[(a, b)] += 1
                    for a in run:
                        unigram[a] += 1
                    run = []
            for a, b in zip(run, run[1:]):
                bigram[(a, b)] += 1
            for a in run:
                unigram[a] += 1

    total = sum(unigram.values())
    V = len(SYLLABLE_SET)
    s = args.smooth

    # 每个 prev 的平滑分母：denom[prev] = count(prev) + s·V
    denom = {syl: round(unigram.get(syl, 0) + s * V, 4) for syl in SYLLABLES}

    # observed 二元的平滑代价（自然对数，未缩放）
    trans = {}
    for (prev, cur), n in bigram.items():
        trans[f"{prev}|{cur}"] = round(-math.log((n + s) / denom[prev]), 6)

    # 分布统计
    vals = sorted(trans.values())
    def q(pct):
        return vals[min(len(vals) - 1, int(pct / 100.0 * len(vals)))]

    print(f"汉字总数: {total_chars}")
    print(f"合法音节总数: {total}, 独立二元: {len(bigram)}, 覆盖音节: {sum(1 for x in SYLLABLE_SET if unigram[x])}/{V}")
    print(f"平滑代价分布(未缩放): min={vals[0]:.3f} p25={q(25):.3f} median={q(50):.3f} p75={q(75):.3f} max={vals[-1]:.3f}")

    # 真正高频的转移（按频次，不是按代价）
    print("\n频次最高的 15 个二元转移：")
    for (prev, cur), n in bigram.most_common(15):
        print(f"  {prev}→{cur}: 频次 {n:5d}, cost {trans[f'{prev}|{cur}']:.3f}")

    # 未见情形的代价范围（用于对 LAMBDA 有个直觉）
    unseen_lo = min(-math.log(s / d) for d in denom.values())
    unseen_hi = max(-math.log(s / d) for d in denom.values())
    print(f"\n未见二元的代价范围: {unseen_lo:.3f} ~ {unseen_hi:.3f}（随 prev 频度变化）")

    meta = {
        "total_syllables": total,
        "unique_bigrams": len(bigram),
        "syllable_count": V,
        "smooth": s,
    }
    payload = {"meta": meta, "denom": denom, "trans": trans}

    with open(out, "w", encoding="utf-8") as f:
        json.dump(payload, f, ensure_ascii=False, separators=(",", ":"))
    print(f"\n已写入 {out} ({out.stat().st_size} bytes)")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())