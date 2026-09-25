"""纠错命中率评测（纯标准库，不依赖 libime / pypinyin）。

衡量两件事，对应本模块的两层职责：

  A. 未命中纠错（dictionary-miss）——打出「不是合法音节」的错串时，能否召回并
     把原音节排到 top-K。这是 CHIME 的「词典未命中」场景，是本模块的核心职责。

  B. 音节级混淆排序——平翘舌 / 前后鼻音打错会得到「另一个合法音节」（如 zh→z
     后 "zong" 仍合法），这时原音节应排在合法错串之后（第 2 位）。这类歧义本质
     要靠词级语言模型解，本模块只验证「信道排序把混淆兄弟排得够高」。

用法：
    python3 -m correction.metric            # 在仓库根目录运行
    python3 correction/metric.py            # 或直接跑文件
"""

from __future__ import annotations

import random
from typing import Dict, List, Tuple

from .channel import confusion_siblings
from .corrector import correct_syllable
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

    print("\n说明：A 是本模块职责（词典未命中纠错）；B 里 top-1 被合法错串占据，")
    print("     完整的平翘舌/前后鼻音消歧要靠词级语言模型，属后续工作。")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())