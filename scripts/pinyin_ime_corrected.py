#!/usr/bin/env python3
"""
pinyin-ime 带后纠错的预测接口
先调用 CLI 获取初始预测，再通过 post_corrector 纠正。

用法:
  # 单句
  echo "zhegewentineyaizaizayanjiu" | python3 scripts/pinyin_ime_corrected.py --top1
  
  # 批量
  python3 scripts/pinyin_ime_corrected.py --batch input.txt --top1
  
  # 仅后纠错（不调用CLI）
  echo "这个问题需要在研究" | python3 scripts/pinyin_ime_corrected.py --correct-only
"""
import sys, os, subprocess, argparse

sys.path.insert(0, os.path.dirname(__file__))
from post_corrector import PostCorrector

CLI = os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli')
LM = os.environ.get("PINYIN_IME_LM", "/usr/share/libime/zh_CN.lm")
DICT = os.environ.get("PINYIN_IME_DICT", "/usr/share/libime/sc.dict")

def run_cli(pinyin, extra_args, timeout=10):
    result = subprocess.run(
        [CLI] + extra_args + [LM, DICT],
        input=pinyin, capture_output=True, text=True, timeout=timeout
    )
    return result.stdout.strip()

def run_batch(filepath, extra_args, timeout=120):
    result = subprocess.run(
        [CLI] + extra_args + [filepath, LM, DICT],
        capture_output=True, text=True, timeout=timeout
    )
    return result.stdout.strip()

def main():
    parser = argparse.ArgumentParser(description='pinyin-ime with post-correction')
    parser.add_argument('--top1', action='store_true', help='Return top-1 candidate only')
    parser.add_argument('--top-k', type=int, help='Return top-K candidates')
    parser.add_argument('--batch', type=str, help='Batch mode: read pinyin from file')
    parser.add_argument('--correct-only', action='store_true', help='Only apply post-correction to stdin text')
    parser.add_argument('--no-correct', action='store_true', help='Disable post-correction')
    parser.add_argument('--quiet', action='store_true', help='Quiet mode')
    args = parser.parse_args()
    
    corrector = PostCorrector(enabled=not args.no_correct)
    
    if args.correct_only:
        # Just apply correction to input text
        for line in sys.stdin:
            text = line.strip()
            if text:
                print(corrector.correct(text))
        return
    
    if args.batch:
        # Batch mode
        extra = ['--batch', args.batch, '--quiet']
        if args.top1:
            extra.append('--top1')
        elif args.top_k:
            extra.extend(['--top-k', str(args.top_k)])
        
        output = run_batch(args.batch, extra)
        lines = output.strip().split('\n')
        
        for line in lines:
            corrected = corrector.correct(line)
            print(corrected)
        return
    
    # Single input mode
    extra = ['--quiet']
    if args.top1:
        extra.append('--top1')
    elif args.top_k:
        extra.extend(['--top-k', str(args.top_k)])
    
    for line in sys.stdin:
        pinyin = line.strip()
        if pinyin:
            result = run_cli(pinyin, extra)
            corrected = corrector.correct(result)
            print(corrected)

if __name__ == '__main__':
    main()
