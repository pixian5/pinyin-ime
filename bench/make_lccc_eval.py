#!/usr/bin/env python3
"""从 fetch_lccc.py 的清洗产物中，确定性抽取「口语留出评测集」（~2000 句）。

为什么需要它：主评测集 corpus/news.txt 是书面语；sentence_eval 已暴露「对话域
纠错最弱」，而口语恰是输入法最该强的场景。若只在新闻上评测，训练口语语料带来的
收益会被测偏。本脚本从 LCCC 清洗产物里抽一份 spoken 域评测集，与 news.txt 同向
可比：

    # 1) 先清洗（见 fetch_lccc.py）
    python fetch_lccc.py --out /workspace/native-build/corpus/lccc_full.txt
    # 2) 再抽评测集
    python make_lccc_eval.py --src /workspace/native-build/corpus/lccc_full.txt \
        --out corpus/lccc_eval.txt
    # 3) 评测（LM 用 run_bench.py，纠错用 correction.sentence_eval）

抽取纪律：按句子 md5 确定性筛选（可复现、无随机种子漂移）。注意本评测集与训练集
同源，训练时理论上应剔除这些句子（泄露纪律见 ab_eval.sh 的 --exclude-corpus；
此处 2000/数千万 重叠可忽略，但严谨起见全量训练宜保留剔除开关）。

用法:
    python make_lccc_eval.py --src <cleaned.txt> --out corpus/lccc_eval.txt --target 2000
"""
from __future__ import annotations

import argparse
import hashlib
import sys
from pathlib import Path


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--src", required=True, help="fetch_lccc.py 的清洗产物（句子/行）")
    ap.add_argument("--out", default="corpus/lccc_eval.txt")
    ap.add_argument("--target", type=int, default=2000)
    ap.add_argument("--mod", type=int, default=15000,
                    help="hash 取模，控制抽样密度（越小样本越多）")
    args = ap.parse_args()

    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    kept = 0
    seen = 0
    with open(args.src, encoding="utf-8") as f, open(out, "w", encoding="utf-8") as o:
        for line in f:
            line = line.rstrip("\n")
            if not line:
                continue
            seen += 1
            h = int(hashlib.md5(line.encode("utf-8")).hexdigest()[:6], 16)
            if h % args.mod == 0:
                o.write(line + "\n")
                kept += 1
                if kept >= args.target:
                    break

    print(f"[done] 抽取 {kept} 句（扫描 {seen} 句，target={args.target}）-> {out}",
          file=sys.stderr)
    if kept == 0:
        print("!! 一句都没抽到，检查 --src 是否为空或 --mod 过大", file=sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())