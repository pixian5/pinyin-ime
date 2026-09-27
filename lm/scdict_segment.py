#!/usr/bin/env python3
# -*- coding: utf-8 -*-
"""scdict_segment.py — 用 sc.dict 词表对中文做最大匹配分词，产出 KenLM 训练语料。

动机：libime 的解码器栅格由 sc.dict 的词条构成，语言模型按「词条字符串」查 trie。
因此替代语言模型的词表必须是 sc.dict 词条的（近似）子集，否则解码器拼出的词在 LM 里
是 <unk>，准确率会雪崩。本脚本保证输出 token 都是 sc.dict 里的词（或单字兜底），
从而 LM 词表 ⊆ sc.dict 词表，栅格可兼容。

用法：
    python3 scdict_segment.py --dict /path/to/dict_sc.txt [--input raw.txt] [--output seg.txt]
    默认从 stdin 读、向 stdout 写：每行一句纯汉字，输出每行空格分隔的词。
"""
import argparse
import re
import sys

CJK = re.compile(r"[\u4e00-\u9fff]+")


def load_words(dict_path):
    """读 dict_sc.txt（word\\tpinyin\\tfreq），返回 (词集合, 最大词长)。"""
    words = set()
    maxlen = 0
    with open(dict_path, encoding="utf-8") as f:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            word = line.split()[0]  # 兼容 libime_pinyindict -d 的空格分隔与 tab 分隔
            if not word:
                continue
            words.add(word)
            if len(word) > maxlen:
                maxlen = len(word)
    return words, maxlen


def segment_fmm(text, words, maxlen):
    """前向最大匹配。代价：所有 token ∈ words ∪ {单字}，保证 LM 词表兼容 sc.dict。"""
    out = []
    i, n = 0, len(text)
    while i < n:
        matched = False
        upto = min(maxlen, n - i)
        for length in range(upto, 0, -1):
            piece = text[i:i + length]
            if piece in words:
                out.append(piece)
                i += length
                matched = True
                break
        if not matched:
            out.append(text[i])   # 未收录的单字兜底（仍是有效 token）
            i += 1
    return out


def main():
    ap = argparse.ArgumentParser(description="sc.dict 最大匹配分词")
    ap.add_argument("--dict", required=True, help="dict_sc.txt 路径")
    ap.add_argument("--input", help="原始文本（每行一句纯汉字），缺省 stdin")
    ap.add_argument("--output", help="输出（每行空格分隔词），缺省 stdout")
    args = ap.parse_args()

    words, maxlen = load_words(args.dict)
    fin = open(args.input, encoding="utf-8") if args.input else sys.stdin
    fout = open(args.output, "w", encoding="utf-8") if args.output else sys.stdout

    for line in fin:
        line = line.rstrip("\n").strip()
        # 只取 CJK 连续段，丢弃数字/拉丁/标点，避免造出残缺词
        for match in CJK.finditer(line):
            seg = segment_fmm(match.group(0), words, maxlen)
            if len(seg) >= 2:
                fout.write(" ".join(seg) + "\n")
    if args.output:
        fout.close()

    # 分词统计打到 stderr，便于核对词表规模
    sys.stderr.write(f"scdict_segment: 词表={len(words)} 最大词长={maxlen}\n")


if __name__ == "__main__":
    main()