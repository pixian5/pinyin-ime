#!/usr/bin/env python3.9
"""
pinyin-ime 性能测试
测量：
  1. CLI 响应延迟（冷启动 + 热启动）
  2. 内存占用
  3. 吞吐量（每秒处理词数）
  4. 首词准确率

用法: python3.9 scripts/performance_test.py
"""

import subprocess
import time
import os
import sys
import statistics

PROJECT_ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
CLI = os.path.join(PROJECT_ROOT, "pinyin-ime-s3", "pinyin_ime_cli_v0.9.83")
LM = os.path.join(PROJECT_ROOT, "pinyin-ime-s3", "zh_CN.lm")
DICT = os.path.join(PROJECT_ROOT, "pinyin-ime-s3", "sc.dict")

# Set library path
LIBIME = "/root/workspace/native-build/opt/libime/lib64"
DEPSLIB = "/root/.local/pinyin-ime/opt/deps-lib"
BUNDLE = "/root/.local/pinyin-ime/opt/fcitx5-x11"
ENV_DIR = "/root/workspace/native-build/mamba-env"
os.environ["LD_LIBRARY_PATH"] = f"{LIBIME}:{DEPSLIB}:{BUNDLE}/lib:{ENV_DIR}/lib"

# Test words
TEST_WORDS = [
    "nihao", "zhongguo", "shurufa", "shijie", "pinyin",
    "beijing", "shanghai", "zhongwen", "diannao", "jianpan",
    "shubiao", "wangluo", "youxiang", "mima", "yonghu",
    "xitong", "ruanjian", "yingjian", "chengxu", "wenjian",
]

EXPECTED = {
    "nihao": "你好", "zhongguo": "中国", "shurufa": "输入法",
    "shijie": "世界", "pinyin": "拼音", "beijing": "北京",
    "shanghai": "上海", "zhongwen": "中文", "diannao": "电脑",
    "jianpan": "键盘", "shubiao": "鼠标", "wangluo": "网络",
    "youxiang": "邮箱", "mima": "密码", "yonghu": "用户",
    "xitong": "系统", "ruanjian": "软件", "yingjian": "硬件",
    "chengxu": "程序", "wenjian": "文件",
}


def run_cli_once(pinyin):
    """Run CLI for a single word and return (result, elapsed)"""
    start = time.perf_counter()
    result = subprocess.run(
        [CLI, LM, DICT],
        input=pinyin + "\n",
        capture_output=True, text=True, timeout=10
    )
    elapsed = time.perf_counter() - start
    lines = result.stdout.strip().split('\n')
    first = lines[0].split('\t')[0] if lines and '\t' in lines[0] else (lines[0] if lines else "")
    return first, elapsed


def get_process_memory():
    """Get fcitx5 process memory usage in KB"""
    try:
        result = subprocess.run(
            ["ps", "-C", "fcitx5", "-o", "rss="],
            capture_output=True, text=True
        )
        if result.stdout.strip():
            return int(result.stdout.strip().split('\n')[0])
    except:
        pass
    return 0


def main():
    print("=== pinyin-ime 性能测试 ===")
    print()

    # 1. Cold start (first call)
    print("--- 1. 冷启动延迟 ---")
    cold_result, cold_time = run_cli_once(TEST_WORDS[0])
    print(f"  首次调用: {cold_time*1000:.1f}ms → {cold_result}")
    print()

    # 2. Warm start (subsequent calls)
    print("--- 2. 热启动延迟 ---")
    warm_times = []
    for word in TEST_WORDS[1:]:
        _, t = run_cli_once(word)
        warm_times.append(t)

    avg_warm = statistics.mean(warm_times)
    median_warm = statistics.median(warm_times)
    min_warm = min(warm_times)
    max_warm = max(warm_times)
    p95_warm = sorted(warm_times)[int(len(warm_times) * 0.95)]

    print(f"  平均:   {avg_warm*1000:.1f}ms")
    print(f"  中位数: {median_warm*1000:.1f}ms")
    print(f"  最小:   {min_warm*1000:.1f}ms")
    print(f"  最大:   {max_warm*1000:.1f}ms")
    print(f"  P95:    {p95_warm*1000:.1f}ms")
    print()

    # 3. Throughput
    print("--- 3. 吞吐量 ---")
    batch_start = time.perf_counter()
    for word in TEST_WORDS * 5:  # 100 words
        run_cli_once(word)
    batch_time = time.perf_counter() - batch_start
    throughput = len(TEST_WORDS) * 5 / batch_time
    print(f"  {len(TEST_WORDS) * 5} 词耗时: {batch_time:.2f}s")
    print(f"  吞吐量: {throughput:.1f} 词/秒")
    print()

    # 4. Accuracy
    print("--- 4. 首词准确率 ---")
    correct = 0
    total = 0
    for word, expected in EXPECTED.items():
        result, _ = run_cli_once(word)
        total += 1
        if expected in result:
            correct += 1

    accuracy = correct / total * 100
    print(f"  正确: {correct}/{total}")
    print(f"  准确率: {accuracy:.1f}%")
    print()

    # 5. Memory usage
    print("--- 5. 内存占用 ---")
    fcitx5_mem = get_process_memory()
    cli_mem = 0
    # Run CLI and check its memory
    proc = subprocess.Popen(
        [CLI, LM, DICT],
        stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.DEVNULL,
        text=True
    )
    proc.stdin.write("nihao\n")
    proc.stdin.flush()
    time.sleep(0.1)
    try:
        cli_mem_result = subprocess.run(
            ["ps", "-p", str(proc.pid), "-o", "rss="],
            capture_output=True, text=True
        )
        if cli_mem_result.stdout.strip():
            cli_mem = int(cli_mem_result.stdout.strip())
    except:
        pass
    proc.terminate()
    proc.wait()

    print(f"  fcitx5:  {fcitx5_mem} KB ({fcitx5_mem/1024:.1f} MB)")
    print(f"  CLI:     {cli_mem} KB ({cli_mem/1024:.1f} MB)")
    print()

    # 6. Data file sizes
    print("--- 6. 数据文件大小 ---")
    lm_size = os.path.getsize(LM) / 1024 / 1024
    dict_size = os.path.getsize(DICT) / 1024 / 1024
    print(f"  zh_CN.lm:  {lm_size:.1f} MB")
    print(f"  sc.dict:   {dict_size:.1f} MB")
    print()

    # Summary
    print("=== 性能总结 ===")
    print(f"  冷启动:    {cold_time*1000:.1f}ms")
    print(f"  热启动:    {avg_warm*1000:.1f}ms (avg)")
    print(f"  吞吐量:    {throughput:.1f} 词/秒")
    print(f"  准确率:    {accuracy:.1f}%")
    print(f"  fcitx5内存: {fcitx5_mem/1024:.1f} MB")
    print(f"  CLI内存:   {cli_mem/1024:.1f} MB")

    return True


if __name__ == "__main__":
    success = main()
    sys.exit(0 if success else 1)
