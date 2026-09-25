"""音节二元转移代价（词级上下文，数据驱动版）。

解决 corrector.py 的「合法-合法」/「词典未命中多候选并列」消歧：仅靠音节先验
无法区分「打错成另一个合法音节」和「本来就该是它」，需要前一个音节的上下文。
用二元转移概率 P(cur | prev) 建模，代价 = LAMBDA * (-ln P)。

与 prior.py 同哲学：
  - 数据来自 wiki+news 语料（calibrate_transition.py），代价 = -ln 平滑概率。
  - LAMBDA 只控制「影响力跨度」，不篡夺信道主导权（类似 prior.py 的 ALPHA）；
    调 LAMBDA 无需重建数据。

数据文件 correction/data/bigram_costs.json（由 calibrate_transition.py 生成）：
  meta:  {total_syllables, unique_bigrams, syllable_count, smooth}
  denom: {prev: count(prev) + smooth*V}   # 每个 prev 的平滑分母
  trans: {"prev|cur": -ln P(cur|prev)}    # 只存见过的二元

未见二元在运行时按同一平滑公式推导：-ln(smooth / denom[prev])，故自然比
「见过的 continuation」更贵，无需单独设惩罚。
"""

from __future__ import annotations

import json
import math
from pathlib import Path
from typing import Dict, List, Optional, Tuple

# 缩放因子：把「自然对数转移代价」压到与信道代价可比的量级。
# 校准语料统计（未缩放）：常见二元 ~1.2~2.5，中位 ~5.4，未见 ~6.0~8.6。
# 取 LAMBDA 使「常见 continuation ↔ 未见 continuation」的差 ≈ 一个信道台阶（0.6）。
LAMBDA = 0.15

_DATA_FILE = Path(__file__).resolve().parent / "data" / "bigram_costs.json"

_loaded = False
_smooth = 0.5
_denom: Dict[str, float] = {}
_trans: Dict[str, float] = {}
_pairs: List[Tuple[str, str]] = []


def _load() -> None:
    global _loaded, _smooth, _denom, _trans, _pairs
    if _loaded:
        return
    if not _DATA_FILE.exists():
        raise FileNotFoundError(
            f"缺少二元转移数据文件 {_DATA_FILE}；请先运行 "
            "python3 -m correction.scripts.calibrate_transition"
        )
    raw = json.loads(_DATA_FILE.read_text(encoding="utf-8"))
    meta = raw["meta"]
    _smooth = float(meta["smooth"])
    _denom = {k: float(v) for k, v in raw["denom"].items()}
    _trans = {k: float(v) for k, v in raw["trans"].items()}
    _pairs = [tuple(k.split("|")) for k in _trans]
    _loaded = True


def transition_cost(prev_syllable: Optional[str], cur_syllable: str) -> float:
    """从 prev_syllable 转移到 cur_syllable 的代价（越小越自然）。

    prev_syllable 为 None 表示句首：返回 0，因为句首的一元先验已由 prior 承担，
    这里不应重复计数。
    """
    _load()
    if prev_syllable is None:
        return 0.0
    raw = _trans.get(f"{prev_syllable}|{cur_syllable}")
    if raw is None:
        d = _denom.get(prev_syllable, _smooth * len(_denom))
        raw = math.log(d / _smooth)  # -ln(smooth / denom)
    return LAMBDA * raw


def observed_bigrams() -> List[Tuple[str, str]]:
    """语料里出现过的 (prev, cur) 二元列表（供评测构造上下文场景）。"""
    _load()
    return list(_pairs)