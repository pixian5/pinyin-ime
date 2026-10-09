#!/usr/bin/env python3
"""Auto-learn from evaluation failures: run eval, collect wrong predictions, learn correct answers."""
import sys, os, subprocess, tempfile, argparse

def main():
    parser = argparse.ArgumentParser(description="Auto-learn from eval failures")
    parser.add_argument("corpus_file", help="Corpus file (Chinese sentences)")
    parser.add_argument("--cli", required=True, help="Path to pinyin_ime_cli")
    parser.add_argument("--lm", required=True, help="Path to language model")
    parser.add_argument("--dict", required=True, help="Path to dictionary")
    parser.add_argument("--sample", type=int, default=0, help="Sample N sentences")
    parser.add_argument("--max-learn", type=int, default=50, help="Max items to learn")
    parser.add_argument("--history-weight", type=float, default=2.0, help="User history weight after learning")
    args = parser.parse_args()
    
    try:
        import pypinyin
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "pypinyin", "-q"], check=True)
        import pypinyin
    
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
        outputs = result.stdout.strip().split('\n')
    finally:
        os.unlink(tmp_path)
    
    # Collect failures
    failures = []
    for i, (py, expected) in enumerate(pinyin_pairs):
        actual = outputs[i] if i < len(outputs) else ""
        if actual != expected:
            failures.append((py, expected, actual))
    
    total = len(pinyin_pairs)
    before_correct = total - len(failures)
    before_acc = before_correct / total * 100
    
    print(f"评测前: {before_correct}/{total} = {before_acc:.1f}% 句级准确率")
    print(f"失败: {len(failures)} 句")
    
    if not failures:
        print("无需学习，全部正确！")
        return
    
    # Phase 2: Learn from failures
    learn_items = failures[:args.max_learn]
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as tf:
        for py, expected, _ in learn_items:
            tf.write(f"{py}\t{expected}\n")
        learn_path = tf.name
    
    try:
        result = subprocess.run(
            [args.cli, '--learn-file', learn_path, '--quiet', args.lm, args.dict],
            capture_output=True, text=True, timeout=60
        )
        print(f"学习: {result.stdout.strip()}")
    finally:
        os.unlink(learn_path)
    
    # Phase 3: Re-evaluate after learning
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as tf:
        for py, _ in pinyin_pairs:
            tf.write(py + '\n')
        tmp_path = tf.name
    
    try:
        result = subprocess.run(
            [args.cli, '--quiet', '--batch', tmp_path, '--top1', 
             '--history-weight', str(args.history_weight),
             args.lm, args.dict],
            capture_output=True, text=True, timeout=120
        )
        outputs = result.stdout.strip().split('\n')
    finally:
        os.unlink(tmp_path)
    
    after_correct = 0
    still_wrong = []
    for i, (py, expected) in enumerate(pinyin_pairs):
        actual = outputs[i] if i < len(outputs) else ""
        if actual == expected:
            after_correct += 1
        else:
            still_wrong.append((py, expected, actual))
    
    after_acc = after_correct / total * 100
    improvement = after_acc - before_acc
    
    print(f"评测后: {after_correct}/{total} = {after_acc:.1f}% 句级准确率")
    print(f"提升: {improvement:+.1f}%")
    print(f"仍失败: {len(still_wrong)} 句")
    
    if still_wrong[:5]:
        print("仍失败示例:")
        for py, expected, actual in still_wrong[:5]:
            print(f"  {py} → {actual} (expected: {expected})")

if __name__ == '__main__':
    main()
