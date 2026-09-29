#!/usr/bin/env python3
"""快速评测：句级 + 字级准确率"""
import subprocess, sys
from pypinyin import pinyin, Style
import jieba

def to_pinyin(text):
    words = list(jieba.cut(text))
    parts = []
    for w in words:
        py = pinyin(w, style=Style.NORMAL, errors='default')
        s = ''.join(''.join(p) for p in py)
        s = ''.join(c for c in s if c.isalpha())
        if s: parts.append(s)
    return ''.join(parts)

def decode(py, cli, lm, dict_path, user_data=None):
    cmd = [cli, "--top1", "--quiet"]
    if user_data: cmd += ["--user-data", user_data]
    cmd += [lm, dict_path]
    r = subprocess.run(cmd, input=py + "\n", capture_output=True, text=True)
    return r.stdout.strip()

def main():
    corpus, user_data, cli, lm, dict_path = sys.argv[1:6]
    sentences = []
    with open(corpus) as f:
        for line in f:
            line = line.strip()
            if line and len(line) >= 2:
                chinese = ''.join(c for c in line if '\u4e00' <= c <= '\u9fff')
                if chinese: sentences.append(chinese)
    
    n = len(sentences)
    c_before = c_after = 0
    ch_before = ch_after = ch_total = 0
    
    for sent in sentences:
        py = to_pinyin(sent)
        if not py: continue
        before = decode(py, cli, lm, dict_path)
        after = decode(py, cli, lm, dict_path, user_data)
        if before == sent: c_before += 1
        if after == sent: c_after += 1
        for i, c in enumerate(sent):
            ch_total += 1
            if i < len(before) and before[i] == c: ch_before += 1
            if i < len(after) and after[i] == c: ch_after += 1
    
    print(f"评测句数: {n}")
    print(f"句级准确率: {c_before/n*100:.1f}% → {c_after/n*100:.1f}% (+{(c_after-c_before)/n*100:.1f}%)")
    print(f"字级准确率: {ch_before/ch_total*100:.1f}% → {ch_after/ch_total*100:.1f}%")

if __name__ == '__main__':
    main()
