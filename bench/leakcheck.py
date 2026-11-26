#!/usr/bin/env python3
"""泄漏审计：holdout 语料与「硬编码预设 / 句式模板 / 旧评测语料」的重叠检查。

设计原则（避免自证）：
- 本模块是**唯一**的泄漏判定实现，`make_holdout.py`（生成时过滤）与
  `check_leakage.py`（事后独立复核）都调用它，不允许各写一套。
- 判定只看可执行的事实：源码里的中文字符串字面量（预设表、模板表、联想表都在
  `cpp/*.h` 与 `cpp/*.cpp` 里）、`abbr_presets.h` 的拼音键、以及两份旧评测语料。

用法：
    python3 bench/leakcheck.py            # 打印泄漏源规模
"""

from __future__ import annotations

import re
import sys
from pathlib import Path

CJK_RE = re.compile(r"[\u4e00-\u9fff]")
LITERAL_RE = re.compile(r'"((?:[^"\\]|\\.)*)"')
PRESET_RE = re.compile(r'\{"([^"]+)",\s*"([^"]+)"')
PINYIN_RE = re.compile(r"^[a-z]+$")


def has_cjk(s: str) -> bool:
    return bool(CJK_RE.search(s))


def extract_literals(repo: Path) -> set[str]:
    """收集源码中所有含中文的字符串字面量（长度 >= 2）。

    预设、句式模板、联想词、长句预测表都以字面量形式硬编码，这里一网打尽。
    """
    out: set[str] = set()
    for path in list((repo / "cpp").glob("*.h")) + list((repo / "cpp").glob("*.cpp")) \
            + list((repo / "fcitx5-addon").glob("*.cpp")):
        try:
            text = path.read_text(encoding="utf-8", errors="ignore")
        except OSError:
            continue
        for m in LITERAL_RE.finditer(text):
            lit = m.group(1)
            if len(lit) >= 2 and has_cjk(lit):
                out.add(lit)
    return out


def load_presets(repo: Path) -> dict[str, str]:
    """解析 `abbr_presets.h` 的 {拼音, 汉字} 表。"""
    presets: dict[str, str] = {}
    path = repo / "cpp" / "abbr_presets.h"
    if not path.exists():
        return presets
    for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
        m = PRESET_RE.match(line.strip())
        if m:
            presets[m.group(1)] = m.group(2)
    return presets


def load_eval_texts(repo: Path) -> set[str]:
    """旧评测语料的期望文本 + 拼音，全部视为已污染。"""
    texts: set[str] = set()
    for name in ("eval_corpus.txt", "eval_sentences.txt"):
        path = repo / "cpp" / name
        if not path.exists():
            continue
        for line in path.read_text(encoding="utf-8", errors="ignore").splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("\t")
            for part in parts[:2]:
                if part.strip():
                    texts.add(part.strip())
    return texts


class LeakSources:
    def __init__(self, repo: Path):
        self.repo = repo
        self.literals = extract_literals(repo)
        self.presets = load_presets(repo)
        self.eval_texts = load_eval_texts(repo)
        # 只保留长度 >= 4 的字面量做“句子被模板包含”判定，短词太容易误报
        self.templates = sorted((s for s in self.literals if len(s) >= 4), key=len, reverse=True)

    def reason(self, text: str, pinyin: str) -> str | None:
        """返回泄漏原因；None 表示干净。"""
        if text in self.eval_texts:
            return "旧评测语料"
        if text in self.literals:
            return "源码字面量（整句命中）"
        if pinyin in self.presets and self.presets[pinyin] == text:
            return "预设表（拼音键直接命中）"
        for tpl in self.templates:
            if text in tpl:
                return f"句式模板包含（{tpl[:20]}…）"
        return None

    def stats(self) -> dict[str, int]:
        return {
            "literals": len(self.literals),
            "templates_ge4": len(self.templates),
            "presets": len(self.presets),
            "eval_texts": len(self.eval_texts),
        }


def main() -> int:
    repo = Path(__file__).resolve().parent.parent
    src = LeakSources(repo)
    print(f"仓库: {repo}")
    for k, v in src.stats().items():
        print(f"  {k}: {v}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
