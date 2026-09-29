#!/usr/bin/env python3
"""从中文语料生成 拼音→中文 训练对，喂给 pinyin_ime_cli --learn-file 做词级训练。

用法:
    python3 train_from_corpus.py <corpus.txt> <output.learn> [--limit N]

流程:
    1. 逐行读中文句子
    2. jieba 分词
    3. pypinyin 转拼音（不带声调）
    4. 输出 "拼音<TAB>中文" 对（仅当整句拼音可恢复时）
"""
import sys
import argparse
from pypinyin import pinyin, Style
import jieba

def to_pinyin(text):
    """将中文转为无声调全拼，非中文字符跳过。"""
    words = list(jieba.cut(text))
    parts = []
    for w in words:
        py = pinyin(w, style=Style.NORMAL, errors='default')
        # 拼平
        s = ''.join(''.join(p) for p in py)
        # 只保留纯字母（过滤标点、数字）
        s = ''.join(c for c in s if c.isalpha())
        if s:
            parts.append(s)
    return ''.join(parts)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('corpus', help='中文语料文件（每行一句）')
    ap.add_argument('output', help='输出训练对文件')
    ap.add_argument('--limit', type=int, default=0, help='最多处理 N 句')
    args = ap.parse_args()

    count = 0
    skipped = 0
    with open(args.corpus) as fin, open(args.output, 'w') as fout:
        for line in fin:
            line = line.strip()
            if not line or line.startswith('#'):
                continue
            # 只保留中文字符
            chinese = ''.join(c for c in line if '\u4e00' <= c <= '\u9fff')
            if len(chinese) < 2:
                continue
            py = to_pinyin(chinese)
            if py and len(py) >= 2:
                fout.write(f'{py}\t{chinese}\n')
                count += 1
            else:
                skipped += 1
            if args.limit and count >= args.limit:
                break

    print(f'生成 {count} 对训练数据（跳过 {skipped} 行）→ {args.output}')

if __name__ == '__main__':
    main()
