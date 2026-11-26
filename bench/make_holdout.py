#!/usr/bin/env python3
"""生成 holdout 语料：从 bench/corpus 抽句，逐句做泄漏过滤后转拼音。

为什么要这个脚本：
- 现有 `cpp/eval_corpus.txt`（189 条）与 `cpp/abbr_presets.h` 是同一批提交里互相改出来的，
  `100% 首选命中` 里有 28.6% 的题面答案直接写在预设表里（见 docs/20261124-1346 分析）。
- 本脚本产出的语料必须与硬编码预设/模板/旧评测语料**零交集**，由 `leakcheck.py` 统一判定。

用法：
    python3 bench/make_holdout.py --per-domain 300 --seed 42
产物：
    bench/holdout/{news,wiki,baike,recent}.tsv   # 每行「拼音<TAB>汉字」
    bench/holdout/manifest.json                  # 抽样参数、过滤统计、sha256

注意（诚实声明）：news/wiki/baike/recent 是否与 LM 训练语料重叠，本脚本无法验证；
因此该语料用于**回归对比**（同版本间涨跌），不用于对外宣称绝对准确率。
"""

from __future__ import annotations

import argparse
import hashlib
import json
import random
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from leakcheck import LeakSources  # noqa: E402

CLEAN_RE = re.compile(r"^[\u4e00-\u9fff]+$")
ASCII_SYL = re.compile(r"^[a-z]+$")
DOMAINS = [
    ("news", "新闻"),
    ("wiki", "百科"),
    ("baike", "词条"),
    ("recent", "近期"),
]
MIN_LEN, MAX_LEN = 6, 40


def load_sentences(path: Path) -> list[str]:
    lines = []
    for raw in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        s = raw.strip()
        if MIN_LEN <= len(s) <= MAX_LEN and CLEAN_RE.match(s):
            lines.append(s)
    return lines


def to_pinyin(text: str, lazy_pinyin) -> str | None:
    """纯字母、无声调拼音。必须严格 ASCII 小写：pypinyin 传错 style 会返回带声调字符（jī），
    那种串喂给 CLI 会被当成非法输入，必须在这里拦掉。"""
    try:
        syllables = lazy_pinyin(text)
    except Exception:
        return None
    if not syllables or any((not ASCII_SYL.match(s)) for s in syllables):
        return None
    return "".join(syllables)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--repo", type=Path, default=Path(__file__).resolve().parent.parent)
    ap.add_argument("--corpus", type=Path, default=None)
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--per-domain", type=int, default=300)
    ap.add_argument("--seed", type=int, default=42)
    args = ap.parse_args()

    repo = args.repo
    corpus = args.corpus or repo / "bench" / "corpus"
    out_dir = args.out or repo / "bench" / "holdout"
    out_dir.mkdir(parents=True, exist_ok=True)

    try:
        from pypinyin import lazy_pinyin
    except ImportError:
        print("缺少 pypinyin：python3 -m pip install pypinyin（建议在项目 venv 内安装）", file=sys.stderr)
        return 2

    leaks = LeakSources(repo)
    print(f"泄漏源: {leaks.stats()}")

    manifest = {
        "seed": args.seed,
        "per_domain": args.per_domain,
        "domains": {},
        "filters": f"len {MIN_LEN}-{MAX_LEN}，纯汉字，去重，与预设/模板/旧评测语料零交集",
        "caveat": ("未对 LM 训练语料做去重，只能用于版本间回归对比，不能对外宣称绝对准确率；"
                   "多音字按 pypinyin 默认读音，未人工校验（少量句子可能因此判错）"),
    }
    total = 0
    for name, label in DOMAINS:
        src = corpus / f"{name}.txt"
        if not src.exists():
            print(f"跳过 {name}: 缺少 {src}")
            continue

        pool = load_sentences(src)
        pool = list(dict.fromkeys(pool))  # 文本去重
        random.Random(args.seed + len(name)).shuffle(pool)

        reasons = Counter()
        accepted: list[tuple[str, str]] = []
        seen_pinyin: set[str] = set()
        for text in pool:
            py = to_pinyin(text, lazy_pinyin)
            if not py:
                reasons["拼音转换失败"] += 1
                continue
            if py in seen_pinyin:
                reasons["拼音重复"] += 1
                continue
            why = leaks.reason(text, py)
            if why:
                reasons[why] += 1
                continue
            accepted.append((py, text))
            seen_pinyin.add(py)
            if len(accepted) >= args.per_domain:
                break

        content = "".join(f"{py}\t{zh}\n" for py, zh in accepted)
        path = out_dir / f"{name}.tsv"
        path.write_text(content, encoding="utf-8")

        manifest["domains"][name] = {
            "label": label,
            "source": str(src.relative_to(repo)),
            "candidates_after_dedup": len(pool),
            "accepted": len(accepted),
            "rejected": dict(reasons),
            "sha256": hashlib.sha256(content.encode("utf-8")).hexdigest(),
        }
        total += len(accepted)
        print(f"{name:6s} 接受 {len(accepted):3d} / 候选 {len(pool):5d}  拒绝原因: {dict(reasons)}")

    manifest["total"] = total
    (out_dir / "manifest.json").write_text(
        json.dumps(manifest, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(f"合计 {total} 句 → {out_dir}")
    if total < 1000:
        print(f"⚠️ 总量 {total} 低于 1000，建议提高 --per-domain 或补充语料", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    sys.exit(main())
