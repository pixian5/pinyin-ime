#!/usr/bin/env python3
"""从中文维基百科抓取真实语料，切成短句后固化到 corpus/。

抓一次就存盘，之后评测都用本地文件，保证结果可复现。

用法:
    python fetch_corpus.py --target 2000 --out corpus/wiki.txt
"""
from __future__ import annotations

import argparse
import json
import re
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

from opencc import OpenCC

API = "https://zh.wikipedia.org/w/api.php"
UA = "pinyin-bench/0.1 (input-method accuracy research)"

# 中文维基正文是繁简混排的，评测简体拼音输入法必须先统一到简体
T2S = OpenCC("t2s")

CJK_RANGE = "\u4e00-\u9fff"
SENT_SPLIT = re.compile(r"[。！？；!?;\n]+")
NON_CJK = re.compile(rf"[^{CJK_RANGE}]")


def fetch_batch(limit: int, retries: int = 3) -> list[str]:
    params = {
        "action": "query",
        "format": "json",
        "prop": "extracts",
        "exintro": "1",
        "explaintext": "1",
        "generator": "random",
        "grnnamespace": "0",
        "grnlimit": str(limit),
    }
    url = API + "?" + urllib.parse.urlencode(params)
    for attempt in range(retries):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=30) as r:
                data = json.load(r)
            pages = data.get("query", {}).get("pages", {})
            return [T2S.convert(p.get("extract", "") or "") for p in pages.values()]
        except Exception as exc:                      # 网络抖动，退避重试
            print(f"  抓取失败({attempt + 1}/{retries}): {exc}", file=sys.stderr)
            time.sleep(2 * (attempt + 1))
    return []


def split_sentences(text: str, min_len: int, max_len: int) -> list[str]:
    out = []
    for chunk in SENT_SPLIT.split(text):
        # 含数字/拉丁字母的句子在剔除非汉字后会残缺（"6月5日"→"月日"），整句丢弃
        if re.search(r"[0-9A-Za-z]", chunk):
            continue
        s = NON_CJK.sub("", chunk).strip()
        if min_len <= len(s) <= max_len:
            out.append(s)
    return out


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--target", type=int, default=2000, help="目标句子数")
    p.add_argument("--batch", type=int, default=50, help="每批抓取文章数（热点下调大以减少往返）")
    p.add_argument("--min-len", type=int, default=4)
    p.add_argument("--max-len", type=int, default=30)
    p.add_argument("--out", default="corpus/wiki.txt")
    p.add_argument("--fresh", action="store_true",
                   help="忽略已有语料重新抓取（默认会在现有文件基础上续抓，适合网络不稳时）")
    args = p.parse_args()

    out = Path(args.out)
    seen: set[str] = set()
    sentences: list[str] = []
    if not args.fresh and out.exists():
        # 热点/弱网下抓取经常中断，默认续抓而不是重来
        for line in out.read_text(encoding="utf-8").splitlines():
            s = line.strip()
            if s and s not in seen:
                seen.add(s)
                sentences.append(s)
        print(f"现有语料 {len(sentences)} 句，继续补充至 {args.target} 句")
    rounds = 0
    print(f"目标 {args.target} 句，来源：中文维基百科随机条目", flush=True)
    while len(sentences) < args.target and rounds < 300:
        rounds += 1
        batch = fetch_batch(args.batch)
        if not batch:
            break
        for text in batch:
            for s in split_sentences(text, args.min_len, args.max_len):
                if s not in seen:
                    seen.add(s)
                    sentences.append(s)
        print(f"  第 {rounds} 轮：累计 {len(sentences)} 句", flush=True)
        time.sleep(1)          # 对 API 礼貌一点，也避免热点下被限流

    sentences = sentences[: args.target]
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(sentences) + "\n", encoding="utf-8")
    chars = sum(len(s) for s in sentences)
    print(f"已写入 {out}：{len(sentences)} 句 / {chars} 字")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
