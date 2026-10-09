#!/usr/bin/env python3
"""
Train user language model from corpus for domain adaptation.
Uses --learn-file to teach the user LM correct sentence patterns.

This is more effective than LM interpolation for small domain corpora
because it preserves the system LM's broad coverage while adding
domain-specific patterns to the user history.

Usage:
  python3 scripts/train_user_lm.py corpus.txt [--cli PATH] [--lm PATH] [--dict PATH]
"""
import sys, os, subprocess, argparse

def main():
    parser = argparse.ArgumentParser(description="Train user LM from corpus")
    parser.add_argument("corpus_file", help="Corpus file (Chinese sentences, one per line)")
    parser.add_argument("--cli", help="Path to pinyin_ime_cli")
    parser.add_argument("--lm", help="Path to language model")
    parser.add_argument("--dict", help="Path to dictionary")
    parser.add_argument("--max-sentences", type=int, default=0, help="Max sentences to learn")
    parser.add_argument("--clear", action="store_true", help="Clear existing user data first")
    args = parser.parse_args()
    
    try:
        import pypinyin
    except ImportError:
        subprocess.run([sys.executable, "-m", "pip", "install", "pypinyin", "-q"], check=True)
        import pypinyin
    
    # Find CLI and model files
    cli = args.cli
    if not cli:
        candidates = [
            os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli'),
            '/usr/local/bin/pinyin_ime_cli',
        ]
        for c in candidates:
            c = os.path.abspath(c)
            if os.path.isfile(c) and os.access(c, os.X_OK):
                cli = c
                break
    
    lm = args.lm
    dict_path = args.dict
    if not lm or not dict_path:
        nb = "/root/workspace/native-build/opt"
        if not lm:
            lm = f"{nb}/libime/lib/libime/zh_CN.lm"
        if not dict_path:
            dict_path = f"{nb}/libime/share/libime/sc.dict"
    
    # Setup env
    nb = "/root/workspace/native-build"
    env = os.environ.copy()
    env['PATH'] = f"{nb}/mamba-env/bin:{env.get('PATH', '')}"
    env['LD_LIBRARY_PATH'] = f"{nb}/mamba-env/lib:{nb}/opt/fcitx5/lib:{nb}/opt/libime/lib:{env.get('LD_LIBRARY_PATH', '')}"
    
    # Clear user data if requested
    if args.clear:
        user_dir = os.path.expanduser("~/.local/share/pinyin-ime")
        for f in ['user.history', 'user.dict']:
            p = os.path.join(user_dir, f)
            if os.path.exists(p):
                os.unlink(p)
                print(f"Cleared {p}")
    
    # Read corpus and convert to learn format
    with open(args.corpus_file) as f:
        sentences = [line.strip() for line in f if line.strip()]
    
    if args.max_sentences > 0:
        sentences = sentences[:args.max_sentences]
    
    # Convert to pinyin\tchinese format
    learn_pairs = []
    for s in sentences:
        chinese = ''.join(c for c in s if '\u4e00' <= c <= '\u9fff')
        if len(chinese) < 2:
            continue
        pinyin = ''.join(pypinyin.lazy_pinyin(chinese, style=pypinyin.Style.NORMAL))
        learn_pairs.append(f"{pinyin}\t{chinese}")
    
    # Write learn file
    import tempfile
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as tf:
        for pair in learn_pairs:
            tf.write(pair + '\n')
        learn_path = tf.name
    
    try:
        print(f"Learning from {len(learn_pairs)} sentences...")
        result = subprocess.run(
            [cli, '--learn-file', learn_path, '--quiet', lm, dict_path],
            capture_output=True, text=True, timeout=120, env=env
        )
        print(result.stdout.strip())
        if result.stderr:
            print(result.stderr.strip())
    finally:
        os.unlink(learn_path)
    
    print(f"Done! User LM trained on {len(learn_pairs)} sentences.")

if __name__ == '__main__':
    main()
