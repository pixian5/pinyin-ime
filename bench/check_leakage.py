#!/usr/bin/env python3
"""独立复核 holdout 语料：与预设/模板/旧评测语料零交集 + 格式自检。

与 `make_holdout.py` 共用 `leakcheck.py` 的判定逻辑，但**重新从磁盘读语料**，
不依赖生成过程的中间状态——生成时漏掉的，这里会报出来。

用法：
    python3 bench/check_leakage.py [--holdout bench/holdout]
退出码：0 干净；1 发现违规。
"""

from __future__ import annotations

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))
from leakcheck import LeakSources  # noqa: E402

CJK_ONLY = re.compile(r"^[\u4e00-\u9fff]+$")
ALPHA_ONLY = re.compile(r"^[a-z]+$")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--holdout", type=Path, default=None)
    args = ap.parse_args()

    repo = Path(__file__).resolve().parent.parent
    holdout = args.holdout or repo / "bench" / "holdout"
    leaks = LeakSources(repo)

    violations: list[dict] = []
    stats: Counter = Counter()
    seen_pinyin: dict[str, str] = {}
    seen_text: dict[str, str] = {}

    for tsv in sorted(holdout.glob("*.tsv")):
        domain = tsv.stem
        for lineno, line in enumerate(tsv.read_text(encoding="utf-8").splitlines(), 1):
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) != 2:
                violations.append({"domain": domain, "line": lineno, "kind": "格式非 2 列"})
                continue
            py, zh = parts
            stats[domain] += 1

            if not ALPHA_ONLY.match(py):
                violations.append({"domain": domain, "line": lineno, "kind": "拼音含非法字符", "value": py})
            if not CJK_ONLY.match(zh):
                violations.append({"domain": domain, "line": lineno, "kind": "期望文本非纯汉字", "value": zh})
            if py in seen_pinyin and seen_pinyin[py] != domain:
                violations.append({"domain": domain, "line": lineno, "kind": "跨域拼音重复", "value": py})
            if zh in seen_text:
                violations.append({"domain": domain, "line": lineno, "kind": "文本重复", "value": zh})
            seen_pinyin[py] = domain
            seen_text[zh] = domain

            why = leaks.reason(zh, py)
            if why:
                violations.append({"domain": domain, "line": lineno, "kind": why, "pinyin": py, "text": zh})

    report = {
        "holdout": str(holdout),
        "leak_sources": leaks.stats(),
        "counts": dict(stats),
        "total": sum(stats.values()),
        "violations": len(violations),
        "samples": violations[:50],
    }
    out = holdout / "leakage_report.json"
    out.write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    print(f"语料: {sum(stats.values())} 句 {dict(stats)}")
    print(f"泄漏源: {leaks.stats()}")
    if violations:
        print(f"❌ 发现 {len(violations)} 条违规，明细见 {out}")
        for v in violations[:10]:
            print("  -", v)
        return 1
    print(f"✅ 零泄漏、格式自检通过（报告 {out}）")
    return 0


if __name__ == "__main__":
    sys.exit(main())
