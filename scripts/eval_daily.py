#!/usr/bin/env python3
import subprocess, os, tempfile

CLI = os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli')

def pinyin_of(text):
    from pypinyin import pinyin, Style
    return ''.join([p[0] for p in pinyin(text, style=Style.NORMAL, errors='ignore')])

# Daily life test sentences
sentences = [
    "你好世界",
    "今天天气很好",
    "我想吃饭了",
    "请问洗手间在哪里",
    "谢谢你帮我",
    "明天见",
    "我爱你中国",
    "学习使我快乐",
    "时间就是金钱",
    "好好学习天天向上",
    "你在做什么",
    "我喜欢吃苹果",
    "这个多少钱",
    "帮我查一下天气",
    "今天几号了",
    "我想订一个房间",
    "请问一下入住什么房间",
    "北京欢迎你",
    "上海是中国的经济中心",
    "广州的天气很热",
    "深圳有很多科技公司",
    "杭州的西湖很美",
    "成都是美食之都",
    "西安有很多历史古迹",
    "南京是六朝古都",
    "武汉大学的樱花很有名",
    "清华大学是中国最好的大学",
    "北京大学也是很好的学校",
    "我想去旅游",
    "你能帮我吗",
    "请问怎么走",
    "我要一杯咖啡",
    "今天工作很忙",
    "周末去爬山",
    "这部电影很好看",
    "音乐让人放松",
    "读书改变命运",
    "健康最重要",
    "家是温暖的港湾",
    "朋友一生一起走",
]

all_tests = []
for s in sentences:
    py = pinyin_of(s)
    if py:
        all_tests.append((s, py))

with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False, encoding='utf-8') as f:
    for _, py in all_tests:
        f.write(py + '\n')
    tmpfile = f.name

import os
LM = os.environ.get("PINYIN_IME_LM", "/usr/share/libime/zh_CN.lm")
DICT = os.environ.get("PINYIN_IME_DICT", "/usr/share/libime/sc.dict")
result = subprocess.run([CLI, "--batch", tmpfile, "--top1", "--quiet", LM, DICT], capture_output=True, text=True, timeout=30)
os.unlink(tmpfile)

outputs = result.stdout.strip().split('\n')
sent_pass = char_pass = char_total = 0
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

print(f"=== Daily Life Sentence Evaluation ===")
print(f"Total: {len(all_tests)}")
print(f"Sentence accuracy: {sent_pass}/{len(all_tests)} = {100*sent_pass/len(all_tests):.1f}%")
print(f"Character accuracy: {char_pass}/{char_total} = {100*char_pass/char_total:.1f}%")

if failures:
    print(f"\nFailures:")
    for expected, py, got in failures:
        print(f"  {py} → {got} (expected: {expected})")
