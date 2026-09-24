#!/usr/bin/env python3
"""M0 评测入口。

用法:
    python run_bench.py --corpus corpus/wiki.txt --engine oracle
    python run_bench.py --corpus corpus/wiki.txt --engine echo --limit 500
    python run_bench.py --corpus corpus/wiki.txt --cli /path/to/libime_bench

第一次跑建议先跑 oracle：它必须输出 100%，否则说明框架算错了。
"""
from __future__ import annotations

import argparse
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from pinyinbench import runner                      # noqa: E402
from pinyinbench.core import Case                   # noqa: E402
from pinyinbench.engines import CliEngine, ENGINES  # noqa: E402


def build_engine(args, cases: list[Case]):
    if args.cli:
        return CliEngine([args.cli] + args.cli_arg, name=args.cli_name or Path(args.cli).stem)
    cls = ENGINES.get(args.engine)
    if cls is None:
        raise SystemExit(f"未知引擎: {args.engine}（可选: {', '.join(ENGINES)}，或用 --cli）")
    eng = cls()
    eng.prepare(cases)          # 只有 oracle 会真正用到
    return eng


def main() -> int:
    p = argparse.ArgumentParser(description="拼音输入法整句准确率评测")
    p.add_argument("--corpus", default="corpus/wiki.txt", help="语料文件（一行一句纯汉字）")
    p.add_argument("--engine", default="oracle", help=f"内置引擎: {', '.join(ENGINES)}")
    p.add_argument("--cli", help="外部引擎可执行文件路径")
    p.add_argument("--cli-arg", action="append", default=[], help="传给外部引擎的额外参数")
    p.add_argument("--cli-name", help="外部引擎在报告中的名字")
    p.add_argument("--limit", type=int, help="只取前 N 句")
    p.add_argument("--min-len", type=int, default=4)
    p.add_argument("--max-len", type=int, default=30)
    p.add_argument("--out", default="results", help="结果输出目录")
    p.add_argument("--show", type=int, default=0, help="打印前 N 条错例")
    args = p.parse_args()

    cases = runner.cases_from_corpus(args.corpus, args.limit, args.min_len, args.max_len)
    if not cases:
        raise SystemExit(f"语料 {args.corpus} 没有切出有效句子")

    engine = build_engine(args, cases)
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
