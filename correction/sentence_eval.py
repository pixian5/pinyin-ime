"""真实整句纠错评测（需 pypinyin；测的是真实语料，不再是合成错字表）。

现有 `metric.py` 的 [A]/[B]/[C] 都是「孤立音节 + 合成错字」，看不到纠错模块在真实
句子上到底多好用。这里补一条更高层的口径：

  把一句中文转成拼音音节串 → 随机打错**一个**音节 → 用 `correct()`（整串二元 Viterbi）
  整串恢复 → 看能否还原原始音节序列（整句还原率）。并对比「有上一音节上下文
  （LAMBDA=0.15） vs 无上下文（LAMBDA=0）」，直接在真实输入上量化 transition 模型的
  边际价值。

默认按四个域分别评测：对话（留出测试集，不进训练语料）、新闻、维基、百科。

用法:
    python3 -m correction.sentence_eval                       # 四域全景
    python3 -m correction.sentence_eval --domains dialogue    # 只看对话
"""
from __future__ import annotations

import argparse
import random
from pathlib import Path
from typing import Dict, List, Optional, Tuple

from . import transition
from .corrector import best_corrected
from .syllables import SYLLABLE_SET
from .typo import corrupt_syllable

KINDS = ("adjacent", "delete", "insert", "substitute", "retroflex", "nasal")


def _get_pypinyin():
    try:
        from pypinyin import lazy_pinyin
        return lazy_pinyin
    except ImportError:
        raise SystemExit("需要 pypinyin: pip install pypinyin")


def to_syllables(sentence: str) -> Optional[List[str]]:
    """句子 → 音节列表；含非普通话音节（如 嗯→'n'）时返回 None（跳过）。"""
    lazy_pinyin = _get_pypinyin()
    ps = lazy_pinyin(sentence)
    return ps if all(x in SYLLABLE_SET for x in ps) else None


def _corrupt_one(syls: List[str], rng: random.Random, kind: str):
    """随机打错一个音节，返回 (打错后的整串, 原始音节列表)；无法按 kind 打错则 None。"""
    for _ in range(40):
        idx = rng.randrange(len(syls))
        t = corrupt_syllable(syls[idx], rng, kind)
        if t != syls[idx]:
            noisy = list(syls)
            noisy[idx] = t
            return "".join(noisy)
    return None


def evaluate(
    domains: List[Tuple[str, str]],
    per_domain: int,
    seed: int,
    kinds: List[str],
) -> Dict[str, Dict[str, Dict[str, float]]]:
    """返回 {域: {错字类型: {ctx: 恢复率, base: 恢复率, total}}}。"""
    rng = random.Random(seed)
    out: Dict[str, Dict[str, Dict[str, float]]] = {}
    for name, path in domains:
        lines = [l.strip() for l in Path(path).read_text(encoding="utf-8").splitlines() if l.strip()]
        rng.shuffle(lines)
        lines = lines[:per_domain]
        # 预转音节
        seqs = []
        for ln in lines:
            syls = to_syllables(ln)
            if syls and len(syls) >= 2:
                seqs.append(syls)
        domain_stats: Dict[str, Dict[str, float]] = {}
        for kind in kinds:
            hit_ctx = hit_base = total = 0
            for syls in seqs:
                noisy = _corrupt_one(syls, rng, kind)
                if noisy is None:
                    continue
                total += 1
                # 有上下文
                if best_corrected(noisy) == syls:
                    hit_ctx += 1
                # 无上下文
                old = transition.LAMBDA
                transition.LAMBDA = 0.0
                try:
                    if best_corrected(noisy) == syls:
                        hit_base += 1
                finally:
                    transition.LAMBDA = old
            domain_stats[kind] = {
                "total": total,
                "base": 100.0 * hit_base / total if total else 0.0,
                "ctx": 100.0 * hit_ctx / total if total else 0.0,
            }
        out[name] = domain_stats
    return out


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--domains", nargs="*", default=None, help="默认 dialogue/news/wiki/baike")
    p.add_argument("--per-domain", type=int, default=40)
    p.add_argument("--seed", type=int, default=0)
    p.add_argument("--kinds", nargs="*", default=["delete", "substitute", "insert", "adjacent"])
    args = p.parse_args()

    root = Path(__file__).resolve().parents[1] / "bench" / "corpus"
    all_domains = [
        ("对话", root / "dialogue.txt"),
        ("新闻", root / "news.txt"),
        ("维基", root / "wiki.txt"),
        ("百科", root / "baike.txt"),
    ]
    if args.domains:
        by_name = {d[0]: d[1] for d in all_domains}
        domains = [(n, by_name[n]) for n in args.domains if n in by_name]
    else:
        domains = all_domains

    results = evaluate(domains, args.per_domain, args.seed, args.kinds)

    print("真实整句纠错评测（打错一个音节 → 整串恢复率，%）")
    print("=" * 64)
    kinds = args.kinds
    header = "  " + "".join(f"{k:>16}" for k in kinds)
    print(f"{'域':<6}{header}")
    for name, stats in results.items():
        row_b = f"  {name:<4} 基础"
        row_c = f"  {'':<4} 有上下文"
        for k in kinds:
            s = stats.get(k)
            if s and s["total"]:
                row_b += f"{s['base']:>15.1f} "
                row_c += f"{s['ctx']:>15.1f} "
            else:
                row_b += f"{'-':>16}"
                row_c += f"{'-':>16}"
        print(row_b)
        print(row_c)
    print("\n说明：『基础』= LAMBDA=0（纯信道+先验）；『有上下文』= LAMBDA=0.15（叠加二元转移）。")
    print("对话域是留出测试集（未参与训练），最能反映泛化。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())