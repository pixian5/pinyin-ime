"""合成错字生成器：用来对纠错模块做可复现的召回率评测。

真实录入错误可归成几类，这里模拟主要几类（对得上 PLAN-A §4.1 的口径）：
  retroflex   平翘舌混淆：zh→z / ch→c / sh→s（少打 h）
  nasal       前后鼻音混淆：ing→in / eng→en 等（少打 / 多打 g）
  adjacent    键盘相邻键误触
  phonetic    语音易混替换：l↔n / l↔r / f↔h
  substitute  替换成随机字母
  delete      删掉一个字母
  insert      多打一个字母
"""

from __future__ import annotations

import random
from typing import Callable, List, Optional, Set, Tuple

from .channel import KEYBOARD_NEIGHBORS, PHONETIC_SWAPS

_RETROFLEX = {"zh": "z", "ch": "c", "sh": "s"}
_NASAL_PAIRS = {"ing": "in", "eng": "en", "ang": "an", "iang": "ian"}
_ALPHABET = "abcdefghijklmnopqrstuvwxyz"


def corrupt_syllable(syl: str, rng: random.Random, kind: str) -> str:
    """给单个音节制造一个错字，返回打错后的串（可能 == 原串，由调用方筛掉）。"""
    if kind == "retroflex":
        for rt, plain in _RETROFLEX.items():
            if syl.startswith(rt):
                return plain + syl[len(rt):]
        return syl
    if kind == "nasal":
        for long, short in _NASAL_PAIRS.items():
            if syl.endswith(long):
                return syl[: -len(long)] + short
        return syl
    if kind == "adjacent":
        swap = [i for i, ch in enumerate(syl) if ch in KEYBOARD_NEIGHBORS]
        if not swap:
            return syl
        i = rng.choice(swap)
        return syl[:i] + rng.choice(sorted(KEYBOARD_NEIGHBORS[syl[i]])) + syl[i + 1:]
    if kind == "phonetic":
        pairs = list(PHONETIC_SWAPS)
        swap = [(i, ch) for i, ch in enumerate(syl) for p in pairs if ch in p]
        if not swap:
            return syl
        i, ch = rng.choice(swap)
        pair = next(p for p in pairs if ch in p)
        other = next(x for x in pair if x != ch)
        return syl[:i] + other + syl[i + 1:]
    if kind == "substitute":
        i = rng.randrange(len(syl))
        ch = rng.choice(_ALPHABET)
        return syl[:i] + ch + syl[i + 1:]
    if kind == "delete":
        i = rng.randrange(len(syl))
        return syl[:i] + syl[i + 1:]
    if kind == "insert":
        i = rng.randrange(len(syl) + 1)
        return syl[:i] + rng.choice(_ALPHABET) + syl[i:]
    raise ValueError(f"unknown kind: {kind}")


def corrupt_to(syl: str, rng: random.Random, kind: str,
               predicate: Callable[[str], bool], max_tries: int = 50) -> Optional[str]:
    """反复按 kind 打错，直到满足 predicate（如「不是合法音节」），超限返回 None。"""
    for _ in range(max_tries):
        t = corrupt_syllable(syl, rng, kind)
        if t and t != syl and predicate(t):
            return t
    return None


def corrupt_sentence(syllables: List[str], rng: random.Random) -> Tuple[str, List[str]]:
    """随机挑一个音节打错，返回 (打错后的整串, 原始音节列)。"""
    idx = rng.randrange(len(syllables))
    noisy = list(syllables)
    for _ in range(50):
        t = corrupt_syllable(syllables[idx], rng,
                             rng.choice(["adjacent", "delete", "insert"]))
        if t != syllables[idx]:
            noisy[idx] = t
            break
    return "".join(noisy), syllables