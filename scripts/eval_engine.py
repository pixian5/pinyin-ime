#!/usr/bin/env python3
"""
Evaluate pinyin-ime accuracy using the actual CLI engine (batch mode).
"""
import subprocess
import sys
import os
import tempfile

CLI = os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli')

def pinyin_of(text):
    from pypinyin import pinyin, Style
    result = pinyin(text, style=Style.NORMAL, errors='ignore')
    return ''.join([p[0] for p in result])

def main():
    corpus_files = ['bench/corpus/news.txt', 'bench/corpus/wiki.txt']
    
    all_tests = []
    for cf in corpus_files:
        if not os.path.exists(cf):
            continue
        with open(cf, 'r', encoding='utf-8') as f:
            lines = [l.strip() for l in f if l.strip()]
        for line in lines[:100]:
            py = pinyin_of(line)
            if py and len(py) > 2:
                all_tests.append((line, py))
    
    # Write pinyin to temp file and run batch mode
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False, encoding='utf-8') as f:
        for _, py in all_tests:
            f.write(py + '\n')
        tmpfile = f.name
    
    result = subprocess.run([CLI, '--batch', tmpfile, '--top1'], capture_output=True, text=True, timeout=60)
    os.unlink(tmpfile)
    
    outputs = result.stdout.strip().split('\n')
    
    total = len(all_tests)
    sent_pass = 0
    char_total = 0
    char_pass = 0
    failures = []
    
    for i, (expected, py) in enumerate(all_tests):
        got = outputs[i].strip() if i < len(outputs) else ""
        
        if got == expected:
            sent_pass += 1
        else:
            failures.append((expected, py, got))
        
        for j, c in enumerate(expected):
            char_total += 1
            if j < len(got) and got[j] == c:
                char_pass += 1
    
    print(f"=== Engine Evaluation (CLI batch mode) ===")
    print(f"Total sentences: {total}")
    print(f"Sentence accuracy: {sent_pass}/{total} = {100*sent_pass/total:.1f}%")
    print(f"Character accuracy: {char_pass}/{char_total} = {100*char_pass/char_total:.1f}%")
    
    if failures:
        print(f"\nFirst 15 failures:")
        for expected, py, got in failures[:15]:
            print(f"  {py[:40]}...")
            print(f"    Expected: {expected[:40]}")
            print(f"    Got:      {got[:40]}")

if __name__ == '__main__':
    main()
