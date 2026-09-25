"""评测核心：语料加载、gold（全拼输入）生成、指标计算。

指标口径对齐 AutoRime / hanzi-chai 的公开做法：
  完全准确率 = 正确句数 / 全部句数
  综合准确率 = 正确字数 / 全部字数   （用 LCS 长度近似，容忍长度不等）
  平均码长   = 拼音串长度 / 汉字数
"""
from __future__ import annotations

import json
import re
import time
from dataclasses import dataclass, field
from pathlib import Path
from typing import Iterable, Sequence

from pypinyin import Style, lazy_pinyin

CJK_RANGE = "\u4e00-\u9fff"
SENT_SPLIT = re.compile(r"[。！？；!?;\n]+")
NON_CJK = re.compile(rf"[^{CJK_RANGE}]")

MIN_SENT_LEN = 4
MAX_SENT_LEN = 30


# ------------------------------------------------------------------ 语料与 case
@dataclass(frozen=True)
class Case:
    gold: str          # 标准答案：汉字短句
    pinyin: str        # 输入：连续全拼，无声调无分隔符（真实用户就是这样敲的）


def hanzi_to_pinyin(text: str) -> str:
    """汉字串 -> 连续全拼。lazy_pinyin 会按词组处理多音字，比逐字转更接近真实读音。"""
    return "".join(lazy_pinyin(text, style=Style.NORMAL))


def hanzi_to_jianpin(text: str) -> str:
    """汉字串 -> 首字母简拼（每音节取首字母，如「北京大学」→ bjdx）。"""
    return "".join(lazy_pinyin(text, style=Style.FIRST_LETTER))


def load_sentences(path: str | Path,
                   min_len: int = MIN_SENT_LEN,
                   max_len: int = MAX_SENT_LEN) -> list[str]:
    """从纯文本语料切出短句，只保留纯汉字句，去重。"""
    raw = Path(path).read_text(encoding="utf-8")
    out: list[str] = []
    seen: set[str] = set()
    for chunk in SENT_SPLIT.split(raw):
        s = NON_CJK.sub("", chunk).strip()
        if not (min_len <= len(s) <= max_len):
            continue
        if s in seen:
            continue
        seen.add(s)
        out.append(s)
    return out


def build_cases(sentences: Iterable[str]) -> list[Case]:
    return [Case(gold=s, pinyin=hanzi_to_pinyin(s)) for s in sentences]


def write_corpus(path: str | Path, sentences: Sequence[str]) -> None:
    Path(path).write_text("\n".join(sentences) + "\n", encoding="utf-8")


# ------------------------------------------------------------------ 指标
def lcs_len(a: str, b: str) -> int:
    """最长公共子序列长度，用于容忍长度不等时的字级比较。"""
    if not a or not b:
        return 0
    prev = [0] * (len(b) + 1)
    for ca in a:
        cur = [0]
        for j, cb in enumerate(b):
            cur.append(prev[j] + 1 if ca == cb else max(prev[j + 1], cur[j]))
        prev = cur
    return prev[-1]


def char_accuracy(gold: str, pred: str) -> float:
    return lcs_len(gold, pred) / max(len(gold), 1)


def percentile(values: Sequence[float], p: float) -> float:
    if not values:
        return 0.0
    s = sorted(values)
    k = min(len(s) - 1, max(0, int(round((p / 100.0) * (len(s) - 1)))))
    return s[k]


@dataclass
class Row:
    gold: str
    pred: str
    pinyin: str
    latency_ms: float

    @property
    def sentence_correct(self) -> int:
        return int(self.pred == self.gold)

    @property
    def char_acc(self) -> float:
        return char_accuracy(self.gold, self.pred)

    @property
    def keys_per_char(self) -> float:
        return len(self.pinyin) / max(len(self.gold), 1)

    def as_dict(self) -> dict:
        return {
            "gold": self.gold,
            "pred": self.pred,
            "pinyin": self.pinyin,
            "latency_ms": round(self.latency_ms, 3),
            "sentence_correct": self.sentence_correct,
            "char_acc": round(self.char_acc, 4),
            "keys_per_char": round(self.keys_per_char, 3),
        }


def aggregate(rows: Sequence[Row]) -> dict:
    n = len(rows)
    if n == 0:
        return {"n": 0}
    lat = [r.latency_ms for r in rows]
    return {
        "n": n,
        "sentence_acc": round(sum(r.sentence_correct for r in rows) / n, 4),
        "char_acc": round(sum(r.char_acc for r in rows) / n, 4),
        "keys_per_char": round(sum(r.keys_per_char for r in rows) / n, 3),
        "latency_p50_ms": round(percentile(lat, 50), 3),
        "latency_p99_ms": round(percentile(lat, 99), 3),
    }


# ------------------------------------------------------------------ 计时辅助
class Timer:
    __slots__ = ("_t0", "elapsed_ms")

    def __enter__(self) -> "Timer":
        self._t0 = time.perf_counter()
        return self

    def __exit__(self, *exc) -> None:
        self.elapsed_ms = (time.perf_counter() - self._t0) * 1000.0


def dump_json(path: str | Path, obj) -> None:
    Path(path).write_text(json.dumps(obj, ensure_ascii=False, indent=2), encoding="utf-8")


@dataclass
class Report:
    engine: str
    corpus: str
    cases: int
    metrics: dict
    rows: list[Row] = field(default_factory=list)
