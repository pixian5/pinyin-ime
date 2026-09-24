#!/usr/bin/env python3
"""rime-ice（雾凇拼音）dict.yaml -> libime_pinyindict 文本格式。

用法:
    python3 convert_rimeice.py --src <rime-ice>/cn_dicts --out rimeice.dict.txt
再转 libime 二进制词典:
    libime_pinyindict rimeice.dict.txt rimeice.dict

转换规则（与 sc.dict 语义对齐，见 PLAN-A.md §13 / README.md）:
    - 跳过 # 注释行；跳过 YAML 头（name:/version:/sort:/---/...，这些行无 tab）。
    - 取 tab 分隔的 3 字段: <词>\t<拼音>\t<频率>。
    - 拼音音节间空格 -> ' 分隔（"ni hao" -> "ni'hao"）。
    - 仅保留纯 CJK 词（统一表意文字 U+4E00-U+9FFF + 扩展A U+3400-U+4DBF）。
    - 按 (词, 拼音) 去重，同键取最大频率。
    - cost = ln(读音频率 / 该词所有读音频率和): 单音字/主流读音≈0，
      罕见读音为负惩罚；全局词频交给语言模型，**不要**把原始词频塞进 cost。

坑（已踩）: 若把 rime-ice 原始整数词频直接当 cost（0~几千万的正数），
会盖过 LM 分数，解码出全是乱码单字。必须按词内读音归一成 log 概率。
"""
from __future__ import annotations

import argparse
import math
import sys
from pathlib import Path


def is_cjk(word: str) -> bool:
    return all(
        0x3400 <= ord(ch) <= 0x4DBF or 0x4E00 <= ord(ch) <= 0x9FFF
        for ch in word
    )


def parse(path: Path, entries: dict) -> int:
    n = 0
    with path.open(encoding="utf-8") as f:
        for line in f:
            s = line.rstrip("\n")
            if not s or s[0] == "#":
                continue
            if "\t" not in s:          # YAML 头 / 元数据行
                continue
            parts = s.split("\t")
            if len(parts) != 3:
                continue
            word, py, freq = parts
            if not word or not py or not is_cjk(word):
                continue
            py = "'".join(py.split())
            if not all(c.isascii() and (c.isalpha() or c == "'") for c in py):
                continue
            try:
                fr = int(float(freq))
            except ValueError:
                continue
            if fr <= 0:
                fr = 1
            key = (word, py)
            if key not in entries or fr > entries[key]:
                entries[key] = fr
            n += 1
    return n


def main() -> int:
    ap = argparse.ArgumentParser(description=__doc__)
    ap.add_argument("--src", required=True, help="cn_dicts 目录")
    ap.add_argument("--files", default="8105.dict.yaml,base.dict.yaml,ext.dict.yaml",
                    help="逗号分隔的 dict.yaml 文件名")
    ap.add_argument("--out", default="rimeice.dict.txt", help="输出文本词库路径")
    args = ap.parse_args()

    entries: dict = {}
    for name in args.files.split(","):
        p = Path(args.src) / name
        if not p.exists():
            print(f"MISSING: {p}", file=sys.stderr)
            continue
        print(f"{name}: {parse(p, entries)} 行", file=sys.stderr)

    by_word: dict = {}
    for (word, py), fr in entries.items():
        by_word.setdefault(word, {})[py] = fr

    with open(args.out, "w", encoding="utf-8") as out:
        for word in sorted(by_word):
            readings = by_word[word]
            total = sum(readings.values())
            for py in sorted(readings):
                out.write(f"{word}\t{py}\t{math.log(readings[py] / total):.6f}\n")

    print(f"去重后词条数={len(entries)} -> {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())