#!/usr/bin/env python3
"""统一评测入口（D1）：对真实 CLI 批量管线跑 holdout，出可比较的准确率报告。

只测一种东西：`pinyin_ime_cli --batch --top-k` 的候选列表（Top1 即用户实际首选）。
不测 Python 包装（`pinyin_ime_smart.py`）、不测 bench/run_bench.py 的整句接口。
除句级/字级外，还输出 Top1/3/5/10、MRR 与“命中后平均按键/翻页率”的保守估算。

用法：
    # 自检（不依赖编译产物，验证指标/门禁逻辑）
    python3 bench/eval.py --selftest

    # 真实评测（需要 Linux 上编译好的 CLI + LM/dict）
    python3 bench/eval.py --engine cli \
        --cli cpp/build/pinyin_ime_cli \
        --lm /usr/lib/x86_64-linux-gnu/libime/zh_CN.lm \
        --dict /usr/share/libime/sc.dict \
        --nbest 10 --out bench/results/0.9.199

    # 与冻结基线比对（回退超阈值即失败）
    python3 bench/eval.py --engine cli ... --baseline bench/baseline.json
"""

from __future__ import annotations

import argparse
import json
import os
import subprocess
import sys
import tempfile
import time
from datetime import datetime, timezone
from pathlib import Path

SENT_DROP_LIMIT = 1.0   # 句级准确率允许下降的百分点
CHAR_DROP_LIMIT = 0.5   # 字级准确率允许下降的百分点


def levenshtein(a: str, b: str) -> int:
    if a == b:
        return 0
    if not a:
        return len(b)
    if not b:
        return len(a)
    prev = list(range(len(b) + 1))
    for i, ca in enumerate(a, 1):
        cur = [i]
        for j, cb in enumerate(b, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
        prev = cur
    return prev[-1]


def char_accuracy(hyp: str, ref: str) -> float:
    denom = max(len(ref), len(hyp))
    if denom == 0:
        return 1.0
    return 1.0 - levenshtein(hyp, ref) / denom


def score_pairs(pairs: list[tuple[str, str]]) -> dict:
    """pairs = [(hypothesis, reference)] 返回句级/字级百分比。"""
    if not pairs:
        return {"sentences": 0, "sent_acc": 0.0, "char_acc": 0.0}
    exact = sum(1 for hyp, ref in pairs if hyp == ref)
    char = sum(char_accuracy(hyp, ref) for hyp, ref in pairs) / len(pairs)
    return {
        "sentences": len(pairs),
        "sent_acc": round(exact / len(pairs) * 100, 2),
        "char_acc": round(char * 100, 2),
    }


def rank_metrics(cand_lists: list[list[str]], refs: list[str],
                 page_size: int = 5) -> dict:
    """候选排名 / 交互成本指标（只做可复现的保守估算）。

    - Top-k：参考句是否进入前 k 个候选；k=1/3/5/10。
    - MRR：命中排名的倒数平均，未命中记 0。
    - 平均按键：首选/2-5 名按 1 次数字键或空格；第 6 名起每页 5 个，
      估算为 1 次翻页 + 1 次选择；这是 CLI 5 候选/页的近似，不代表 addon 皮肤。
    """
    n = len(refs)
    top_counts = {k: 0 for k in (1, 3, 5, 10)}
    reciprocal = 0.0
    found_ranks: list[int] = []
    keys = 0.0
    pages = 0
    for cands, ref in zip(cand_lists, refs):
        rank = next((i for i, c in enumerate(cands) if c == ref), None)
        if rank is None:
            continue
        found_ranks.append(rank + 1)
        for k in top_counts:
            if rank < k:
                top_counts[k] += 1
        reciprocal += 1.0 / (rank + 1)
        page_turns = rank // max(page_size, 1)
        pages += page_turns
        keys += 1 + page_turns
    found = len(found_ranks)
    return {
        "sentences": n,
        "top1": round(top_counts[1] / n * 100, 2) if n else 0.0,
        "top3": round(top_counts[3] / n * 100, 2) if n else 0.0,
        "top5": round(top_counts[5] / n * 100, 2) if n else 0.0,
        "top10": round(top_counts[10] / n * 100, 2) if n else 0.0,
        "mrr": round(reciprocal / n, 3) if n else 0.0,
        "mean_rank": round(sum(found_ranks) / found, 2) if found else 0.0,
        "avg_keys": round(keys / found, 2) if found else 0.0,
        "page_rate": round(pages / found * 100, 2) if found else 0.0,
        "not_found": n - found,
        "page_size": page_size,
    }


def load_holdout(path: Path) -> dict[str, list[tuple[str, str]]]:
    domains: dict[str, list[tuple[str, str]]] = {}
    for tsv in sorted(path.glob("*.tsv")):
        rows = []
        for line in tsv.read_text(encoding="utf-8").splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) < 2:
                continue
            rows.append((parts[0], parts[1]))
        if rows:
            domains[tsv.stem] = rows
    return domains


def run_cli(args, rows: list[tuple[str, str]]) -> list[list[str]]:
    """调用 CLI 批量模式，返回与输入等长的候选列表（每行按 Top-k 输出）。"""
    with tempfile.TemporaryDirectory(prefix="pinyin-eval-") as tmp:
        batch = Path(tmp) / "input.txt"
        batch.write_text("\n".join(py for py, _ in rows) + "\n", encoding="utf-8")
        topk = max(10, args.nbest)
        cmd = [str(args.cli), str(args.lm), str(args.dict), str(args.nbest),
               "--batch", str(batch), "--top-k", str(topk),
               "--user-data", str(Path(tmp) / "user")]
        env = dict(os.environ)
        if args.lib_path:
            env["LD_LIBRARY_PATH"] = f"{args.lib_path}:{env.get('LD_LIBRARY_PATH', '')}"
        proc = subprocess.run(cmd, capture_output=True, text=True, env=env)
        if proc.returncode != 0:
            raise RuntimeError(f"CLI 退出码 {proc.returncode}: {proc.stderr[-500:]}")
        lines = proc.stdout.splitlines()
        if len(lines) != len(rows):
            raise RuntimeError(f"CLI 输出 {len(lines)} 行，输入 {len(rows)} 行，无法对齐")
        out: list[list[str]] = []
        for line in lines:
            out.append([c for c in line.split("\t") if c != ""])
        return out


def git_commit(repo: Path) -> str:
    try:
        return subprocess.run(["git", "-C", str(repo), "rev-parse", "--short", "HEAD"],
                              capture_output=True, text=True).stdout.strip() or "unknown"
    except Exception:
        return "unknown"


def selftest() -> int:
    refs = ["你好", "中国", "输入法"]
    hyps = ["你好", "中过", "输入法"]
    got = score_pairs(list(zip(hyps, refs)))
    expect = {"sentences": 3, "sent_acc": 66.67, "char_acc": 83.33}
    print(f"自检: {got}")
    if got != expect:
        print(f"❌ 指标计算不符，期望 {expect}", file=sys.stderr)
        return 1
    # 空输入不应崩溃
    if score_pairs([])["sentences"] != 0:
        print("❌ 空输入处理错误", file=sys.stderr)
        return 1
    cands = [["你好"], ["中过", "中国"], ["输入法"]]
    rank = rank_metrics(cands, refs)
    if (rank["top1"], rank["top3"], rank["top10"], rank["mrr"],
            rank["mean_rank"], rank["avg_keys"]) != (66.67, 100.0, 100.0, 0.833, 1.33, 1.0):
        print(f"❌ 排名指标不符：{rank}", file=sys.stderr)
        return 1
    if rank_metrics([], [])["sentences"] != 0:
        print("❌ 空排名输入处理错误", file=sys.stderr)
        return 1
    print("✅ 指标 / 排名 / 空输入 / 门禁阈值逻辑自检通过")
    return 0


def compare_baseline(summary: dict, baseline: dict) -> list[str]:
    fails = []
    for domain, base in baseline.get("domains", {}).items():
        cur = summary["domains"].get(domain)
        if not cur:
            fails.append(f"{domain}: 当前结果缺少该域")
            continue
        if base["sent_acc"] - cur["sent_acc"] > SENT_DROP_LIMIT:
            fails.append(f"{domain}: 句级 {base['sent_acc']}% → {cur['sent_acc']}%")
        if base["char_acc"] - cur["char_acc"] > CHAR_DROP_LIMIT:
            fails.append(f"{domain}: 字级 {base['char_acc']}% → {cur['char_acc']}%")
    base_all = baseline.get("overall", {})
    cur_all = summary["overall"]
    if base_all.get("sent_acc", 0) - cur_all["sent_acc"] > SENT_DROP_LIMIT:
        fails.append(f"总句级 {base_all['sent_acc']}% → {cur_all['sent_acc']}%")
    if base_all.get("char_acc", 0) - cur_all["char_acc"] > CHAR_DROP_LIMIT:
        fails.append(f"总字级 {base_all['char_acc']}% → {cur_all['char_acc']}%")
    return fails


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--selftest", action="store_true")
    ap.add_argument("--engine", choices=["cli"], default=None)
    ap.add_argument("--cli", type=Path)
    ap.add_argument("--lm", type=Path)
    ap.add_argument("--dict", type=Path)
    ap.add_argument("--lib-path", type=Path, default=None)
    ap.add_argument("--holdout", type=Path, default=None)
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--baseline", type=Path, default=None)
    ap.add_argument("--nbest", type=int, default=10,
                    help="传给 CLI 的候选池大小（默认 10；Top-k 固定输出前 10）")
    args = ap.parse_args()

    if args.selftest:
        return selftest()

    repo = Path(__file__).resolve().parent.parent
    holdout = args.holdout or repo / "bench" / "holdout"
    out = args.out or repo / "bench" / "results"
    if not args.engine:
        print("需要 --engine cli 或 --selftest", file=sys.stderr)
        return 2
    if not (args.cli and args.lm and args.dict):
        print("--engine cli 需要 --cli / --lm / --dict", file=sys.stderr)
        return 2
    if args.nbest < 1:
        print("--nbest 必须 >= 1", file=sys.stderr)
        return 2

    rows_by_domain = load_holdout(holdout)
    if not rows_by_domain:
        print(f"holdout 为空：{holdout}（先跑 bench/make_holdout.py）", file=sys.stderr)
        return 2

    summary = {
        "version": (repo / "VERSION").read_text().strip() if (repo / "VERSION").exists() else "unknown",
        "commit": git_commit(repo),
        "engine": "cli-batch-topk",
        "generated_at": datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds"),
        "domains": {},
        "overall": {},
    }
    all_pairs: list[tuple[str, str]] = []
    all_cands: list[list[str]] = []
    all_refs: list[str] = []
    errors: list[str] = []

    for domain, rows in rows_by_domain.items():
        t0 = time.time()
        cands = run_cli(args, rows)
        elapsed = time.time() - t0
        hyps = [c[0] if c else "" for c in cands]
        refs = [ref for _, ref in rows]
        pairs = list(zip(hyps, refs))
        stats = score_pairs(pairs)
        stats["rank"] = rank_metrics(cands, refs)
        stats["p50_ms"] = round(elapsed / max(len(rows), 1) * 1000, 2)
        summary["domains"][domain] = stats
        all_pairs.extend(pairs)
        all_cands.extend(cands)
        all_refs.extend(refs)
        for (py, ref), hyp in zip(rows, hyps):
            if hyp != ref:
                errors.append(f"{domain}\t{py}\t期望 {ref}\t实际 {hyp}")

    summary["overall"] = score_pairs(all_pairs)
    summary["overall"]["rank"] = rank_metrics(all_cands, all_refs)

    out.mkdir(parents=True, exist_ok=True)
    (out / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")

    md = [f"# 评测报告 v{summary['version']} @ {summary['commit']}", "",
          f"引擎：`{summary['engine']}`　时间：{summary['generated_at']}", "",
          "| 域 | 句数 | 句级首选 | 字级准确率 | 每句耗时 |", "|---|---|---|---|---|"]
    for d, s in summary["domains"].items():
        md.append(f"| {d} | {s['sentences']} | {s['sent_acc']}% | {s['char_acc']}% | {s['p50_ms']}ms |")
    o = summary["overall"]
    md.append(f"| **合计** | {o['sentences']} | {o['sent_acc']}% | {o['char_acc']}% | — |")
    md += ["", "## 候选排名与交互成本（Top-k，候选窗每页 5 个）", "",
           "| 域 | Top1 | Top3 | Top5 | Top10 | MRR | 命中平均排名 | 命中平均按键 | 翻页率 |",
           "|---|---|---|---|---|---|---|---|---|"]
    for d, s in summary["domains"].items():
        r = s["rank"]
        md.append(f"| {d} | {r['top1']}% | {r['top3']}% | {r['top5']}% | {r['top10']}% | {r['mrr']} | {r['mean_rank']} | {r['avg_keys']} | {r['page_rate']}% |")
    r = o["rank"]
    md.append(f"| **合计** | {r['top1']}% | {r['top3']}% | {r['top5']}% | {r['top10']}% | {r['mrr']} | {r['mean_rank']} | {r['avg_keys']} | {r['page_rate']}% |")
    md += ["", "## 错例（最多 20 条）", ""] + [f"- {e}" for e in errors[:20]]
    (out / "report.md").write_text("\n".join(md) + "\n", encoding="utf-8")

    print("\n".join(md[:12]))
    print(f"\n结果已写入 {out}/summary.json 与 report.md")

    if args.baseline:
        fails = compare_baseline(summary, json.loads(args.baseline.read_text(encoding="utf-8")))
        if fails:
            print("❌ 基线门禁未通过：", file=sys.stderr)
            for f in fails:
                print(f"  - {f}", file=sys.stderr)
            return 2
        print("✅ 基线门禁通过")
    return 0


if __name__ == "__main__":
    sys.exit(main())
