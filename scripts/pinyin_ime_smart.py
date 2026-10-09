#!/usr/bin/env python3
"""
pinyin-ime 智能预测入口 (v0.9.68)
统一集成: 长句切分 → CLI预测 → 频率boost → 后纠错 → 输出

v0.9.68 新增:
  - 长句切分: 自动在边界音节后插入空格，提升长句准确率
  - 频率boost增强: top1模式内部取top-k→boost→返回top1
  - pinyin→word 映射: 记录哪个拼音选了哪个词，精确匹配优先提升

用法:
  # 单句 top1
  echo "zhegewentineyaizaizayanjiu" | python3 scripts/pinyin_ime_smart.py --top1

  # 批量 (文件每行一个拼音)
  python3 scripts/pinyin_ime_smart.py --batch input.txt --top1

  # 批量 + 记录频率
  python3 scripts/pinyin_ime_smart.py --batch input.txt --top1 --record-freq

  # 多候选 + 频率boost
  echo "nihao" | python3 scripts/pinyin_ime_smart.py --top-k 5

  # 仅后纠错
  echo "这个问题需要在研究" | python3 scripts/pinyin_ime_smart.py --correct-only

  # 禁用长句切分
  echo "verylongpinyin" | python3 scripts/pinyin_ime_smart.py --top1 --no-split
"""
import sys, os, subprocess, argparse

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from post_corrector import PostCorrector
from user_freq import UserFreqTracker
from sentence_splitter import SentenceSplitter

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

# top1 模式内部取 top-k 用于频率 boost
TOP1_INTERNAL_K = 5


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


def predict_single(pinyin, top1=False, top_k=None, corrector=None, 
                   freq_tracker=None, splitter=None, record_freq=False):
    """预测单个拼音
    
    流程: 长句切分 → CLI预测 → 频率boost → 后纠错 → 输出
    """
    # 1. 长句切分：在边界音节后插入空格
    if splitter:
        pinyin = splitter.process(pinyin)
    
    # 2. CLI 预测
    if top1 and freq_tracker:
        # top1 模式 + 频率 boost：内部取 top-k → boost → 返回 top1
        extra = ['--quiet', '--top-k', str(TOP1_INTERNAL_K)]
        result = run_cli_single(pinyin, extra)
        
        if '\t' in result:
            candidates = result.split('\t')
            boosted = freq_tracker.boost_candidates(candidates, pinyin)
            top1_result = boosted[0]
        else:
            top1_result = result
    elif top1:
        extra = ['--quiet', '--top1']
        top1_result = run_cli_single(pinyin, extra)
    elif top_k:
        extra = ['--quiet', '--top-k', str(top_k)]
        result = run_cli_single(pinyin, extra)
        
        if '\t' in result:
            candidates = result.split('\t')
            if freq_tracker:
                candidates = freq_tracker.boost_candidates(candidates, pinyin)
            # 后纠错 top1
            if corrector:
                candidates[0] = corrector.correct(candidates[0])
            return '\t'.join(candidates), candidates[0] if candidates else ''
        else:
            if corrector:
                result = corrector.correct(result)
            return result, result
    else:
        # 默认模式
        extra = ['--quiet', '--top1']
        top1_result = run_cli_single(pinyin, extra)
    
    # 3. 后纠错
    if corrector:
        top1_result = corrector.correct(top1_result)
    
    # 4. 记录频率
    if record_freq and freq_tracker and top1_result and not top1_result.startswith('ERROR'):
        freq_tracker.record(top1_result, pinyin=pinyin)
    
    return top1_result, top1_result


def main():
    parser = argparse.ArgumentParser(description='pinyin-ime 智能预测 (长句切分 + CLI + 频率boost + 后纠错)')
    parser.add_argument('--top1', action='store_true', help='Return top-1 candidate only')
    parser.add_argument('--top-k', type=int, help='Return top-K candidates')
    parser.add_argument('--batch', type=str, help='Batch mode: read pinyin from file')
    parser.add_argument('--correct-only', action='store_true', help='Only apply post-correction to stdin text')
    parser.add_argument('--no-correct', action='store_true', help='Disable post-correction')
    parser.add_argument('--no-freq', action='store_true', help='Disable frequency boost')
    parser.add_argument('--no-split', action='store_true', help='Disable long sentence splitting')
    parser.add_argument('--record-freq', action='store_true', help='Record word frequency (with pinyin mapping)')
    parser.add_argument('--quiet', action='store_true', help='Quiet mode')
    args = parser.parse_args()

    corrector = PostCorrector(enabled=not args.no_correct)
    freq_tracker = None if args.no_freq else UserFreqTracker()
    splitter = None if args.no_split else SentenceSplitter()

    if args.correct_only:
        for line in sys.stdin:
            text = line.strip()
            if text:
                print(corrector.correct(text))
        return

    if args.batch:
        # Batch mode
        # 读取输入文件
        with open(args.batch) as f:
            pinyin_lines = [l.strip() for l in f if l.strip()]
        
        # 长句切分处理
        if splitter:
            processed_lines = [splitter.process(p) for p in pinyin_lines]
        else:
            processed_lines = pinyin_lines
        
        # 写入临时文件用于 batch 模式
        import tempfile
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False) as tmp:
            tmp.write('\n'.join(processed_lines))
            tmp_path = tmp.name
        
        try:
            if args.top1 and freq_tracker:
                # top1 + 频率 boost: 内部取 top-k
                extra = ['--quiet', '--top-k', str(TOP1_INTERNAL_K)]
                output = run_cli_batch(tmp_path, extra, timeout=300)
                
                for i, line in enumerate(output.strip().split('\n')):
                    original_py = pinyin_lines[i] if i < len(pinyin_lines) else ''
                    if '\t' in line:
                        candidates = line.split('\t')
                        boosted = freq_tracker.boost_candidates(candidates, original_py)
                        result = boosted[0]
                    else:
                        result = line
                    
                    if corrector:
                        result = corrector.correct(result)
                    if args.record_freq and freq_tracker and result:
                        freq_tracker.record(result, pinyin=original_py)
                    print(result)
            elif args.top1:
                extra = ['--quiet', '--top1']
                output = run_cli_batch(tmp_path, extra, timeout=300)
                
                for i, line in enumerate(output.strip().split('\n')):
                    if corrector:
                        line = corrector.correct(line)
                    print(line)
            elif args.top_k:
                extra = ['--quiet', '--top-k', str(args.top_k)]
                output = run_cli_batch(tmp_path, extra, timeout=300)
                
                for i, line in enumerate(output.strip().split('\n')):
                    original_py = pinyin_lines[i] if i < len(pinyin_lines) else ''
                    if '\t' in line:
                        candidates = line.split('\t')
                        if freq_tracker:
                            candidates = freq_tracker.boost_candidates(candidates, original_py)
                        if corrector:
                            candidates[0] = corrector.correct(candidates[0])
                        print('\t'.join(candidates))
                    else:
                        if corrector:
                            line = corrector.correct(line)
                        print(line)
            else:
                extra = ['--quiet', '--top1']
                output = run_cli_batch(tmp_path, extra, timeout=300)
                for line in output.strip().split('\n'):
                    if corrector:
                        line = corrector.correct(line)
                    print(line)
        finally:
            os.unlink(tmp_path)
        
        if freq_tracker:
            freq_tracker.save()
        return

    # Single input mode (stdin)
    for line in sys.stdin:
        pinyin = line.strip()
        if pinyin:
            result, _ = predict_single(
                pinyin, 
                top1=args.top1, 
                top_k=args.top_k,
                corrector=corrector,
                freq_tracker=freq_tracker,
                splitter=splitter,
                record_freq=args.record_freq
            )
            print(result)

    if freq_tracker:
        freq_tracker.save()


if __name__ == '__main__':
    main()
