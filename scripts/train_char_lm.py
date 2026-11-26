#!/usr/bin/env python3
"""train_char_lm.py — 从 bench/corpus 训练字符 trigram，生成 pinyin-ime 重排序模型。

为什么需要：系统自带的 zh_CN.lm 在同音歧义上的区分力不足，首选句级只有
27.83%；而 nbest=10 的召回上限是 46.08%。本模型不解码、不换词典，只在
nbest 候选之间按「这个字串像不像正常中文」重排，把潜力吃回来。

用法：
    python3 scripts/train_char_lm.py [--out data/char_trigram.bin]
    python3 scripts/train_char_lm.py --corpus bench/corpus --out /tmp/x.bin

输出二进制格式（小端，固定版本 v1）：
    magic  4s  "PICL"
    version u16 = 1
    vocab_n u16
    for each vocab: len u8 + utf8 bytes
    unigram_total u32
    unigram_count u16 * vocab_n
    bigram_ctx_n u32; each: prev u16, total u16
    bigram_entry_n u32; each: prev u16, ch u16, count u16
    trigram_ctx_n u32; each: p2 u16, p1 u16, total u16
    trigram_entry_n u32; each: p2 u16, p1 u16, ch u16, count u16

计数均 < 65536（本语料实测最大 16178），因此统一用 u16 压缩；语料若超过
该上限会在写文件前报错，避免静默截断。
"""
from __future__ import annotations

import argparse
import datetime
import hashlib
import json
import struct
import sys
from collections import defaultdict
from pathlib import Path

MAGIC = b"PICL"
VERSION = 1
SPECIALS = ["<s>", "</s>", "<unk>"]


def read_corpus(corpus: Path):
    uni = defaultdict(int)
    bi = defaultdict(lambda: defaultdict(int))
    tri = defaultdict(lambda: defaultdict(int))
    for f in sorted(corpus.glob("*.txt")):
        for line in f.read_text(encoding="utf-8", errors="ignore").splitlines():
            line = line.strip()
            if not line:
                continue
            p2 = p = "<s>"
            for ch in line:
                if ch.isspace():
                    p2 = p = "<s>"
                    continue
                uni[ch] += 1
                bi[p][ch] += 1
                tri[(p2, p)][ch] += 1
                p2, p = p, ch
            bi[p]["</s>"] += 1
            tri[(p2, p)]["</s>"] += 1
    return uni, bi, tri


def build_vocab(uni):
    chars = sorted(uni.keys())
    vocab = SPECIALS + chars
    if len(vocab) > 65535:
        raise SystemExit(f"词表过大：{len(vocab)} > 65535")
    return vocab


def write_model(path: Path, vocab, uni, bi, tri):
    ids = {ch: i for i, ch in enumerate(vocab)}
    max_count = max(
        [max(uni.values())]
        + [max(v.values()) for v in bi.values()]
        + [max(v.values()) for v in tri.values()]
    )
    max_total = max(
        [sum(v.values()) for v in bi.values()]
        + [sum(v.values()) for v in tri.values()]
    )
    if max_count > 65535 or max_total > 65535:
        raise SystemExit(f"计数超出 u16：max_count={max_count} max_total={max_total}")

    bigram_ctx = []
    for prev, nxt in bi.items():
        if prev not in ids:
            continue
        total = sum(nxt.values())
        if total > 65535:
            raise SystemExit(f"bigram 上下文总数溢出：{prev} {total}")
        bigram_ctx.append((ids[prev], total))
    bigram_ctx.sort()

    bigram_entries = []
    for prev, nxt in bi.items():
        if prev not in ids:
            continue
        for ch, count in nxt.items():
            if ch not in ids:
                continue
            bigram_entries.append((ids[prev], ids[ch], count))
    bigram_entries.sort()

    trigram_ctx = []
    for (p2, p1), nxt in tri.items():
        if p2 not in ids or p1 not in ids:
            continue
        total = sum(nxt.values())
        if total > 65535:
            raise SystemExit(f"trigram 上下文总数溢出：{p2},{p1} {total}")
        trigram_ctx.append((ids[p2], ids[p1], total))
    trigram_ctx.sort()

    trigram_entries = []
    for (p2, p1), nxt in tri.items():
        if p2 not in ids or p1 not in ids:
            continue
        for ch, count in nxt.items():
            if ch not in ids:
                continue
            trigram_entries.append((ids[p2], ids[p1], ids[ch], count))
    trigram_entries.sort()

    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(MAGIC)
        f.write(struct.pack("<H", VERSION))
        f.write(struct.pack("<H", len(vocab)))
        for ch in vocab:
            raw = ch.encode("utf-8")
            if len(raw) > 255:
                raise SystemExit(f"词表项过长：{ch!r}")
            f.write(struct.pack("<B", len(raw)))
            f.write(raw)
        f.write(struct.pack("<I", sum(uni.values())))
        for ch in vocab:
            f.write(struct.pack("<H", uni.get(ch, 0)))
        f.write(struct.pack("<I", len(bigram_ctx)))
        for prev, total in bigram_ctx:
            f.write(struct.pack("<HH", prev, total))
        f.write(struct.pack("<I", len(bigram_entries)))
        for prev, ch, count in bigram_entries:
            f.write(struct.pack("<HHH", prev, ch, count))
        f.write(struct.pack("<I", len(trigram_ctx)))
        for p2, p1, total in trigram_ctx:
            f.write(struct.pack("<HHH", p2, p1, total))
        f.write(struct.pack("<I", len(trigram_entries)))
        for p2, p1, ch, count in trigram_entries:
            f.write(struct.pack("<HHHH", p2, p1, ch, count))

    print(
        f"已写入 {path}：词表 {len(vocab)}，bigram {len(bigram_entries)} 条，"
        f"trigram {len(trigram_entries)} 条，{path.stat().st_size} 字节"
    )


def corpus_metadata(corpus: Path) -> dict:
    files = []
    for f in sorted(corpus.glob("*.txt")):
        text = f.read_text(encoding="utf-8", errors="ignore").splitlines()
        sentences = [line.strip() for line in text if line.strip()]
        files.append({
            "file": f.name,
            "sha256": hashlib.sha256(f.read_bytes()).hexdigest(),
            "sentences": len(sentences),
            "chars": sum(len(s) for s in sentences),
        })
    return {
        "files": files,
        "sentences": sum(x["sentences"] for x in files),
        "chars": sum(x["chars"] for x in files),
    }


def write_metadata(path: Path, corpus: Path, model: Path, alpha: float) -> None:
    version_file = Path(__file__).resolve().parent.parent / "VERSION"
    version = version_file.read_text(encoding="utf-8").strip() if version_file.exists() else "unknown"
    meta = {
        "kind": "pinyin-ime-char-trigram-v1",
        "version": version,
        "alpha": alpha,
        "built_at": datetime.datetime.now(datetime.timezone.utc).isoformat(),
        "model": model.name,
        "model_sha256": hashlib.sha256(model.read_bytes()).hexdigest(),
        "model_bytes": model.stat().st_size,
        "corpus": str(corpus),
        **corpus_metadata(corpus),
    }
    path.parent.mkdir(parents=True, exist_ok=True)
    path.write_text(json.dumps(meta, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"已写入模型元数据 {path}")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", type=Path,
                    default=Path(__file__).resolve().parent.parent / "bench/corpus")
    ap.add_argument("--out", type=Path,
                    default=Path(__file__).resolve().parent.parent / "data/char_trigram.bin")
    ap.add_argument("--meta-out", type=Path,
                    default=None,
                    help="可选：输出模型元数据 JSON（语料 hash、alpha、版本）")
    ap.add_argument("--alpha", type=float, default=0.25,
                    help="仅写入元数据，供运行时 kAlpha 对齐检查；不会改 C++ 常量")
    args = ap.parse_args()
    if not args.corpus.is_dir():
        print(f"找不到语料目录：{args.corpus}", file=sys.stderr)
        return 2
    uni, bi, tri = read_corpus(args.corpus)
    if not uni:
        print("语料为空", file=sys.stderr)
        return 2
    vocab = build_vocab(uni)
    write_model(args.out, vocab, uni, bi, tri)
    if args.meta_out:
        write_metadata(args.meta_out, args.corpus, args.out, args.alpha)
    return 0


if __name__ == "__main__":
    sys.exit(main())
