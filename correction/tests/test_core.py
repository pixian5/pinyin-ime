"""correction 模块的确定性单元测试（标准库 unittest，不依赖 pypinyin/libime）。

运行：python3 -m unittest discover -s correction/tests -v
（或 cd correction && python3 -m unittest）
"""

import os
import sys
import unittest

# 保证能 import 到 correction 包（无论从哪里跑）。
sys.path.insert(0, os.path.dirname(os.path.dirname(os.path.dirname(os.path.abspath(__file__)))))

from correction import (
    SYLLABLES,
    SYLLABLE_SET,
    NUM_SYLLABLES,
    correct_syllable,
    correct,
    best_corrected,
    get_trie,
    levenshtein,
)
from correction.channel import channel_score, confusion_siblings


class TestEditDistance(unittest.TestCase):
    def test_known_cases(self):
        self.assertEqual(levenshtein("", ""), 0)
        self.assertEqual(levenshtein("abc", "abc"), 0)
        self.assertEqual(levenshtein("abc", ""), 3)
        self.assertEqual(levenshtein("kitten", "sitting"), 3)
        self.assertEqual(levenshtein("zhong", "zong"), 1)  # 少一个 h

    def test_brute_force_agreement(self):
        """随机小串上，滚动 DP 与朴素 DP 一致。"""
        def naive(s, t):
            m, n = len(s), len(t)
            dp = [[0] * (n + 1) for _ in range(m + 1)]
            for i in range(m + 1):
                dp[i][0] = i
            for j in range(n + 1):
                dp[0][j] = j
            for i in range(1, m + 1):
                for j in range(1, n + 1):
                    dp[i][j] = min(dp[i - 1][j] + 1, dp[i][j - 1] + 1,
                                   dp[i - 1][j - 1] + (0 if s[i - 1] == t[j - 1] else 1))
            return dp[m][n]

        import itertools
        import random
        rng = random.Random(0)
        alphabet = "zhongu"
        for _ in range(200):
            l1, l2 = rng.randint(0, 6), rng.randint(0, 6)
            s = "".join(rng.choice(alphabet) for _ in range(l1))
            t = "".join(rng.choice(alphabet) for _ in range(l2))
            self.assertEqual(levenshtein(s, t), naive(s, t), (s, t))


class TestTrie(unittest.TestCase):
    def setUp(self):
        self.t = get_trie()

    def test_size_and_membership(self):
        self.assertEqual(len(self.t), NUM_SYLLABLES)
        for s in SYLLABLES:
            self.assertIn(s, self.t)

    def test_search_returns_exact(self):
        for s in SYLLABLES:
            found = {syl for syl, d in self.t.search_within_edit_distance(s, 2)}
            self.assertIn(s, found, f"{s} 应被自身召回")

    def test_recall_after_delete(self):
        """每个音节删任意一位后，原音节必须在编辑距离≤2 的召回里。"""
        misses = 0
        for s in SYLLABLES:
            for i in range(len(s)):
                t = s[:i] + s[i + 1:]
                if not t:
                    continue
                found = {syl for syl, _ in self.t.search_within_edit_distance(t, 2)}
                if s not in found:
                    misses += 1
        self.assertEqual(misses, 0)

    def test_max_dist_bound(self):
        """所有召回结果确实 ≤ max_dist。"""
        for query in ("zhong", "zhing", "b" * 3, "xy"):
            for syl, d in self.t.search_within_edit_distance(query, 0):
                self.assertLessEqual(d, 0)
            for syl, d in self.t.search_within_edit_distance(query, 2):
                self.assertLessEqual(d, 2)


class TestChannel(unittest.TestCase):
    def test_exact_is_zero(self):
        for s in SYLLABLES:
            self.assertEqual(channel_score(s, s), 0.0)

    def test_confusion_siblings(self):
        self.assertIn("zong", confusion_siblings("zhong"))
        self.assertIn("yin", confusion_siblings("ying"))
        self.assertIn("sen", confusion_siblings("seng"))

    def test_confusion_cheaper_than_generic(self):
        # 平翘舌混淆应比「随便换个不相关字母」便宜
        self.assertLess(channel_score("zong", "zhong"), channel_score("zong", "song"))


class TestCorrectSyllable(unittest.TestCase):
    def test_exact_ranks_first(self):
        for s in SYLLABLES:
            top = correct_syllable(s)[0]
            self.assertEqual(top[0], s)

    def test_retroflex_candidate_high(self):
        # zhing 不是合法音节，应能纠成 zhong（相邻键 i->o）
        ranks = correct_syllable("zhing")
        self.assertIn("zhong", [s for s, _, _ in ranks[:3]])

    def test_nasal_candidate(self):
        ranks = correct_syllable("zhon")  # 少了 g
        self.assertEqual(ranks[0][0], "zhong")


class TestCorrectSentence(unittest.TestCase):
    def test_exact_segmentation(self):
        self.assertEqual(best_corrected("nihao"), ["ni", "hao"])
        self.assertEqual(best_corrected("zhongguo"), ["zhong", "guo"])
        self.assertEqual(best_corrected("woshixuesheng"),
                         ["wo", "shi", "xue", "sheng"])

    def test_sentence_correction(self):
        # zhing 打错 -> 纠成 zhong
        self.assertEqual(best_corrected("zhingguo"), ["zhong", "guo"])

    def test_empty(self):
        self.assertIsNone(best_corrected(""))
        # 说明：只要输入非空，「纠错」总会把任意字母串映射到某些音节（这正是
        # 编辑距离≤2 纠错的本性，比如 "qq" 能被纠成 "sa"）。真正区分「这串根本不
        # 是拼音」要靠词级语言模型给置信度阈值，属 PLANA §4.1 之外的后续工作。
        # 因此这里不把「乱串返回 None」当契约。


if __name__ == "__main__":
    unittest.main(verbosity=2)