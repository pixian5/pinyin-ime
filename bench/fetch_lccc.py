#!/usr/bin/env python3
"""下载并清洗 LCCC 中文对话语料，输出「句子/行」纯文本，供 LM 训练与纠错语料。

为什么是 LCCC：sentence_eval 实测「对话域最弱」——口语恰是输入法最该强的
场景，而现有训练语料（新闻/百科）缺口语。LCCC（Large-scale Cleaned Chinese
Conversation）是中文开放域对话的标杆来源，用来补日常口语/对话的洞
（对应 README 待开发目标 #5、PLAN-A.md §4）。

数据格式：`LCCD.json` 是嵌套数组 `[对话[句,...], ...]`，每句**已按词分词**
（空格分隔）。本脚本把每句还原成**无空格的纯文本**——分词交给下游的
`scdict_segment.py`（前向最大匹配），保证训练出的 LM 词表 ⊆ sc.dict，栅格
才与解码器兼容（见 lm/README.md）。若直接沿用 LCCC 自带分词，词粒度与
sc.dict 不一致，LM 里会大量 <unk>。

清洗规则（针对 LCCC 实测噪音）：
- 去空格还原纵文；剔除替换字符 U+FFFD 与控制字符；
- 按汉字占比 + 句长过滤掉整段噪音/过短/过长句。

用法:
    # 小样本验证（默认不限，先跑 5000 句看质量）
    python fetch_lccc.py --out corpus/lccc.txt --max-utterances 5000

    # 全量（LCCC-large 约 1200 万轮对话，产出大文件，按需）
    python fetch_lccc.py --out corpus/lccc.txt

依赖: modelscope（下载）、ijson（流式解析）。
许可提示: LCCC 原始许可为自定义（基于微博语料，限研究用途），仅用于评测与
技术验证；训练出的 LM 若要分发，需另行评估语料许可（见 ANALYSIS.md）。
"""
from __future__ import annotations

import argparse
import re
import sys
import zipfile
from pathlib import Path

import ijson

DATASET = "OmniData/LCCC"
ZIP_MEMBER = "raw/LCCC-large.zip"
JSON_MEMBER = "LCCD.json"

# 汉字占比用的正则（过滤纯数字/符号/外文噪音）
_HAS_CJK = re.compile(r"[\u4e00-\u9fff]")


def ensure_zip(data_dir: Path) -> Path:
    """下载 LCCC-large.zip（modelscope），已存在则跳过。"""
    data_dir.mkdir(parents=True, exist_ok=True)
    zip_path = data_dir / ZIP_MEMBER
    if zip_path.exists() and zip_path.stat().st_size > 0:
        print(f"[fetch] {zip_path} 已存在 ({zip_path.stat().st_size} bytes)，跳过下载",
              file=sys.stderr)
        return zip_path

    print(f"[fetch] 下载 {DATASET} -> {zip_path} ...", file=sys.stderr)
    from modelscope import snapshot_download

    snapshot_download(
        DATASET,
        repo_type="dataset",
        allow_file_pattern=ZIP_MEMBER,
        local_dir=str(data_dir),
    )
    if not zip_path.exists():
        raise SystemExit(f"下载后未找到 {zip_path}")
    return zip_path


def cjk_ratio(text: str) -> float:
    if not text:
        return 0.0
    return len(_HAS_CJK.findall(text)) / len(text)


def clean(text: str, min_len: int, max_len: int) -> str | None:
    """清洗单条句子：去空格还原纵文、剔除坏字符，返回无空格纯文本或 None。"""
    # 替换字符（LCCC 编码损坏）+ 控制字符
    text = text.replace("\ufffd", "")
    text = re.sub(r"[\x00-\x1f\x7f]", "", text)
    # 去所有空白（把预分词的空格还原成纯文本）
    text = re.sub(r"\s+", "", text)
    if not (min_len <= len(text) <= max_len):
        return None
    if cjk_ratio(text) < 0.5:
        return None
    return text


def main() -> int:
    ap = argparse.ArgumentParser(description="下载并清洗 LCCC 语料为句子/行纯文本")
    ap.add_argument("--out", default="corpus/lccc.txt")
    ap.add_argument("--data-dir", default="../../native-build/corpus/lccc")
    ap.add_argument("--max-utterances", type=int, default=0,
                    help="0=不限（全量）；>0=N 句后停止（小样本验证）")
    ap.add_argument("--min-len", type=int, default=3)
    ap.add_argument("--max-len", type=int, default=50)
    args = ap.parse_args()

    zip_path = ensure_zip(Path(args.data_dir))
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)

    kept = 0
    seen = 0
    with zipfile.ZipFile(zip_path) as zf, \
         zf.open(JSON_MEMBER) as fh, \
         open(out, "w", encoding="utf-8", buffering=1 << 18) as of:
        # 顶层是 [对话[句,...], ...]；item 依次产出每个对话（list[str]）
        for dialogue in ijson.items(fh, "item"):
            if not isinstance(dialogue, list):
                continue
            for utter in dialogue:
                if not isinstance(utter, str):
                    continue
                seen += 1
                c = clean(utter, args.min_len, args.max_len)
                if c is None:
                    continue
                of.write(c + "\n")
                kept += 1
                if args.max_utterances > 0 and kept >= args.max_utterances:
                    print(f"[done] 已到 --max-utterances={args.max_utterances}，提前停止",
                          file=sys.stderr)
                    print(f"[stat] 看过 {seen} 句，保留 {kept} 句 -> {out}",
                          file=sys.stderr)
                    return 0

    print(f"[done] 全量清洗完成：看过 {seen} 句，保留 {kept} 句 -> {out}",
          file=sys.stderr)
    print(f"[stat] 保留率 {kept / max(seen, 1) * 100:.1f}%", file=sys.stderr)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())