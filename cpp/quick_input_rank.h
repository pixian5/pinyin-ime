// 纠错与候选排序模块 —— quick_input.h 的片段（v0.9.178 拆出）。
// 注意：本文件不是独立头文件，必须在 quick_input.h 的 namespace pinyin_ime 内 include；
// 拆分只为缩小 quick_input.h 体积、明确模块归属，不改变任何调用方代码。

// ── v0.9.145 智能拼音纠错 ──────────────────────────────────
// 检测并修正常见的拼音输入错误
inline std::vector<std::string> smart_pinyin_correct(const std::string& input) {
    std::vector<std::string> corrections;
    if (input.empty() || input.size() < 2) return corrections;

    // 常见拼音错误模式：{错误模式, 修正}
    static const std::vector<std::pair<std::string, std::string>> typo_patterns = {
        // 遗漏韵母
        {"zhonggu", "zhongguo"},   // 中国 遗漏 o
        {"meiyo", "meiyou"},       // 没有 遗漏 u
        {"buxhuan", "buxihuan"},   // 不喜欢 i→x
        // 声母混淆
        {"zhongguo", "zhongguo"},  // 正确，不修正
        {"chongguo", "zhongguo"},  // ch→zh
        {"zongguo", "zhongguo"},   // z→zh (补h)
        {"shuzi", "shuzi"},        // 正确
        {"suzi", "shuzi"},         // s→sh
        // 韵母混淆
        {"nihao", "nihao"},        // 正确
        {"nihao", "nihao"},        // 正确
        // 前后鼻音
        {"zaijian", "zaijian"},    // 正确
        // 常见多字词遗漏
        {"xiexie", "xiexie"},      // 正确
        {"xieixe", "xiexie"},      // 字母顺序错误
        // n/l 混淆
        {"lihao", "nihao"},        // l→n
        // h 遗漏
        {"suzi", "shuzi"},
        {"cizi", "chizi"},
        // 常见整词纠错
        {"shurufa", "shurufa"},    // 正确
        {"shurufa", "shurufa"},
    };

    // 生成修正候选（去重）
    for (auto& pattern : typo_patterns) {
        if (input == pattern.first && pattern.first != pattern.second) {
            // 避免重复
            bool dup = false;
            for (auto& c : corrections) if (c == pattern.second) { dup = true; break; }
            if (!dup) corrections.push_back(pattern.second);
        }
    }

    // 通用纠错：尝试在常见位置插入/删除/替换字符
    // 1. 尝试在末尾补全常见韵母
    if (input.size() >= 3) {
        std::string last2 = input.substr(input.size() - 2);
        // 如果以声母结尾，尝试补韵母
        static const std::set<std::string> consonants = {
            "zh", "ch", "sh", "b", "p", "m", "f", "d", "t",
            "n", "l", "g", "k", "h", "j", "q", "x", "r", "z", "c", "s"
        };
        if (consonants.count(last2) || consonants.count(input.substr(input.size() - 1))) {
            // 尝试补 "a", "e", "i", "o", "u"
            for (auto vowel : {'a', 'e', 'i', 'o', 'u'}) {
                std::string corrected = input + vowel;
                bool dup = false;
                for (auto& c : corrections) if (c == corrected) { dup = true; break; }
                if (!dup) corrections.push_back(corrected);
            }
        }
    }

    // 2. 尝试在 zh/ch/sh 位置补 h
    for (size_t i = 0; i + 1 < input.size(); ++i) {
        if ((input[i] == 'z' || input[i] == 'c' || input[i] == 's') &&
            input[i + 1] != 'h' && input[i + 1] != 'a' && input[i + 1] != 'e' &&
            input[i + 1] != 'i' && input[i + 1] != 'o' && input[i + 1] != 'u' &&
            input[i + 1] != 'n' && input[i + 1] != 'g') {
            std::string corrected = input;
            corrected.insert(i + 1, "h");
            bool dup = false;
            for (auto& c : corrections) if (c == corrected) { dup = true; break; }
            if (!dup) corrections.push_back(corrected);
        }
    }

    return corrections;
}

// ── v0.9.145 增强上下文排序（支持多词上下文） ──────────────
// 不仅看上一个词，还看上上个词，实现更精准的候选排序
inline std::vector<std::string>
context_rank_candidates_v2(const std::vector<std::string>& candidates,
                            const std::string& last_committed,
                            const std::string& prev_committed = "") {
    auto result = context_rank_candidates(candidates, last_committed);

    // 二级上下文：两个词的组合关联
    if (!prev_committed.empty() && !last_committed.empty() && result.size() > 1) {
        // 常见三词组合
        static const std::vector<std::vector<std::string>> trigram_rules = {
            {"你", "好", "吗"},     // 你好吗
            {"我", "的", "名"},     // 我的名字
            {"不", "知", "道"},     // 不知道
            {"现", "在", "时"},     // 现在时间
            {"今", "天", "天"},     // 今天天气
            {"中", "国", "人"},     // 中国人
            {"人", "工", "智"},     // 人工智能
            {"数", "据", "科"},     // 数据科学
            {"网", "络", "安"},     // 网络安全
            {"系", "统", "架"},     // 系统架构
        };

        for (auto& rule : trigram_rules) {
            if (prev_committed == rule[0] && last_committed == rule[1]) {
                // 把包含 rule[2] 的候选排到前面
                for (size_t i = 1; i < result.size(); ++i) {
                    if (result[i].find(rule[2]) != std::string::npos) {
                        std::swap(result[0], result[i]);
                        break;
                    }
                }
                break;
            }
        }
    }

    return result;
}

// ── v0.9.145 扩展句式模板（新增 20 个常用模板） ──────────────
inline std::vector<std::pair<std::string, std::string>>
extended_sentence_templates() {
    auto templates = builtin_sentence_templates();  // 原有 34 个

    // v0.9.145 新增 20 个
    std::vector<std::pair<std::string, std::string>> new_templates = {
        {"ks", "开始"},           // 开始
        {"js", "结束"},           // 结束
        {"wz", "正在"},           // 正在
        {"yy", "已有"},           // 已有
        {"xy", "需要"},           // 需要
        {"my", "没有"},           // 没有
        {"bm", "不好意思"},       // 不好意思
        {"dl", "登录"},           // 登录
        {"tc", "退出"},           // 退出
        {"sc", "删除"},           // 删除
        {"xg", "修改"},           // 修改
        {"cx", "查询"},           // 查询
        {"tj", "添加"},           // 添加
        {"gx", "更新"},           // 更新
        {"az", "安装"},           // 安装
        {"xz", "下载"},           // 下载
        {"sc", "上传"},           // 上传
        {"bf", "备份"},           // 备份
        {"hf", "恢复"},           // 恢复
        {"bt", "抱歉，"},         // 抱歉
    };

    for (auto& t : new_templates) {
        bool dup = false;
        for (auto& existing : templates) {
            if (existing.first == t.first) { dup = true; break; }
        }
        if (!dup) templates.push_back(t);
    }

    return templates;
}

// ── v0.9.145 候选词质量评分 ──────────────────────────────────
// 对候选词进行质量评分，用于排序优化
inline int candidate_quality_score(const std::string& candidate,
                                    const std::string& input) {
    int score = 0;

    // 1. 长度匹配：候选词长度与输入拼音预期长度越接近越好
    // UTF-8 中文每字 3 字节，拼音每音节约 2-4 字母
    size_t expected_chars = input.size() / 3;  // 估计汉字个数
    size_t expected_bytes = expected_chars * 3;  // 估计 UTF-8 字节数
    if (candidate.size() == expected_bytes) score += 10;
    else if (candidate.size() >= expected_bytes / 2 && candidate.size() <= expected_bytes * 2) score += 5;
    // 给所有非空候选基础分
    if (!candidate.empty()) score += 1;

    // 2. 常用字加分
    static const std::set<std::string> common_chars = {
        "的", "是", "在", "我", "有", "人", "这", "中",
        "大", "为", "上", "个", "国", "不", "到", "会",
        "时", "要", "说", "就", "出", "会", "可", "也",
        "对", "生", "能", "而", "子", "那", "得", "于",
    };
    for (auto& ch : common_chars) {
        if (candidate.find(ch) != std::string::npos) score += 2;
    }

    // 3. 包含标点减分（通常不是首选）
    for (char c : candidate) {
        if (c == ' ' || c == '	') score -= 1;
    }

    return score;
}

// 统一入口：搜狗式快速输入 v6（v0.9.145）
inline std::vector<std::string> quick_candidates_v6(const std::string& input,
                    const std::string& preceding = "",
                    const std::string& prev2 = "",
                    const std::string& theme_name = "default") {
    // 基于 v5
    auto result = quick_candidates_v5(input, preceding, theme_name);

    // 智能拼音纠错候选
    auto corrections = smart_pinyin_correct(input);
    for (auto& c : corrections) {
        bool dup = false;
        for (auto& r : result) if (r == c) { dup = true; break; }
        if (!dup) result.push_back(c);
    }

    // 扩展句式模板
    auto templates = extended_sentence_templates();
    for (auto& t : templates) {
        if (input == t.first) {
            bool dup = false;
            for (auto& r : result) if (r == t.second) { dup = true; break; }
            if (!dup) result.insert(result.begin(), t.second);
        }
    }

    // 增强上下文排序
    if (!preceding.empty()) {
        result = context_rank_candidates_v2(result, preceding, prev2);
    }

    return result;
}

// ── v0.9.146 动态规划拼音分词 ──────────────────────────────────
// 完整的有效拼音音节表（用于DP分词）
inline const std::set<std::string>& valid_pinyin_syllables() {
    static const std::set<std::string> syllables = {
        // a
        "a", "ai", "an", "ang", "ao",
        // b
        "ba", "bai", "ban", "bang", "bao", "bei", "ben", "beng", "bi", "bian",
        "biao", "bie", "bin", "bing", "bo", "bu",
        // c
        "ca", "cai", "can", "cang", "cao", "ce", "cen", "ceng", "cha", "chai",
        "chan", "chang", "chao", "che", "chen", "cheng", "chi", "chong", "chou",
        "chu", "chua", "chuai", "chuan", "chuang", "chui", "chun", "chuo", "ci",
        "cong", "cou", "cu", "cuan", "cui", "cun", "cuo",
        // d
        "da", "dai", "dan", "dang", "dao", "de", "dei", "den", "deng", "di",
        "dia", "dian", "diao", "die", "ding", "diu", "dong", "dou", "du", "duan",
        "dui", "dun", "duo",
        // e
        "e", "ei", "en", "eng", "er",
        // f
        "fa", "fan", "fang", "fei", "fen", "feng", "fo", "fou", "fu",
        // g
        "ga", "gai", "gan", "gang", "gao", "ge", "gei", "gen", "geng", "gong",
        "gou", "gu", "gua", "guai", "guan", "guang", "gui", "gun", "guo",
        // h
        "ha", "hai", "han", "hang", "hao", "he", "hei", "hen", "heng", "hong",
        "hou", "hu", "hua", "huai", "huan", "huang", "hui", "hun", "huo",
        // j
        "ji", "jia", "jian", "jiang", "jiao", "jie", "jin", "jing", "jiong",
        "jiu", "ju", "juan", "jue", "jun",
        // k
        "ka", "kai", "kan", "kang", "kao", "ke", "kei", "ken", "keng", "kong",
        "kou", "ku", "kua", "kuai", "kuan", "kuang", "kui", "kun", "kuo",
        // l
        "la", "lai", "lan", "lang", "lao", "le", "lei", "leng", "li", "lia",
        "lian", "liang", "liao", "lie", "lin", "ling", "liu", "long", "lou",
        "lu", "luan", "lun", "luo", "lv", "lve",
        // m
        "ma", "mai", "man", "mang", "mao", "me", "mei", "men", "meng", "mi",
        "mian", "miao", "mie", "min", "ming", "miu", "mo", "mou", "mu",
        // n
        "na", "nai", "nan", "nang", "nao", "ne", "nei", "nen", "neng", "ni",
        "nian", "niang", "niao", "nie", "nin", "ning", "niu", "nong", "nou",
        "nu", "nuan", "nun", "nuo", "nv", "nve",
        // o
        "o", "ou",
        // p
        "pa", "pai", "pan", "pang", "pao", "pei", "pen", "peng", "pi", "pian",
        "piao", "pie", "pin", "ping", "po", "pou", "pu",
        // q
        "qi", "qia", "qian", "qiang", "qiao", "qie", "qin", "qing", "qiong",
        "qiu", "qu", "quan", "que", "qun",
        // r
        "ran", "rang", "rao", "re", "ren", "reng", "ri", "rong", "rou", "ru",
        "rua", "ruan", "rui", "run", "ruo",
        // s
        "sa", "sai", "san", "sang", "sao", "se", "sen", "seng", "sha", "shai",
        "shan", "shang", "shao", "she", "shei", "shen", "sheng", "shi", "shong",
        "shou", "shu", "shua", "shuai", "shuan", "shuang", "shui", "shun", "shuo",
        "si", "song", "sou", "su", "suan", "sui", "sun", "suo",
        // t
        "ta", "tai", "tan", "tang", "tao", "te", "teng", "ti", "tian", "tiao",
        "tie", "ting", "tong", "tou", "tu", "tuan", "tui", "tun", "tuo",
        // w
        "wa", "wai", "wan", "wang", "wei", "wen", "weng", "wo", "wu",
        // x
        "xi", "xia", "xian", "xiang", "xiao", "xie", "xin", "xing", "xiong",
        "xiu", "xu", "xuan", "xue", "xun",
        // y
        "ya", "yan", "yang", "yao", "ye", "yi", "yin", "ying", "yong", "you",
        "yu", "yuan", "yue", "yun",
        // z
        "za", "zai", "zan", "zang", "zao", "ze", "zei", "zen", "zeng", "zha",
        "zhai", "zhan", "zhang", "zhao", "zhe", "zhei", "zhen", "zheng", "zhi",
        "zhong", "zhou", "zhu", "zhua", "zhuai", "zhuan", "zhuang", "zhui",
        "zhun", "zhuo", "zi", "zong", "zou", "zu", "zuan", "zui", "zun", "zuo",
    };
    return syllables;
}

// DP-based 最优拼音分词：最小化音节数量（偏好长音节匹配）
inline std::vector<std::string> dp_pinyin_segment(const std::string& input) {
    int n = (int)input.size();
    if (n == 0) return {};

    const auto& syllables = valid_pinyin_syllables();
    const int INF = 1e9;

    // dp[i] = 从位置i到末尾的最小音节数（覆盖全部字符）
    // choice[i] = 最优切分中位置i选的音节长度
    std::vector<int> dp(n + 1, INF);
    std::vector<int> choice(n + 1, 0);
    dp[n] = 0;  // 空串=0个音节

    // 从后往前DP
    for (int i = n - 1; i >= 0; --i) {
        // 尝试所有可能的音节长度（1-6），优先长音节
        for (int len = 6; len >= 1; --len) {
            if (i + len > n) continue;
            std::string syllable = input.substr(i, len);
            if (syllables.count(syllable) && dp[i + len] < INF) {
                int score = 1 + dp[i + len];
                if (score < dp[i]) {
                    dp[i] = score;
                    choice[i] = len;
                }
            }
        }
        // 单字符回退：如果无法匹配任何音节，跳过一个字符
        if (dp[i] >= INF && dp[i + 1] < INF) {
            dp[i] = 1 + dp[i + 1];  // 作为1个无效音节
            choice[i] = 1;
        }
    }

    // 回溯构造分词结果
    std::vector<std::string> result;
    int i = 0;
    while (i < n) {
        int len = choice[i];
        if (len <= 0) break;
        result.push_back(input.substr(i, len));
        i += len;
    }

    return result;
}

// v0.9.146 带分隔符的DP分词结果
// v0.9.179：判断整串能否被有效音节**完整覆盖**（零残字）。
// 与 dp_pinyin_segment 的差别：后者允许"单字符残段回退"（用于提示分词，不判合法性），
// 且以音节数最少为目标——`guangxuershiwunian…` 会被切成长音节 `xue`，多出来的 'r' 变成残段，
// 于是合法拼音被误判成非法，CLI 便把它当英文原样回显。
// 这里只做可达性判断：每个位置都必须落在某个有效音节边界上。
inline bool pinyin_fully_segmentable(const std::string& input) {
    if (input.empty()) return false;
    const auto& syl = valid_pinyin_syllables();
    const size_t n = input.size();
    std::vector<char> can(n + 1, 0);
    can[0] = 1;  // 起点可达
    for (size_t i = 0; i < n; ++i) {
        if (!can[i]) continue;
        const size_t maxLen = std::min<size_t>(6, n - i);
        for (size_t len = 1; len <= maxLen; ++len) {
            if (syl.count(input.substr(i, len))) can[i + len] = 1;
        }
    }
    return can[n] != 0;
}

inline std::string dp_pinyin_segment_hint(const std::string& input) {
    auto segments = dp_pinyin_segment(input);
    if (segments.empty()) return {};

    std::string result;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) result += "'";
        result += segments[i];
    }
    return result;
}

// v0.9.146 增强长句分割（使用DP分词）
inline std::vector<std::string> long_sentence_split_v2(const std::string& input) {
    if (input.size() < 4) return {};

    auto segments = dp_pinyin_segment(input);
    if (segments.size() < 2) return {};

    // 按常见词长分组（2-4音节一组）
    std::vector<std::string> groups;
    size_t i = 0;
    while (i < segments.size()) {
        // 尝试2-4音节一组
        std::string group;
        int take = 0;

        // 优先尝试4音节，然后3，然后2
        for (int try_take : {4, 3, 2}) {
            if (i + try_take <= segments.size()) {
                std::string candidate;
                for (int j = 0; j < try_take; ++j) {
                    candidate += segments[i + j];
                }
                // 简单启发式：如果组合后长度合理（6-12字符），采用
                if (candidate.size() >= 4 && candidate.size() <= 12) {
                    group = candidate;
                    take = try_take;
                    break;
                }
            }
        }

        if (take == 0) {
            group = segments[i];
            take = 1;
        }

        groups.push_back(group);
        i += take;
    }

    return groups;
}

// ── v0.9.147 增强皮肤系统 ──────────────────────────────────
// 扩展皮肤属性：字体大小/候选窗样式/间距/边框等
struct SkinConfig {
    std::string name;
    std::string description;
    // 颜色（ANSI转义码）
    std::string preedit_color;
    std::string candidate_color;
    std::string focus_color;
    std::string index_color;
    std::string hint_color;
    std::string reset_color;
    // 候选窗样式
    int font_size = 14;
    int candidate_spacing = 2;      // 候选词间距
    int max_candidates_per_page = 5;
    bool show_index = true;          // 显示序号
    bool show_pinyin = true;         // 显示拼音
    bool highlight_first = true;     // 高亮首选
    std::string window_style = "horizontal";  // horizontal/vertical
    std::string border_style = "none";        // none/ascii/unicode
    std::string background_color = "";
    // 动画
    bool enable_animation = false;
    int animation_speed_ms = 100;
};

// 内置皮肤列表
inline SkinConfig get_skin(const std::string& name) {
    if (name == "sogou" || name == "搜狗") {
        return {"sogou", "搜狗经典皮肤",
                "[36m", "[37m", "[1;33m", "[90m", "[90m", "[0m",
                14, 3, 5, true, true, true, "horizontal", "unicode", "[44m", true, 80};
    } else if (name == "dark" || name == "暗色") {
        return {"dark", "暗色主题",
                "[36m", "[37m", "[1;33m", "[90m", "[90m", "[0m",
                14, 2, 5, true, true, true, "horizontal", "none", "", false, 0};
    } else if (name == "high" || name == "高对比") {
        return {"high", "高对比度主题",
                "[1;37m", "[1;37m", "[1;31m", "[1;36m", "[1;35m", "[0m",
                16, 3, 5, true, true, true, "horizontal", "ascii", "[40m", false, 0};
    } else if (name == "retro" || name == "复古") {
        return {"retro", "复古主题",
                "[33m", "[32m", "[1;33m", "[35m", "[36m", "[0m",
                12, 2, 5, true, false, true, "horizontal", "ascii", "", false, 0};
    } else if (name == "matrix" || name == "黑客") {
        return {"matrix", "黑客帝国主题",
                "[32m", "[32m", "[1;32m", "[90m", "[32m", "[0m",
                14, 2, 5, true, true, true, "horizontal", "none", "[40m", true, 50};
    } else if (name == "ocean" || name == "海洋") {
        return {"ocean", "海洋主题",
                "[34m", "[36m", "[1;34m", "[90m", "[35m", "[0m",
                14, 2, 5, true, true, true, "horizontal", "unicode", "[44;1m", false, 0};
    } else if (name == "solar" || name == "日光") {
        return {"solar", "日光主题",
                "[33m", "[37m", "[1;31m", "[33m", "[35m", "[0m",
                14, 2, 5, true, true, true, "horizontal", "none", "[43m", false, 0};
    } else if (name == "vertical" || name == "竖排") {
        return {"vertical", "竖排候选窗",
                "[36m", "[37m", "[1;33m", "[90m", "[90m", "[0m",
                14, 1, 9, true, true, true, "vertical", "unicode", "", false, 0};
    }
    // 默认皮肤
    return {"default", "默认皮肤",
            "", "", "", "", "", "",
            14, 2, 5, true, true, true, "horizontal", "none", "", false, 0};
}

// 列出所有内置皮肤
inline std::vector<std::pair<std::string, std::string>> list_skins() {
    return {
        {"default", "默认皮肤"},
        {"sogou", "搜狗经典皮肤"},
        {"dark", "暗色主题"},
        {"high", "高对比度主题"},
        {"retro", "复古主题"},
        {"matrix", "黑客帝国主题"},
        {"ocean", "海洋主题"},
        {"solar", "日光主题"},
        {"vertical", "竖排候选窗"},
    };
}

// 从文件加载自定义皮肤
// 格式：key=value 每行一个属性
inline SkinConfig load_skin_from_file(const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "r");
    if (!fp) return get_skin("default");

    SkinConfig skin = get_skin("default");
    char line[512];
    while (std::fgets(line, sizeof(line), fp)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.empty() || s[0] == '#') continue;

        size_t eq = s.find('=');
        if (eq == std::string::npos) continue;
        std::string key = s.substr(0, eq);
        std::string val = s.substr(eq + 1);

        if (key == "name") skin.name = val;
        else if (key == "description") skin.description = val;
        else if (key == "preedit_color") skin.preedit_color = val;
        else if (key == "candidate_color") skin.candidate_color = val;
        else if (key == "focus_color") skin.focus_color = val;
        else if (key == "index_color") skin.index_color = val;
        else if (key == "hint_color") skin.hint_color = val;
        else if (key == "font_size") skin.font_size = std::stoi(val);
        else if (key == "candidate_spacing") skin.candidate_spacing = std::stoi(val);
        else if (key == "max_candidates_per_page") skin.max_candidates_per_page = std::stoi(val);
        else if (key == "show_index") skin.show_index = (val == "true" || val == "1");
        else if (key == "show_pinyin") skin.show_pinyin = (val == "true" || val == "1");
        else if (key == "highlight_first") skin.highlight_first = (val == "true" || val == "1");
        else if (key == "window_style") skin.window_style = val;
        else if (key == "border_style") skin.border_style = val;
        else if (key == "background_color") skin.background_color = val;
        else if (key == "enable_animation") skin.enable_animation = (val == "true" || val == "1");
        else if (key == "animation_speed_ms") skin.animation_speed_ms = std::stoi(val);
    }
    std::fclose(fp);
    return skin;
}

// 保存皮肤到文件
inline bool save_skin_to_file(const SkinConfig& skin, const std::string& path) {
    FILE* fp = std::fopen(path.c_str(), "w");
    if (!fp) return false;
    std::fprintf(fp, "# pinyin-ime skin file\n");
    std::fprintf(fp, "name=%s\n", skin.name.c_str());
    std::fprintf(fp, "description=%s\n", skin.description.c_str());
    std::fprintf(fp, "preedit_color=%s\n", skin.preedit_color.c_str());
    std::fprintf(fp, "candidate_color=%s\n", skin.candidate_color.c_str());
    std::fprintf(fp, "focus_color=%s\n", skin.focus_color.c_str());
    std::fprintf(fp, "index_color=%s\n", skin.index_color.c_str());
    std::fprintf(fp, "hint_color=%s\n", skin.hint_color.c_str());
    std::fprintf(fp, "font_size=%d\n", skin.font_size);
    std::fprintf(fp, "candidate_spacing=%d\n", skin.candidate_spacing);
    std::fprintf(fp, "max_candidates_per_page=%d\n", skin.max_candidates_per_page);
    std::fprintf(fp, "show_index=%s\n", skin.show_index ? "true" : "false");
    std::fprintf(fp, "show_pinyin=%s\n", skin.show_pinyin ? "true" : "false");
    std::fprintf(fp, "highlight_first=%s\n", skin.highlight_first ? "true" : "false");
    std::fprintf(fp, "window_style=%s\n", skin.window_style.c_str());
    std::fprintf(fp, "border_style=%s\n", skin.border_style.c_str());
    std::fprintf(fp, "background_color=%s\n", skin.background_color.c_str());
    std::fprintf(fp, "enable_animation=%s\n", skin.enable_animation ? "true" : "false");
    std::fprintf(fp, "animation_speed_ms=%d\n", skin.animation_speed_ms);
    std::fclose(fp);
    return true;
}

// 用皮肤渲染候选窗
inline std::string render_candidate_window_with_skin(
        const std::vector<std::string>& candidates,
        const std::string& preedit,
        const SkinConfig& skin,
        int page = 0) {
    if (candidates.empty()) return "";

    std::string result;
    int per_page = skin.max_candidates_per_page;
    int start = page * per_page;
    int end = std::min(start + per_page, (int)candidates.size());

    // 边框
    if (skin.border_style == "unicode") {
        result += "\xe2\x94\x8c";  // ┌
        for (int i = 0; i < 40; ++i) result += "\xe2\x94\x80";  // ─
        result += "\xe2\x94\x90\n";  // ┐
    } else if (skin.border_style == "ascii") {
        result += "+";
        for (int i = 0; i < 40; ++i) result += "-";
        result += "+\n";
    }

    // 拼音预编辑区
    if (skin.show_pinyin && !preedit.empty()) {
        if (!skin.background_color.empty()) result += skin.background_color;
        result += skin.preedit_color + preedit + skin.reset_color;
        if (!skin.background_color.empty()) result += skin.reset_color;
        result += "\n";
    }

    // 候选词
    for (int i = start; i < end; ++i) {
        if (!skin.background_color.empty()) result += skin.background_color;
        // 序号
        if (skin.show_index) {
            result += skin.index_color + std::to_string(i - start + 1) + "." + skin.reset_color;
            for (int s = 0; s < skin.candidate_spacing; ++s) result += " ";
        }
        // 候选词
        if (i == 0 && skin.highlight_first) {
            result += skin.focus_color + candidates[i] + skin.reset_color;
        } else {
            result += skin.candidate_color + candidates[i] + skin.reset_color;
        }
        if (!skin.background_color.empty()) result += skin.reset_color;

        if (skin.window_style == "vertical") {
            result += "\n";
        } else {
            for (int s = 0; s < skin.candidate_spacing; ++s) result += " ";
        }
    }

    // 底部边框
    if (skin.border_style == "unicode") {
        result += "\n\xe2\x94\x94";
        for (int i = 0; i < 40; ++i) result += "\xe2\x94\x80";
        result += "\xe2\x94\x98";
    } else if (skin.border_style == "ascii") {
        result += "\n+";
        for (int i = 0; i < 40; ++i) result += "-";
        result += "+";
    }

    return result;
}
