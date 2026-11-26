#!/usr/bin/env python3
"""候选召回上限分析（v0.9.180）：给定引擎的**带 N 候选 dump**，算 recall@K 与候选内字级上限。

用途：回答"再训练重排序/换 LM 能涨多少"这类问题，避免拍脑袋定目标。

与 eval.py 的分工：
  eval.py   只测 top1（用户实际拿到的），是产品口径；
  recall.py 看前 K 个候选里有没有正解，是**上限口径**——用来判断瓶颈在"选不对"还是"根本没生成"。

dump 格式（一行一条输入，制表符分隔，第一列是首选）：

    libime_bench /usr/lib/x86_64-linux-gnu/libime/zh_CN.lm \
                  /usr/share/libime/sc.dict 10 < bench/holdout/all_pinyin.txt > /tmp/dump.txt
    python3 bench/recall.py --dump /tmp/dump.txt

**行序必须与 holdout 一致**：holdout 目录下按文件名排序（baike/news/recent/wiki），
每个文件按行序；同 `cat bench/holdout/*.tsv | cut -f1` 的顺序。

用法：
    python3 bench/recall.py --dump /tmp/dump.txt [--holdout bench/holdout] [--ks 1,5,10]
"""

from __future__ import annotations

import argparse
import sys
from pathlib import Path


def levenshtein(a: str, b: str) -> int:
    """编辑距离；字级准确率 = 1 - 距离/长度。"""
    if a == b:
        return 0
    if not a or not b:
        return len(a) or len(b)
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
        prev = cur
    return prev[-1]


def char_acc(pred: str, gold: str) -> float:
    return 1.0 - levenshtein(pred, gold) / max(len(gold), len(pred) if pred else len(gold))


def load_holdout(holdout_dir: Path) -> list[tuple[str, str, str]]:
    """按 cat *.tsv 的文件名序读取，返回 (域, 拼音, 期望文本)。"""
    rows: list[tuple[str, str, str]] = []
    for f in sorted(holdout_dir.glob("*.tsv")):
        for line in f.read_text(encoding="utf-8").splitlines():
            if not line.strip():
                continue
            parts = line.split("\t")
            py, txt = parts[0], parts[1]
            rows.append((f.stem, py, txt))
    return rows


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--dump", type=Path, required=True, help="引擎候选 dump（一行一条，\\t 分隔）")
    ap.add_argument("--holdout", type=Path, default=Path(__file__).resolve().parent / "holdout")
    ap.add_argument("--ks", default="1,5,10")
    args = ap.parse_args()

    ks = [int(x) for x in args.ks.split(",") if x.strip()]
    rows = load_holdout(args.holdout)
    dump = [l.split("\t") for l in args.dump.read_text(encoding="utf-8").splitlines()]
    if len(dump) != len(rows):
        print(f"❌ 行数不一致：dump {len(dump)} 行，holdout {len(rows)} 行", file=sys.stderr)
        print("   检查 dump 的输入文件顺序是否与 holdout 一致（baike/news/recent/wiki）", file=sys.stderr)
        return 2

    per_domain: dict[str, dict] = {}
    hits = {k: 0 for k in ks}
    char_ceiling = 0.0
    for (dom, _py, gold), cands in zip(rows, dump):
        d = per_domain.setdefault(dom, {"n": 0, **{f"h{k}": 0 for k in ks}, "char": 0.0})
        d["n"] += 1
        for k in ks:
            if gold in cands[:k]:
                hits[k] += 1
                d[f"h{k}"] += 1
        # 字级上限只取前 max(ks) 列：libime_bench 在 nbest 之后还会追加单词级候选，
        # 那些不属于"整句候选"，算进来会虚高（实测 90.38% → 90.21%）。
        nonempty = [c for c in cands[:max(ks)] if c]
        if nonempty:
            best = max(char_acc(c, gold) for c in nonempty)
            char_ceiling += best
            d["char"] += best

    n = len(rows)
    print(f"{'域':<8}{'句数':>6}" + "".join(f"{'recall@'+str(k):>12}" for k in ks) + f"{'字级上限':>10}")
    for dom in sorted(per_domain):
        d = per_domain[dom]
        line = f"{dom:<8}{d['n']:>6}" + "".join(f"{d['h'+str(k)]/d['n']*100:>11.2f}%" for k in ks)
        line += f"{d['char']/d['n']*100:>9.2f}%"
        print(line)
    line = f"{'合计':<8}{n:>6}" + "".join(f"{hits[k]/n*100:>11.2f}%" for k in ks)
    line += f"{char_ceiling/n*100:>9.2f}%"
    print(line)
    print("\n读法：recall@K = 前 K 个候选里出现完整正解的比例（重排序的句级上限）；")
    print("      字级上限 = 取前 K 个里字级最高者（词典/解码的字级上限）。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
