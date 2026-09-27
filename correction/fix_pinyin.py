"""fix_pinyin.py — 拼音纠错桥（把 M3 纠错模块接到解码前端）。

用途：把（可能打错的）连续全拼纠正成正确音节串，供 libime 解码前端在送解码前先修
复输入。这是 M3 纠错模块「集成到解码管线」的前端形态——不触发引擎重建，即可让 C++
CLI / Fcitx5 addon 复用纯 Python 的 CHIME 信道 + 音节二元转移模型。

用法（从 stdin 读每行一段连续全拼，输出 `'` 分隔的正确音节串）：
    echo "nihqa" | python3 -m correction.fix_pinyin
    # → ni'hao

拆开理解：
    correct("nihqa") -> (["ni", "hao"], cost)  →  join("'") -> "ni'hao"
    libime 用 ' 做隔音符，解码 "ni'hao" 与 "nihao" 同义，但消除了错误音节。

边界（诚实声明）：这是「前端纠正」而非把模型 port 进 libime 原生解码器；性能上
每行一次 Python 进程 fork，适合行模式/评测，交互式需另做常驻服务或 C++ port。
"""
from __future__ import annotations

import sys

from .corrector import correct


def fix(pinyin: str) -> str | None:
    """纠正整串连续全拼，返回 `'` 分隔的正确音节串；无法纠则返回 None。"""
    top = correct(pinyin)
    if top is None:
        return None
    syllables, _cost = top
    return "'".join(syllables)


def main() -> int:
    for line in sys.stdin:
        pinyin = line.strip()
        if not pinyin:
            continue
        out = fix(pinyin)
        if out is None:
            # 纠不出就原样返回，交由解码器按模糊音/简拼兜底。
            out = pinyin
        print(out)
    return 0


if __name__ == "__main__":
    sys.exit(main())