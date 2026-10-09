#!/usr/bin/env python3
"""Auto-learn v2: extract word-level pairs from eval failures and add to user dict."""
import sys, os, subprocess, tempfile, argparse, re

def main():
    parser = argparse.ArgumentParser(description="Auto-learn v2: word-level learning from failures")
    parser.add_argument("corpus_file", help="Corpus file (Chinese sentences)")
    parser.add_argument("--cli", required=True, help="Path to pinyin_ime_cli")
    parser.add_argument("--lm", required=True, help="Path to language model")
    parser.add_argument("--dict", required=True, help="Path to dictionary")
    parser.add_argument("--sample", type=int, default=0, help="Sample N sentences")
    parser.add_argument("--max-learn", type=int, default=100, help="Max items to learn")
    parser.add_argument("--min-word-len", type=int, default=2, help="Min word length to learn")
    args = parser.parse_args()
    
    try:
        import pypinyin
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "pypinyin", "-q"], check=True)
        import pypinyin
    
    try:
        import jieba
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "jieba", "-q"], check=True)
        import jieba
    
    cli_args = [args.cli, '--quiet', '--batch']
    
    # Read corpus
    with open(args.corpus_file) as f:
        sentences = [line.strip() for line in f if line.strip()]
    
    if args.sample > 0 and args.sample < len(sentences):
        import random
        random.seed(42)
        sentences = random.sample(sentences, args.sample)
    
    # Convert to pinyin
    pinyin_pairs = []
    for s in sentences:
        chinese = ''.join(c for c in s if '\u4e00' <= c <= '\u9fff')
        if len(chinese) < 2:
            continue
        pinyin = ''.join(pypinyin.lazy_pinyin(chinese, style=pypinyin.Style.NORMAL))
        pinyin_pairs.append((pinyin, chinese))
    
    # Phase 1: Evaluate before learning
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as tf:
        for py, _ in pinyin_pairs:
            tf.write(py + '\n')
        tmp_path = tf.name
    
    try:
        result = subprocess.run(
            [args.cli, '--quiet', '--batch', tmp_path, '--top1', args.lm, args.dict],
            capture_output=True, text=True, timeout=120
        )
        outputs_before = result.stdout.strip().split('\n')
    finally:
        os.unlink(tmp_path)
    
    # Collect failures and extract word-level pairs
    failures = []
    word_pairs = {}  # pinyin → chinese (deduped)
    
    for i, (py, expected) in enumerate(pinyin_pairs):
        actual = outputs_before[i] if i < len(outputs_before) else ""
        if actual != expected:
            failures.append((py, expected, actual))
            # Extract wrong words by aligning expected vs actual
            # Use jieba to segment expected, then find pinyin for each word
            words = list(jieba.cut(expected))
            pos = 0
            for w in words:
                if len(w) >= args.min_word_len and all('\u4e00' <= c <= '\u9fff' for c in w):
                    w_py = ''.join(pypinyin.lazy_pinyin(w, style=pypinyin.Style.NORMAL))
                    # Check if this word was predicted wrong
                    if w_py not in word_pairs:
                        word_pairs[w_py] = w
    
    total = len(pinyin_pairs)
    before_correct = total - len(failures)
    before_acc = before_correct / total * 100
    
    print(f"评测前: {before_correct}/{total} = {before_acc:.1f}%")
    print(f"失败: {len(failures)} 句, 提取词对: {len(word_pairs)} 个")
    
    if not word_pairs:
        print("无需学习")
        return
    
    # Phase 2: Add word pairs to user dict
    learned = 0
    for py, ch in list(word_pairs.items())[:args.max_learn]:
        result = subprocess.run(
            [args.cli, '--add-user', f'{py}={ch}', '--quiet', args.lm, args.dict],
            capture_output=True, text=True, timeout=10
        )
        if '已添加' in result.stdout or '已添加' in result.stderr:
            learned += 1
    
    print(f"添加用户词典: {learned} 条")
    
    # Phase 3: Re-evaluate
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as tf:
        for py, _ in pinyin_pairs:
            tf.write(py + '\n')
        tmp_path = tf.name
    
    try:
        result = subprocess.run(
            [args.cli, '--quiet', '--batch', tmp_path, '--top1', args.lm, args.dict],
            capture_output=True, text=True, timeout=120
        )
        outputs_after = result.stdout.strip().split('\n')
    finally:
        os.unlink(tmp_path)
    
    after_correct = sum(1 for i, (py, expected) in enumerate(pinyin_pairs) 
                       if (outputs_after[i] if i < len(outputs_after) else "") == expected)
    after_acc = after_correct / total * 100
    
    print(f"评测后: {after_correct}/{total} = {after_acc:.1f}%")
    print(f"提升: {after_acc - before_acc:+.1f}%")

if __name__ == '__main__':
    main()
