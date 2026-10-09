#!/usr/bin/env python3
"""
拼音音节解析器
将连续拼音串切分为音节列表。

例如:
  "jintiandetianqi" → ["jin", "tian", "de", "tian", "qi"]
  "nihao" → ["ni", "hao"]
  "zhangsan" → ["zhang", "san"]

算法: 基于合法拼音音节表，用动态规划找最优切分。
"""
import re

# 拼音声母 (含零声母的情况由韵母单独成音节处理)
INITIALS = {
    'b', 'p', 'm', 'f',
    'd', 't', 'n', 'l',
    'g', 'k', 'h',
    'j', 'q', 'x',
    'zh', 'ch', 'sh', 'r',
    'z', 'c', 's',
    'y', 'w',
}

# 拼音韵母 (含带声调的韵母，但我们处理无声调拼音)
FINALS = {
    # 单韵母
    'a', 'o', 'e', 'i', 'u', 'ü', 'v',
    # 复韵母
    'ai', 'ei', 'ui', 'ao', 'ou', 'iu', 'ie', 'üe', 've',
    # 鼻韵母
    'an', 'en', 'in', 'un', 'ün', 'vn',
    'ang', 'eng', 'ing', 'ong', 'iong',
    # 其他
    'ia', 'iao', 'ian', 'iang', 'iang',
    'ua', 'uai', 'uan', 'uang', 'uo',
    'üan', 'van', 'n', 'ng',
    'er', 'ue',
}

# 特殊整音节 (零声母，韵母单独成音节)
STANDALONE_SYLLABLES = {
    'a', 'o', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'er',
    'ang', 'eng',
}

# 完整合法音节表 (声母+韵母组合)
# 为了准确性，直接列举所有合法音节
VALID_SYLLABLES = set()

def _build_syllable_table():
    """构建合法拼音音节表"""
    # 声母 → 可搭配的韵母
    combos = {
        'b':  ['a', 'o', 'ai', 'ei', 'ao', 'an', 'en', 'ang', 'eng', 'i', 'ie', 'iao', 'ian', 'in', 'ing', 'u'],
        'p':  ['a', 'o', 'ai', 'ei', 'ao', 'an', 'en', 'ang', 'eng', 'i', 'ie', 'iao', 'ian', 'in', 'ing', 'u'],
        'm':  ['a', 'o', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'ie', 'iao', 'ian', 'in', 'ing', 'u'],
        'f':  ['a', 'o', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'u'],
        'd':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'ie', 'iao', 'iu', 'ian', 'in', 'ing', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        't':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'ie', 'iao', 'ian', 'in', 'ing', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        'n':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'ie', 'in', 'ing', 'u', 'uo', 'uan', 'vn', 've'],
        'l':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'ie', 'iao', 'iu', 'ian', 'in', 'ing', 'u', 'uo', 'uan', 'vn', 've'],
        'g':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'u', 'uo', 'ui', 'uan', 'uang', 'un', 'ong'],
        'k':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'u', 'uo', 'ui', 'uan', 'uang', 'un', 'ong'],
        'h':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'u', 'uo', 'ui', 'uan', 'uang', 'un', 'ong'],
        'j':  ['i', 'ia', 'ie', 'iao', 'iu', 'ian', 'iang', 'in', 'ing', 'iong', 'u', 'ue', 've', 'vn', 'van'],
        'q':  ['i', 'ia', 'ie', 'iao', 'iu', 'ian', 'iang', 'in', 'ing', 'iong', 'u', 'ue', 've', 'vn', 'van'],
        'x':  ['i', 'ia', 'ie', 'iao', 'iu', 'ian', 'iang', 'in', 'ing', 'iong', 'u', 'ue', 've', 'vn', 'van'],
        'zh': ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        'ch': ['a', 'e', 'ai', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        'sh': ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        'r':  ['e', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        'z':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        'c':  ['a', 'e', 'ai', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        's':  ['a', 'e', 'ai', 'ei', 'ao', 'ou', 'an', 'en', 'ang', 'eng', 'i', 'u', 'uo', 'ui', 'uan', 'un', 'ong'],
        'y':  ['a', 'i', 'o', 'ou', 'an', 'ang', 'e', 'in', 'ing', 'ao', 'ian', 'iang', 'ue', 'van', 'vn'],
        'w':  ['a', 'ai', 'an', 'ang', 'e', 'ei', 'en', 'eng', 'o', 'u', 'uo'],
    }
    
    for initial, finals in combos.items():
        for f in finals:
            VALID_SYLLABLES.add(initial + f)
    
    # 零声母独立音节
    for s in STANDALONE_SYLLABLES:
        VALID_SYLLABLES.add(s)
    
    # 一些额外的合法音节
    extra = {
        'lo', 'yo', 'ke', 'he',  # 叹词等
        'niao', 'miu',  # 少见但合法
        'den',  # 扽
        'nun',  # 嫩的变体
        'chua',  # 欻
        'rua',  # 扌
    }
    VALID_SYLLABLES.update(extra)

_build_syllable_table()


def split_syllables(pinyin):
    """将连续拼音串切分为音节列表
    
    使用动态规划找最优切分（音节数最少 = 长音节优先）。
    
    Args:
        pinyin: 连续拼音串，如 "jintiandetianqi"
    
    Returns:
        音节列表，如 ["jin", "tian", "de", "tian", "qi"]
        如果无法完全切分，返回尽可能多的切分，剩余部分作为最后一个元素
    """
    pinyin = pinyin.strip().lower()
    if not pinyin:
        return []
    
    # 如果包含空格，按空格分割后递归处理每段
    if ' ' in pinyin:
        result = []
        for part in pinyin.split():
            result.extend(split_syllables(part))
        return result
    
    n = len(pinyin)
    # dp[i] = 从位置i到末尾的最优切分（None表示无法切分）
    # 我们用BFS/动态规划，从后往前
    # dp[i] = (音节数, 切分列表)
    dp = [None] * (n + 1)
    dp[n] = (0, [])
    
    for i in range(n - 1, -1, -1):
        best = None
        # 尝试不同长度的音节 (1-6字符)
        for length in range(1, min(7, n - i + 1)):
            syllable = pinyin[i:i + length]
            if syllable in VALID_SYLLABLES and dp[i + length] is not None:
                count = 1 + dp[i + length][0]
                if best is None or count < best[0]:
                    best = (count, [syllable] + dp[i + length][1])
        dp[i] = best
    
    if dp[0] is not None:
        return dp[0][1]
    
    # 无法完全切分，用贪心法尽可能切分
    return _greedy_split(pinyin)


def _greedy_split(pinyin):
    """贪心切分：尽可能匹配长音节"""
    result = []
    i = 0
    n = len(pinyin)
    while i < n:
        matched = False
        for length in range(min(6, n - i), 0, -1):
            syllable = pinyin[i:i + length]
            if syllable in VALID_SYLLABLES:
                result.append(syllable)
                i += length
                matched = True
                break
        if not matched:
            # 无法匹配，取一个字符作为未知音节
            result.append(pinyin[i])
            i += 1
    return result


def join_syllables(syllables):
    """将音节列表重新拼接为连续拼音串"""
    return ''.join(syllables)


def split_at_boundaries(pinyin, boundary_syllables=None):
    """在指定音节边界处切分拼音
    
    Args:
        pinyin: 连续拼音串
        boundary_syllables: 作为切分点的音节集合（如 {'de', 'le', 'shi'} 对应 的/了/是）
    
    Returns:
        切分后的段列表，如 ["jintian", "de", "tianqizhenhaokan"]
    """
    if boundary_syllables is None:
        boundary_syllables = set()
    
    syllables = split_syllables(pinyin)
    if not syllables:
        return [pinyin]
    
    segments = []
    current = []
    for s in syllables:
        current.append(s)
        if s in boundary_syllables and len(current) >= 2:
            segments.append(''.join(current))
            current = []
    if current:
        segments.append(''.join(current))
    
    return segments if segments else [pinyin]


if __name__ == '__main__':
    # 测试
    tests = [
        ("nihao", ["ni", "hao"]),
        ("jintiandetianqi", ["jin", "tian", "de", "tian", "qi"]),
        ("zhangsan", ["zhang", "san"]),
        ("zhongguo", ["zhong", "guo"]),
        ("womingmingzhidao", ["wo", "ming", "ming", "zhi", "dao"]),
        ("xianzai", ["xian", "zai"]),
        ("shanghai", ["shang", "hai"]),
        ("zhongguorenmmin", ["zhong", "guo", "ren", "m", "min"]),
        ("jintiandetianqizhenhaokan", ["jin", "tian", "de", "tian", "qi", "zhen", "hao", "kan"]),
        ("suiranwoxianzaibuzaiyiqigongzuoledanshiwomenrengranshihaopengyou", None),
    ]
    
    for pinyin, expected in tests:
        result = split_syllables(pinyin)
        status = "✅" if expected is None or result == expected else "❌"
        print(f"{status} {pinyin}")
        print(f"   → {result}")
        if expected:
            print(f"   期望: {expected}")
        print()
    
    # 测试边界切分
    print("=== 边界切分测试 ===")
    boundaries = {'de', 'le', 'shi', 'zai', 'he', 'yu', 'de', 'er', 'you'}
    for pinyin in ["jintiandetianqizhenhaokan", "womingmingzhidaotazaiyipan"]:
        segments = split_at_boundaries(pinyin, boundaries)
        print(f"  {pinyin}")
        print(f"  → {segments}")
        print()
