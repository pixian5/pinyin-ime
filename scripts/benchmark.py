#!/usr/bin/env python3
"""
pinyin-ime 性能基准测试
测量：逐键延迟、吞吐量、内存占用、首词命中率
"""
import subprocess
import sys
import os
import time
import tempfile
import statistics

CLI = os.path.join(os.path.dirname(__file__), '..', 'cpp', 'build', 'pinyin_ime_cli')
LD = "/root/workspace/native-build/mamba-env/lib:/root/workspace/native-build/opt/fcitx5/lib:/root/workspace/native-build/opt/libime/lib"

os.environ['LD_LIBRARY_PATH'] = LD + ':' + os.environ.get('LD_LIBRARY_PATH', '')

# 测试用例（拼音 → 期望首选）
TESTS = [
    ("nihao", "你好"),
    ("shijie", "世界"),
    ("zhongguo", "中国"),
    ("beijingdaxue", "北京大学"),
    ("xiexie", "谢谢"),
    ("woaini", "我爱你"),
    ("zaijian", "再见"),
    ("mingbai", "明白"),
    ("woshizhongguoren", "我是中国人"),
    ("woxiangdingyigefangjian", "我想订一个房间"),
    ("nihaozhongguo", "你好中国"),
    ("jintiandetianqi", "今天的天气"),
    ("woaixuexi", "我爱学习"),
    ("qingbangwochaxunyixia", "请帮我查询一下"),
    ("xuesheng", "学生"),
    ("laoshi", "老师"),
    ("keji", "科技"),
    ("zhongguoren", "中国人"),
    ("shanghai", "上海"),
    ("beijing", "北京"),
]

def run_batch(pinyins, extra_args=None):
    """批量运行CLI，返回结果列表和耗时"""
    extra_args = extra_args or []
    with tempfile.NamedTemporaryFile(mode='w', suffix='.txt', delete=False, encoding='utf-8') as f:
        for py in pinyins:
            f.write(py + '\n')
        tmpfile = f.name
    
    start = time.perf_counter()
    result = subprocess.run(
        [CLI, '--batch', tmpfile, '--top1', '--quiet'] + extra_args,
        capture_output=True, text=True, timeout=30
    )
    elapsed = time.perf_counter() - start
    os.unlink(tmpfile)
    
    outputs = result.stdout.strip().split('\n')
    return outputs, elapsed

def benchmark_latency():
    """逐键延迟测试：逐个字符输入测量延迟"""
    print("=== 逐键延迟测试 ===")
    test_word = "nihaozhongguo"
    latencies = []
    
    for i in range(1, len(test_word) + 1):
        partial = test_word[:i]
        # 运行多次取平均
        times = []
        for _ in range(5):
            start = time.perf_counter()
            subprocess.run(
                [CLI, '--top1', '--quiet'],
                input=partial + '\n', capture_output=True, text=True, timeout=5
            )
            times.append(time.perf_counter() - start)
        latencies.append(statistics.median(times))
    
    avg = statistics.mean(latencies) * 1000
    p50 = statistics.median(latencies) * 1000
    p99 = max(latencies) * 1000
    
    print(f"  输入: {test_word} ({len(test_word)} 键)")
    print(f"  平均延迟: {avg:.2f} ms/键")
    print(f"  P50 延迟: {p50:.2f} ms/键")
    print(f"  P99 延迟: {p99:.2f} ms/键")
    print(f"  总耗时: {sum(latencies)*1000:.1f} ms")
    return avg, p50, p99

def benchmark_accuracy(tests, extra_args=None, label=""):
    """准确率测试"""
    pinyins = [py for py, _ in tests]
    outputs, elapsed = run_batch(pinyins, extra_args)
    
    correct = 0
    char_total = 0
    char_correct = 0
    failures = []
    
    for i, (py, expected) in enumerate(tests):
        got = outputs[i].strip() if i < len(outputs) else ""
        if got == expected:
            correct += 1
        else:
            failures.append((py, expected, got))
        for j, c in enumerate(expected):
            char_total += 1
            if j < len(got) and got[j] == c:
                char_correct += 1
    
    total = len(tests)
    sent_acc = 100 * correct / total
    char_acc = 100 * char_correct / char_total
    
    print(f"=== 准确率测试 {label} ===")
    print(f"  句级: {correct}/{total} = {sent_acc:.1f}%")
    print(f"  字级: {char_correct}/{char_total} = {char_acc:.1f}%")
    print(f"  耗时: {elapsed*1000:.1f} ms ({elapsed*1000/total:.2f} ms/句)")
    
    if failures:
        print(f"  失败 ({len(failures)}):")
        for py, exp, got in failures[:5]:
            print(f"    {py} → {got} (期望: {exp})")
    
    return sent_acc, char_acc

def benchmark_throughput():
    """吞吐量测试"""
    print("=== 吞吐量测试 ===")
    # 生成1000个短拼音
    words = ["nihao", "shijie", "zhongguo", "beijing", "shanghai", 
             "xiexie", "zaijian", "woaini", "mingbai", "keyi"] * 100
    
    _, elapsed = run_batch(words)
    rate = len(words) / elapsed
    
    print(f"  {len(words)} 句 / {elapsed:.3f}s = {rate:.0f} 句/秒")
    print(f"  平均: {elapsed*1000/len(words):.2f} ms/句")
    return rate

def benchmark_features():
    """各功能模式测试"""
    print("=== 功能模式性能 ===")
    modes = [
        ([], "全拼"),
        (["--fuzzy"], "模糊音"),
        (["--correct"], "纠错"),
        (["--fuzzy", "--correct"], "模糊+纠错"),
        (["--shuangpin"], "双拼(小鹤)"),
    ]
    
    for args, label in modes:
        pinyins = [py for py, _ in TESTS[:10]]
        _, elapsed = run_batch(pinyins, args)
        print(f"  {label}: {elapsed*1000/len(pinyins):.2f} ms/句")

def main():
    print("=" * 50)
    print("  pinyin-ime 性能基准测试")
    print(f"  {time.strftime('%Y-%m-%d %H:%M:%S')}")
    print("=" * 50)
    
    # 版本
    r = subprocess.run([CLI, "--version"], capture_output=True, text=True, timeout=5)
    print(f"  版本: {r.stdout.strip()}")
    print()
    
    # 1. 准确率
    benchmark_accuracy(TESTS, label="(全拼)")
    print()
    
    # 2. 模糊音准确率
    fuzzy_tests = [("cifan", "吃饭"), ("zeli", "这里"), ("lanfang", "南方")]
    benchmark_accuracy(fuzzy_tests, ["--fuzzy"], label="(模糊音)")
    print()
    
    # 3. 纠错准确率
    correct_tests = [("senme", "什么"), ("shengri", "生日")]
    benchmark_accuracy(correct_tests, ["--correct"], label="(纠错)")
    print()
    
    # 4. 逐键延迟
    benchmark_latency()
    print()
    
    # 5. 吞吐量
    benchmark_throughput()
    print()
    
    # 6. 各模式性能
    benchmark_features()
    print()
    
    print("=" * 50)
    print("  基准测试完成")
    print("=" * 50)

if __name__ == "__main__":
    main()
