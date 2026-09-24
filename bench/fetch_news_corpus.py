#!/usr/bin/env python3
"""从人民网 RSS 抓取新闻语料，切成短句后固化到 corpus/。

选人民网的原因：AutoRime 公开的基线数字用的是《人民日报》语料，
人民网 RSS 与之同源同分布，**可以直接横向对比**；而中文维基百科
专有名词密集，会严重低估输入法的真实水平（详见 PLAN-A.md 第 9 节）。

每个频道约 380KB / 100 条，抓 10 个频道总共约 4MB，对热点很友好。
失败的频道会被跳过，不会中断整体抓取。

用法:
    python fetch_news_corpus.py --target 2000 --out corpus/news.txt
"""
from __future__ import annotations

import argparse
import re
import sys
import time
import urllib.request
from pathlib import Path

from opencc import OpenCC

T2S = OpenCC("t2s")
UA = "pinyin-bench/0.1 (input-method accuracy research)"

# 人民网各频道 RSS；覆盖时政/国际/社会/财经/文教/体育等，避免单一领域偏差
FEEDS = [
    ("时政", "http://www.people.com.cn/rss/politics.xml"),
    ("国际", "http://www.people.com.cn/rss/world.xml"),
    ("社会", "http://www.people.com.cn/rss/society.xml"),
    ("财经", "http://www.people.com.cn/rss/finance.xml"),
    ("文化", "http://www.people.com.cn/rss/culture.xml"),
    ("教育", "http://www.people.com.cn/rss/edu.xml"),
    ("体育", "http://www.people.com.cn/rss/sports.xml"),
    ("军事", "http://www.people.com.cn/rss/military.xml"),
    ("健康", "http://www.people.com.cn/rss/health.xml"),
    ("海峡", "http://www.people.com.cn/rss/haixia.xml"),
]

CJK_RANGE = "\u4e00-\u9fff"
SENT_SPLIT = re.compile(r"[。！？；!?;\n]+")
NON_CJK = re.compile(rf"[^{CJK_RANGE}]")
TAG = re.compile(r"<[^>]+>")
ITEM = re.compile(r"<item>(.*?)</item>", re.S)
CDATA = re.compile(r"<!\[CDATA\[(.*?)\]\]>", re.S)


def fetch(url: str, retries: int = 2) -> str:
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=30) as r:
                return r.read().decode("utf-8", errors="ignore")
        except Exception as exc:
            print(f"    重试 {attempt + 1}/{retries + 1}: {exc}", file=sys.stderr)
            time.sleep(2)
    return ""


def strip_html(s: str) -> str:
    m = CDATA.search(s)
    if m:
        s = m.group(1)
    return TAG.sub("", s)


def split_sentences(text: str, min_len: int, max_len: int) -> list[str]:
    out = []
    for chunk in SENT_SPLIT.split(text):
        # 含数字/拉丁字母的句子在剔除非汉字后会残缺（"6月5日"→"月日"），整句丢弃。
        # 含丨｜| 的多是编辑署名行（"总监制丨闫帅南"），人名堆砌无意义，同样丢弃。
        if re.search(r"[0-9A-Za-z丨｜|]", chunk):
            continue
        s = NON_CJK.sub("", chunk).strip()
        if min_len <= len(s) <= max_len:
            out.append(s)
    return out


def main() -> int:
    p = argparse.ArgumentParser()
    p.add_argument("--target", type=int, default=2000)
    p.add_argument("--min-len", type=int, default=4)
    p.add_argument("--max-len", type=int, default=30)
    p.add_argument("--out", default="corpus/news.txt")
    args = p.parse_args()

    seen: set[str] = set()
    sentences: list[str] = []
    out = Path(args.out)

    for name, url in FEEDS:
        if len(sentences) >= args.target:
            break
        print(f"[{name}] {url}", flush=True)
        raw = fetch(url)
        if not raw:
            print(f"    跳过（抓取失败）", file=sys.stderr)
            continue
        got = 0
        for item in ITEM.findall(raw):
            fields = {
                "title": re.search(r"<title>(.*?)</title>", item, re.S),
                "description": re.search(r"<description>(.*?)</description>", item, re.S),
            }
            text = " ".join(strip_html(m.group(1)) for m in fields.values() if m)
            for s in split_sentences(T2S.convert(text), args.min_len, args.max_len):
                if s not in seen:
                    seen.add(s)
                    sentences.append(s)
                    got += 1
        print(f"    +{got} 句（累计 {len(sentences)}）", flush=True)
        time.sleep(1)

    sentences = sentences[: args.target]
    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(sentences) + "\n", encoding="utf-8")
    chars = sum(len(s) for s in sentences)
    print(f"已写入 {out}：{len(sentences)} 句 / {chars} 字")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
