#!/usr/bin/env python3
"""简拼（首字母输入）整句准确率评测。

把每个汉字转成拼音首字母（如「北京大学」→ bjdx），喂给 libime 引擎，对比整句还原率。
与全拼评测（run_bench.py）的唯一区别是输入口径：全拼 → 简拼（每音节取首字母）。

用途：量化「只打首字母」这一输入方式能省多少键、损失多少准确率，作为简拼能力的可回归基线。

用法:
    python3 jianpin_eval.py --corpus corpus/news.txt --cli ../cpp/build/libime_bench \
      --cli-arg <语言模型路径> --cli-arg <系统词典路径> --cli-arg 5

安全说明：本脚本只读语料用于切句与生成简拼 gold，输出仅为聚合指标；请勿用 --show 打印
逐句错例（会把语料文本带入终端），默认关闭。
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from pinyinbench import runner
from pinyinbench.core import Case, hanzi_to_jianpin, load_sentences
from pinyinbench.engines import CliEngine


def build_jianpin_cases(path: str, limit: int | None = None,
                        min_len: int = 4, max_len: int = 30) -> list[Case]:
    sentences = load_sentences(path, min_len=min_len, max_len=max_len)
    if limit:
        sentences = sentences[:limit]
    return [Case(gold=s, pinyin=hanzi_to_jianpin(s)) for s in sentences]


def main() -> int:
    p = argparse.ArgumentParser(description="简拼（首字母输入）整句准确率评测")
    p.add_argument("--corpus", default="corpus/news.txt", help="语料文件（一行一句纯汉字）")
    p.add_argument("--cli", required=True, help="外部引擎可执行文件路径（libime_bench）")
    p.add_argument("--cli-arg", action="append", default=[], help="传给外部引擎的额外参数")
    p.add_argument("--limit", type=int, help="只取前 N 句")
    p.add_argument("--min-len", type=int, default=4)
    p.add_argument("--max-len", type=int, default=30)
    p.add_argument("--out", default="results", help="结果输出目录")
    p.add_argument("--show", type=int, default=0, help="打印前 N 条错例（会带入语料文本，慎用）")
    args = p.parse_args()

    cases = build_jianpin_cases(args.corpus, args.limit, args.min_len, args.max_len)
    if not cases:
        raise SystemExit(f"语料 {args.corpus} 没有切出有效句子")

    engine = CliEngine([args.cli] + args.cli_arg, name=Path(args.cli).stem + "_jianpin")
    try:
        report = runner.run(engine, cases, corpus_name=args.corpus)
    finally:
        engine.close()

    print(runner.format_report(report))
    paths = runner.save(report, args.out)
    print(f"  逐句结果        {paths['csv']}")
    print(f"  汇总            {paths['summary']}")

    if args.show:
        bad = [r for r in report.rows if not r.sentence_correct][: args.show]
        print("\n错例（gold -> pred）:")
        for r in bad:
            print(f"  {r.gold}  ->  {r.pred or '(空)'}   [{r.pinyin}]")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())