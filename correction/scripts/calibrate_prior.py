#!/usr/bin/env python3
"""从中文语料统计音节频度，生成 correction/prior.py 的代价表。

用法:
    python3 -m correction.scripts.calibrate_prior          # 用默认语料
    python3 -m correction.scripts.calibrate_prior --corpus /path/to/corpus.txt
    python3 -m correction.scripts.calibrate_prior --alpha 0.04  # 调整缩放

输出: 打印代价表 + 写入 /tmp/prior_cost_table.py（可直接粘贴到 prior.py）

设计见 prior.py 模块文档：先验范围须 < 信道最小差 0.35，只做 tie-break。
"""
from __future__ import annotations

import argparse
import json
import math
import re
from collections import Counter
from pathlib import Path

# 延迟导入：pypinyin 可能未安装（只在校准时需要）
def _get_pypinyin():
    try:
        from pypinyin import lazy_pinyin
        return lazy_pinyin
    except ImportError:
        raise SystemExit("需要 pypinyin: pip install pypinyin")


def main() -> int:
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("--corpus", nargs="*", default=None,
                   help="语料文件路径（默认 bench/corpus/wiki.txt + news.txt）")
    p.add_argument("--alpha", type=float, default=0.035,
                   help="缩放因子（默认 0.035，先验跨度 ≈0.30）")
    p.add_argument("--smooth", type=float, default=0.5,
                   help="未覆盖音节的平滑频度（默认 0.5）")
    p.add_argument("--out", default="/tmp/prior_cost_table.py",
                   help="输出文件路径")
    args = p.parse_args()

    lazy_pinyin = _get_pypinyin()

    # 确定语料文件
    if args.corpus:
        corpus_files = [Path(f) for f in args.corpus]
    else:
        # 默认：项目内 bench/corpus/ 下的 wiki.txt + news.txt
        project_root = Path(__file__).resolve().parents[2]
        corpus_files = [
            project_root / "bench" / "corpus" / "wiki.txt",
            project_root / "bench" / "corpus" / "news.txt",
        ]

    # 读取并合并语料
    texts = []
    for f in corpus_files:
        if f.exists():
            texts.append(f.read_text(encoding="utf-8"))
            print(f"语料: {f} ({f.stat().st_size} bytes)")
        else:
            print(f"警告: 语料不存在: {f}")
    if not texts:
        raise SystemExit("无可用语料")
    text = re.sub(r"[^\u4e00-\u9fff]", "", "".join(texts))
    print(f"汉字总数: {len(text)}")

    # 转拼音 + 统计频度
    from correction.syllables import SYLLABLES, SYLLABLE_SET
    pinyins = lazy_pinyin(text)
    freq = Counter(pinyins)
    total = sum(freq.values())
    print(f"拼音总数: {total}")

    covered = sum(1 for s in SYLLABLES if freq.get(s, 0) > 0)
    print(f"420 音节覆盖: {covered}/420")

    # 构建代价表
    freq_table = {s: max(freq.get(s, 0), args.smooth) for s in SYLLABLES}
    costs = {s: round(args.alpha * (-math.log(f / total)), 4) for s, f in freq_table.items()}

    cost_range = max(costs.values()) - min(costs.values())
    print(f"\nalpha={args.alpha}, 代价范围: {min(costs.values()):.3f} ~ {max(costs.values()):.3f}")
    print(f"跨度: {cost_range:.3f} (信道最小差 0.35, 比例: {cost_range/0.35:.0%})")

    # top/bottom
    sorted_items = sorted(costs.items(), key=lambda x: x[1])
    print(f"最常见: {sorted_items[0]}")
    print(f"最罕见: {sorted_items[-1]}")
    print(f"中位数: {sorted_items[210]}")

    # 写出 Python 代码片段
    out = Path(args.out)
    with open(out, "w") as f:
        f.write(f"FREQ_TOTAL = {total}\n")
        f.write(f"ALPHA = {args.alpha:.4f}\n")
        f.write("_COST_TABLE = {\n")
        for s in SYLLABLES:
            f.write(f"    {s!r}: {costs[s]},\n")
        f.write("}\n")
    print(f"\n已写入 {out}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
