#!/usr/bin/env python3
"""快速批量评测：中文语料 → pypinyin转拼音 → CLI --batch → 比对。

用法：python3 scripts/fast_eval.py [corpus_name ...]
不指定则评测全部语料。不打印语料内容（安全纪律），仅输出准确率统计。

性能：利用 CLI --batch 模式（单进程、模型常驻），14000+ 句 < 5 秒。
"""
import subprocess, os, sys, tempfile, time
from pypinyin import pinyin, Style

CLI = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                   "cpp", "build", "pinyin_ime_cli")
NB = "/workspace/native-build"
LM = f"{NB}/opt/libime/lib/libime/zh_CN.lm"
DICT = f"{NB}/opt/libime/share/libime/sc.dict"
ENV = os.environ.copy()
ENV["LD_LIBRARY_PATH"] = f"{NB}/opt/libime/lib:{NB}/opt/fcitx5/lib:{NB}/mamba-env/lib"

CORPORA_DIR = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))),
                           "bench", "corpus")


def pinyin_of(text):
    """中文 → 连续全拼（无分隔符）。"""
    return ''.join(p[0] for p in pinyin(text, style=Style.NORMAL, errors='ignore'))


def eval_corpus(name, chunk_size=2000):
    """评测单个语料：返回 (pass_count, total_count, elapsed_seconds)。"""
    path = os.path.join(CORPORA_DIR, f"{name}.txt")
    if not os.path.exists(path):
        return 0, 0, 0

    with open(path, encoding='utf-8') as f:
        lines = [l.strip() for l in f if l.strip()]

    # 中文 → 拼音
    pairs = []
    for text in lines:
        py = pinyin_of(text)
        if py:
            pairs.append((py, text))

    if not pairs:
        return 0, 0, 0

    t0 = time.time()
    total_pass = 0

    for i in range(0, len(pairs), chunk_size):
        chunk = pairs[i:i + chunk_size]
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt',
                                         delete=False, encoding='utf-8') as f:
            for py, _ in chunk:
                f.write(py + '\n')
            pf = f.name

        r = subprocess.run(
            [CLI, "--batch", pf, "--top1", "--quiet", LM, DICT],
            capture_output=True, text=True, env=ENV, timeout=30
        )
        os.unlink(pf)
        outs = r.stdout.strip().split('\n')
        for j, (_, expected) in enumerate(chunk):
            got = outs[j] if j < len(outs) else ""
            if got == expected:
                total_pass += 1

    return total_pass, len(pairs), time.time() - t0


def main():
    names = sys.argv[1:] if len(sys.argv) > 1 else [
        'daily', 'dialogue', 'news', 'wiki', 'recent', 'baike'
    ]

    print("=== pinyin-ime 快速批量评测 ===")
    print(f"CLI: {CLI}")
    print(f"LM:  {LM}")
    print(f"Dict:{DICT}")
    print()

    total_p = total_t = 0
    total_time = 0

    for name in names:
        p, t, elapsed = eval_corpus(name)
        if t == 0:
            print(f"  {name:12s}: (文件不存在或为空)")
            continue
        pct = p * 100 / t
        print(f"  {name:12s}: {p:5d}/{t:5d} = {pct:5.1f}%  ({elapsed:.2f}s)")
        total_p += p
        total_t += t
        total_time += elapsed

    if total_t > 0:
        print(f"  {'─' * 48}")
        print(f"  {'TOTAL':12s}: {total_p:5d}/{total_t:5d} = {total_p*100/total_t:5.1f}%  ({total_time:.2f}s)")


if __name__ == '__main__':
    main()
