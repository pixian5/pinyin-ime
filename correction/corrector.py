"""容错纠错入口。

接口：
  1. correct_syllable(query)                    —— 单个（可能打错的）音节 → 有序候选。
  2. correct_syllable_in_context(query, prev)   —— 同上，但再加「上一音节→候选」的
                                                    二元转移代价，供词级上下文消歧。
  3. correct(pinyin)                            —— 整串连续全拼 → 「切分 + 纠错」路径。

排序式（CHIME）：总代价 = 信道代价（channel.py）× 音节先验（prior.py）。
有上下文时再叠加：转移代价（transition.py，前一个音节 → 当前音节的二元概率）。
「合法输入」永远被信任：若 query 本身是合法音节，它恒排第一，纠错候选排其后。

整串用 Viterbi（本质是「带纠错 + 词级上下文的前向匹配」）。二元转移只依赖「最后
一个音节」，故 Viterbi 状态按 (位置, 最后一个音节) 展开——这是 bigram HMM 的
标准拆法，代价是精确的，不是近似的 beam。为控制状态规模，对每个位置的前沿做
beam 剪枝（见 BEAM_WIDTH，实际远大于竞争音节的真实数量）。

边界（诚实声明）：二元转移把「上一音节上下文」纳入后，能消解一部分「合法-合法」
歧义。但仍是音节级而非词级；纯词级（整词/整句语言模型）仍在 libime 侧，不在本
模块职责内。且二元表的好坏直接取决于语料覆盖（见 transition.py 与校准脚本）。
"""

from __future__ import annotations

from typing import Dict, List, Optional, Tuple

from . import channel, prior, transition
from .syllables import SYLLABLES
from .trie import SyllableTrie

# 最长的音节长度（如 zhuang / chuang / shuang = 6），切分时按此窗口扫描。
MAX_SYLLABLE_LEN = max(len(s) for s in SYLLABLES)
# 每个位置前沿保留的状态数（按 (代价, 音节数, 路径序) 剪枝）。
BEAM_WIDTH = 64

_trie: Optional[SyllableTrie] = None


def get_trie() -> SyllableTrie:
    global _trie
    if _trie is None:
        _trie = SyllableTrie(SYLLABLES)
    return _trie


def syllable_prior(syllable: str) -> float:
    """音节先验代价（越小越常见）。见 prior.py。"""
    return prior.syllable_prior_cost(syllable)


def _base_candidates(query: str, max_dist: int) -> List[Tuple[str, int, float]]:
    """query 的候选（末项为「信道+先验」，不含转移代价）。"""
    cands = get_trie().search_within_edit_distance(query, max_dist)
    return [
        (syl, dist, channel.channel_score(query, syl) + syllable_prior(syl))
        for syl, dist in cands
    ]


def correct_syllable_in_context(
    query: str, prev_syllable: Optional[str] = None, max_dist: int = 2
) -> List[Tuple[str, int, float]]:
    """返回按 (是否精确命中, 总代价, 编辑距离, 音节) 升序的候选。

    三元组：(正确音节, 单元编辑距离, 总代价=信道+先验+转移)。
    prev_syllable 为 None 时等价于无上下文的 correct_syllable（转移代价为 0）。
    """
    ranked = []
    for syl, dist, base in _base_candidates(query, max_dist):
        score = base + transition.transition_cost(prev_syllable, syl)
        ranked.append((syl, dist, score))
    # 合法输入信任优先：query 本身若合法恒排第一，其余按总代价升序。
    ranked.sort(key=lambda t: (0 if t[0] == query else 1, t[2], t[1], t[0]))
    return ranked


def correct_syllable(query: str, max_dist: int = 2) -> List[Tuple[str, int, float]]:
    """单个音节的纠错候选（无上下文）。见 correct_syllable_in_context。"""
    return correct_syllable_in_context(query, None, max_dist)


def correct(pinyin: str, max_dist: int = 2) -> Optional[Tuple[List[str], float]]:
    """整串连续全拼 → 最优「切分 + 纠错」路径，返回 (音节列表, 总代价) 或 None。

    二元 Viterbi：状态按 (位置, 最后一个音节) 展开，转移代价取自 transition.py，
    句首（prev=None）转移代价为 0。同代价 tie-break：音节更少优先（最长匹配），
    再按音节字典序，保证结果确定。
    """
    n = len(pinyin)
    if n == 0:
        return None

    # 预计算每个 (起点, 长度) 子串的候选 base（不含转移），避免按状态重复查 trie。
    cands: Dict[Tuple[int, int], List[Tuple[str, int, float]]] = {}
    for i in range(n):
        for k in range(1, MAX_SYLLABLE_LEN + 1):
            if i + k > n:
                break
            cands[(i, k)] = _base_candidates(pinyin[i:i + k], max_dist)

    # best[i][last_syllable] = (最小总代价, 音节列表)；last_syllable 为 None 表示句首。
    best: List[Dict[Optional[str], Tuple[float, List[str]]]] = [
        {} for _ in range(n + 1)
    ]
    best[0] = {None: (0.0, [])}

    def _ranked_min(states: Dict[Optional[str], Tuple[float, List[str]]]):
        return sorted(states.items(), key=lambda kv: (kv[1][0], len(kv[1][1]), kv[1][1]))

    for i in range(n):
        states = best[i]
        if not states:
            continue
        # 前沿 beam 剪枝：只保留前 BEAM_WIDTH 个状态（此位置之后转移只依赖最后音节，
        # 被剪掉的高代价状态几乎不可能翻盘）。
        if len(states) > BEAM_WIDTH:
            best[i] = dict(_ranked_min(states)[:BEAM_WIDTH])
            states = best[i]
        for prev_syl, (cost_i, path_i) in states.items():
            for k in range(1, MAX_SYLLABLE_LEN + 1):
                key = (i, k)
                if key not in cands:
                    break
                j = i + k
                for syl, _dist, base in cands[key]:
                    total = cost_i + base + transition.transition_cost(prev_syl, syl)
                    cand_path = path_i + [syl]
                    cur = best[j].get(syl)
                    better = (
                        cur is None
                        or total < cur[0]
                        or (total == cur[0] and len(cand_path) < len(cur[1]))
                        or (total == cur[0] and len(cand_path) == len(cur[1])
                            and cand_path < cur[1])
                    )
                    if better:
                        best[j][syl] = (total, cand_path)

    if not best[n]:
        return None
    _syl, (cost, path) = _ranked_min(best[n])[0]
    return (list(path), cost)


def best_corrected(pinyin: str, max_dist: int = 2) -> Optional[List[str]]:
    """便捷封装：返回最优切分音节列（失败返回 None）。"""
    top = correct(pinyin, max_dist=max_dist)
    return top[0] if top else None