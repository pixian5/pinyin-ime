// 长句切分与短语管理模块 —— quick_input.h 的片段（v0.9.178 拆出）。
// 注意：本文件不是独立头文件，必须在 quick_input.h 的 namespace pinyin_ime 内 include；
// 拆分只为缩小 quick_input.h 体积、明确模块归属，不改变任何调用方代码。

// ── v0.9.137 长句智能切分 ──────────────────────────────────
// 对无隔音符的长拼音串进行智能音节切分。
// 例如 "nihaoshijie" → ["ni", "hao", "shi", "jie"]
// 处理零声母 (y/w: yi→i, wu→u, yu→v)、三拼音 (chuang, shuang 等)

// 完整拼音音节表（含声调省略）
inline const std::set<std::string> &pinyin_syllable_set() {
    static const std::set<std::string> s = {
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
        "zhun", "zhuo", "zi", "zong", "zou", "zu", "zuan", "zui", "zun", "zuo"
    };
    return s;
}

// 判断是否是合法拼音音节
inline bool is_pinyin_syllable(const std::string &s) {
    return pinyin_syllable_set().count(s) > 0;
}

// 智能切分：动态规划，最大化合法音节数
inline std::vector<std::string> smart_segment(const std::string &pinyin) {
    if (pinyin.empty()) return {};
    int n = pinyin.size();
    auto &syllables = pinyin_syllable_set();

    // dp[i] = 从位置 i 开始能切出的最佳方案（音节数最少=优先长音节）
    // 用回溯+记忆化
    // memo[i].first = 是否有有效切分; memo[i].second = 最佳切分
    std::vector<std::pair<bool, std::vector<std::string>>> memo(n + 1, {false, {}});
    std::vector<bool> computed(n + 1, false);
    computed[n] = true;
    memo[n] = {true, {}};  // 末尾：空切分，有效

    std::function<std::vector<std::string>(int)> solve = [&](int pos) -> std::vector<std::string> {
        if (computed[pos]) return memo[pos].second;
        computed[pos] = true;
        memo[pos] = {false, {}};  // 默认：无法切分

        // 尝试所有可能的音节长度 (1-6)，优先长音节（从长到短）
        for (int len = std::min(6, n - pos); len >= 1; --len) {
            std::string syllable = pinyin.substr(pos, len);
            if (syllables.count(syllable)) {
                auto rest = solve(pos + len);
                if (memo[pos + len].first) {
                    // 有效切分
                    std::vector<std::string> candidate = {syllable};
                    candidate.insert(candidate.end(), rest.begin(), rest.end());
                    if (!memo[pos].first || candidate.size() < memo[pos].second.size()) {
                        memo[pos] = {true, candidate};
                    }
                }
            }
        }
        return memo[pos].second;
    };

    auto result = solve(0);
    if (!memo[0].first) {
        // 无法完全切分，返回原始输入作为单个元素
        return {pinyin};
    }
    return result;
}

// 切分结果转为空格分隔的字符串
inline std::string smart_segment_string(const std::string &pinyin) {
    auto segments = smart_segment(pinyin);
    std::string result;
    for (size_t i = 0; i < segments.size(); ++i) {
        if (i > 0) result += " ";
        result += segments[i];
    }
    return result;
}

// 在拼音串中自动插入隔音符
inline std::string insert_separators(const std::string &pinyin) {
    return smart_segment_string(pinyin);
}

// ── v0.9.138 自定义短语运行时管理 ──────────────────────────
// 用户可运行时增删查自定义快捷词，持久化到 user_phrases.txt
struct UserPhraseManager {
    std::vector<std::pair<std::string, std::string>> phrases;  // (abbr, full)
    std::string file_path;

    explicit UserPhraseManager(const std::string &path = "") : file_path(path) {
        if (!path.empty()) load();
    }

    // 添加短语（去重）
    bool add(const std::string &abbr, const std::string &full) {
        if (abbr.empty() || full.empty()) return false;
        for (auto &p : phrases) {
            if (p.first == abbr) { p.second = full; return true; }  // 更新已有
        }
        phrases.push_back({abbr, full});
        return true;
    }

    // 删除短语
    bool remove(const std::string &abbr) {
        auto it = std::find_if(phrases.begin(), phrases.end(),
            [&](const auto &p) { return p.first == abbr; });
        if (it == phrases.end()) return false;
        phrases.erase(it);
        return true;
    }

    // 查找短语
    std::string lookup(const std::string &abbr) const {
        for (auto &p : phrases) {
            if (p.first == abbr) return p.second;
        }
        return "";
    }

    // 列出所有短语
    std::vector<std::pair<std::string, std::string>> list_all() const {
        return phrases;
    }

    // 搜索短语（按缩写或全文）
    std::vector<std::pair<std::string, std::string>> search(const std::string &keyword) const {
        std::vector<std::pair<std::string, std::string>> results;
        for (auto &p : phrases) {
            if (p.first.find(keyword) != std::string::npos ||
                p.second.find(keyword) != std::string::npos) {
                results.push_back(p);
            }
        }
        return results;
    }

    // 持久化
    bool save() const {
        if (file_path.empty()) return false;
        FILE *fp = std::fopen(file_path.c_str(), "w");
        if (!fp) return false;
        for (auto &p : phrases) {
            std::fprintf(fp, "%s\t%s\n", p.first.c_str(), p.second.c_str());
        }
        std::fclose(fp);
        return true;
    }

    bool load() {
        if (file_path.empty()) return false;
        phrases.clear();
        FILE *fp = std::fopen(file_path.c_str(), "r");
        if (!fp) return false;
        char line[4096];
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.empty()) continue;
            size_t tab = s.find('\t');
            if (tab != std::string::npos) {
                phrases.push_back({s.substr(0, tab), s.substr(tab + 1)});
            }
        }
        std::fclose(fp);
        return true;
    }

    // 格式化显示
    std::string format_list() const {
        std::string result;
        for (size_t i = 0; i < phrases.size(); ++i) {
            result += phrases[i].first + " → " + phrases[i].second + "\n";
        }
        return result;
    }
};
