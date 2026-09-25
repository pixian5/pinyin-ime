"""纠错命中率评测（纯标准库，不依赖 libime / pypinyin）。

衡量三件事，对应模块的三层职责：

  A. 未命中纠错（dictionary-miss）——打出「不是合法音节」的错串时，能否召回并
     把原音节排到 top-K。这是 CHIME 的「词典未命中」场景，是本模块的核心职责。

  B. 音节级混淆排序——平翘舌 / 前后鼻音打错会得到「另一个合法音节」（如 zh→z
     后 "zong" 仍合法），这时原音节应排在合法错串之后（第 2 位）。这类歧义本质
     要靠词级语言模型解，本模块只验证「信道排序把混淆兄弟排得够高」。

  C. 词级上下文消歧——对语料里「见过」的二元 (prev, s)，把 s 打成非合法错串，
     对比「无上下文」与「有 prev 上下文」的 top-1 命中。二元转移模型（transition.py）
     若有效，应表现为 top1 提升（flip 多、broken 少）。

用法：
    python3 -m correction.metric            # 在仓库根目录运行
    python3 correction/metric.py            # 或直接跑文件
"""

from __future__ import annotations

import random
from typing import Dict, List, Tuple

from .channel import confusion_siblings
from .corrector import correct_syllable, correct_syllable_in_context
from .syllables import SYLLABLES, SYLLABLE_SET
from .typo import corrupt_to


def miss_recovery(seed: int = 0, kinds: Tuple[str, ...] = ("delete", "insert", "substitute")
                  ) -> Dict[str, Dict[str, float]]:
    """指标 A：未命中纠错的 top-1 / top-3 命中率（按错字类型分组）。

    对每个音节、每种错字类型，生成一个「不落在音节表里」的错串（保证进入纠错分支），
    再跑 correct_syllable 看原音节是否被召回并排进 top-1 / top-3。
    """
    rng = random.Random(seed)
    stats: Dict[str, Dict[str, int]] = {}
    for kind in kinds:
        hit1 = hit3 = total = 0
        for syl in SYLLABLES:
            t = corrupt_to(syl, rng, kind, predicate=lambda x: x not in SYLLABLE_SET)
            if t is None:
                continue  # 该音节无法制造「非音节」错字（如单字母音节），跳过
            total += 1
            ranked = [s for s, _, _ in correct_syllable(t)]
            if ranked and ranked[0] == syl:
                hit1 += 1
            if syl in ranked[:3]:
                hit3 += 1
        stats[kind] = {
            "total": total,
            "top1": 100.0 * hit1 / total if total else 0.0,
            "top3": 100.0 * hit3 / total if total else 0.0,
        }
    return stats


def confusion_ranking(seed: int = 0) -> Dict[str, float]:
    """指标 B：音节级混淆（平翘舌 / 前后鼻音）下，原音节是否排在 top-2。

    （合法错串自身排第 1，原音节应紧跟其后。）
    """
    hits = total = 0
    for syl in SYLLABLES:
        siblings = [s for s in confusion_siblings(syl) if s in SYLLABLE_SET]
        for sib in siblings:
            total += 1
            ranked = [s for s, _, _ in correct_syllable(sib)]
            if syl in ranked[:2]:
                hits += 1
    return {"total": total, "top2": 100.0 * hits / total if total else 0.0}


def context_recovery(seed: int = 0, kinds: Tuple[str, ...] = ("delete", "insert", "substitute"),
                     max_pairs: int = None) -> Dict[str, Dict[str, float]]:
    """指标 C：词级上下文消歧的边际价值。

    对语料里每个「见过」的二元 (prev, s)，把 s 打成非合法错串 t（进入纠错分支），
    分别用「无上下文 correct_syllable」和「有上下文 correct_syllable_in_context(t, prev)」
    排序，对比 top-1 / top-3。flip = 无上下文错、有上下文对；broken = 反之。

    返回每类错字的 top1/top3 提升与 flip/broken 计数。
    """
    from .transition import observed_bigrams

    rng = random.Random(seed)
    pairs = observed_bigrams()
    rng.shuffle(pairs)

    stats: Dict[str, Dict[str, float]] = {}
    for kind in kinds:
        hit1_base = hit1_ctx = hit3_base = hit3_ctx = flip = broken = total = 0
        for prev, s in pairs:
            t = corrupt_to(s, rng, kind, predicate=lambda x: x not in SYLLABLE_SET)
            if t is None:
                continue
            total += 1
            base = [x for x, _, _ in correct_syllable(t)]
            ctx = [x for x, _, _ in correct_syllable_in_context(t, prev)]
            b1 = bool(base) and base[0] == s
            c1 = bool(ctx) and ctx[0] == s
            hit1_base += b1
            hit1_ctx += c1
            hit3_base += s in base[:3]
            hit3_ctx += s in ctx[:3]
            flip += (not b1) and c1
            broken += b1 and (not c1)
            if max_pairs and total >= max_pairs:
                break
        stats[kind] = {
            "total": total,
            "top1_base": 100.0 * hit1_base / total if total else 0.0,
            "top1_ctx": 100.0 * hit1_ctx / total if total else 0.0,
            "top3_base": 100.0 * hit3_base / total if total else 0.0,
            "top3_ctx": 100.0 * hit3_ctx / total if total else 0.0,
            "flip": flip,
            "broken": broken,
        }
    return stats


def _fmt_table(table: List[Tuple[str, str, str, str]]) -> str:
    w = max(len(r[0]) for r in table)
    out = []
    for name, a, b, c in table:
        out.append(f"  {name:<{w}}  {a:>6}  {b:>6}  {c:>6}")
    return "\n".join(out)


def main(argv=None) -> int:
    print("纠错命中率评测  (correction)")
    print("=" * 60)

    print("\n[A] 未命中纠错（dictionary-miss recovery）")
    print("    错字类型           top1      top3      样本")
    a = miss_recovery()
    rows = []
    for kind, s in a.items():
        rows.append((kind, f"{s['top1']:.2f}%", f"{s['top3']:.2f}%", str(s["total"])))
    print(_fmt_table([("类型", "top1", "top3", "样本")] + rows))

    b = confusion_ranking()
    print("\n[B] 音节级混淆排序（confusion sibling → top2）")
    print(f"    top2 命中率 = {b['top2']:.2f}%  （样本 {b['total']}）")

    print("\n[C] 词级上下文消歧（无上下文 → 有 prev 上下文）")
    c = context_recovery(max_pairs=5000)  # 采样 observed 二元，保证评测可复现且够快
    crows = []
    for kind, s in c.items():
        crows.append((
            kind,
            f"{s['top1_base']:.2f}% → {s['top1_ctx']:.2f}%",
            f"{s['top3_base']:.2f}% → {s['top3_ctx']:.2f}%",
            f"{s['total']}（flip {s['flip']} / broken {s['broken']}）",
        ))
    print(_fmt_table([("类型", "top1", "top3", "样本")] + crows))

    print("\n说明：A/B 是单音节（无上下文）；C 注入「上一音节」上下文，直接量化")
    print("     transition.py 的二元转移模型对 delete/substitute 消歧的边际价值。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())