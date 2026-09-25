"""noisy-channel 代价模型：键盘物理邻近 + 中文语音易混（平翘舌 / 前后鼻音）。

排序式（CHIME）：P(正确音节) × P(打了错的 | 本来想打对的)。这里实现后半的
「信道」，用代价 = -ln 概率表示，越小越可能。

两部分代价：
  1. 通用打字错：键盘相邻键误触（sub 0.6）、l/n / l/r / f/h 等语音替换（sub 0.4）、
     增删普通字母（1.0）、其他替换（1.2）——用加权 Levenshtein 算。
  2. 中文特化「音节级混淆」：平翘舌 zh↔z / ch↔c / sh↔s、前后鼻音 ing↔in /
     eng↔en / ang↔an / iang↔ian——查「混淆兄弟」直接给定 CONFUSION_COST。

为什么第 2 部分要单独建模而不是把 h/g 的插入设便宜：把 h/g 全局设便宜会
过度生成（"u"-->"gu" 这种插入 g 也会变便宜），而 zh↔z 只在音首、鼻音只在韵尾，
必须看整音节位置。显式列「混淆兄弟」最可靠，也正好对应 PLAN-A §4.1 的启发式。
"""

from __future__ import annotations

from typing import Dict, FrozenSet, Set

COST_INS_DEL = 1.0            # 增删一个普通字母
COST_SUB_PHONETIC = 0.4       # l/n、l/r、f/h 易混替换
COST_SUB_ADJACENT = 0.6       # 键盘相邻键误触
COST_SUB_DEFAULT = 1.2        # 其他替换（调参：原 1.5→1.2，缩小与邻接 0.6 的差距，让先验有更多 tie-break 空间）
CONFUSION_COST = 0.35         # 平翘舌 / 前后鼻音的整音节混淆

# 键盘物理相邻关系（QWERTY，含对角）。用于把「相邻键误触」判为更可能。
KEYBOARD_NEIGHBORS: Dict[str, Set[str]] = {
    "q": set("was"), "w": set("qeasd"), "e": set("wrsdf"), "r": set("etdfg"),
    "t": set("ryfgh"), "y": set("tughj"), "u": set("yihjk"), "i": set("uojkl"),
    "o": set("ipkl"), "p": set("ol"),
    "a": set("qwsxz"), "s": set("qweadzxc"), "d": set("wersfcxv"),
    "f": set("ertdgcvb"), "g": set("rtyfhvbn"), "h": set("tyugjbnm"),
    "j": set("yuihknm"), "k": set("uiojlm"), "l": set("iopk"),
    "z": set("asx"), "x": set("asdzc"), "c": set("sdfxv"), "v": set("dfgcb"),
    "b": set("fghvn"), "n": set("ghjbm"), "m": set("hjkn"),
}

# 语音层面的真·替换易混对（不是加/删 h/g 就能解释的）。
PHONETIC_SWAPS: Set[FrozenSet[str]] = {
    frozenset({"l", "n"}), frozenset({"l", "r"}), frozenset({"f", "h"}),
}

# 前后鼻音成对尾缀（长鼻音 ↔ 短鼻音）。
_NASAL_PAIRS = {"ing": "in", "eng": "en", "ang": "an", "iang": "ian"}
_RETROFLEX = {"zh": "z", "ch": "c", "sh": "s"}


def _sub_cost(a: str, b: str) -> float:
    if frozenset((a, b)) in PHONETIC_SWAPS:
        return COST_SUB_PHONETIC
    if b in KEYBOARD_NEIGHBORS.get(a, set()):
        return COST_SUB_ADJACENT
    return COST_SUB_DEFAULT


def generic_distance(query: str, target: str) -> float:
    """通用打字错的加权编辑距离（不含音节级混淆）。"""
    m, n = len(query), len(target)
    dp = [[0.0] * (n + 1) for _ in range(m + 1)]
    for i in range(1, m + 1):
        dp[i][0] = dp[i - 1][0] + COST_INS_DEL
    for j in range(1, n + 1):
        dp[0][j] = dp[0][j - 1] + COST_INS_DEL
    for i in range(1, m + 1):
        for j in range(1, n + 1):
            a, b = query[i - 1], target[j - 1]
            dp[i][j] = min(
                dp[i - 1][j] + COST_INS_DEL,
                dp[i][j - 1] + COST_INS_DEL,
                dp[i - 1][j - 1] + (0.0 if a == b else _sub_cost(a, b)),
            )
    return dp[m][n]


def confusion_siblings(syl: str) -> Set[str]:
    """与 syl 存在「平翘舌 / 前后鼻音」整音节混淆关系的音节集合。"""
    out: Set[str] = set()
    for rt, plain in _RETROFLEX.items():
        if syl.startswith(rt):
            out.add(plain + syl[len(rt):])
    for long, short in _NASAL_PAIRS.items():
        if syl.endswith(long):
            out.add(syl[: -len(long)] + short)
        elif syl.endswith(short):
            out.add(syl[: -len(short)] + long)
    out.discard(syl)
    return out


def channel_score(query: str, target: str) -> float:
    """query（打错的）与 target（正确音节）之间的信道代价。"""
    if query == target:
        return 0.0
    # 音节级混淆（平翘舌 / 前后鼻音）显式命中，直接给低代价。
    if target in confusion_siblings(query) or query in confusion_siblings(target):
        return CONFUSION_COST
    return generic_distance(query, target)