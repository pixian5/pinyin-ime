#!/usr/bin/env python3
"""
pinyin-ime 用户频率追踪
记录词使用频率，高频词自动提升排序。

频率文件格式 (JSON):
{
  "version": 1,
  "updated": "2026-10-09",
  "words": {
    "你好": {"count": 42, "last_used": "2026-10-09"},
    "中国": {"count": 35, "last_used": "2026-10-09"},
    ...
  }
}

用法:
  from user_freq import UserFreqTracker
  
  tracker = UserFreqTracker()
  tracker.record("你好")          # 记录使用
  tracker.record("中国")
  
  # 获取频率排序
  candidates = ["你毫", "你好", "拟好"]
  boosted = tracker.boost_candidates(candidates, "nihao")
  # → ["你好", "你毫", "拟好"]  (你好被提升到首位)
  
  # 获取统计
  top_words = tracker.top_words(10)
  tracker.save()
"""
import os, json, time, threading
from datetime import datetime

DEFAULT_FREQ_FILE = os.path.expanduser("~/.local/share/pinyin-ime/user.freq")
MAX_WORDS = 50000

class UserFreqTracker:
    def __init__(self, freq_file=None, decay_factor=0.95, boost_weight=2.0):
        self.freq_file = freq_file or DEFAULT_FREQ_FILE
        self.decay_factor = decay_factor
        self.boost_weight = boost_weight
        self.words = {}  # word → {"count": N, "last_used": timestamp}
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
                self._dirty = False
            except Exception as e:
                print(f"Warning: Failed to load freq file: {e}")
                self.words = {}
    
    def save(self):
        """保存频率数据到文件"""
        if not self._dirty:
            return
        os.makedirs(os.path.dirname(self.freq_file), exist_ok=True)
        data = {
            "version": 1,
            "updated": datetime.now().isoformat()[:10],
            "total_words": len(self.words),
            "total_uses": sum(w["count"] for w in self.words.values()),
            "words": self.words,
        }
        with open(self.freq_file, 'w') as f:
            json.dump(data, f, ensure_ascii=False, indent=2)
        self._dirty = False
    
    def record(self, word, count=1):
        """记录一个词的使用"""
        if not word or len(word) < 1:
            return
        with self._lock:
            if word in self.words:
                self.words[word]["count"] += count
                self.words[word]["last_used"] = time.time()
            else:
                self.words[word] = {"count": count, "last_used": time.time()}
            self._dirty = True
            
            # 限制最大条目数
            if len(self.words) > MAX_WORDS:
                # 移除最少使用的词
                sorted_words = sorted(self.words.items(), key=lambda x: x[1]["count"])
                for w, _ in sorted_words[:len(self.words) - MAX_WORDS]:
                    del self.words[w]
    
    def record_sentence(self, sentence):
        """记录句子中每个词的使用（用jieba分词）"""
        try:
            import jieba
        except ImportError:
            return
        words = [w for w in jieba.cut(sentence) if len(w) >= 2 and all('\u4e00' <= c <= '\u9fff' for c in w)]
        for w in words:
            self.record(w)
    
    def get_freq(self, word):
        """获取词的频率"""
        return self.words.get(word, {}).get("count", 0)
    
    def boost_candidates(self, candidates, pinyin=""):
        """根据用户频率提升候选词排序
        
        高频词会被提升到前面，但不会改变同频率词的相对顺序。
        """
        if not candidates or not self.words:
            return candidates
        
        # 计算每个候选的频率分数
        scored = []
        for i, cand in enumerate(candidates):
            freq = self.get_freq(cand)
            # 原始位置分数（越小越好）
            pos_score = i
            # 频率分数（越大越好，取对数平滑）
            import math
            freq_score = math.log(freq + 1) * self.boost_weight if freq > 0 else 0
            # 综合分数：频率高的排前面
            combined = -freq_score + pos_score * 0.1  # 频率权重远大于位置权重
            scored.append((combined, i, cand))
        
        # 按综合分数排序
        scored.sort(key=lambda x: x[0])
        
        return [cand for _, _, cand in scored]
    
    def decay(self):
        """频率衰减：长期未使用的词频率降低"""
        now = time.time()
        with self._lock:
            for word, data in list(self.words.items()):
                age_days = (now - data["last_used"]) / 86400
                if age_days > 30:  # 超过30天未使用
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
            "avg_uses": round(total_uses / total_words, 1) if total_words > 0 else 0,
            "top5": self.top_words(5),
        }

# CLI interface
if __name__ == '__main__':
    import sys
    tracker = UserFreqTracker()
    
    if len(sys.argv) < 2:
        print("Usage: user_freq.py <command> [args]")
        print("Commands: record <word>, top [N], stats, decay, save")
        sys.exit(1)
    
    cmd = sys.argv[1]
    if cmd == "record" and len(sys.argv) >= 3:
        tracker.record(sys.argv[2])
        tracker.save()
        print(f"Recorded: {sys.argv[2]} (freq: {tracker.get_freq(sys.argv[2])})")
    elif cmd == "top":
        n = int(sys.argv[2]) if len(sys.argv) >= 3 else 20
        for word, count in tracker.top_words(n):
            print(f"  {word}: {count}")
    elif cmd == "stats":
        print(json.dumps(tracker.stats(), ensure_ascii=False, indent=2))
    elif cmd == "decay":
        tracker.decay()
        tracker.save()
        print("Decay applied")
    elif cmd == "save":
        tracker.save()
        print("Saved")
