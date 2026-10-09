#!/usr/bin/env python3
"""
pinyin-ime 智能预测入口 (v0.9.67)
统一集成 CLI预测 → 后纠错 → 频率boost → 输出

这是 pinyin-ime 的推荐使用方式，比直接调用 CLI 更准确。

用法:
  # 单句
  echo "zhegewentineyaizaizayanjiu" | python3 scripts/pinyin_ime_smart.py --top1

  # 批量 (文件每行一个拼音)
  python3 scripts/pinyin_ime_smart.py --batch input.txt --top1

  # 批量 + 记录频率
  python3 scripts/pinyin_ime_smart.py --batch input.txt --top1 --record-freq

  # 多候选 + 频率boost
  echo "nihao" | python3 scripts/pinyin_ime_smart.py --top-k 5

  # 仅后纠错
  echo "这个问题需要在研究" | python3 scripts/pinyin_ime_smart.py --correct-only
"""
import sys, os, subprocess, argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from post_corrector import PostCorrector
from user_freq import UserFreqTracker

# Setup paths
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
result = subprocess.run(
    ["bash", "-c", f"cd {project_root} && source ./setup_env.sh 2>/dev/null && echo \"$PINYIN_IME_CLI|$PINYIN_IME_LM|$PINYIN_IME_DICT\""],
    capture_output=True, text=True
)
parts = result.stdout.strip().split("|")
CLI = parts[0] if len(parts) > 0 and parts[0] else os.path.join(project_root, "cpp", "build", "pinyin_ime_cli")
LM = parts[1] if len(parts) > 1 and parts[1] else "/usr/share/libime/zh_CN.lm"
DICT = parts[2] if len(parts) > 2 and parts[2] else "/usr/share/libime/sc.dict"


def run_cli_single(pinyin, extra_args, timeout=10):
    """Run CLI for single pinyin input"""
    try:
        result = subprocess.run(
            [CLI] + extra_args + [LM, DICT],
            input=pinyin, capture_output=True, text=True, timeout=timeout
        )
        return result.stdout.strip()
    except Exception as e:
        return f"ERROR: {e}"


def run_cli_batch(filepath, extra_args, timeout=120):
    """Run CLI in batch mode"""
    try:
        result = subprocess.run(
            [CLI, "--batch", filepath] + extra_args + [LM, DICT],
            capture_output=True, text=True, timeout=timeout
        )
        return result.stdout.strip()
    except Exception as e:
        return f"ERROR: {e}"


def main():
    parser = argparse.ArgumentParser(description='pinyin-ime 智能预测 (CLI + 后纠错 + 频率boost)')
    parser.add_argument('--top1', action='store_true', help='Return top-1 candidate only')
    parser.add_argument('--top-k', type=int, help='Return top-K candidates')
    parser.add_argument('--batch', type=str, help='Batch mode: read pinyin from file')
    parser.add_argument('--correct-only', action='store_true', help='Only apply post-correction to stdin text')
    parser.add_argument('--no-correct', action='store_true', help='Disable post-correction')
    parser.add_argument('--no-freq', action='store_true', help='Disable frequency boost')
    parser.add_argument('--record-freq', action='store_true', help='Record word frequency (batch mode)')
    parser.add_argument('--quiet', action='store_true', help='Quiet mode')
    args = parser.parse_args()

    corrector = PostCorrector(enabled=not args.no_correct)
    freq_tracker = None if args.no_freq else UserFreqTracker()

    if args.correct_only:
        for line in sys.stdin:
            text = line.strip()
            if text:
                print(corrector.correct(text))
        return

    if args.batch:
        # Batch mode
        extra = ['--quiet']
        if args.top1:
            extra.append('--top1')
        elif args.top_k:
            extra.extend(['--top-k', str(args.top_k)])

        output = run_cli_batch(args.batch, extra)
        lines = output.strip().split('\n')

        for line in lines:
            # Apply frequency boost for multi-candidate
            if args.top_k and '\t' in line:
                candidates = line.split('\t')
                if freq_tracker:
                    candidates = freq_tracker.boost_candidates(candidates)
                # Apply correction to top candidate
                if corrector:
                    candidates[0] = corrector.correct(candidates[0])
                print('\t'.join(candidates))
            else:
                # Single candidate
                corrected = corrector.correct(line)
                # Record frequency
                if args.record_freq and freq_tracker and corrected:
                    freq_tracker.record(corrected)
                print(corrected)

        if args.record_freq and freq_tracker:
            freq_tracker.save()
        return

    # Single input mode (stdin)
    extra = ['--quiet']
    if args.top1:
        extra.append('--top1')
    elif args.top_k:
        extra.extend(['--top-k', str(args.top_k)])

    for line in sys.stdin:
        pinyin = line.strip()
        if pinyin:
            result = run_cli_single(pinyin, extra)

            if args.top_k and '\t' in result:
                candidates = result.split('\t')
                if freq_tracker:
                    candidates = freq_tracker.boost_candidates(candidates)
                if corrector:
                    candidates[0] = corrector.correct(candidates[0])
                print('\t'.join(candidates))
            else:
                corrected = corrector.correct(result)
                if freq_tracker and corrected:
                    freq_tracker.record(corrected)
                print(corrected)

    if freq_tracker:
        freq_tracker.save()


if __name__ == '__main__':
    main()
