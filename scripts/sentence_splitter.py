#!/usr/bin/env python3
"""
长句切分器 (v0.9.68)
对长拼音串在自然语言边界处插入空格，帮助 LM 分词，提升长句准确率。

原理:
  libime LM 对短序列准确率高，长连续拼音退化严重。
  在自然语言边界音节（的、了、在、是、和、与等）后插入空格，
  给 LM 分词提示，同时保留全局上下文（一次 CLI 调用）。
  
  实测效果（100句采样）:
    news:    40% → 45%  (+5%)
    dialogue: 70% → 75%  (+5%)
    recent:  28% → 43%  (+15%)
    daily:   98.6% → 98.6% (无变化)
    baike:   34% → 35%  (+1%)
    wiki:    19% → 20%  (+1%)

用法:
  from sentence_splitter import SentenceSplitter
  
  splitter = SentenceSplitter()
  spaced = splitter.insert_boundary_spaces("jintiandetianqizhenhaokan")
  # → "jintian de tianqizhenhaokan" (如果足够长)
  
  # 判断是否需要处理
  if splitter.needs_split(pinyin):
      pinyin = splitter.insert_boundary_spaces(pinyin)
"""
import os, sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
from pinyin_syllable import split_syllables

# 切分阈值：音节数超过此值才插入边界空格
SPLIT_THRESHOLD = 8  # 8个音节以下不处理

# 边界音节 — 在这些音节后插入空格
# 这些字在中文中常作为分界点（助词、连词、介词、代词、副词等）
BOUNDARY_SYLLABLES = {
    # 助词
    'de',   # 的
    'le',   # 了
    'di',   # 地
    # 介词
    'zai',  # 在
    'cong', # 从
    'xiang',# 向
    'wang', # 往
    'dui',  # 对
    'wei',  # 为
    # 连词
    'he',   # 和
    'yu',   # 与
    'er',   # 而
    'dan',  # 但
    'ke',   # 可
    # 判断词 — shi/you/bu 已移除：常与后续组成复合词(实现/有效/不断)，拆开反而降准确率
    # 代词
    'wo',   # 我
    'ni',   # 你
    'ta',   # 他/她
    'zhe',  # 这
    'na',   # 那
    # 副词
    'dou',  # 都
    'jiu',  # 就
    'hai',  # 还
    'ye',   # 也
    # 量词
    'ge',   # 个
    # 方位/趋向
    'dao',  # 到
    # 使令
    'rang', # 让
    'bei',  # 被
    'ba',   # 把
    'gei',  # 给
}


class SentenceSplitter:
    def __init__(self, threshold=SPLIT_THRESHOLD, verbose=False):
        self.threshold = threshold
        self.verbose = verbose
    
    def needs_split(self, pinyin):
        """判断是否需要插入边界空格"""
        if ' ' in pinyin:
            return False  # 已有空格
        syllables = split_syllables(pinyin)
        return len(syllables) > self.threshold
    
    def insert_boundary_spaces(self, pinyin):
        """在边界音节后插入空格
        
        Args:
            pinyin: 连续拼音串
        
        Returns:
            带空格的拼音串（如果需要切分），或原串（如果不需要）
        """
        pinyin = pinyin.strip()
        if not pinyin:
            return pinyin
        
        # 已有空格的不处理
        if ' ' in pinyin:
            return pinyin
        
        syllables = split_syllables(pinyin)
        if len(syllables) <= self.threshold:
            return pinyin
        
        # 在边界音节后插入空格
        result = []
        for i, syl in enumerate(syllables):
            result.append(syl)
            if syl in BOUNDARY_SYLLABLES and i < len(syllables) - 1:
                result.append(' ')
        
        spaced = ''.join(result)
        
        if self.verbose and spaced != pinyin:
            print(f"  [splitter] {len(syllables)} syllables → {spaced}")
        
        return spaced
    
    def split(self, pinyin):
        """兼容接口：返回切分后的段列表"""
        spaced = self.insert_boundary_spaces(pinyin)
        if spaced == pinyin:
            return [pinyin]
        return spaced.split()
    
    def process(self, pinyin):
        """主接口：处理拼音，返回带边界空格的版本"""
        return self.insert_boundary_spaces(pinyin)


# 保留旧的 predict 接口兼容性
def predict_with_split(pinyin, cli_path, lm_path, dict_path, top_k=1, verbose=False):
    """切分 + CLI 预测
    
    先插入边界空格，再送 CLI 预测。
    """
    import subprocess
    
    splitter = SentenceSplitter(verbose=verbose)
    processed = splitter.process(pinyin)
    
    extra = ['--quiet']
    if top_k > 1:
        extra.extend(['--top-k', str(top_k)])
    else:
        extra.append('--top1')
    
    try:
        result = subprocess.run(
            [cli_path] + extra + [lm_path, dict_path],
            input=processed, capture_output=True, text=True, timeout=10
        )
        return result.stdout.strip()
    except Exception as e:
        return pinyin


if __name__ == '__main__':
    splitter = SentenceSplitter(verbose=True)
    
    test_cases = [
        "nihao",
        "jintiandetianqizhenhaokan",
        "womingmingzhidaotazaiyipanshuohuakshiquermeitingjian",
        "zhongguorenmminzaigongchandangdelingdaoxiayijingqilaiyilaile",
        "suiranwoxianzaibuzaiyiqigongzuoledanshiwomenrengranshihaopengyou",
        "ruhexiangbaohuyanjingyiyangbaohuziranhehuanjing",
        "huangheanlanshizhonghuaernvdeqiannianqipan",
    ]
    
    print("=== 边界空格切分测试 ===\n")
    for pinyin in test_cases:
        print(f"输入: {pinyin}")
        result = splitter.process(pinyin)
        if result != pinyin:
            print(f"切分: {result}")
        else:
            print(f"切分: (无需切分)")
        print()
