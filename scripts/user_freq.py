#!/usr/bin/env python3
"""
pinyin-ime 用户频率追踪 (v0.9.68)
记录词使用频率 + pinyin→word 映射，高频词和精确匹配自动提升排序。

频率文件格式 (JSON):
{
  "version": 2,
  "updated": "2026-10-09",
  "words": {
    "你好": {"count": 42, "last_used": 1234567890},
    "中国": {"count": 35, "last_used": 1234567890},
    ...
  },
  "pinyin_map": {
    "nihao": {"你好": 40, "拟好": 2},
    "zhongguo": {"中国": 30, "中过": 5},
    ...
  }
}

用法:
  from user_freq import UserFreqTracker
  
  tracker = UserFreqTracker()
  
  # 记录使用（带拼音映射）
  tracker.record("你好", pinyin="nihao")
  tracker.record("中国", pinyin="zhongguo")
  
  # 频率 boost — 拼音精确匹配优先
  candidates = ["你毫", "你好", "拟好"]
  boosted = tracker.boost_candidates(candidates, "nihao")
  # → ["你好", "你毫", "拟好"]  (你好被提升到首位)
  
  tracker.save()
"""
import os, json, time, threading, math
from datetime import datetime

DEFAULT_FREQ_FILE = os.path.expanduser("~/.local/share/pinyin-ime/user.freq")
MAX_WORDS = 50000
MAX_PINYIN_MAP = 50000


class UserFreqTracker:
    def __init__(self, freq_file=None, decay_factor=0.95, boost_weight=2.0):
        self.freq_file = freq_file or DEFAULT_FREQ_FILE
        self.decay_factor = decay_factor
        self.boost_weight = boost_weight
        self.words = {}  # word → {"count": N, "last_used": timestamp}
        self.pinyin_map = {}  # pinyin → {word: count}
        self._lock = threading.Lock()
        self._dirty = False
        self.load()
    
    def load(self):
        """从文件加载频率数据"""
        if os.path.isfile(self.freq_file):
            try:
                with open(self.freq_file) as f:
                    data = json.load(f)
                self.words = data.get("words", {})
                self.pinyin_map = data.get("pinyin_map", {})
                self._dirty = False
            except Exception as e:
                print(f"Warning: Failed to load freq file: {e}")
                self.words = {}
                self.pinyin_map = {}
    
    def save(self):
        """保存频率数据到文件"""
        if not self._dirty:
            return
        os.makedirs(os.path.dirname(self.freq_file), exist_ok=True)
        data = {
            "version": 2,
            "updated": datetime.now().isoformat()[:10],
            "total_words": len(self.words),
            "total_uses": sum(w["count"] for w in self.words.values()),
            "total_pinyin": len(self.pinyin_map),
            "words": self.words,
            "pinyin_map": self.pinyin_map,
        }
        with open(self.freq_file, 'w') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        self._dirty = False
    
    def record(self, word, count=1, pinyin=None):
        """记录一个词的使用
        
        Args:
            word: 选中词
            count: 计数
            pinyin: 对应的拼音（可选，用于精确映射）
        """
        if not word or len(word) < 1:
            return
        with self._lock:
            # 记录词频
            if word in self.words:
                self.words[word]["count"] += count
                self.words[word]["last_used"] = time.time()
            else:
                self.words[word] = {"count": count, "last_used": time.time()}
            
            # 记录 pinyin→word 映射
            if pinyin:
                pinyin = pinyin.lower().strip()
                if pinyin not in self.pinyin_map:
                    self.pinyin_map[pinyin] = {}
                self.pinyin_map[pinyin][word] = self.pinyin_map[pinyin].get(word, 0) + count
            
            self._dirty = True
            
            # 限制最大条目数
            if len(self.words) > MAX_WORDS:
                sorted_words = sorted(self.words.items(), key=lambda x: x[1]["count"])
                for w, _ in sorted_words[:len(self.words) - MAX_WORDS]:
                    del self.words[w]
            
            if len(self.pinyin_map) > MAX_PINYIN_MAP:
                # 移除最少使用的拼音映射
                sorted_py = sorted(self.pinyin_map.items(), 
                                   key=lambda x: sum(x[1].values()))
                for py, _ in sorted_py[:len(self.pinyin_map) - MAX_PINYIN_MAP]:
                    del self.pinyin_map[py]
    
    def record_sentence(self, sentence, pinyin=None):
        """记录句子中每个词的使用（用jieba分词）"""
        try:
            import jieba
        except ImportError:
            # fallback: record whole sentence
            self.record(sentence, pinyin=pinyin)
            return
        words = [w for w in jieba.cut(sentence) if len(w) >= 2 and all('\u4e00' <= c <= '\u9fff' for c in w)]
        for w in words:
            self.record(w)
    
    def get_freq(self, word):
        """获取词的频率"""
        return self.words.get(word, {}).get("count", 0)
    
    def get_pinyin_freq(self, pinyin, word):
        """获取 pinyin→word 的精确频率"""
        pinyin = pinyin.lower().strip()
        return self.pinyin_map.get(pinyin, {}).get(word, 0)
    
    def boost_candidates(self, candidates, pinyin=""):
        """根据用户频率提升候选词排序
        
        排序优先级:
          1. pinyin→word 精确匹配的高频词
          2. 词频高的词
          3. 原始位置
        """
        if not candidates:
            return candidates
        
        if not self.words and not self.pinyin_map:
            return candidates
        
        pinyin = pinyin.lower().strip() if pinyin else ""
        
        # 检查是否有 pinyin 精确映射
        py_map = self.pinyin_map.get(pinyin, {}) if pinyin else {}
        
        scored = []
        for i, cand in enumerate(candidates):
            # 词频分数
            word_freq = self.get_freq(cand)
            word_score = math.log(word_freq + 1) * self.boost_weight if word_freq > 0 else 0
            
            # pinyin 精确匹配分数（权重更高）
            py_freq = py_map.get(cand, 0) if py_map else 0
            py_score = math.log(py_freq + 1) * self.boost_weight * 3 if py_freq > 0 else 0
            
            # 综合分数：频率越高排越前（分数越小越前）
            combined = -(py_score + word_score) + i * 0.1
            scored.append((combined, i, cand))
        
        scored.sort(key=lambda x: x[0])
        
        return [cand for _, _, cand in scored]
    
    def get_top_for_pinyin(self, pinyin, n=5):
        """获取某拼音的历史 top-N 词"""
        pinyin = pinyin.lower().strip()
        py_map = self.pinyin_map.get(pinyin, {})
        if not py_map:
            return []
        sorted_words = sorted(py_map.items(), key=lambda x: -x[1])
        return [(w, c) for w, c in sorted_words[:n]]
    
    def decay(self):
        """频率衰减：长期未使用的词频率降低"""
        now = time.time()
        with self._lock:
            for word, data in list(self.words.items()):
                age_days = (now - data["last_used"]) / 86400
                if age_days > 30:
                    data["count"] = int(data["count"] * self.decay_factor)
                    if data["count"] < 1:
                        del self.words[word]
                    self._dirty = True
    
    def top_words(self, n=20):
        """获取使用频率最高的N个词"""
        sorted_words = sorted(self.words.items(), key=lambda x: -x[1]["count"])
        return [(w, d["count"]) for w, d in sorted_words[:n]]
    
    def stats(self):
        """获取统计信息"""
        total_words = len(self.words)
        total_uses = sum(d["count"] for d in self.words.values())
        return {
            "total_words": total_words,
            "total_uses": total_uses,
            "total_pinyin": len(self.pinyin_map),
            "avg_uses": round(total_uses / total_words, 1) if total_words > 0 else 0,
            "top5": self.top_words(5),
        }


# CLI interface
if __name__ == '__main__':
    import sys
    tracker = UserFreqTracker()
    
    if len(sys.argv) < 2:
        print("Usage: user_freq.py <command> [args]")
        print("Commands: record <word> [pinyin], top [N], stats, decay, save, pytop <pinyin> [N]")
        sys.exit(1)
    
    cmd = sys.argv[1]
    if cmd == "record" and len(sys.argv) >= 3:
        word = sys.argv[2]
        pinyin = sys.argv[3] if len(sys.argv) >= 4 else None
        tracker.record(word, pinyin=pinyin)
        tracker.save()
        print(f"Recorded: {word} (freq: {tracker.get_freq(word)}, pinyin: {pinyin})")
    elif cmd == "top":
        n = int(sys.argv[2]) if len(sys.argv) >= 3 else 20
        for word, count in tracker.top_words(n):
            print(f"  {word}: {count}")
    elif cmd == "pytop":
        pinyin = sys.argv[2]
        n = int(sys.argv[3]) if len(sys.argv) >= 4 else 5
        for word, count in tracker.get_top_for_pinyin(pinyin, n):
            print(f"  {pinyin} → {word}: {count}")
    elif cmd == "stats":
        print(json.dumps(tracker.stats(), ensure_ascii=False, indent=2))
    elif cmd == "decay":
        tracker.decay()
        tracker.save()
        print("Decay applied")
    elif cmd == "save":
        tracker.save()
        print("Saved")
