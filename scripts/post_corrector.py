#!/usr/bin/env python3
"""
pinyin-ime 句子后纠错引擎
初始预测后用规则引擎修正常见错误模式。

三类纠正:
  1. 同音字混淆: 在/再、的/得/地、做/作、反应/反映 等
  2. 量词搭配: 一只猫(非一支)、一支笔(非一只)、一头牛 等
  3. 常见错别字: 按部就班(非按步)、一鼓作气(非一股) 等

用法:
  from post_corrector import PostCorrector
  corrector = PostCorrector()
  fixed = corrector.correct("这个问题需要在研究")
  # → "这个问题需要再研究"
"""
import re

class PostCorrector:
    def __init__(self, enabled=True, homophone_fix=True, measure_word_fix=True, common_mistake_fix=True):
        self.enabled = enabled
        self.homophone_fix = homophone_fix
        self.measure_word_fix = measure_word_fix
        self.common_mistake_fix = common_mistake_fix
        self.correction_count = 0
        self._build_rules()
    
    def _build_rules(self):
        """构建纠正规则"""
        # === 1. 同音字混淆规则 ===
        # 格式: (错误模式, 替换, 条件函数或None)
        self.homophone_rules = [
            # 在/再: "需要再"/"再试"/"再接"/"再来"/"再次"/"再不"/"再也" → 再
            #       "在家里"/"在学校"/"在做什么"/"在哪里"/"现在"/"正在" → 在
            (r'需要再', r'需要再', None),   # 正确，不修
            (r'需要(?<!再)在(?!家|学|哪|做|这|那|哪|里|上|下|前|后|左|右|中|内|外|间|处|一起)', r'需要再', 
             lambda m: True),  # "需要在研究" → "需要再研究"
            (r'(?<=[想打算准备])再', r'再', None),  # 正确
            # 再/在 上下文规则
            (r'再(?=[试次来去说过做看想写买吃喝用给找让请派送教])', r'再', None),  # "再试" → 再 (正确)
            (r'在(?=[试次来去说过做看想写买吃喝用给找让请派送教])', r'再', None),  # "在试" → "再试"
            (r'在是', r'再试', None),  # "在是一次" → "再试一次" (常见LM错误)
            
            # 的/得/地
            (r'(?<=[动词])的(?=[很非常更加越来越])', r'得', None),  # "跑的很快" → "跑得很快"
            (r'(?<=[形容词副词])的(?=[说走跑笑哭叫])', r'地', None),  # "高兴的说" → "高兴地说"
            
            # 做/作
            (r'(?<=请)作(?=[业课])', r'作', None),  # "请作业" 正确
            (r'(?<=要|能|会|想|去|来|敢|肯|愿)作(?=[业课]不行)', r'做', None),
            
            # 反应/反映
            (r'反映(?=[快慢强烈迟钝])', r'反应', None),  # "反映很快" → "反应很快"
            (r'反应(?=[情况问题意见建议])', r'反映', None),  # "反应情况" → "反映情况"
            
            # 交代/交待
            (r'交待(?=[任务工作事情])', r'交代', None),
            
            # 权力/权利
            (r'权力(?=[义务])', r'权利', None),  # "权力义务" → "权利义务"
            (r'权利(?=[机关部门执行])', r'权力', None),  # "权利机关" → "权力机关"
        ]
        
        # 简化的在/再规则（最常见错误）
        self.zai_zai_rules = [
            # "在" 应为 "再" 的上下文
            (r'在是', '再试'),  # LM常见错误: "在是一次" → "再试一次"
            (r'在(?=[试次来去过做看想写买吃喝用给找让请派送教叫])', '再'),
            (r'(?<=需要)在(?!家|学|哪|做|这|那|里|上|下|前|后|左|右|中|内|外|一起|一起)', '再'),
            (r'(?<=不会)在(?=[试次来去过做看想])', '再'),
            (r'(?<=还要)在(?=[试次来去过做看想])', '再'),
            # "再" 应为 "在" 的上下文
            (r'再(?=[家学里上下来去哪这那])', '在'),
            (r'(?<=现)再', '在'),  # "现再" → "现在"
            (r'(?<=正)再', '在'),  # "正再" → "正在"
            (r'(?<=实)再', '在'),  # "实再" → "实在"
        ]
        
        # === 2. 量词搭配规则 ===
        # 格式: (错误量词, 正确量词, 名词集合)
        self.measure_word_rules = {
            '只': {  # 只用于动物(部分)
                'correct_for': {'猫', '狗', '鸡', '鸟', '羊', '虎', '狼', '熊', '兔', '鹿', '鹰', '燕', '蝶', '蜂', '蚂蚁', '蝴蝶', '老虎'},
                'wrong_for': {'笔', '队伍', '歌', '箭', '枪'},
            },
            '支': {  # 支用于细长物
                'correct_for': {'笔', '队伍', '歌', '箭', '枪', '蜡烛', '香烟', '粉笔', '铅笔', '钢笔', '毛笔'},
                'wrong_for': {'猫', '狗', '鸡', '鸟'},
            },
            '头': {  # 头用于大型动物
                'correct_for': {'牛', '猪', '驴', '骡', '象', '鲸', '狮子', '豹'},
                'wrong_for': set(),
            },
            '条': {  # 条用于细长物/鱼
                'correct_for': {'鱼', '蛇', '狗', '河', '路', '绳子', '毛巾', '新闻', '命', '腿', '项链', '围巾'},
                'wrong_for': set(),
            },
            '棵': {  # 棵用于植物
                'correct_for': {'树', '草', '花', '苗', '菜', '松', '柳', '竹', '白菜', '松树', '柳树'},
                'wrong_for': set(),
            },
            '朵': {  # 朵用于花/云
                'correct_for': {'花', '云', '彩云', '雪花', '浪花', '梅花', '玫瑰', '菊花'},
                'wrong_for': set(),
            },
            '本': {  # 本用于书籍/册
                'correct_for': {'书', '字典', '杂志', '小说', '教材', '课本', '笔记', '账', '护照', '日记'},
                'wrong_for': set(),
            },
            '部': {  # 部用于作品/机器
                'correct_for': {'电影', '小说', '电话', '手机', '车', '机器', '电梯', '相机', '电脑', '电视'},
                'wrong_for': set(),
            },
            '场': {  # 场用于活动/雨/雪
                'correct_for': {'雨', '雪', '风', '比赛', '电影', '戏', '演出', '会议', '战争', '球'},
                'wrong_for': set(),
            },
        }
        
        # === 3. 常见错别字（成语/固定搭配）===
        self.idiom_rules = [
            # 格式: (错误, 正确)
            ('按步就班', '按部就班'),
            ('一股作气', '一鼓作气'),
            ('再接再励', '再接再厉'),
            ('名付其实', '名副其实'),
            ('穿流不息', '川流不息'),
            ('一愁莫展', '一筹莫展'),
            ('金壁辉煌', '金碧辉煌'),
            ('走头无路', '走投无路'),
            ('仗义直言', '仗义执言'),
            ('直言不诲', '直言不讳'),
            ('世外桃园', '世外桃源'),
            ('一诺千斤', '一诺千金'),
            ('甘败下风', '甘拜下风'),
            ('自暴自弃', '自暴自弃'),  # 正确，不修
            ('谈笑风声', '谈笑风生'),
            ('三翻五次', '三番五次'),
            ('一如继往', '一如既往'),
            ('默守成规', '墨守成规'),
            ('不径而走', '不胫而走'),
            ('委曲求全', '委曲求全'),  # 正确，不修
            ('换然一新', '焕然一新'),
            ('中流抵柱', '中流砥柱'),
            ('草管人命', '草菅人命'),
            ('鞠躬尽粹', '鞠躬尽瘁'),
            ('心旷神疑', '心旷神怡'),
            ('鬼计多端', '诡计多端'),
            ('一望无银', '一望无垠'),
            ('原形必露', '原形毕露'),
            ('迫不急待', '迫不及待'),
            ('破斧沉舟', '破釜沉舟'),
            ('精兵减政', '精兵简政'),
            ('相辅相承', '相辅相成'),
            ('消声匿迹', '销声匿迹'),
            ('浮想联遍', '浮想联翩'),
            ('出奇不意', '出其不意'),
            ('好高骛远', '好高骛远'),  # 正确
            ('和霭可亲', '和蔼可亲'),
            ('天衣无逢', '天衣无缝'),
            ('锐不可挡', '锐不可当'),
            ('记忆尤新', '记忆犹新'),
            ('巧夺天功', '巧夺天工'),
            ('随声附合', '随声附和'),
            ('别出心裁', '别出心裁'),  # 正确
            ('虎据龙盘', '虎踞龙盘'),
            ('顶梁住', '顶梁柱'),
            ('拌脚石', '绊脚石'),
            ('挺而走险', '铤而走险'),
        ]
        # 去掉"正确不修"的（错误==正确）
        self.idiom_rules = [(w, c) for w, c in self.idiom_rules if w != c]
    
    def correct(self, text):
        """对文本进行后纠错，返回纠正后的文本"""
        if not self.enabled or not text:
            return text
        
        original = text
        
        # 1. 同音字混淆纠正
        if self.homophone_fix:
            text = self._fix_homophone(text)
        
        # 2. 量词搭配纠正
        if self.measure_word_fix:
            text = self._fix_measure_word(text)
        
        # 3. 常见错别字纠正
        if self.common_mistake_fix:
            text = self._fix_idiom(text)
        
        if text != original:
            self.correction_count += 1
        
        return text
    
    def _fix_homophone(self, text):
        """同音字混淆纠正"""
        # 在/再 规则（最常见的错误）
        for pattern, replacement in self.zai_zai_rules:
            text = re.sub(pattern, replacement, text)
        
        # 反应/反映
        text = re.sub(r'反映(?=[快慢强烈迟钝大很比较])', r'反应', text)
        text = re.sub(r'反应(?=[情况问题意见建议])', r'反映', text)
        
        # 做/作: "作" 在动词位置应为 "做"
        text = re.sub(r'(?<=要|能|会|想|去|来|敢|肯|愿|请|让|帮)作(?![业课为])', r'做', text)
        
        # 的/得: 动词+的+副词 → 动词+得+副词
        text = re.sub(r'(跑|走|飞|写|说|唱|跳|干|做|学|练|打|踢|游|开|吃|喝|看|听|想|用|算|变|长)的(?=[很非常更加越来越不太真特别相当比较])', r'\1得', text)
        
        return text
    
    def _fix_measure_word(self, text):
        """量词搭配纠正"""
        # 匹配 "一X名词" 模式
        m = re.search(r'一([个只支头条棵朵本部场])([\u4e00-\u9fff]+)', text)
        if not m:
            return text
        
        measure = m.group(1)
        noun = m.group(2)
        
        if measure in self.measure_word_rules:
            rule = self.measure_word_rules[measure]
            # 如果当前量词对这个名词是错的
            if noun in rule.get('wrong_for', set()):
                # 找正确的量词
                for correct_mw, correct_rule in self.measure_word_rules.items():
                    if noun in correct_rule.get('correct_for', set()):
                        text = text[:m.start(1)] + correct_mw + text[m.end(1):]
                        break
            # 如果用了"个"但应该用专用量词
            if measure == '个':
                for correct_mw, correct_rule in self.measure_word_rules.items():
                    if noun in correct_rule.get('correct_for', set()) and correct_mw != '个':
                        text = text[:m.start(1)] + correct_mw + text[m.end(1):]
                        break
        
        return text
    
    def _fix_idiom(self, text):
        """常见错别字纠正（成语/固定搭配）"""
        for wrong, correct in self.idiom_rules:
            if wrong in text:
                text = text.replace(wrong, correct)
        return text
    
    def stats(self):
        return {"correction_count": self.correction_count}

if __name__ == '__main__':
    corrector = PostCorrector()
    
    test_cases = [
        "这个问题需要在研究",   # 在→再
        "在试一次",             # 在→再
        "在是一次",             # 在是→再试
        "现在家里",             # 现在→现在 (不修)
        "在家里看书",           # 在→在 (不修)
        "反映很快",             # 反映→反应
        "反应情况",             # 反应→反映
        "跑的很快",             # 的→得
        "一支猫",               # 支→只
        "一只笔",               # 只→支
        "按步就班",             # 步→部
        "一股作气",             # 股→鼓
        "再接再励",             # 励→厉
        "穿流不息",             # 穿→川
    ]
    
    print("=== 后纠错测试 ===\n")
    for text in test_cases:
        fixed = corrector.correct(text)
        changed = "✅" if fixed != text else "→"
        print(f"  {text}  {changed}  {fixed}")
    
    print(f"\n纠正次数: {corrector.correction_count}")
