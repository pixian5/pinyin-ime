#!/usr/bin/env python3
"""clean_corpus.py — 生成「句子级去重 + 5-gram 近似去重」的训练语料。

这不是最终的文档级切分，而是 P0 的第一步：
- 以 `bench/holdout/*.tsv` 为固定 test；
- 从 `bench/corpus/*.txt` 删除与 test 完全相同的句子；
- 再删除与任一 test 句子共享 >= N 个字符 5-gram 的训练句，降低同文档改写造成的泄漏；
- 默认排除纯评测语料 `lccc_eval.txt`、`daily.txt`、`dialogue.txt`，不拿 eval 当 train。

真正的文档级 split 需要 fetch 阶段保留 doc_id/url；当前语料已经拍平成句子，
因此 manifest 会明确标注这个限制，不能把本脚本产物说成“完全无泄漏”。

用法：
    python3 bench/clean_corpus.py --out /tmp/pinyin-clean
    python3 bench/clean_corpus.py --out /tmp/pinyin-clean --near-dup-threshold 2
    python3 bench/clean_corpus.py --out /tmp/pinyin-clean --allow-eval-sets
"""
from __future__ import annotations

import argparse
import hashlib
import json
import sys
from collections import defaultdict
from pathlib import Path

DEFAULT_CORPUS = Path(__file__).resolve().parent / "corpus"
DEFAULT_HOLDOUT = Path(__file__).resolve().parent / "holdout"
EVAL_ONLY = {"lccc_eval.txt", "daily.txt", "dialogue.txt"}


def sha256_file(path: Path) -> str:
    h = hashlib.sha256()
    with path.open("rb") as f:
        for chunk in iter(lambda: f.read(1 << 20), b""):
            h.update(chunk)
    return h.hexdigest()


def read_test_sentences(holdout: Path) -> list[str]:
    sentences: list[str] = []
    for f in sorted(holdout.glob("*.tsv")):
        for line in f.read_text(encoding="utf-8", errors="ignore").splitlines():
            parts = line.split("\t")
            if len(parts) >= 2 and parts[1].strip():
                sentences.append(parts[1].strip())
    return sentences


def ngrams(text: str, n: int = 5) -> set[str]:
    if len(text) < n:
        return {text}
    return {text[i : i + n] for i in range(len(text) - n + 1)}


def make_test_index(test_sentences: list[str], n: int = 5):
    index: dict[str, set[int]] = defaultdict(set)
    for i, sentence in enumerate(test_sentences):
        for gram in ngrams(sentence, n):
            index[gram].add(i)
    return index


def max_shared_ngrams(text: str, index: dict[str, set[int]], n: int = 5) -> int:
    counts: dict[int, int] = defaultdict(int)
    for gram in ngrams(text, n):
        for i in index.get(gram, ()):
            counts[i] += 1
    return max(counts.values()) if counts else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--corpus", type=Path, default=DEFAULT_CORPUS)
    ap.add_argument("--holdout", type=Path, default=DEFAULT_HOLDOUT)
    ap.add_argument("--out", type=Path, required=True)
    ap.add_argument("--near-dup-threshold", type=int, default=2,
                    help="与任一 test 共享多少个 5-gram 后剔除；0 表示只做 exact 去重")
    ap.add_argument("--allow-eval-sets", action="store_true",
                    help="允许把 lccc_eval/daily/dialogue 当训练语料（默认禁止）")
    args = ap.parse_args()

    if not args.corpus.is_dir():
        print(f"找不到语料目录：{args.corpus}", file=sys.stderr)
        return 2
    if not args.holdout.is_dir():
        print(f"找不到 holdout 目录：{args.holdout}", file=sys.stderr)
        return 2
    if args.near_dup_threshold < 0:
        print("--near-dup-threshold 不能为负", file=sys.stderr)
        return 2

    test = read_test_sentences(args.holdout)
    if not test:
        print("holdout 为空", file=sys.stderr)
        return 2
    test_set = set(test)
    index = make_test_index(test)
    threshold = args.near_dup_threshold or 10**9

    args.out.mkdir(parents=True, exist_ok=True)
    manifest_files = []
    total_kept = 0
    total_exact_removed = 0
    total_near_removed = 0
    skipped = []

    for f in sorted(args.corpus.glob("*.txt")):
        if not args.allow_eval_sets and f.name in EVAL_ONLY:
            skipped.append(f.name)
            continue
        kept: list[str] = []
        exact_removed = 0
        near_removed = 0
        for line in f.read_text(encoding="utf-8", errors="ignore").splitlines():
            sentence = line.strip()
            if not sentence:
                continue
            if sentence in test_set:
                exact_removed += 1
                continue
            if args.near_dup_threshold and max_shared_ngrams(sentence, index) >= threshold:
                near_removed += 1
                continue
            kept.append(sentence)
        if not kept:
            continue
        out_file = args.out / f.name
        out_file.write_text("\n".join(kept) + "\n", encoding="utf-8")
        manifest_files.append({
            "file": f.name,
            "source_sha256": sha256_file(f),
            "output_sha256": sha256_file(out_file),
            "sentences": len(kept),
            "chars": sum(len(s) for s in kept),
            "exact_removed": exact_removed,
            "near_dup_removed": near_removed,
        })
        total_kept += len(kept)
        total_exact_removed += exact_removed
        total_near_removed += near_removed

    if not manifest_files:
        print("清洗后没有训练语料", file=sys.stderr)
        return 2

    manifest = {
        "kind": "pinyin-ime-clean-train-v1",
        "caveat": "句子级/5-gram 近似去重，不是文档级切分；同文档剩余句子仍可能残留风格泄漏",
        "corpus": str(args.corpus),
        "holdout": str(args.holdout),
        "near_dup_threshold": args.near_dup_threshold,
        "eval_only_skipped": skipped,
        "test_sentences": len(test),
        "test_sha256": hashlib.sha256("\n".join(test).encode("utf-8")).hexdigest(),
        "train_sentences": total_kept,
        "train_chars": sum(x["chars"] for x in manifest_files),
        "exact_removed": total_exact_removed,
        "near_dup_removed": total_near_removed,
        "files": manifest_files,
    }
    (args.out / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
    )
    print(
        f"训练语料已写入 {args.out}：{total_kept} 句 / "
        f"{sum(x['chars'] for x in manifest_files)} 字；"
        f"exact 剔除 {total_exact_removed}，5-gram 近似剔除 {total_near_removed}，"
        f"跳过 eval-only {len(skipped)} 个"
    )
    print("注意：这是句子级/近似去重，文档级 split 仍待 fetch 阶段补 doc_id。")
    return 0


if __name__ == "__main__":
    sys.exit(main())
