#!/usr/bin/env python3
"""Batch corpus evaluation: convert Chinese sentences to pinyin, run CLI --batch --top1, compare."""
import sys, os, subprocess, tempfile

def main():
    if len(sys.argv) < 5:
        print("Usage: eval_corpus.py corpus.txt CLI LM DICT [--sample N] [--history-weight F]")
        sys.exit(1)
    
    corpus_file, cli, lm, dict_path = sys.argv[1], sys.argv[2], sys.argv[3], sys.argv[4]
    sample = 0
    history_weight = 0
    if "--sample" in sys.argv:
        idx = sys.argv.index("--sample")
        sample = int(sys.argv[idx + 1])
    if "--history-weight" in sys.argv:
        idx = sys.argv.index("--history-weight")
        history_weight = float(sys.argv[idx + 1])
    
    try:
        import pypinyin
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "pypinyin", "-q"], check=True)
        import pypinyin
    
    # Read corpus
    with open(corpus_file) as f:
        sentences = [line.strip() for line in f if line.strip()]
    
    if sample > 0 and sample < len(sentences):
        import random
        random.seed(42)
        sentences = random.sample(sentences, sample)
    
    # Convert to pinyin
    pinyin_pairs = []
    for s in sentences:
        chinese = ''.join(c for c in s if '\u4e00' <= c <= '\u9fff')
        if len(chinese) < 2:
            continue
        pinyin = ''.join(pypinyin.lazy_pinyin(chinese, style=pypinyin.Style.NORMAL))
        pinyin_pairs.append((pinyin, chinese))
    
    if not pinyin_pairs:
        print("No valid sentences found")
        return
    
    # Write pinyin to temp file for batch processing
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as tf:
        for py, _ in pinyin_pairs:
            tf.write(py + '\n')
        tmp_path = tf.name
    
    try:
        # Build command
        cmd = [cli, '--quiet', '--batch', tmp_path, '--top1']
        if history_weight > 0:
            cmd += ['--history-weight', str(history_weight)]
        cmd += [lm, dict_path]
        
        # Run CLI in batch mode
        result = subprocess.run(cmd, capture_output=True, text=True, timeout=120)
        outputs = result.stdout.strip().split('\n')
    finally:
        os.unlink(tmp_path)
    
    # Compare
    sent_correct = 0
    char_correct = 0
    char_total = 0
    failures = []
    
    for i, (py, expected) in enumerate(pinyin_pairs):
        actual = outputs[i] if i < len(outputs) else ""
        if actual == expected:
            sent_correct += 1
        else:
            if len(failures) < 10:
                failures.append(f"  {py} → {actual} (expected: {expected})")
        # Character-level
        for j, c in enumerate(expected):
            char_total += 1
            if j < len(actual) and actual[j] == c:
                char_correct += 1
    
    total = len(pinyin_pairs)
    sent_acc = sent_correct / total * 100 if total > 0 else 0
    char_acc = char_correct / char_total * 100 if char_total > 0 else 0
    
    print(f"语料: {os.path.basename(corpus_file)}")
    print(f"句数: {total}")
    print(f"句级准确率: {sent_correct}/{total} = {sent_acc:.1f}%")
    print(f"字级准确率: {char_correct}/{char_total} = {char_acc:.1f}%")
    if failures:
        print(f"失败示例 (前{len(failures)}):")
        for f in failures:
            print(f)

if __name__ == '__main__':
    main()
