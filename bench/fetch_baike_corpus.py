#!/usr/bin/env python3
"""从百度百科抓取词条摘要作文本语料，切成纯汉字句子后固化到 corpus/。

百度百科详情页 HTML 有反爬（安全验证拦截），但官方公开的「词条卡」接口
`BaikeLemmaCardApi` 直接返回每个词条约 400 字的**摘要正文**（`abstract` 字段），
干净、结构化，正适合做百科语料。全页正文抓不了，但摘要已足以覆盖高频词条的
「领域词汇 + 常见句式」，对纠错模块的音节/二元统计足够。

词条名从 `--terms` 文件读取（默认 `baike_terms.txt`，由 jieba 对已有语料分词、
抽高频名词生成——`from collections import Counter; import jieba.posseg as pseg`）。

百科摘要天然带数字/年份（如「长江全长6363千米」「李白701年—762年」），这里
**剥离**非汉字字符而非整句丢弃，保住百科的领域词覆盖——与人民网抓取脚本
「含数字整句丢弃」的策略不同，因为百科几乎句句带数字，整句丢弃会基本什么都不剩。

用法:
    python fetch_baike_corpus.py --terms baike_terms.txt --out corpus/baike.txt
"""
from __future__ import annotations

import argparse
import html
import json
import re
import sys
import time
import urllib.parse
import urllib.request
from pathlib import Path

API = "https://baike.baidu.com/api/openapi/BaikeLemmaCardApi"
UA = "pinyin-bench/0.1 (input-method accuracy research)"

SENT_SPLIT = re.compile(r"[。！？；!?;\n]+")
NON_CJK = re.compile(r"[^\u4e00-\u9fff]")
TAG = re.compile(r"<[^>]+>")


def fetch_abstract(word: str, bk_length: int = 600, retries: int = 2) -> str:
    """取词条摘要正文（HTML 已去标签、实体已还原），失败返回空串。"""
    params = {
        "scope": "103", "format": "json", "appid": "379020",
        "bk_key": word, "bk_length": str(bk_length),
    }
    url = API + "?" + urllib.parse.urlencode(params)
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=15) as r:
                data = json.load(r)
            ab = data.get("abstract") or ""
            return TAG.sub("", html.unescape(ab))
        except Exception as exc:
            if attempt == retries:
                print(f"    取词失败 {word}: {exc}", file=sys.stderr)
                return ""
            time.sleep(1.5 * (attempt + 1))
    return ""


def split_sentences(text: str, min_len: int, max_len: int) -> list[str]:
    """摘要 → 纯汉字句子列表；末段若被截断（无句末标点）则丢弃。"""
    text = text.strip()
    chunks = SENT_SPLIT.split(text)
    if chunks and not text.endswith(("。", "！", "？", "；", "!", "?", ";")):
        chunks = chunks[:-1]  # 摘要在约 400 字处被硬截断，末段是残句
    out = []
    for chunk in chunks:
        s = NON_CJK.sub("", chunk).strip()
        if min_len <= len(s) <= max_len:
            out.append(s)
    return out


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--terms", default="baike_terms.txt", help="词条名列表（每行一个）")
    p.add_argument("--out", default="corpus/baike.txt", help="输出纯汉字句子文件")
    p.add_argument("--target-chars", type=int, default=200_000, help="累计汉字数目标")
    p.add_argument("--max-terms", type=int, default=2000, help="最多抓取的词条数（0=不限）")
    p.add_argument("--min-len", type=int, default=4)
    p.add_argument("--max-len", type=int, default=40)
    p.add_argument("--sleep", type=float, default=0.15, help="每次请求间隔（秒），对接口礼貌一点")
    args = p.parse_args()

    terms_path = Path(args.terms)
    if not terms_path.exists():
        raise SystemExit(f"词条文件不存在: {terms_path}")
    terms = [ln.strip() for ln in terms_path.read_text(encoding="utf-8").splitlines() if ln.strip()]
    if args.max_terms:
        terms = terms[: args.max_terms]

    out = Path(args.out)
    seen: set[str] = set()
    sentences: list[str] = []
    chars = 0
    ok = 0

    for i, word in enumerate(terms, 1):
        if chars >= args.target_chars:
            break
        ab = fetch_abstract(word)
        if not ab:
            time.sleep(args.sleep)
            continue
        for s in split_sentences(ab, args.min_len, args.max_len):
            if s not in seen:
                seen.add(s)
                sentences.append(s)
                chars += len(s)
        if ab:
            ok += 1
        if i % 50 == 0:
            print(f"  进度 {i}/{len(terms)}：命中 {ok} 个词条，累计 {len(sentences)} 句 / {chars} 字", flush=True)
        time.sleep(args.sleep)

    out.parent.mkdir(parents=True, exist_ok=True)
    out.write_text("\n".join(sentences) + ("\n" if sentences else ""), encoding="utf-8")
    print(f"已写入 {out}：{len(sentences)} 句 / {chars} 字（命中 {ok}/{len(terms)} 个词条）")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())