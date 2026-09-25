#!/usr/bin/env python3
"""抓取 / 清洗 LCCC 中文对话口语语料，输出到 corpus/lccc.txt。

为什么是 LCCC：README「真实整句评测」显示**对话域纠错最弱**（delete / substitute
明显低于新闻、百科域），而训练语料（wiki/news/recent/baike）几乎全是书面语。
LCCC（Large-scale Cleaned Chinese Conversation, NLPCC 2020, MIT 许可）是**对话域**
的大规模普通用户语料，正是当前缺的那块地基。

原始格式：JSONL，每行是一段对话 = 字符串列表；每个字符串内**字与字用空格分隔**
（如 "啊 我 好 爱 虾 仁 蛋 黄 酱"）。清洗规则见 clean_turn()。

用法:
    # 在线抓取（LCCC-base train，约 370MB gz）并清洗
    python fetch_lccc_corpus.py --split base_train --out corpus/lccc.txt \
        --exclude-corpus corpus/news.txt --exclude-corpus corpus/dialogue.txt

    # 用已下载的本地文件先做小样本验证
    python fetch_lccc_corpus.py --in corpus_probe/lccc_base_valid.jsonl.gz \
        --out /tmp/lccc_sample.txt --max-sent 20000 --dedup

防泄漏纪律：--exclude-corpus 会剔除已存在于其它语料（尤其是**留出测试集**
corpus/dialogue.txt 与主评测集 corpus/news.txt）中的句子，保证训练 / 评测不串。

内存纪律：LCCC-base train 约 16M turn，逐句攒进内存会被 OOM 杀掉（沙箱 3.7GB 无
swap），因此默认**流式落盘**、不去重；需要去重时交给 `sort -u`（磁盘换内存），
或用 --dedup（仅小样本）。
"""
from __future__ import annotations

import argparse
import gzip
import io
import json
import re
import sys
import urllib.request
from collections import Counter
from pathlib import Path

# HuggingFace: silver/lccc（MIT）。base = 微博清洗对话，large = 更多来源。
URLS = {
    "base_train": "https://huggingface.co/datasets/silver/lccc/resolve/main/lccc_base_train.jsonl.gz",
    "base_valid": "https://huggingface.co/datasets/silver/lccc/resolve/main/lccc_base_valid.jsonl.gz",
    "base_test": "https://huggingface.co/datasets/silver/lccc/resolve/main/lccc_base_test.jsonl.gz",
    "large": "https://huggingface.co/datasets/silver/lccc/resolve/main/lccc_large.jsonl.gz",
}

CJK_RANGE = "\u4e00-\u9fff"
# 只保留汉字；其余（英文 / 数字 / 表情 / URL / 标点 / 空白）一律视作噪声
NON_CJK = re.compile(rf"[^{CJK_RANGE}]")
URL_PAT = re.compile(r"https?://|www\.|\.com|\.cn", re.I)
# 微博转发噪声前缀（LCCC 官方清洗后仍可能残留少量）
NOISE_PREFIX = re.compile(r"^(转发内容|原图|视频|网页链接|全文|查看图片|评论太火爆)")

UA = "pinyin-ime/0.1 (corpus research)"


def download(url: str, dst: Path, retries: int = 3) -> Path:
    """下载 gz 到 dst；已存在且非空则复用（支持断点续跑）。"""
    if dst.exists() and dst.stat().st_size > 0:
        print(f"[fetch] 复用已下载文件 {dst} ({dst.stat().st_size} B)", file=sys.stderr)
        return dst
    dst.parent.mkdir(parents=True, exist_ok=True)
    tmp = dst.with_suffix(dst.suffix + ".part")
    for attempt in range(retries + 1):
        try:
            req = urllib.request.Request(url, headers={"User-Agent": UA})
            with urllib.request.urlopen(req, timeout=60) as r, open(tmp, "wb") as w:
                total = int(r.headers.get("Content-Length") or 0)
                got = 0
                while True:
                    chunk = r.read(1 << 20)
                    if not chunk:
                        break
                    w.write(chunk)
                    got += len(chunk)
                    if total:
                        print(f"\r[fetch] {got * 100 // total}% ({got}/{total} B)",
                              end="", file=sys.stderr)
            print(file=sys.stderr)
            tmp.rename(dst)
            print(f"[fetch] 完成 -> {dst} ({dst.stat().st_size} B)", file=sys.stderr)
            return dst
        except Exception as exc:  # noqa: BLE001 - 网络异常统一重试
            print(f"\n[fetch] 重试 {attempt + 1}/{retries + 1}: {exc}", file=sys.stderr)
            if tmp.exists():
                tmp.unlink()
    raise SystemExit(f"[fetch] 下载失败: {url}")


def open_text(path: Path):
    if path.suffix == ".gz":
        return gzip.open(path, "rt", encoding="utf-8", errors="ignore")
    return open(path, "r", encoding="utf-8", errors="ignore")


def load_excluded(paths: list) -> set:
    """读入需要排除的语料（一行一句纯文本），返回句子集合。"""
    ex: set = set()
    for p in paths:
        if not p.exists():
            print(f"[warn] 排除语料不存在，跳过: {p}", file=sys.stderr)
            continue
        for line in p.read_text(encoding="utf-8", errors="ignore").splitlines():
            s = line.strip()
            if s:
                ex.add(s)
    return ex


def iter_turns(path: Path):
    """逐行解析 JSONL，产出对话中的每个 turn 字符串。"""
    with open_text(path) as f:
        for raw in f:
            raw = raw.strip()
            if not raw:
                continue
            try:
                turns = json.loads(raw)
            except json.JSONDecodeError:
                continue
            if isinstance(turns, dict):  # 少量行是 {"dialog": [...], ...}
                turns = turns.get("dialog") or turns.get("conversation") or []
            if not isinstance(turns, list):
                continue
            stack = list(turns)
            while stack:
                t = stack.pop(0)
                if isinstance(t, str):
                    yield t
                elif isinstance(t, list):  # 嵌套一层
                    stack = list(t) + stack


def clean_turn(text: str, min_len: int, max_len: int):
    """把一条对话 turn 洗成「纯汉字句」，不合规返回 None。

    步骤：噪声前缀 / URL 丢弃 → 去字间空格与非汉字 → 长度过滤。
    """
    if not text:
        return None
    if URL_PAT.search(text) or NOISE_PREFIX.match(text.strip()):
        return None
    s = NON_CJK.sub("", text)
    if not (min_len <= len(s) <= max_len):
        return None
    return s


def main() -> int:
    p = argparse.ArgumentParser(
        description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    src = p.add_argument_group("数据源（--in 与 --split 二选一）")
    src.add_argument("--split", choices=sorted(URLS), default=None,
                     help="在线下载指定分片")
    src.add_argument("--in", dest="infile", default=None,
                     help="本地 JSONL（.gz 亦可），跳过下载")
    src.add_argument("--cache", default=None,
                     help="下载缓存路径（默认 corpus_probe/<split>.jsonl.gz）")

    p.add_argument("--out", default="corpus/lccc.txt", help="输出语料（一行一句纯汉字）")
    p.add_argument("--stats", default=None, help="可选：把清洗统计写成 JSON")
    p.add_argument("--min-len", type=int, default=4, help="最短字数（默认 4，与评测口径一致）")
    p.add_argument("--max-len", type=int, default=30, help="最长字数（默认 30）")
    p.add_argument("--max-sent", type=int, default=0, help="最多输出句数，0=不限")
    p.add_argument("--exclude-corpus", action="append", default=[],
                   help="排除其中已出现的句子（可重复；务必排除留出测试集）")
    p.add_argument("--dedup", action="store_true",
                   help="就地内存去重（仅小样本用；全量请改用 `sort -u`）")
    args = p.parse_args()

    here = Path(__file__).resolve().parent
    out = Path(args.out)
    if not out.is_absolute():
        out = here / out

    if args.infile:
        src_path = Path(args.infile)
        if not src_path.is_absolute():
            src_path = here / src_path
        if not src_path.exists():
            raise SystemExit(f"[err] 本地文件不存在: {src_path}")
    elif args.split:
        cache = Path(args.cache) if args.cache else (
            here / "corpus_probe" / f"{args.split}.jsonl.gz")
        if not cache.is_absolute():
            cache = here / cache
        src_path = download(URLS[args.split], cache)
    else:
        raise SystemExit("[err] 需要 --split 或 --in 指定数据源")

    excluded = load_excluded([Path(x) if Path(x).is_absolute() else here / x
                              for x in args.exclude_corpus])
    if excluded:
        print(f"[info] 排除语料句子数: {len(excluded)}", file=sys.stderr)

    # 注意：seen 只在 --dedup 时启用。千万级句子放内存 set 会被 OOM 杀掉
    # （沙箱 3.7GB 无 swap，实测 10M 句 anon-rss 1.35GB 即被 kill）。
    # 全量去重请交给磁盘：`sort -u in.txt > out.txt`。
    counter: Counter = Counter()
    seen: set = set() if args.dedup else frozenset()
    kept_n = 0
    chars = 0
    out.parent.mkdir(parents=True, exist_ok=True)
    with open(out, "w", encoding="utf-8") as w:
        for turn in iter_turns(src_path):
            counter["turns"] += 1
            s = clean_turn(turn, args.min_len, args.max_len)
            if s is None:
                counter["dropped_invalid"] += 1
                continue
            if s in excluded:
                counter["dropped_excluded"] += 1
                continue
            if args.dedup:
                if s in seen:
                    counter["dropped_dup"] += 1
                    continue
                seen.add(s)
            w.write(s + "\n")
            kept_n += 1
            chars += len(s)
            if args.max_sent and kept_n >= args.max_sent:
                break

    counter["kept"] = kept_n
    counter["chars"] = chars
    print(
        f"[done] {out}  句数={kept_n}  字数={chars}  turns={counter['turns']}  "
        f"丢弃(非法/重复/排除)="
        f"{counter['dropped_invalid']}/{counter['dropped_dup']}/{counter['dropped_excluded']}",
        file=sys.stderr)
    if args.stats:
        sp = Path(args.stats)
        if not sp.is_absolute():
            sp = here / sp
        sp.write_text(json.dumps(dict(counter), ensure_ascii=False, indent=2),
                      encoding="utf-8")
    return 0 if kept_n else 1


if __name__ == "__main__":
    raise SystemExit(main())
