"""容错纠错入口。

接口：
  1. correct_syllable(query) —— 单个（可能打错的）音节 → 有序候选。
  2. correct(pinyin)          —— 整串连续全拼 → 「切分 + 纠错」路径。

排序式（CHIME）：总代价 = 信道代价（channel.py）× 音节先验（prior.py）。
「合法输入」永远被信任：若 query 本身是合法音节，它恒排第一，纠错候选排其后。

整串用 Viterbi（本质是「带纠错的前向最大匹配」），与 lm/scdict_segment.py 的口径
一致：同代价时优先「音节更少」的切分（最长匹配）。

边界（诚实声明）：音节级纠错无法区分「打错成另一个合法音节」和「本来就该是它」
（如 zh→z 后 "zong" 仍是合法音节）。这类「合法-合法」歧义要词级语言模型解，
不在本模块职责内；本模块负责 CHIME 的「词典未命中」：打出非合法音节时召回排序。
"""

from __future__ import annotations

from typing import List, Optional, Tuple

from . import channel, prior
from .syllables import SYLLABLES
from .trie import SyllableTrie

# 最长的音节长度（如 zhuang / chuang / shuang = 6），切分时按此窗口扫描。
MAX_SYLLABLE_LEN = max(len(s) for s in SYLLABLES)

_trie: Optional[SyllableTrie] = None


def get_trie() -> SyllableTrie:
    global _trie
    if _trie is None:
        _trie = SyllableTrie(SYLLABLES)
    return _trie


def syllable_prior(syllable: str) -> float:
    """音节先验代价（越小越常见）。见 prior.py。"""
    return prior.syllable_prior_cost(syllable)


def correct_syllable(query: str, max_dist: int = 2) -> List[Tuple[str, int, float]]:
    """返回按 (是否精确命中, 总代价, 编辑距离, 音节) 升序的候选。

    三元组：(正确音节, 单元编辑距离, 总代价=信道+先验)。
    """
    cands = get_trie().search_within_edit_distance(query, max_dist)
    ranked = []
    for syl, dist in cands:
        score = channel.channel_score(query, syl) + syllable_prior(syl)
        ranked.append((syl, dist, score))
    # 合法输入信任优先：query 本身若合法恒排第一，其余按总代价升序。
    ranked.sort(key=lambda t: (0 if t[0] == query else 1, t[2], t[1], t[0]))
    return ranked


def correct(pinyin: str, max_dist: int = 2) -> Optional[Tuple[List[str], float]]:
    """整串连续全拼 → 最优「切分 + 纠错」路径，返回 (音节列表, 总代价) 或 None。

    同代价 tie-break：音节更少优先（最长匹配），再按音节字典序，保证结果确定。
    """
    n = len(pinyin)
    if n == 0:
        return None
    # best[i] = (最小总代价, 音节列表)；None 表示不可达。
    best: List[Optional[Tuple[float, List[str]]]] = [None] * (n + 1)
    best[0] = (0.0, [])

    for i in range(n):
        if best[i] is None:
            continue
        cost_i, path_i = best[i]
        for k in range(1, MAX_SYLLABLE_LEN + 1):
            if i + k > n:
                break
            sub = pinyin[i:i + k]
            for syl, _dist, score in correct_syllable(sub, max_dist):
                total = cost_i + score
                cand_path = path_i + [syl]
                cur = best[i + k]
                better = (
                    cur is None
                    or total < cur[0]
                    or (total == cur[0] and len(cand_path) < len(cur[1]))
                    or (total == cur[0] and len(cand_path) == len(cur[1])
                        and cand_path < cur[1])
                )
                if better:
                    best[i + k] = (total, cand_path)

    if best[n] is None:
        return None
    cost, path = best[n]
    return (list(path), cost)


def best_corrected(pinyin: str, max_dist: int = 2) -> Optional[List[str]]:
    """便捷封装：返回最优切分音节列（失败返回 None）。"""
    top = correct(pinyin, max_dist=max_dist)
    return top[0] if top else None