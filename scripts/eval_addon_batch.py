#!/usr/bin/env python3
"""真实 fcitx5 addon 批量评测（v0.9.202 原型）。

不经过 CLI：启动私有 Xvfb + dbus 会话 + fcitx5，加载已安装的 libpinyinime.so；
用一个最小 GTK3 Entry 作为前台，xdotool 真实敲每个 holdout 拼音，空格提交首选，
再从 Entry 读取整句。目的：给 addon 一个和 CLI 同口径的 top1/字级数字。

必须由 `scripts/eval_addon_batch.sh` 包一层 `dbus-run-session`；否则 GTK im module
和 fcitx5 不在同一个 session bus，输入不会进入 addon。

用法：
    bash scripts/eval_addon_batch.sh --limit 20
    bash scripts/eval_addon_batch.sh --out bench/results/addon-0.9.202
"""
from __future__ import annotations

import argparse
import json
import os
import queue
import select
import shutil
import statistics
import subprocess
import sys
import tempfile
import threading
import time
from datetime import datetime, timezone
from pathlib import Path

APP_CODE = r'''
import gi
gi.require_version("Gtk", "3.0")
from gi.repository import Gtk, GLib
import queue
import sys
import threading

entry = None
win = None
commands = queue.Queue()

def read_stdin():
    for line in sys.stdin:
        commands.put(line.rstrip("\n"))
    commands.put("QUIT")

threading.Thread(target=read_stdin, daemon=True).start()

def poll():
    try:
        while True:
            cmd = commands.get_nowait()
            seq = ""
            if " " in cmd:
                cmd, seq = cmd.split(" ", 1)
            if cmd == "DUMP":
                print("TEXT\t%s\t%s" % (seq, entry.get_text()), flush=True)
            elif cmd == "CLEAR":
                entry.set_text("")
                print("OK\t%s" % seq, flush=True)
            elif cmd == "QUIT":
                Gtk.main_quit()
                return False
    except queue.Empty:
        pass
    return True

def on_map(_win):
    print("WID=%d" % win.get_window().get_xid(), flush=True)
    entry.grab_focus()

win = Gtk.Window(title="pinyin-ime-addon-batch")
entry = Gtk.Entry()
win.add(entry)
win.connect("map", on_map)
win.connect("destroy", Gtk.main_quit)
win.show_all()
win.present()
print("READY", flush=True)
GLib.timeout_add(15, poll)
Gtk.main()
'''


def find_display() -> int:
    for n in range(90, 130):
        if not Path(f"/tmp/.X11-unix/X{n}").exists():
            return n
    raise RuntimeError("找不到空闲 X display（90-129 都被占用）")


def wait_file(path: Path, timeout: float) -> bool:
    end = time.time() + timeout
    while time.time() < end:
        if path.exists():
            return True
        time.sleep(0.1)
    return False


def char_accuracy(hyp: str, ref: str) -> float:
    if hyp == ref:
        return 1.0
    if not hyp:
        return 0.0 if ref else 1.0
    if not ref:
        return 0.0
    # 与 bench/eval.py 同一实现；这里不再 import，避免脚本依赖 bench 包结构。
    prev = list(range(len(ref) + 1))
    for i, ca in enumerate(hyp, 1):
        cur = [i]
        for j, cb in enumerate(ref, 1):
            cur.append(min(prev[j] + 1, cur[j - 1] + 1, prev[j - 1] + (ca != cb)))
        prev = cur
    return 1.0 - prev[-1] / max(len(ref), len(hyp))


def score(hyps: list[str], refs: list[str]) -> dict:
    if not refs:
        return {"sentences": 0, "sent_acc": 0.0, "char_acc": 0.0}
    exact = sum(1 for h, r in zip(hyps, refs) if h == r)
    chars = sum(char_accuracy(h, r) for h, r in zip(hyps, refs)) / len(refs)
    return {
        "sentences": len(refs),
        "sent_acc": round(exact / len(refs) * 100, 2),
        "char_acc": round(chars * 100, 2),
    }


def candidate_metrics(candidates: list[list[str]], refs: list[str]) -> dict:
    """候选池指标：top-k 句级命中率与 MRR（正确句在候选池中的首个名次）。"""
    ks = (1, 5, 10)
    hits = {k: 0 for k in ks}
    reciprocal = 0.0
    for cands, ref in zip(candidates, refs):
        rank = 0
        for i, cand in enumerate(cands, 1):
            if cand == ref:
                rank = i
                break
        for k in ks:
            if rank and rank <= k:
                hits[k] += 1
        if rank:
            reciprocal += 1.0 / rank
    n = max(len(refs), 1)
    out = {f"top{k}_sent_acc": round(hits[k] * 100.0 / n, 2) for k in ks}
    out["mrr"] = round(reciprocal / n, 4)
    out["candidate_pool_avg"] = round(
        sum(len(c) for c in candidates) / max(len(candidates), 1), 1)
    return out


def percentile(values: list[float], p: float) -> float:
    if not values:
        return 0.0
    vals = sorted(values)
    idx = min(len(vals) - 1, max(0, int(round((p / 100.0) * (len(vals) - 1)))))
    return round(vals[idx], 1)


def load_rows(holdout: Path, domains: list[str] | None) -> dict[str, list[tuple[str, str]]]:
    out = {}
    for tsv in sorted(holdout.glob("*.tsv")):
        if domains and tsv.stem not in domains:
            continue
        rows = []
        for line in tsv.read_text(encoding="utf-8").splitlines():
            if not line.strip() or line.startswith("#"):
                continue
            parts = line.split("\t")
            if len(parts) >= 2:
                rows.append((parts[0], parts[1]))
        if rows:
            out[tsv.stem] = rows
    return out


def read_line(q: queue.Queue, timeout: float) -> str:
    try:
        return q.get(timeout=timeout)
    except queue.Empty:
        raise RuntimeError(f"等待 addon harness 输出超时（{timeout}s）")


def read_until(q: queue.Queue, prefix: str, timeout: float) -> str:
    """忽略 READY/GTK 噪声，直到拿到指定前缀的一行。"""
    end = time.time() + timeout
    while time.time() < end:
        line = read_line(q, max(0.1, end - time.time()))
        if line.startswith(prefix):
            return line
    raise RuntimeError(f"等待 addon harness 输出前缀 {prefix!r} 超时")


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--holdout", type=Path, default=None)
    ap.add_argument("--out", type=Path, default=None)
    ap.add_argument("--limit", type=int, default=0, help="每个域最多取 N 句；0=全量")
    ap.add_argument("--domains", default="", help="逗号分隔域名")
    ap.add_argument("--out-timeout", type=float, default=3.0)
    ap.add_argument("--type-delay", type=int, default=5, help="xdotool type 每键延迟 ms")
    ap.add_argument("--settle-ms", type=int, default=100, help="敲完拼音后等待 addon 处理 ms")
    ap.add_argument("--commit-timeout-ms", type=int, default=1200, help="空格后等待提交完成的上限 ms")
    args = ap.parse_args()

    repo = Path(__file__).resolve().parent.parent
    holdout = args.holdout or repo / "bench" / "holdout"
    out = args.out or repo / "bench" / "results" / "addon"
    domains = [d.strip() for d in args.domains.split(",") if d.strip()] or None
    rows_by_domain = load_rows(holdout, domains)
    if not rows_by_domain:
        print(f"holdout 为空：{holdout}", file=sys.stderr)
        return 2
    if args.limit > 0:
        rows_by_domain = {d: r[: args.limit] for d, r in rows_by_domain.items()}

    if not os.environ.get("DBUS_SESSION_BUS_ADDRESS"):
        print("❌ 没有 DBUS_SESSION_BUS_ADDRESS；请用 scripts/eval_addon_batch.sh 启动", file=sys.stderr)
        return 2
    for exe in ("Xvfb", "fcitx5", "xdotool"):
        if not shutil.which(exe):
            print(f"❌ 缺少可执行文件：{exe}", file=sys.stderr)
            return 2

    work = Path(tempfile.mkdtemp(prefix="pinyin-addon-eval."))
    home = work / "home"
    xdg = home / ".config"
    xdg.mkdir(parents=True, exist_ok=True)
    (xdg / "fcitx5").mkdir(parents=True, exist_ok=True)
    (xdg / "fcitx5" / "profile").write_text(
        "[Groups/0]\nName=Default\nDefault Layout=us\nDefaultIM=pinyinime\n\n"
        "[Groups/0/Items/0]\nName=pinyinime\nLayout=\n\n[GroupOrder]\n0=Default\n",
        encoding="utf-8",
    )
    runtime = work / "runtime"
    runtime.mkdir()
    app_path = work / "addon_app.py"
    app_path.write_text(APP_CODE, encoding="utf-8")
    app_log = work / "app.log"
    fcitx_log = work / "fcitx5.log"
    xvfb_log = work / "xvfb.log"
    cand_path = work / "candidates.tsv"

    display_num = find_display()
    display = f":{display_num}"
    env = dict(os.environ)
    env.update({
        "DISPLAY": display,
        "HOME": str(home),
        "XDG_CONFIG_HOME": str(xdg),
        "XDG_RUNTIME_DIR": str(runtime),
        "GTK_IM_MODULE": "fcitx",
        "QT_IM_MODULE": "fcitx",
        "XMODIFIERS": "@im=fcitx",
        "NO_AT_BRIDGE": "1",
        "PINYINIME_EVAL_CAND": str(cand_path),
    })
    env.pop("PYTHONPATH", None)

    xvfb = fcitx = app = None
    app_q: queue.Queue[str] = queue.Queue()
    try:
        xvfb = subprocess.Popen(["Xvfb", display, "-screen", "0", "1024x768x24"],
                                stdout=xvfb_log.open("w"), stderr=subprocess.STDOUT, env=env)
        if not wait_file(Path(f"/tmp/.X11-unix/X{display_num}"), 5):
            raise RuntimeError("Xvfb 启动超时")
        fcitx = subprocess.Popen(["fcitx5", "-D", "--verbose", "default=4"],
                                 stdout=fcitx_log.open("w"), stderr=subprocess.STDOUT, env=env)
        loaded = False
        end = time.time() + 12
        while time.time() < end:
            if fcitx_log.exists() and "Loaded addon pinyinime" in fcitx_log.read_text(errors="ignore"):
                loaded = True
                break
            time.sleep(0.2)
        if not loaded:
            raise RuntimeError(f"fcitx5 未加载 pinyinime（见 {fcitx_log}）")

        app = subprocess.Popen([sys.executable, str(app_path)],
                               stdin=subprocess.PIPE, stdout=subprocess.PIPE,
                               stderr=app_log.open("w"), text=True, bufsize=1, env=env)
        assert app.stdin and app.stdout

        def read_app():
            for line in app.stdout:
                app_q.put(line.rstrip("\n"))

        threading.Thread(target=read_app, daemon=True).start()
        # GitHub runner 首次加载 GTK im module 比本地 chroot 慢；首行可能
        # 超过 5s 才出现 READY。放宽首个等待，之后仍用短超时。
        wid_line = ""
        for attempt in range(10):
            line = read_line(app_q, 20 if attempt == 0 else 5)
            if line.startswith("WID="):
                wid_line = line
                break
        if not wid_line:
            raise RuntimeError("GTK 未返回窗口 ID（只看到 READY/其他输出）")
        wid = wid_line.split("=", 1)[1]
        subprocess.run(["xdotool", "windowfocus", wid], env=env,
                       stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5)
        # 窗口获得焦点后 fcitx5 还要完成输入上下文切换；首句太急会原样回显拼音。
        time.sleep(0.8)

        summary = {
            "version": (repo / "VERSION").read_text().strip() if (repo / "VERSION").exists() else "unknown",
            "engine": "fcitx5-addon-xdotool",
            "addon": "libpinyinime.so",
            "generated_at": datetime.now(timezone.utc).astimezone().isoformat(timespec="seconds"),
            "holdout": str(holdout),
            "domains": {},
            "overall": {},
        }
        all_hyps: list[str] = []
        all_refs: list[str] = []
        errors = []
        latencies = []
        all_cands: list[list[str]] = []

        def cmd(text: str) -> None:
            app.stdin.write(text + "\n")
            app.stdin.flush()

        seq = 0
        for domain, rows in rows_by_domain.items():
            hyps, refs, lat, cand_lists = [], [], [], []
            for py, ref in rows:
                seq += 1
                if seq > 1:
                    cmd(f"CLEAR {seq}")
                    read_until(app_q, f"OK\t{seq}", 5)
                cand_path.unlink(missing_ok=True)
                t0 = time.time()
                subprocess.run(["xdotool", "type", "--clearmodifiers", "--delay",
                                str(args.type_delay), py], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=10)
                time.sleep(args.settle_ms / 1000.0)
                subprocess.run(["xdotool", "key", "space"], env=env,
                               stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL, timeout=5)
                # 长句的转换/提交是异步的：只睡 30ms 会读到空 Entry，随后 CLEAR 又把
                # 上一句的迟到提交带到下一句。轮询到非空或超时，再取最后一次。
                hyp = ""
                deadline = time.time() + args.commit_timeout_ms / 1000.0
                while True:
                    time.sleep(0.05)
                    cmd(f"DUMP {seq}")
                    line = read_until(app_q, f"TEXT\t{seq}\t", args.out_timeout)
                    hyp = line.split("\t", 2)[2]
                    if hyp or time.time() >= deadline:
                        break
                lat.append((time.time() - t0) * 1000)
                hyps.append(hyp)
                refs.append(ref)
                cands = []
                if cand_path.exists():
                    for line in cand_path.read_text(errors="ignore").splitlines():
                        if line.startswith("CAND\t"):
                            cands.append(line.split("\t", 1)[1])
                cand_lists.append(cands)
                if hyp != ref:
                    errors.append(f"{domain}\t{py}\t期望 {ref}\t实际 {hyp}")
                if seq % 25 == 0:
                    print(f"  {seq} 句，最近 {domain}: {hyp[:20]}", flush=True)
            stats = score(hyps, refs)
            stats["p50_ms"] = percentile(lat, 50)
            stats["p95_ms"] = percentile(lat, 95)
            stats["p99_ms"] = percentile(lat, 99)
            stats.update(candidate_metrics(cand_lists, refs))
            summary["domains"][domain] = stats
            all_hyps.extend(hyps)
            all_refs.extend(refs)
            latencies.extend(lat)
            all_cands.extend(cand_lists)
            print(f"[{domain}] {stats}", flush=True)

        summary["overall"] = score(all_hyps, all_refs)
        summary["overall"]["p50_ms"] = percentile(latencies, 50)
        summary["overall"]["p95_ms"] = percentile(latencies, 95)
        summary["overall"]["p99_ms"] = percentile(latencies, 99)
        summary["overall"].update(candidate_metrics(all_cands, all_refs))
        summary["errors"] = len(errors)

        out.mkdir(parents=True, exist_ok=True)
        (out / "summary.json").write_text(json.dumps(summary, ensure_ascii=False, indent=2) + "\n",
                                          encoding="utf-8")
        md = [f"# addon 真实上屏评测 v{summary['version']}", "",
              f"引擎：`{summary['engine']}`（{summary['addon']}）", "",
              "| 域 | 句数 | Top1 | Top5 | Top10 | MRR | 字级 | P50 | P95 | P99 |",
              "|---|---|---|---|---|---|---|---|---|---|"]
        for d, s in summary["domains"].items():
            md.append(f"| {d} | {s['sentences']} | {s['sent_acc']}% | "
                      f"{s['top5_sent_acc']}% | {s['top10_sent_acc']}% | {s['mrr']} | "
                      f"{s['char_acc']}% | {s['p50_ms']}ms | {s['p95_ms']}ms | {s['p99_ms']}ms |")
        o = summary["overall"]
        md.append(f"| **合计** | {o['sentences']} | {o['sent_acc']}% | "
                  f"{o['top5_sent_acc']}% | {o['top10_sent_acc']}% | {o['mrr']} | "
                  f"{o['char_acc']}% | {o['p50_ms']}ms | {o['p95_ms']}ms | {o['p99_ms']}ms |")
        md += ["", f"错误句数：{len(errors)}", "", "## 错例（最多 20 条）", ""]
        md += [f"- {e}" for e in errors[:20]]
        (out / "report.md").write_text("\n".join(md) + "\n", encoding="utf-8")
        print("\n".join(md[:10]))
        print(f"\n结果已写入 {out}/summary.json 与 report.md")
        return 0
    finally:
        for p in (app, fcitx, xvfb):
            if p is not None:
                p.terminate()
                try:
                    p.wait(timeout=3)
                except subprocess.TimeoutExpired:
                    p.kill()
        if os.environ.get("PINYINIME_EVAL_KEEP"):
            print(f"work kept: {work}", file=sys.stderr)
        else:
            shutil.rmtree(work, ignore_errors=True)


if __name__ == "__main__":
    sys.exit(main())
