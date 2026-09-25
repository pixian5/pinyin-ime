#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""prepare_skypile.py — 从 SkyPile-150B JSONL 分片抽取清洗后的纯汉字句子，供 LM 训练。

SkyPile-150B 是 2020 年代中文网页文本（含新闻/百科/论坛等），每条 record 是
`{"text": "...", ...}`，`text` 是整页清洗后的正文（约 1000 字/页）。本脚本把
`text` 按句末标点切成「每行一句纯汉字」，输出交给 scdict_segment.py 用 sc.dict
词表分词（保证 LM 词表 ⊆ sc.dict）。

清洗规则与 lm/prepare_thucnews.py 一致：只保留 CJK 连续段，剥数字/拉丁/标点；
长度过滤（短于 min-len 或长于 max-len 的丢弃）滤掉碎片与超长噪音。

用法：
    python3 prepare_skypile.py --jsonl a.jsonl --jsonl b.jsonl --out skypile.txt
    # 输入可是 .jsonl 或 .jsonl.gz（流式，不吃内存）
"""
import argparse
import gzip
import json
import re
import sys


CJK = re.compile(r"[\u4e00-\u9fff]+")
SPLIT = re.compile(r"[。！？；!?;\n\r]+")


def open_maybe_gz(path):
    if path.endswith(".gz"):
        return gzip.open(path, "rt", encoding="utf-8", errors="ignore")
    return open(path, "r", encoding="utf-8", errors="ignore")


def clean_sentence_parts(text, min_len, max_len):
    """把一段整页文本切为若干纯汉字句段（仅 CJK，长度过滤）。"""
    for part in SPLIT.split(text):
        for seg in CJK.finditer(part):
            s = seg.group(0)
            if min_len <= len(s) <= max_len:
                yield s


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--jsonl", action="append", default=[],
                    help="SkyPile JSONL 分片路径（可重复）")
    ap.add_argument("--out", required=True, help="输出纯汉字句子文件（每行一句）")
    ap.add_argument("--min-len", type=int, default=3, help="最短字数（默认 3）")
    ap.add_argument("--max-len", type=int, default=128, help="最长字数（默认 128）")
    ap.add_argument("--max-chars", type=int, default=0,
                    help="累计汉字数上限（0=不限）")
    ap.add_argument("--field", default="text", help="record 里的正文字段名（默认 text）")
    args = ap.parse_args()

    if not args.jsonl:
        ap.error("至少给一个 --jsonl")

    total_chars = 0
    total_lines = 0
    bad = 0
    with open(args.out, "w", encoding="utf-8") as out:
        for path in args.jsonl:
            with open_maybe_gz(path) as f:
                for raw in f:
                    raw = raw.strip()
                    if not raw:
                        continue
                    if args.max_chars and total_chars >= args.max_chars:
                        break
                    try:
                        rec = json.loads(raw)
                    except json.JSONDecodeError:
                        bad += 1
                        continue
                    text = rec.get(args.field) if isinstance(rec, dict) else None
                    if not isinstance(text, str) or not text:
                        bad += 1
                        continue
                    for s in clean_sentence_parts(text, args.min_len, args.max_len):
                        out.write(s + "\n")
                        total_chars += len(s)
                        total_lines += 1
                        if args.max_chars and total_chars >= args.max_chars:
                            break
            if args.max_chars and total_chars >= args.max_chars:
                break

    print(f"prepare_skypile: 输出 {total_lines} 句 / {total_chars} 字 -> {args.out}",
          file=sys.stderr)
    if bad:
        print(f"prepare_skypile: 跳过 {bad} 条坏记录", file=sys.stderr)


if __name__ == "__main__":
    main()