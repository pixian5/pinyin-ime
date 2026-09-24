#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""diagnose_vocab.py — 量化「评测语料中被 LM 词表覆盖的比例」，定位 LM 退化根因。

libime 解码器按词条字符串查 LM trie，LM 词表缺词 → 该词按 <unk> 打分。
用同一评测语料，对比「官方 lm_sc.arpa」与「自训 arpa」的词表覆盖度，
能直接说明自训 LM 为何变好/变差（词表缺口通常比 n-gram 数量更关键）。

用法：
  python3 diagnose_vocab.py --corpus corpus/news.txt --dict dict_sc.txt \
      --arpa-a lm_sc.arpa --arpa-b zh_CN_news.arpa --label-a official --label-b news
"""
import argparse
import re
from collections import Counter

CJK = re.compile(r"[\u4e00-\u9fff]+")


def load_scdict_words(dict_path):
    words = set()
    maxlen = 0
    with open(dict_path, encoding="utf-8") as f:
        for line in f:
            w = line.split("\t")[0]
            if w:
                words.add(w)
                maxlen = max(maxlen, len(w))
    return words, maxlen


def segment(text, words, maxlen):
    out, i, n = [], 0, len(text)
    while i < n:
        hit = False
        for L in range(min(maxlen, n - i), 0, -1):
            if text[i:i + L] in words:
                out.append(text[i:i + L])
                i += L
                hit = True
                break
        if not hit:
            out.append(text[i])
            i += 1
    return out


def load_arpa_vocab(arpa_path):
    """解析 ARPA 的 \\1-grams: 段，返回词集合。"""
    vocab = set()
    in_section = False
    with open(arpa_path, encoding="utf-8", errors="ignore") as f:
        for line in f:
            line = line.rstrip("\n")
            if line == "\\1-grams:":
                in_section = True
                continue
            if in_section:
                if line == "" or line.startswith("\\"):
                    break
                parts = line.split("\t")
                # 格式: prob \t word [\t backoff]
                if len(parts) >= 2:
                    vocab.add(parts[1])
    return vocab


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", required=True)
    ap.add_argument("--dict", required=True)
    ap.add_argument("--arpa-a", required=True)
    ap.add_argument("--arpa-b", required=True)
    ap.add_argument("--label-a", default="official")
    ap.add_argument("--label-b", default="trained")
    ap.add_argument("--top", type=int, default=25)
    args = ap.parse_args()

    words, maxlen = load_scdict_words(args.dict)

    # 评测语料词频（按 sc.dict 分词，与训练/解码口径一致）
    freq = Counter()
    with open(args.corpus, encoding="utf-8") as f:
        for line in f:
            for m in CJK.finditer(line):
                freq.update(segment(m.group(0), words, maxlen))
    tot_tok = sum(freq.values())
    tot_dist = len(freq)

    va = load_arpa_vocab(args.arpa_a)
    vb = load_arpa_vocab(args.arpa_b)

    def coverage(vocab):
        cov_tok = sum(c for w, c in freq.items() if w in vocab)
        cov_dist = sum(1 for w in freq if w in vocab)
        return cov_tok, cov_dist

    a_tok, a_dist = coverage(va)
    b_tok, b_dist = coverage(vb)

    print(f"评测语料（sc.dict 分词）：{tot_dist} 个不重复词 / {tot_tok} 个词次")
    print(f"{args.label_a} 词表: {len(va)}  覆盖 {a_dist}/{tot_dist} 词型"
          f"({100*a_dist/tot_dist:.1f}%)  {a_tok}/{tot_tok} 词次({100*a_tok/tot_tok:.2f}%)")
    print(f"{args.label_b} 词表: {len(vb)}  覆盖 {b_dist}/{tot_dist} 词型"
          f"({100*b_dist/tot_dist:.1f}%)  {b_tok}/{tot_tok} 词次({100*b_tok/tot_tok:.2f}%)")

    missing = [(w, c) for w, c in freq.most_common()
               if w in va and w not in vb]
    only_b = [(w, c) for w, c in freq.most_common()
              if w not in va and w in vb]
    print(f"\n官方有、自训缺的高频词（前 {args.top}，'词[频次]'）：")
    for w, c in missing[:args.top]:
        print(f"  {w}[{c}]")
    print(f"\n自训有、官方缺的高频词（前 {args.top}）：")
    for w, c in only_b[:args.top]:
        print(f"  {w}[{c}]")


if __name__ == "__main__":
    main()