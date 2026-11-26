// 云拼音与模糊音模块 —— quick_input.h 的片段（v0.9.178 拆出）。
// 注意：本文件不是独立头文件，必须在 quick_input.h 的 namespace pinyin_ime 内 include；
// 拆分只为缩小 quick_input.h 体积、明确模块归属，不改变任何调用方代码。

// ── v0.9.132 云拼音候选词接口 ──────────────────────────────
// 定义云候选接口框架，支持本地缓存和离线降级
struct CloudCandidate {
    std::string chinese;
    std::string pinyin;
    int confidence = 0;       // 置信度 0-100
    std::string source;       // 来源标识
};

struct CloudPinyinConfig {
    std::string endpoint;     // 云服务地址
    std::string api_key;      // API密钥
    bool enabled = false;
    int timeout_ms = 3000;    // 超时
    size_t max_candidates = 5;
    std::string cache_dir;    // 本地缓存目录
};

class CloudPinyinClient {
public:
    CloudPinyinConfig config;

    explicit CloudPinyinClient(const CloudPinyinConfig &cfg = {}) : config(cfg) {}

    // 查询云候选（同步接口）
    // 实际部署时用 libcurl/HTTP 请求；此处提供框架和本地缓存
    std::vector<CloudCandidate> query(const std::string &pinyin) {
        if (!config.enabled || pinyin.empty()) return {};

        // 1. 查本地缓存
        auto cached = load_cache(pinyin);
        if (!cached.empty()) return cached;

        // 2. 云端请求（框架——实际需 HTTP 客户端）
        // POST {endpoint}/pinyin
        // Body: {"pinyin": "...", "api_key": "..."}
        // Response: {"candidates": [{"chinese": "...", "confidence": 90}]}
        //
        // 此处返回空——离线降级
        // 实际实现需要 libcurl 或 socket 编程

        return {};
    }

    // 异步查询接口（回调方式）
    void query_async(const std::string &pinyin,
                     std::function<void(const std::vector<CloudCandidate>&)> callback) {
        // 实际实现用线程池/异步IO
        callback(query(pinyin));
    }

    // 合并云候选到本地候选列表
    std::vector<std::string> merge_candidates(
        const std::vector<std::string> &local,
        const std::vector<CloudCandidate> &cloud) const {
        std::vector<std::string> result = local;
        for (auto &c : cloud) {
            bool dup = false;
            for (auto &r : result) {
                if (r == c.chinese) { dup = true; break; }
            }
            if (!dup) result.push_back(c.chinese);
        }
        return result;
    }

    // 缓存管理
    std::string cache_path(const std::string &pinyin) const {
        if (config.cache_dir.empty()) return "";
        return config.cache_dir + "/cloud_" + pinyin + ".cache";
    }

    bool save_cache(const std::string &pinyin,
                    const std::vector<CloudCandidate> &cands) const {
        std::string path = cache_path(pinyin);
        if (path.empty()) return false;
        // 确保缓存目录存在
        if (!config.cache_dir.empty()) {
            std::string cmd = "mkdir -p " + config.cache_dir;
            // 明确忽略返回码：目录已存在时 mkdir -p 也返回 0，失败由下面 fopen 兜底
            if (std::system(cmd.c_str()) != 0) { /* 忽略，交给 fopen 判断 */ }
        }
        FILE *fp = std::fopen(path.c_str(), "w");
        if (!fp) return false;
        for (auto &c : cands) {
            std::fprintf(fp, "%s\t%s\t%d\t%s\n",
                         c.chinese.c_str(), c.pinyin.c_str(),
                         c.confidence, c.source.c_str());
        }
        std::fclose(fp);
        return true;
    }

    std::vector<CloudCandidate> load_cache(const std::string &pinyin) const {
        std::vector<CloudCandidate> result;
        std::string path = cache_path(pinyin);
        if (path.empty()) return result;
        FILE *fp = std::fopen(path.c_str(), "r");
        if (!fp) return result;
        char line[1024];
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.empty()) continue;
            std::vector<std::string> parts;
            size_t start = 0;
            for (size_t i = 0; i <= s.size(); ++i) {
                if (i == s.size() || s[i] == '\t') {
                    parts.push_back(s.substr(start, i - start));
                    start = i + 1;
                }
            }
            if (parts.size() >= 2) {
                CloudCandidate c;
                c.chinese = parts[0];
                c.pinyin = parts[1];
                if (parts.size() >= 3) c.confidence = std::atoi(parts[2].c_str());
                if (parts.size() >= 4) c.source = parts[3];
                result.push_back(c);
            }
        }
        std::fclose(fp);
        return result;
    }

    // 清空缓存
    void clear_cache() const {
        if (config.cache_dir.empty()) return;
        // 实际实现用 filesystem 遍历删除
    }
};

// ── v0.9.135 模糊音（Fuzzy Pinyin）──────────────────────────
// 搜狗式模糊音：z/zh, c/ch, s/sh, n/l, f/h, an/ang, en/eng, in/ing
// 用户打 z 时同时匹配 zh，打 n 时同时匹配 l，扩大候选范围。
struct FuzzyPinyinConfig {
    bool z_zh = true;      // z ↔ zh
    bool c_ch = true;      // c ↔ ch
    bool s_sh = true;      // s ↔ sh
    bool n_l = true;       // n ↔ l
    bool f_h = false;      // f ↔ h（误打较多，默认关）
    bool an_ang = true;    // an ↔ ang
    bool en_eng = true;    // en ↔ eng
    bool in_ing = true;    // in ↔ ing
    bool r_l = false;      // r ↔ l（方言模糊，默认关）
    bool ue_ve = false;    // ue ↔ ve（默认关）

    // 从字符串解析配置（如 "z_zh,c_ch,n_l"）
    static FuzzyPinyinConfig parse(const std::string &s) {
        FuzzyPinyinConfig cfg;
        if (s == "all" || s == "全开") {
            cfg.f_h = true; cfg.r_l = true; cfg.ue_ve = true;
            return cfg;
        }
        if (s.empty()) return cfg;
        // 逐项解析
        auto has = [&](const std::string &key) {
            return s.find(key) != std::string::npos;
        };
        cfg.z_zh = has("z_zh");
        cfg.c_ch = has("c_ch");
        cfg.s_sh = has("s_sh");
        cfg.n_l = has("n_l");
        cfg.f_h = has("f_h");
        cfg.an_ang = has("an_ang");
        cfg.en_eng = has("en_eng");
        cfg.in_ing = has("in_ing");
        cfg.r_l = has("r_l");
        cfg.ue_ve = has("ue_ve");
        return cfg;
    }
};

// 模糊音映射表：返回输入音节的所有模糊变体
inline std::vector<std::string> fuzzy_syllable_variants(
    const std::string &syllable, const FuzzyPinyinConfig &cfg) {
    std::vector<std::string> result = {syllable};
    auto add = [&](const std::string &s) {
        for (auto &r : result) if (r == s) return;
        result.push_back(s);
    };

    // 声母模糊：z↔zh, c↔ch, s↔sh, n↔l, f↔h, r↔l
    if (syllable.size() >= 1) {
        char first = syllable[0];
        std::string rest = syllable.substr(1);

        if (cfg.z_zh) {
            if (first == 'z' && syllable.size() > 1 && syllable[1] != 'h')
                add("zh" + rest);
            if (first == 'z' && syllable.size() > 1 && syllable[1] == 'h')
                add("z" + syllable.substr(2));
        }
        if (cfg.c_ch) {
            if (first == 'c' && syllable.size() > 1 && syllable[1] != 'h')
                add("ch" + rest);
            if (first == 'c' && syllable.size() > 1 && syllable[1] == 'h')
                add("c" + syllable.substr(2));
        }
        if (cfg.s_sh) {
            if (first == 's' && syllable.size() > 1 && syllable[1] != 'h')
                add("sh" + rest);
            if (first == 's' && syllable.size() > 1 && syllable[1] == 'h')
                add("s" + syllable.substr(2));
        }
        if (cfg.n_l) {
            if (first == 'n') add("l" + rest);
            if (first == 'l') add("n" + rest);
        }
        if (cfg.f_h) {
            if (first == 'f') add("h" + rest);
            if (first == 'h' && syllable.size() > 1 && syllable[1] != 'h')
                add("f" + rest);
        }
        if (cfg.r_l) {
            if (first == 'r') add("l" + rest);
            if (first == 'l') add("r" + rest);
        }
    }

    // 韵母模糊：an↔ang, en↔eng, in↔ing, ue↔ve
    // 对所有已有变体（含声母模糊产生的）都尝试韵母替换
    {
        auto try_replace = [&](const std::string &from, const std::string &to) {
            size_t n = result.size();
            for (size_t i = 0; i < n; ++i) {
                size_t pos = result[i].find(from);
                if (pos != std::string::npos && pos > 0) {
                    std::string variant = result[i].substr(0, pos) + to + result[i].substr(pos + from.size());
                    add(variant);
                }
            }
        };
        if (cfg.an_ang) { try_replace("an", "ang"); try_replace("ang", "an"); }
        if (cfg.en_eng) { try_replace("en", "eng"); try_replace("eng", "en"); }
        if (cfg.in_ing) { try_replace("in", "ing"); try_replace("ing", "in"); }
        if (cfg.ue_ve) { try_replace("ue", "ve"); try_replace("ve", "ue"); }
    }

    return result;
}

// 对完整拼音串（可能多音节，用空格或隔音符分隔）做模糊展开
// 返回所有可能的拼音组合
inline std::vector<std::string> fuzzy_pinyin_expand(
    const std::string &pinyin, const FuzzyPinyinConfig &cfg) {
    if (pinyin.empty()) return {};

    // 按空格/隔音符切分音节
    std::vector<std::string> syllables;
    std::string current;
    for (char c : pinyin) {
        if (c == ' ' || c == '\'') {
            if (!current.empty()) { syllables.push_back(current); current.clear(); }
        } else {
            current += c;
        }
    }
    if (!current.empty()) syllables.push_back(current);

    if (syllables.empty()) return {pinyin};

    // 对每个音节做模糊展开，然后做笛卡尔积
    std::vector<std::string> result = {""};
    for (auto &syl : syllables) {
        auto variants = fuzzy_syllable_variants(syl, cfg);
        std::vector<std::string> new_result;
        for (auto &prefix : result) {
            for (auto &variant : variants) {
                if (prefix.empty()) new_result.push_back(variant);
                else new_result.push_back(prefix + " " + variant);
            }
        }
        result = std::move(new_result);
        // 限制组合爆炸：最多 32 种组合
        if (result.size() > 32) result.resize(32);
    }

    return result;
}
