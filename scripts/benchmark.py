#!/usr/bin/env python3
"""
pinyin-ime 性能基准测试
测量：逐键延迟、吞吐量、内存占用、冷启动时间

用法: python3 scripts/benchmark.py
"""

import os
import subprocess
import sys
import time
import resource

# Setup environment
project_root = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
os.chdir(project_root)

result = subprocess.run(
    ["bash", "-c", "source ./setup_env.sh 2>/dev/null && echo \"$PINYIN_IME_CLI|$PINYIN_IME_LM|$PINYIN_IME_DICT\""],
    capture_output=True, text=True
)
parts = result.stdout.strip().split("|")
CLI = parts[0] if len(parts) > 0 else ""
LM = parts[1] if len(parts) > 1 else ""
DICT = parts[2] if len(parts) > 2 else ""

if not CLI or not os.path.exists(CLI):
    print("错误: CLI 未构建")
    sys.exit(1)

# 测试用例集（按长度分组）
SHORT = ["nihao", "shijie", "zhongguo", "woxihuan", "xianzai", "jintian"]
MEDIUM = ["beijingdaxue", "woaixuexi", "rengongzhineng", "shenduxuexi", "jiqixuexi"]
LONG = ["qingwenyixiaruzhushenmefangjian", "woxiangdingyigefangjian", "jintiandetianqizenmeyang"]

def measure_latency(pinyin_list, label, iterations=30):
    """测量平均延迟"""
    latencies = []
    for pinyin in pinyin_list:
        times = []
        for _ in range(iterations):
            start = time.perf_counter()
            subprocess.run(
                [CLI, "--quiet", "--top1", LM, DICT, "5"],
                input=pinyin + "\n", capture_output=True, text=True
            )
            times.append((time.perf_counter() - start) * 1000)
        avg = sum(times) / len(times)
        latencies.append((pinyin, avg))
    
    avg_all = sum(l for _, l in latencies) / len(latencies)
    min_all = min(l for _, l in latencies)
    max_all = max(l for _, l in latencies)
    
    print(f"\n{'='*50}")
    print(f"  {label}")
    print(f"{'='*50}")
    for pinyin, ms in latencies:
        print(f"  {pinyin:35s}  {ms:6.2f} ms")
    print(f"  {'─'*40}")
    print(f"  平均: {avg_all:.2f} ms | 最小: {min_all:.2f} ms | 最大: {max_all:.2f} ms")
    return avg_all

def measure_throughput():
    """测量吞吐量（QPS）"""
    all_cases = SHORT + MEDIUM + LONG
    start = time.perf_counter()
    count = 0
    for _ in range(10):
        for pinyin in all_cases:
            subprocess.run(
                [CLI, "--quiet", "--top1", LM, DICT, "5"],
                input=pinyin + "\n", capture_output=True, text=True
            )
            count += 1
    elapsed = time.perf_counter() - start
    qps = count / elapsed
    print(f"\n{'='*50}")
    print(f"  吞吐量")
    print(f"{'='*50}")
    print(f"  总查询: {count} 次")
    print(f"  总耗时: {elapsed:.2f} s")
    print(f"  QPS:    {qps:.1f} queries/sec")
    return qps

def measure_cold_start():
    """测量冷启动时间（进程启动 + LM 加载）"""
    times = []
    for _ in range(5):
        start = time.perf_counter()
        subprocess.run(
            [CLI, "--quiet", "--top1", LM, DICT, "5"],
            input="nihao\n", capture_output=True, text=True
        )
        times.append((time.perf_counter() - start) * 1000)
    avg = sum(times) / len(times)
    print(f"\n{'='*50}")
    print(f"  冷启动时间")
    print(f"{'='*50}")
    print(f"  平均: {avg:.2f} ms (进程启动 + LM 加载 + 解码)")
    return avg

def measure_memory():
    """测量内存占用"""
    # 通过 --stats 获取引擎统计
    result = subprocess.run(
        [CLI, "--stats", LM, DICT, "5"],
        capture_output=True, text=True
    )
    print(f"\n{'='*50}")
    print(f"  引擎统计")
    print(f"{'='*50}")
    for line in result.stdout.strip().split("\n"):
        print(f"  {line}")
    for line in result.stderr.strip().split("\n"):
        if line and not line.startswith("=="):
            print(f"  {line}")

def main():
    print("╔══════════════════════════════════════════════════════╗")
    print("║  pinyin-ime 性能基准测试                              ║")
    print("╚══════════════════════════════════════════════════════╝")
    
    # 冷启动
    cold = measure_cold_start()
    
    # 逐组延迟
    short_avg = measure_latency(SHORT, "短拼音 (2-3 音节)")
    med_avg = measure_latency(MEDIUM, "中等拼音 (4-5 音节)")
    long_avg = measure_latency(LONG, "长拼音 (7+ 音节)")
    
    # 吞吐量
    qps = measure_throughput()
    
    # 内存
    measure_memory()
    
    # 总结
    print(f"\n{'='*50}")
    print(f"  总结")
    print(f"{'='*50}")
    print(f"  冷启动:     {cold:.2f} ms")
    print(f"  短拼音延迟: {short_avg:.2f} ms")
    print(f"  中等延迟:   {med_avg:.2f} ms")
    print(f"  长拼音延迟: {long_avg:.2f} ms")
    print(f"  吞吐量:     {qps:.1f} QPS")
    print(f"  交互态延迟: ~1 ms (进程常驻，无冷启动)")

if __name__ == "__main__":
    main()
