#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""prepare_thucnews.py — 从 THUCNews.zip 抽取清洗后的纯汉字句子，供 LM 训练。

THUCNews（THUNLP 公开新闻分类语料）自带空格分词。这里剥离其自带分词与标点，
还原为纯汉字字符流，再按句末标点切句，输出「每行一句纯汉字」的原始文本，
随后交给 scdict_segment.py 用 sc.dict 词表重新分词（保证 LM 词表兼容）。

各分类轮转取样（round-robin），避免只取到靠前分类而引入领域偏差。
"""
import argparse
import re
import zipfile
from collections import defaultdict

CJK = re.compile(r"[\u4e00-\u9fff]+")
# 句末/子句分隔标点（含中文与半角）
SPLIT = re.compile(r"[。！？；!?;\n\r]+")


def clean_sentence_parts(text):
    """把一行文本切为若干纯汉字句段。"""
    for part in SPLIT.split(text):
        for seg in CJK.finditer(part):
            s = seg.group(0)
            if len(s) >= 2:
                yield s


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--zip", required=True, help="THUCNews.zip 路径")
    ap.add_argument("--out", required=True, help="输出纯汉字句子文件（每行一句）")
    ap.add_argument("--max-chars", type=int, default=200_000_000,
                    help="累计汉字数上限（默认 2 亿，约 7000 万词，trigram 训练 RAM 可控）")
    args = ap.parse_args()

    with zipfile.ZipFile(args.zip) as zf:
        members = [m for m in zf.infolist()
                   if not m.is_dir() and m.filename.lower().endswith(".txt")]
        # 按分类轮转：key = 分类目录，value = 该分类的文件列表
        by_cat = defaultdict(list)
        for m in members:
            parts = m.filename.split("/")
            cat = parts[1] if len(parts) >= 3 else "_root"
            by_cat[cat].append(m)
        # 打散：把各分类文件交错成一个队列
        cats = list(by_cat.keys())
        queue = []
        max_files = max(len(v) for v in by_cat.values())
        for i in range(max_files):
            for cat in cats:
                if i < len(by_cat[cat]):
                    queue.append(by_cat[cat][i])

        total_chars = 0
        total_lines = 0
        with open(args.out, "w", encoding="utf-8") as out:
            for m in queue:
                if total_chars >= args.max_chars:
                    break
                try:
                    text = zf.read(m).decode("utf-8", errors="ignore")
                except Exception:
                    continue
                for s in clean_sentence_parts(text):
                    out.write(s + "\n")
                    total_chars += len(s)
                    total_lines += 1
                    if total_chars >= args.max_chars:
                        break

    print(f"prepare_thucnews: 输出 {total_lines} 句 / {total_chars} 字 -> {args.out}")


if __name__ == "__main__":
    main()