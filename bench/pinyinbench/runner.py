"""评测执行器：跑一批 case，产出逐句 CSV + 汇总指标。"""
from __future__ import annotations

import csv
from pathlib import Path

from .core import Case, Report, Row, Timer, aggregate, build_cases, dump_json, load_sentences
from .engines import Engine


def run(engine: Engine, cases: list[Case], corpus_name: str = "") -> Report:
    rows: list[Row] = []
    for case in cases:
        with Timer() as t:
            cands = engine.predict(case.pinyin)
        pred = cands[0] if cands else ""
        rows.append(Row(gold=case.gold, pred=pred, pinyin=case.pinyin, latency_ms=t.elapsed_ms))
    return Report(engine=engine.name, corpus=corpus_name, cases=len(rows),
                  metrics=aggregate(rows), rows=rows)


def cases_from_corpus(path: str, limit: int | None = None,
                      min_len: int = 4, max_len: int = 30) -> list[Case]:
    sentences = load_sentences(path, min_len=min_len, max_len=max_len)
    if limit:
        sentences = sentences[:limit]
    return build_cases(sentences)


def save(report: Report, outdir: str) -> dict[str, str]:
    out = Path(outdir)
    out.mkdir(parents=True, exist_ok=True)
    # 文件名带上语料名，否则换语料再跑会把上一次的结果覆盖掉
    corpus_stem = Path(report.corpus).stem if report.corpus else "cases"
    stem = f"{report.engine}_{corpus_stem}"
    csv_path = out / f"{stem}_cases.csv"
    json_path = out / f"{stem}_summary.json"

    with csv_path.open("w", newline="", encoding="utf-8") as f:
        w = csv.DictWriter(f, fieldnames=list(report.rows[0].as_dict().keys()))
        w.writeheader()
        for r in report.rows:
            w.writerow(r.as_dict())

    summary = {
        "engine": report.engine,
        "corpus": report.corpus,
        "metrics": report.metrics,
    }
    dump_json(json_path, summary)
    return {"csv": str(csv_path), "summary": str(json_path)}


def format_report(report: Report) -> str:
    m = report.metrics
    if not m.get("n"):
        return f"{report.engine}: 无用例"
    lines = [
        f"引擎: {report.engine}    语料: {report.corpus}",
        f"  用例数          {m['n']}",
        f"  完全准确率(句级) {m['sentence_acc'] * 100:6.2f}%",
        f"  综合准确率(字级) {m['char_acc'] * 100:6.2f}%",
        f"  平均码长        {m['keys_per_char']:.3f} 键/字",
        f"  延迟 P50 / P99  {m['latency_p50_ms']:.2f} / {m['latency_p99_ms']:.2f} ms",
    ]
    return "\n".join(lines)
