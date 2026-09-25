"""编辑距离。

只放「单元代价」的经典 Levenshtein，用于候选生成里的「编辑距离 ≤ k」剪枝
（这是 CHIME 的第一步：在字典里找编辑距离 ≤2 的合法拼音）。加权的
noisy-channel 代价不在这，见 channel.py（那一步只负责排序，不负责召回）。
"""


def levenshtein(s: str, t: str) -> int:
    """把 s 变到 t 的最小单元编辑操作数（插入 / 删除 / 替换各记 1）。"""
    if len(s) < len(t):
        s, t = t, s
    # prev 是「上一行」，cur 是「当前行」；用滚动数组省内存。
    prev = list(range(len(t) + 1))
    for i, cs in enumerate(s, 1):
        cur = [i]
        for j, ct in enumerate(t, 1):
            cur.append(min(
                prev[j] + 1,                          # 删除 s[i-1]
                cur[j - 1] + 1,                       # 在 s 里插入 t[j-1]
                prev[j - 1] + (0 if cs == ct else 1)  # 替换
            ))
        prev = cur
    return prev[-1]