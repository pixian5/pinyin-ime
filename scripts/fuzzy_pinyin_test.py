#!/usr/bin/env python3.9
"""
模糊音 / 多模式输入测试
通过 CLI 工具测试不同输入模式：
  - 正常全拼
  - 简拼（首字母缩写）
  - 多字词组
  - 英文混排

用法: python3.9 scripts/fuzzy_pinyin_test.py
"""

import subprocess
import sys
import os

CLI = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "pinyin-ime-s3", "pinyin_ime_cli_v0.9.83")
LM = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "pinyin-ime-s3", "zh_CN.lm")
DICT = os.path.join(os.path.dirname(os.path.dirname(os.path.abspath(__file__))), "pinyin-ime-s3", "sc.dict")

# (name, input, expected_first_candidate)
TEST_CASES = [
    # 正常全拼
    ("全拼 nihao",      "nihao",      "你好"),
    ("全拼 zhongguo",   "zhongguo",   "中国"),
    ("全拼 shurufa",    "shurufa",    "输入法"),
    ("全拼 shijie",     "shijie",     "世界"),
    ("全拼 diannao",    "diannao",    "电脑"),
    ("全拼 jianpan",    "jianpan",    "键盘"),
    ("全拼 shubiao",    "shubiao",    "鼠标"),
    ("全拼 zhongwen",   "zhongwen",   "中文"),
    ("全拼 pinyin",     "pinyin",     "拼音"),
    ("全拼 beijing",    "beijing",    "北京"),
    ("全拼 shanghai",   "shanghai",   "上海"),
    # 简拼（首字母）
    ("简拼 zg",         "zg",         None),    # 可能匹配"中国"
    ("简拼 nh",         "nh",         None),    # 可能匹配"你好"
    ("简拼 sj",         "sj",         None),    # 可能匹配"世界"
    # 多字
    ("多字 woshizhongguoren", "woshizhongguoren", None),
    ("多字 zhongguorenmin",   "zhongguorenmin",   None),
    # 英文
    ("英文 hello",     "hello",      None),
    ("英文 test",      "test",       None),
]


def run_cli(pinyin):
    """Run CLI and return first candidate"""
    try:
        result = subprocess.run(
            [CLI, LM, DICT],
            input=pinyin + "\n",
            capture_output=True, text=True, timeout=5
        )
        lines = result.stdout.strip().split('\n')
        if lines and lines[0]:
            # First column of first line
            return lines[0].split('\t')[0] if '\t' in lines[0] else lines[0]
        return ""
    except Exception as e:
        return f"ERROR: {e}"


def main():
    print("=== 模糊音 / 多模式输入测试 ===")
    print(f"{'='*65}")
    print(f"{'测试名':<28} {'输入':<22} {'结果':<12} {'状态'}")
    print(f"{'='*65}")

    passed = 0
    total = len(TEST_CASES)

    for name, pinyin, expected in TEST_CASES:
        result = run_cli(pinyin)

        if expected:
            if expected in result:
                status = "✅"
                passed += 1
            else:
                status = "❌"
        else:
            # No specific expectation, just check it doesn't error
            if "ERROR" not in result and result:
                status = "✅"
                passed += 1
            else:
                status = "❌"

        display_result = result[:10] + "..." if len(result) > 10 else result
        print(f"{name:<28} {pinyin:<22} {display_result:<12} {status}")

    print(f"{'='*65}")
    print(f"测试通过: {passed}/{total}")
    return passed == total


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
