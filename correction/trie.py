"""音节 Trie + 编辑距离 ≤ k 的候选生成。

只用单元代价的 Levenshtein 做剪枝（保证「编辑距离 ≤2」这个口径是精确的，
不被 noisy-channel 加权污染）。查询串（可能是打错的拼音）在 trie 上逐字符走，
维护一条 DP 行：row[i] = 查询串前 i 个字符 与 当前前缀 的编辑距离。
"""

from __future__ import annotations

from typing import Dict, List, Optional, Tuple


class _Node:
    __slots__ = ("children", "is_end")

    def __init__(self) -> None:
        self.children: Dict[str, "_Node"] = {}
        self.is_end: bool = False


class SyllableTrie:
    """由一组音节字符串构造的 trie，支持前缀精确匹配与模糊检索。"""

    def __init__(self, syllables) -> None:
        self.root = _Node()
        self._count = 0
        for s in syllables:
            self.insert(s)

    def insert(self, word: str) -> None:
        node = self.root
        for ch in word:
            node = node.children.setdefault(ch, _Node())
        if not node.is_end:
            node.is_end = True
            self._count += 1

    def __contains__(self, word: str) -> bool:
        node = self.root
        for ch in word:
            if ch not in node.children:
                return False
            node = node.children[ch]
        return node.is_end

    def __len__(self) -> int:
        return self._count

    def search_within_edit_distance(self, query: str, max_dist: int = 2) -> List[Tuple[str, int]]:
        """返回 (音节, 编辑距离) 列表，其中音节与 query 的编辑距离 ≤ max_dist。

        思路：在 trie 上 DFS，每向下一层就把「当前前缀」延长一个字符，
        用滚动 DP 行算「query 与当前前缀」的编辑距离。
        剪枝条件（sound）：若 min(row) > max_dist，则任何更长的后缀都只会更远，
        这棵子树可以整棵砍掉（见模块注释里的 heredity 论证）。
        """
        q = query
        m = len(q)
        results: List[Tuple[str, int]] = []
        # 空前缀 vs query：需要删除 i 个字符。
        root_row = list(range(m + 1))
        self._dfs(self.root, "", q, m, max_dist, root_row, results)
        return results

    def _dfs(self, node: _Node, prefix: str, q: str, m: int, max_dist: int,
             prev_row: List[int], results: List[Tuple[str, int]]) -> None:
        # 到达某个合法音节结尾，且整串 query 都能在这个预算内对上。
        if node.is_end and prev_row[m] <= max_dist:
            results.append((prefix, prev_row[m]))
        # 剪枝：当前行所有值都超预算，子树整体放弃。
        if min(prev_row) > max_dist:
            return
        for ch, child in node.children.items():
            row = [prev_row[0] + 1]  # 延长前缀：row[0] = len(prefix)+1
            for i in range(1, m + 1):
                d = min(
                    row[i - 1] + 1,                                  # 删除 query[i-1]
                    prev_row[i] + 1,                                  # 插入 ch
                    prev_row[i - 1] + (0 if q[i - 1] == ch else 1)    # 替换
                )
                row.append(d)
            self._dfs(child, prefix + ch, q, m, max_dist, row, results)


def _default_trie():
    # 延迟导入，避免 syllables 模块与 trie 互相依赖。
    from .syllables import SYLLABLES
    return SyllableTrie(SYLLABLES)