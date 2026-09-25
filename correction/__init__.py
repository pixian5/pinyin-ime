"""容错纠错模块（correction）。

纯标准库实现，不依赖 libime / pypinyin，可独立测试、独立回归。
对外核心接口见 corrector.py：

    from correction import correct_syllable, correct_syllable_in_context, correct, best_corrected
"""

from .corrector import (
    correct,
    correct_syllable,
    correct_syllable_in_context,
    best_corrected,
    get_trie,
    syllable_prior,
    MAX_SYLLABLE_LEN,
)
from .edit_distance import levenshtein
from .syllables import SYLLABLES, SYLLABLE_SET, NUM_SYLLABLES
from .trie import SyllableTrie
from . import transition

__all__ = [
    "SYLLABLES",
    "SYLLABLE_SET",
    "NUM_SYLLABLES",
    "MAX_SYLLABLE_LEN",
    "SyllableTrie",
    "levenshtein",
    "correct_syllable",
    "correct_syllable_in_context",
    "correct",
    "best_corrected",
    "get_trie",
    "syllable_prior",
    "transition",
]

__version__ = "0.2.0"