#!/usr/bin/env python3
import subprocess, os, tempfile, sys

CLI = os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli')

def pinyin_of(text):
    from pypinyin import pinyin, Style
    return ''.join([p[0] for p in pinyin(text, style=Style.NORMAL, errors='ignore')])

def main():
    corpus_files = ['bench/corpus/news.txt', 'bench/corpus/wiki.txt']
    all_tests = []
    for cf in corpus_files:
        if not os.path.exists(cf): continue
        with open(cf, 'r', encoding='utf-8') as f:
            lines = [l.strip() for l in f if l.strip()]
        for line in lines[:100]:
            py = pinyin_of(line)
            if py and len(py) > 2:
                all_tests.append((line, py))
    
    # Test with different options
    for opts_name, opts in [("baseline", []), ("correct", ["--correct"]), ("fuzzy", ["--fuzzy"])]:
        with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False, encoding='utf-8') as f:
            for _, py in all_tests:
                f.write(py + '\n')
            tmpfile = f.name
        
        result = subprocess.run([CLI, '--batch', tmpfile, '--top1'] + opts, 
                              capture_output=True, text=True, timeout=60)
        os.unlink(tmpfile)
        
        outputs = result.stdout.strip().split('\n')
        sent_pass = char_pass = char_total = 0
        
        for i, (expected, py) in enumerate(all_tests):
            got = outputs[i].strip() if i < len(outputs) else ""
            if got == expected: sent_pass += 1
            for j, c in enumerate(expected):
                char_total += 1
                if j < len(got) and got[j] == c: char_pass += 1
        
        print(f"{opts_name:12s}: sent={100*sent_pass/len(all_tests):.1f}% ({sent_pass}/{len(all_tests)})  char={100*char_pass/char_total:.1f}%")

if __name__ == '__main__':
    main()
