"""pinyinbench — 拼音输入法整句准确率评测框架（M0）。

设计原则：
  1. 引擎只需实现 `predict(pinyin) -> [候选]`，框架与引擎实现完全解耦
  2. 评测时引擎看不到 gold；只有 oracle 例外（用于自检框架）
  3. 每次运行都产出可回归的 CSV + JSON，改动前后可直接对比
"""
from .core import Case, Report, Row, aggregate, build_cases, load_sentences
from .engines import CliEngine, EchoEngine, Engine, OracleEngine

__all__ = [
    "Case", "Report", "Row", "aggregate", "build_cases", "load_sentences",
    "Engine", "OracleEngine", "EchoEngine", "CliEngine",
]
