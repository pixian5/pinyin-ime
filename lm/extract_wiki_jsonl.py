#!/usr/bin/env python3
"""extract_wiki_jsonl.py — 从 modelscope 中文维基 JSONL 抽取「句子/行」纯文本。

输入：AI-ModelScope/wikipedia-cn-20230720-filtered 的 wikipedia-cn-20230720-filtered.jsonl，
每行一个 JSON 对象，`completion` 字段是已清洗的维基文本块（无需再去 wiki 标记）。
输出：每行一句纯汉字文本（按标点切句、CJK 占比过滤）。--max-sentences 可提前停止。

用法：
  python3 lm/extract_wiki_jsonl.py --input wikipedia_zh.jsonl \
      --out wiki_train.txt --max-sentences 4000000
"""
from __future__ import annotations

import argparse
import json
import re
import sys

_CJK = re.compile(r"[\u4e00-\u9fff]")


def _cjk_ratio(text: str) -> float:
    if not text:
        return 0.0
    return len(_CJK.findall(text)) / len(text)


def sentences(block: str) -> list[str]:
    out = []
    for line in block.replace("\r", "").split("\n"):
        for chunk in re.split(r"[。！？!?；;]", line):
            chunk = chunk.strip()
            # 去掉首尾残留的列表/标题/括号符号
            chunk = chunk.strip("·-*#（()[\]【】\u3000 \t")
            if 3 <= len(chunk) <= 60 and _cjk_ratio(chunk) >= 0.6:
                out.append(chunk)
    return out


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--input", required=True)
    ap.add_argument("--out", required=True)
    ap.add_argument("--max-sentences", type=int, default=0, help="0=不限")
    args = ap.parse_args()

    kept = 0
    fout = open(args.out, "w", encoding="utf-8", buffering=1 << 18)
    with open(args.input, "rb") as fin:
        for raw in fin:
            try:
                obj = json.loads(raw)
            except Exception:
                continue
            text = obj.get("completion", "") or ""
            if not text:
                continue
            for s in sentences(text):
                fout.write(s + "\n")
                kept += 1
                if args.max_sentences and kept >= args.max_sentences:
                    break
            if args.max_sentences and kept >= args.max_sentences:
                break
    fout.close()
    print(f"[stat] 抽取 {kept} 句 -> {args.out}", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())