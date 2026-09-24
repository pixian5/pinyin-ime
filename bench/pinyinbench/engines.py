"""引擎适配器。

所有引擎只做一件事：给一串连续全拼，返回候选列表（第一个即首选）。
这样评测框架与引擎实现完全解耦——libime、librime、自研引擎都只要实现这个接口。
"""
from __future__ import annotations

import subprocess
from abc import ABC, abstractmethod
from typing import Sequence

from .core import Case


class Engine(ABC):
    name = "engine"

    def prepare(self, cases: Sequence[Case]) -> None:
        """可选的预处理钩子。**只有 oracle 会用到它**，真实引擎必须只依赖输入拼音。"""

    @abstractmethod
    def predict(self, pinyin: str) -> list[str]:
        ...

    def close(self) -> None:
        ...


class OracleEngine(Engine):
    """作弊引擎：直接返回标准答案。

    用途是验证评测框架本身算得对不对——它的分数必须恒为 100%。
    任何真实引擎都不应该看得到 gold。
    """

    name = "oracle"
    _index: dict[str, str] = {}

    def prepare(self, cases: Sequence[Case]) -> None:
        self._index = {c.pinyin: c.gold for c in cases}

    def predict(self, pinyin: str) -> list[str]:
        gold = self._index.get(pinyin)
        return [gold] if gold else []


class EchoEngine(Engine):
    """占位引擎：把拼音原样返回。

    分数必然接近 0，用来确认框架在非满分场景下也能正确统计。
    """

    name = "echo"

    def predict(self, pinyin: str) -> list[str]:
        return [pinyin]


class CliEngine(Engine):
    """通过子进程行协议调用外部引擎（libime / librime / 自研均可）。

    协议（与 WeType harness 的行协议同构，便于后端插拔）：
      写: "<pinyin>\\n"
      读: 一行，候选以 \\t 分隔，第一个为首选；空行表示无候选
    进程常驻，避免每 case 重启带来的启动开销污染延迟统计。
    """

    def __init__(self, argv: Sequence[str], name: str = "cli", cwd: str | None = None):
        self._argv = list(argv)
        self.name = name
        self._proc = subprocess.Popen(
            self._argv,
            stdin=subprocess.PIPE,
            stdout=subprocess.PIPE,
            stderr=subprocess.DEVNULL,
            text=True,
            bufsize=1,
            cwd=cwd,
        )

    def predict(self, pinyin: str) -> list[str]:
        assert self._proc.stdin and self._proc.stdout
        self._proc.stdin.write(pinyin + "\n")
        self._proc.stdin.flush()
        line = self._proc.stdout.readline()
        if not line:
            raise RuntimeError(f"{self.name}: 引擎进程意外退出")
        line = line.rstrip("\n")
        return [c for c in line.split("\t") if c]

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


ENGINES: dict[str, type[Engine]] = {
    "oracle": OracleEngine,
    "echo": EchoEngine,
}
