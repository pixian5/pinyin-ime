#!/usr/bin/env python3
"""extract_wiki.py — 流式抽取中文维基正文为「句子/行」纯文本，供 LM 训练。

从 zhwiki pages-articles.xml.bz2 流式读 <page>，取主命名空间(ns=0)的 <text>，
去 wiki 标记、按标点切句、只保留 CJK 占比高的行。--max-sentences 可提前停止
（bz2 顺序解压，提前停止省时省空间，只需下载到的部分即可开始处理）。

用法：
  python3 lm/extract_wiki.py --input zhwiki-pages-articles.xml.bz2 \
      --out wiki_train.txt --max-sentences 4000000
"""
from __future__ import annotations

import argparse
import bz2
import io
import re
import sys
from xml.etree import ElementTree as ET

_CJK = re.compile(r"[\u4e00-\u9fff]")


def _local(tag: str) -> str:
    return tag.rsplit("}", 1)[-1] if "}" in tag else tag


def _cjk_ratio(text: str) -> float:
    if not text:
        return 0.0
    return len(_CJK.findall(text)) / len(text)


def remove_braces(text: str) -> str:
    """删除 {{...}}（尽可能配平；best-effort）。"""
    out = []
    depth = 0
    i, n = 0, len(text)
    while i < n:
        if text.startswith("{{", i):
            depth += 1
            i += 2
            continue
        if text.startswith("}}", i):
            if depth > 0:
                depth -= 1
                i += 2
                continue
        if depth == 0:
            out.append(text[i])
        i += 1
    return "".join(out)


def strip_markup(text: str) -> str:
    """去 wiki 标记（best-effort，残留噪音由 CJK/句长过滤兜底）。"""
    text = re.sub(r"<!--.*?-->", "", text, flags=re.S)
    text = re.sub(r"<ref[^>]*?/>", "", text)
    text = re.sub(r"<ref[^>]*>.*?</ref>", "", text, flags=re.S)
    text = re.sub(r"<(math|gallery|timeline|score|imagemap|source)[^>]*>.*?</\1>", "",
                  text, flags=re.S)
    text = re.sub(r"<[^>]+>", "", text)
    text = remove_braces(text)
    # 先删媒体/分类链接，再处理普通链接 [[link|显示]] -> 显示
    text = re.sub(r"\[\[(?:File|Image|Media|Category|Wikipedia|Template):[^\]]*\]\]",
                  "", text, flags=re.I)
    text = re.sub(r"\[\[(?:[^\[\]|]*\|)?([^\[\]]+)\]\]", r"\1", text)
    text = re.sub(r"^\s*[|!{].*$", "", text, flags=re.M)   # 表格行 / 列表起始
    text = re.sub(r"^\s*=+.*?=+\s*$", "", text, flags=re.M)  # 章节标题
    text = re.sub(r"https?://\S+", "", text)
    text = re.sub(r"&[a-zA-Z#0-9]+;", " ", text)
    return text


def sentences(text: str) -> list[str]:
    out = []
    for line in text.split("\n"):
        for chunk in re.split(r"[。！？!?；;]", line):
            chunk = chunk.strip()
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
    articles = 0
    skipped_redirect = 0
    fout = open(args.out, "w", encoding="utf-8", buffering=1 << 18)
    raw = bz2.BZ2File(args.input, "rb")
    stream = io.TextIOWrapper(raw, encoding="utf-8", errors="replace")
    try:
        for _event, elem in ET.iterparse(stream, events=("end",)):
            if _local(elem.tag) != "page":
                continue
            ns = "0"
            body = ""
            for child in elem:
                ln = _local(child.tag)
                if ln == "ns":
                    ns = (child.text or "0").strip()
                elif ln == "revision":
                    for sub in child:
                        if _local(sub.tag) == "text":
                            body = sub.text or ""
            elem.clear()
            if ns.strip() != "0":
                continue
            if not body:
                continue
            if body.lstrip().startswith("#REDIRECT") or body.lstrip().startswith("#重定向"):
                skipped_redirect += 1
                continue
            articles += 1
            for s in sentences(strip_markup(body)):
                fout.write(s + "\n")
                kept += 1
                if args.max_sentences and kept >= args.max_sentences:
                    break
            if args.max_sentences and kept >= args.max_sentences:
                break
    finally:
        fout.close()
    print(f"[stat] 文章 {articles}（跳过重定向 {skipped_redirect}），保留 {kept} 句 -> {args.out}",
          file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())