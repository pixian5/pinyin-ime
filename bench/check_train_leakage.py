#!/usr/bin/env python3
"""check_train_leakage.py — 校验清洗后的训练语料没有 test 泄漏。

检查项：
1. 训练目录不得包含纯评测文件名（lccc_eval/daily/dialogue）；
2. 训练句与 test 句不得逐字相同；
3. 训练句与任一 test 句不得共享 >= threshold 个字符 5-gram。

注意：这是句子级/近似去重检查，不是文档级切分。通过只代表“没有逐句原文
和明显 5-gram 重复”，不代表同文档风格泄漏已经消失。

用法：
    python3 bench/check_train_leakage.py --train /tmp/pinyin-clean
    python3 bench/check_train_leakage.py --train /tmp/pinyin-clean --json-out /tmp/leak.json
"""
from __future__ import annotations

import argparse
import json
import sys
from collections import defaultdict
from pathlib import Path

DEFAULT_HOLDOUT = Path(__file__).resolve().parent / "holdout"
EVAL_ONLY = {"lccc_eval.txt", "daily.txt", "dialogue.txt"}


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


def max_shared(text: str, index: dict[str, set[int]], n: int = 5) -> int:
    counts: dict[int, int] = defaultdict(int)
    for gram in ngrams(text, n):
        for i in index.get(gram, ()):
            counts[i] += 1
    return max(counts.values()) if counts else 0


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--train", type=Path, required=True)
    ap.add_argument("--holdout", type=Path, default=DEFAULT_HOLDOUT)
    ap.add_argument("--near-dup-threshold", type=int, default=2)
    ap.add_argument("--json-out", type=Path)
    args = ap.parse_args()

    if not args.train.is_dir():
        print(f"找不到训练目录：{args.train}", file=sys.stderr)
        return 2
    if not args.holdout.is_dir():
        print(f"找不到 holdout 目录：{args.holdout}", file=sys.stderr)
        return 2

    test = read_test_sentences(args.holdout)
    if not test:
        print("holdout 为空", file=sys.stderr)
        return 2
    test_set = set(test)
    index: dict[str, set[int]] = defaultdict(set)
    for i, sentence in enumerate(test):
        for gram in ngrams(sentence):
            index[gram].add(i)

    bad_eval_files = []
    exact_hits = []
    near_hits = []
    train_sentences = 0
    for f in sorted(args.train.glob("*.txt")):
        if f.name in EVAL_ONLY:
            bad_eval_files.append(f.name)
        for line in f.read_text(encoding="utf-8", errors="ignore").splitlines():
            sentence = line.strip()
            if not sentence:
                continue
            train_sentences += 1
            if sentence in test_set:
                exact_hits.append((f.name, sentence))
                continue
            if max_shared(sentence, index) >= args.near_dup_threshold:
                near_hits.append((f.name, sentence))

    report = {
        "kind": "pinyin-ime-train-leakage-check-v1",
        "train_dir": str(args.train),
        "holdout": str(args.holdout),
        "train_sentences": train_sentences,
        "test_sentences": len(test),
        "near_dup_threshold": args.near_dup_threshold,
        "eval_only_files": bad_eval_files,
        "exact_hits": len(exact_hits),
        "near_dup_hits": len(near_hits),
        "exact_examples": [x[1] for x in exact_hits[:5]],
        "near_dup_examples": [x[1] for x in near_hits[:5]],
        "caveat": "句子级/5-gram 近似去重；不是文档级切分",
    }
    if args.json_out:
        args.json_out.parent.mkdir(parents=True, exist_ok=True)
        args.json_out.write_text(
            json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8"
        )

    print(
        f"train={train_sentences} test={len(test)} "
        f"exact={len(exact_hits)} near_dup={len(near_hits)} "
        f"eval_only={len(bad_eval_files)}"
    )
    if bad_eval_files or exact_hits or near_hits:
        if bad_eval_files:
            print(f"!! 训练目录包含 eval-only 文件：{bad_eval_files}", file=sys.stderr)
        if exact_hits:
            print(f"!! 仍有 {len(exact_hits)} 句逐字相同", file=sys.stderr)
        if near_hits:
            print(
                f"!! 仍有 {len(near_hits)} 句与 test 共享 >= "
                f"{args.near_dup_threshold} 个 5-gram",
                file=sys.stderr,
            )
        return 1
    print("训练语料泄漏检查通过（句子级 + 5-gram 近似）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
