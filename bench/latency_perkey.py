#!/usr/bin/env python3
"""逐键延迟评测：把整句全拼拆成逐键喂给引擎，统计每键耗时。

背景：
  run_bench.py 的「延迟」是进程外 CliEngine 从「写完整串拼音」到「读回候选」
  的一次 round-trip 耗时（平均 19 字的整句约 26ms）。这等价于「整句一次喂完」，
  跟真实输入法里用户每敲一个键、引擎增量更新一次的单键成本不是一回事。

  本脚本对应 libime_bench 的 perkey 模式（cpp/libime_bench.cpp）：引擎侧逐字符
  ctx.type() 并计时，把每键耗时返回。这样量到的才是「每敲一键」的体感延迟。

用法:
    python latency_perkey.py --corpus corpus/news.txt --cli /path/to/libime_bench --limit 1000

输出:
    控制台打印汇总（用例数 / 总键数 / 平均单键 / P50 / P99），
    并在 results/ 下写一份 latency_perkey_summary.json 供回归对比。
"""
from __future__ import annotations

import argparse
import subprocess
import sys
from pathlib import Path

sys.path.insert(0, str(Path(__file__).resolve().parent))

from pinyinbench.core import (                       # noqa: E402
    dump_json,
    hanzi_to_pinyin,
    load_sentences,
    percentile,
)


class PerkeyCli:
    """驱动 libime_bench 的 perkey 子进程。

    协议：写 "<完整全拼>\\n"，读回一行 \t 分隔的毫秒数，
    每个数对应输入串里一个有效拼音键的逐键耗时。
    """

    def __init__(self, argv: list[str]):
        self._argv = list(argv)
        self._proc = subprocess.Popen(
            self._argv,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
        )

    def measure(self, pinyin: str) -> list[float]:
        assert self._proc.stdin and self._proc.stdout
        self._proc.stdin.write(pinyin + "\n")
        self._proc.stdin.flush()
        line = self._proc.stdout.readline()
        if not line:
            raise RuntimeError("perkey 引擎进程意外退出")
        return [float(x) for x in line.split() if x]

    def close(self) -> None:
        if self._proc.poll() is None:
            try:
                self._proc.stdin.close()  # type: ignore[union-attr]
            except Exception:
                pass
            self._proc.terminate()
            try:
                self._proc.wait(timeout=5)
            except subprocess.TimeoutExpired:
                self._proc.kill()


def main() -> int:
    p = argparse.ArgumentParser(description="拼音输入法逐键延迟评测（perkey）")
    p.add_argument("--corpus", default="corpus/news.txt", help="语料文件（一行一句纯汉字）")
    p.add_argument("--cli", required=True, help="libime_bench 可执行文件路径")
    p.add_argument("--cli-arg", action="append", default=[], help="传给引擎的额外参数（在 perkey 之后）")
    p.add_argument("--limit", type=int, help="只取前 N 句（默认全部）")
    p.add_argument("--min-len", type=int, default=4)
    p.add_argument("--max-len", type=int, default=30)
    p.add_argument("--out", default="results", help="结果输出目录")
    args = p.parse_args()

    sentences = load_sentences(args.corpus, args.min_len, args.max_len)
    if args.limit:
        sentences = sentences[: args.limit]
    if not sentences:
        raise SystemExit(f"语料 {args.corpus} 没有切出有效句子")

    # 引擎参数：perkey 必须是第一个参数，后面的才是 lm / dict / nbest
    engine = PerkeyCli([args.cli, "perkey"] + args.cli_arg)
    lats: list[float] = []
    try:
        for i, s in enumerate(sentences, 1):
            py = hanzi_to_pinyin(s)
            # 全拼拼音串按有效字母数生成，正常不会出现非法字符；
            # 若个别字转不出拼音会得到空串，跳过，避免污染键数与延迟统计。
            if not py:
                continue
            lats.extend(engine.measure(py))
    finally:
        engine.close()

    if not lats:
        raise SystemExit("没有采集到任何逐键耗时")

    n_sent = len([1 for s in sentences if hanzi_to_pinyin(s)])
    mean_ms = sum(lats) / len(lats)
    summary = {
        "engine": "libime_perkey",
        "corpus": args.corpus,
        "sentences": n_sent,
        "keys": len(lats),
        "mean_ms": round(mean_ms, 3),
        "p50_ms": round(percentile(lats, 50), 3),
        "p99_ms": round(percentile(lats, 99), 3),
    }
    print(f"引擎: libime_perkey    语料: {args.corpus}")
    print(f"  句数               {summary['sentences']}")
    print(f"  总键数             {summary['keys']}")
    print(f"  平均单键           {summary['mean_ms']:.2f} ms")
    print(f"  延迟 P50 / P99     {summary['p50_ms']:.2f} / {summary['p99_ms']:.2f} ms")

    out = Path(args.out)
    out.mkdir(parents=True, exist_ok=True)
    summary_path = out / "latency_perkey_summary.json"
    dump_json(summary_path, summary)
    print(f"  汇总              {summary_path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())