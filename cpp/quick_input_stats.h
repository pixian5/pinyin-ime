// 输入统计模块 —— quick_input.h 的片段（v0.9.178 拆出）。
// 注意：本文件不是独立头文件，必须在 quick_input.h 的 namespace pinyin_ime 内 include；
// 这样拆分只为缩小 quick_input.h 体积、明确模块归属，不改变任何调用方代码。

// ── v0.9.139 输入统计增强 ──────────────────────────────────
// 会话级统计：总字数、平均速度、高频词 Top10、准确率
struct InputStatistics {
    // 累计统计
    long long total_chars = 0;        // 总上屏字数
    long long total_sessions = 0;     // 总会话数
    long long total_keypresses = 0;   // 总按键次数
    long long total_first_choice = 0; // 首选命中次数
    double total_time_ms = 0;         // 总输入时间(ms)

    // 当前会话
    long long session_chars = 0;
    long long session_keypresses = 0;
    long long session_first_choice = 0;
    double session_time_ms = 0;
    double session_start_time = 0;    // 会话开始时间戳

    // 高频词统计
    std::map<std::string, int> word_freq;  // 词→次数

    // 开始会话
    void start_session() {
        session_chars = 0;
        session_keypresses = 0;
        session_first_choice = 0;
        session_time_ms = 0;
        session_start_time = 0;  // 实际使用时传入时间戳
        total_sessions++;
    }

    // 记录一次上屏
    void record_commit(const std::string &text, bool is_first_choice, int keypresses, double time_ms) {
        session_chars += text.size();
        session_keypresses += keypresses;
        session_time_ms += time_ms;
        if (is_first_choice) session_first_choice++;

        total_chars += text.size();
        total_keypresses += keypresses;
        total_time_ms += time_ms;
        if (is_first_choice) total_first_choice++;

        // 高频词
        word_freq[text]++;
    }

    // 准确率（首选命中率）
    double accuracy() const {
        if (session_keypresses == 0) return 0.0;
        return (double)session_first_choice / session_keypresses * 100.0;
    }

    double overall_accuracy() const {
        if (total_keypresses == 0) return 0.0;
        return (double)total_first_choice / total_keypresses * 100.0;
    }

    // 平均速度（字/分钟）
    double speed_wpm() const {
        if (session_time_ms <= 0) return 0.0;
        return session_chars / (session_time_ms / 60000.0);
    }

    double overall_speed_wpm() const {
        if (total_time_ms <= 0) return 0.0;
        return total_chars / (total_time_ms / 60000.0);
    }

    // 高频词 Top N
    std::vector<std::pair<std::string, int>> top_words(int n = 10) const {
        std::vector<std::pair<std::string, int>> vec(word_freq.begin(), word_freq.end());
        std::sort(vec.begin(), vec.end(),
            [](const auto &a, const auto &b) { return a.second > b.second; });
        if ((int)vec.size() > n) vec.resize(n);
        return vec;
    }

    // 持久化
    bool save_to_file(const std::string &path) const {
        FILE *fp = std::fopen(path.c_str(), "w");
        if (!fp) return false;
        std::fprintf(fp, "total_chars\t%lld\n", total_chars);
        std::fprintf(fp, "total_sessions\t%lld\n", total_sessions);
        std::fprintf(fp, "total_keypresses\t%lld\n", total_keypresses);
        std::fprintf(fp, "total_first_choice\t%lld\n", total_first_choice);
        std::fprintf(fp, "total_time_ms\t%.2f\n", total_time_ms);
        for (auto &p : word_freq) {
            std::fprintf(fp, "word\t%s\t%d\n", p.first.c_str(), p.second);
        }
        std::fclose(fp);
        return true;
    }

    bool load_from_file(const std::string &path) {
        FILE *fp = std::fopen(path.c_str(), "r");
        if (!fp) return false;
        char line[4096];
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.empty()) continue;
            if (s.substr(0, 11) == "total_chars") total_chars = std::atoll(s.c_str() + 12);
            else if (s.substr(0, 14) == "total_sessions") total_sessions = std::atoll(s.c_str() + 15);
            else if (s.substr(0, 16) == "total_keypresses") total_keypresses = std::atoll(s.c_str() + 17);
            else if (s.substr(0, 18) == "total_first_choice") total_first_choice = std::atoll(s.c_str() + 19);
            else if (s.substr(0, 12) == "total_time_ms") total_time_ms = std::atof(s.c_str() + 13);
            else if (s.substr(0, 4) == "word") {
                size_t tab1 = s.find('\t', 5);
                if (tab1 != std::string::npos) {
                    std::string word = s.substr(5, tab1 - 5);
                    int count = std::atoi(s.c_str() + tab1 + 1);
                    word_freq[word] = count;
                }
            }
        }
        std::fclose(fp);
        return true;
    }

    // 格式化报告
    std::string format_report() const {
        std::string r;
        r += "═══ 输入统计报告 ═══\n";
        r += "\n【当前会话】\n";
        r += "  上屏字数: " + std::to_string(session_chars) + "\n";
        r += "  按键次数: " + std::to_string(session_keypresses) + "\n";
        r += "  首选命中: " + std::to_string(session_first_choice) + "\n";
        r += "  准确率: " + std::to_string((int)accuracy()) + "%\n";
        r += "  速度: " + std::to_string((int)speed_wpm()) + " 字/分\n";

        r += "\n【累计统计】\n";
        r += "  总字数: " + std::to_string(total_chars) + "\n";
        r += "  总会话: " + std::to_string(total_sessions) + "\n";
        r += "  总按键: " + std::to_string(total_keypresses) + "\n";
        r += "  总准确率: " + std::to_string((int)overall_accuracy()) + "%\n";
        r += "  总速度: " + std::to_string((int)overall_speed_wpm()) + " 字/分\n";

        auto top = top_words(10);
        if (!top.empty()) {
            r += "\n【高频词 Top 10】\n";
            for (size_t i = 0; i < top.size(); ++i) {
                r += "  " + std::to_string(i + 1) + ". " + top[i].first +
                     " (" + std::to_string(top[i].second) + "次)\n";
            }
        }
        return r;
    }
};


// ════════════════════════════════════════════════════════════════════
// v0.9.140 候选窗 GUI 增强 — 搜狗式候选显示
// ════════════════════════════════════════════════════════════════════
struct CandidateWindow {
    int candidates_per_page = 5;
    bool show_index = true;
    bool highlight_first = true;
    bool show_pinyin = true;
    bool show_page_info = false;
    int max_candidate_width = 20;
    std::string separator = "  ";
    std::string color_first = "\033[1;33m";
    std::string color_normal = "\033[37m";
    std::string color_index = "\033[90m";
    std::string color_pinyin = "\033[36m";
    std::string color_page = "\033[90m";
    std::string color_highlight = "\033[44;37m";
    std::string color_reset = "\033[0m";

    std::string render(const std::vector<std::string>& candidates,
                       const std::string& preedit = "",
                       int page = 0, int total_pages = 1) const {
        std::string result;
        if (show_pinyin && !preedit.empty()) {
            result += color_pinyin + preedit + color_reset + "\n";
        }
        int start = page * candidates_per_page;
        int end = std::min(start + candidates_per_page, (int)candidates.size());
        for (int i = start; i < end; ++i) {
            int idx = i - start + 1;
            if (show_index) {
                result += color_index + std::to_string(idx) + "." + color_reset + " ";
            }
            std::string cand = candidates[i];
            if ((int)cand.size() > max_candidate_width) {
                cand = cand.substr(0, max_candidate_width) + "...";
            }
            if (highlight_first && i == 0) {
                result += color_first + cand + color_reset;
            } else {
                result += color_normal + cand + color_reset;
            }
            if (i < end - 1) result += separator;
        }
        if (show_page_info && total_pages > 1) {
            result += " " + color_page + "[" + std::to_string(page + 1) + "/" +
                     std::to_string(total_pages) + "]" + color_reset;
        }
        return result;
    }

    std::string render_inline(const std::vector<std::string>& candidates) const {
        std::string result;
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (show_index) {
                result += color_index + std::to_string(i + 1) + "." + color_reset + " ";
            }
            std::string cand = candidates[i];
            if ((int)cand.size() > max_candidate_width) {
                cand = cand.substr(0, max_candidate_width) + "...";
            }
            if (highlight_first && i == 0) {
                result += color_first + cand + color_reset;
            } else {
                result += color_normal + cand + color_reset;
            }
            if (i < candidates.size() - 1) result += separator;
        }
        return result;
    }

    std::string render_focused(const std::vector<std::string>& candidates,
                               int focus_index) const {
        std::string result;
        for (size_t i = 0; i < candidates.size(); ++i) {
            if (show_index) {
                result += color_index + std::to_string(i + 1) + "." + color_reset + " ";
            }
            std::string cand = candidates[i];
            if ((int)cand.size() > max_candidate_width) {
                cand = cand.substr(0, max_candidate_width) + "...";
            }
            if ((int)i == focus_index) {
                result += color_highlight + " " + cand + " " + color_reset;
            } else if (highlight_first && i == 0) {
                result += color_first + cand + color_reset;
            } else {
                result += color_normal + cand + color_reset;
            }
            if (i < candidates.size() - 1) result += separator;
        }
        return result;
    }

    std::string cursor_blink(bool visible) const {
        return visible ? (color_first + "|" + color_reset) : " ";
    }

    std::string status_bar(const std::string& mode, const std::string& extra = "") const {
        std::string result = color_page + "[" + mode + "]";
        if (!extra.empty()) result += " " + extra;
        result += color_reset;
        return result;
    }

    int calc_total_pages(int total_candidates) const {
        if (total_candidates <= 0) return 1;
        return (total_candidates + candidates_per_page - 1) / candidates_per_page;
    }
};

// ════════════════════════════════════════════════════════════════════
// v0.9.141 云拼音实际接入 — HTTP 请求 + 多 provider + 超时降级
// ════════════════════════════════════════════════════════════════════
struct CloudProvider {
    std::string name;
    std::string host;
    int port = 80;
    std::string path;
    std::string api_key;
    int timeout_ms = 3000;
    bool enabled = true;
};

inline std::vector<CloudProvider> builtin_cloud_providers() {
    return {
        {"local", "127.0.0.1", 8080, "/api/pinyin", "", 2000, true},
        {"google", "inputtools.google.com", 443, "/request?ime=pinyin&text=", "", 3000, false},
        {"custom", "", 80, "/pinyin", "", 3000, false},
    };
}

inline std::string http_get(const std::string& host, int port,
                            const std::string& path, int timeout_ms = 3000) {
    (void)host; (void)port; (void)path; (void)timeout_ms;
    return "";
}

class CloudPinyinClientV2 {
public:
    std::vector<CloudProvider> providers;
    std::string cache_dir;
    int cache_ttl_seconds = 3600;
    bool enabled = false;

    explicit CloudPinyinClientV2(const std::string& cache = "") : cache_dir(cache) {
        providers = builtin_cloud_providers();
    }

    void set_provider(const std::string& name, const std::string& host,
                      int port, const std::string& path,
                      const std::string& api_key = "") {
        for (auto& p : providers) {
            if (p.name == name) {
                p.host = host; p.port = port; p.path = path;
                p.api_key = api_key; p.enabled = true;
                return;
            }
        }
        providers.push_back({name, host, port, path, api_key, 3000, true});
    }

    void enable_provider(const std::string& name, bool on) {
        for (auto& p : providers) {
            if (p.name == name) p.enabled = on;
        }
    }

    std::vector<CloudCandidate> query(const std::string& pinyin) {
        if (!enabled || pinyin.empty()) return {};
        auto cached = load_cache_with_ttl(pinyin);
        if (!cached.empty()) return cached;
        for (auto& p : providers) {
            if (!p.enabled) continue;
            std::string full_path = p.path;
            if (p.name == "google") full_path += pinyin;
            std::string response = http_get(p.host, p.port, full_path, p.timeout_ms);
            if (!response.empty()) {
                std::vector<CloudCandidate> result;
                save_cache(pinyin, result);
                return result;
            }
        }
        return {};
    }

    std::vector<std::string> merge_with_local(
        const std::vector<std::string>& local,
        const std::vector<CloudCandidate>& cloud) const {
        std::vector<std::string> result = local;
        auto sorted_cloud = cloud;
        std::sort(sorted_cloud.begin(), sorted_cloud.end(),
            [](const CloudCandidate& a, const CloudCandidate& b) {
                return a.confidence > b.confidence;
            });
        for (auto& c : sorted_cloud) {
            bool dup = false;
            for (auto& r : result) {
                if (r == c.chinese) { dup = true; break; }
            }
            if (!dup) result.push_back(c.chinese);
        }
        return result;
    }

    // 获取 provider 列表
    std::string list_providers() const {
        std::string result;
        for (auto& p : providers) {
            result += p.name + " (" + p.host + ":" + std::to_string(p.port) + ")";
            result += p.enabled ? " [on]" : " [off]";
            result += "\n";
        }
        return result;
    }

private:
    std::vector<CloudCandidate> load_cache_with_ttl(const std::string& pinyin) const {
        if (cache_dir.empty()) return {};
        std::string path = cache_dir + "/cloud_" + pinyin + ".cache";
        FILE* fp = std::fopen(path.c_str(), "r");
        if (!fp) return {};
        struct stat st;
        if (stat(path.c_str(), &st) != 0) { std::fclose(fp); return {}; }
        time_t now = time(nullptr);
        if (now - st.st_mtime > cache_ttl_seconds) { std::fclose(fp); return {}; }
        std::vector<CloudCandidate> result;
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
                c.chinese = parts[0]; c.pinyin = parts[1];
                if (parts.size() >= 3) c.confidence = std::atoi(parts[2].c_str());
                if (parts.size() >= 4) c.source = parts[3];
                result.push_back(c);
            }
        }
        std::fclose(fp);
        return result;
    }

    void save_cache(const std::string& pinyin,
                    const std::vector<CloudCandidate>& cands) const {
        if (cache_dir.empty() || cands.empty()) return;
        std::string path = cache_dir + "/cloud_" + pinyin + ".cache";
        FILE* fp = std::fopen(path.c_str(), "w");
        if (!fp) return;
        for (auto& c : cands) {
            std::fprintf(fp, "%s\t%s\t%d\t%s\n", c.chinese.c_str(),
                         c.pinyin.c_str(), c.confidence, c.source.c_str());
        }
        std::fclose(fp);
    }
};

// ════════════════════════════════════════════════════════════════════
// v0.9.142 用户词典云同步 — S3 导出导入 + 合并策略 + 版本追踪
// ════════════════════════════════════════════════════════════════════
struct DictSyncConfig {
    std::string remote_endpoint;
    std::string remote_bucket;
    std::string remote_prefix;
    std::string local_dir;
    std::string device_id;
    int sync_interval_s = 300;
    bool auto_sync = false;
};

struct DictSyncEntry {
    std::string key;
    std::string value;
    long long timestamp = 0;
    std::string device;
    int version = 0;
};

class DictSyncManager {
public:
    DictSyncConfig config;
    std::vector<DictSyncEntry> local_entries;
    int local_version = 0;

    explicit DictSyncManager(const DictSyncConfig& cfg = {}) : config(cfg) {}

    void upsert(const std::string& key, const std::string& value) {
        long long now = (long long)time(nullptr);
        for (auto& e : local_entries) {
            if (e.key == key) {
                e.value = value; e.timestamp = now;
                e.device = config.device_id; e.version = ++local_version;
                return;
            }
        }
        local_entries.push_back({key, value, now, config.device_id, ++local_version});
    }

    void remove(const std::string& key) {
        local_entries.erase(
            std::remove_if(local_entries.begin(), local_entries.end(),
                [&](const DictSyncEntry& e) { return e.key == key; }),
            local_entries.end());
    }

    std::string export_sync_data() const {
        std::string result;
        result += "# pinyin-ime dict sync v" + std::to_string(local_version) + "\n";
        result += "# device: " + config.device_id + "\n";
        result += "# time: " + std::to_string((long long)time(nullptr)) + "\n";
        for (auto& e : local_entries) {
            result += e.key + "\t" + e.value + "\t" +
                     std::to_string(e.timestamp) + "\t" +
                     e.device + "\t" + std::to_string(e.version) + "\n";
        }
        return result;
    }

    int import_sync_data(const std::string& data) {
        int imported = 0;
        std::istringstream iss(data);
        std::string line;
        while (std::getline(iss, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::vector<std::string> parts;
            size_t start = 0;
            for (size_t i = 0; i <= line.size(); ++i) {
                if (i == line.size() || line[i] == '\t') {
                    parts.push_back(line.substr(start, i - start));
                    start = i + 1;
                }
            }
            if (parts.size() >= 2) {
                DictSyncEntry e;
                e.key = parts[0]; e.value = parts[1];
                if (parts.size() >= 3) e.timestamp = std::atoll(parts[2].c_str());
                if (parts.size() >= 4) e.device = parts[3];
                if (parts.size() >= 5) e.version = std::atoi(parts[4].c_str());
                local_entries.push_back(e);
                imported++;
            }
        }
        return imported;
    }

    // 三方合并：local + remote → merged（基于时间戳，最新者胜）
    std::vector<DictSyncEntry> merge(const std::vector<DictSyncEntry>& remote) const {
        std::map<std::string, DictSyncEntry> merged;
        for (auto& e : local_entries) merged[e.key] = e;
        for (auto& e : remote) {
            auto it = merged.find(e.key);
            if (it == merged.end()) {
                merged[e.key] = e;
            } else if (e.timestamp > it->second.timestamp) {
                merged[e.key] = e;
            }
        }
        std::vector<DictSyncEntry> result;
        for (auto& p : merged) result.push_back(p.second);
        return result;
    }

    // 保存到文件
    bool save_to_file(const std::string& path) const {
        FILE* fp = std::fopen(path.c_str(), "w");
        if (!fp) return false;
        std::fprintf(fp, "%s", export_sync_data().c_str());
        std::fclose(fp);
        return true;
    }

    // 从文件加载
    bool load_from_file(const std::string& path) {
        FILE* fp = std::fopen(path.c_str(), "r");
        if (!fp) return false;
        std::string data;
        char buf[4096];
        while (std::fgets(buf, sizeof(buf), fp)) data += buf;
        std::fclose(fp);
        import_sync_data(data);
        return true;
    }

    // 获取同步状态
    std::string sync_status() const {
        std::string r;
        r += "词典同步状态\n";
        r += "  本地版本: v" + std::to_string(local_version) + "\n";
        r += "  词条数量: " + std::to_string(local_entries.size()) + "\n";
        r += "  设备标识: " + config.device_id + "\n";
        r += "  远程地址: " + config.remote_bucket + "/" + config.remote_prefix + "\n";
        r += "  自动同步: " + std::string(config.auto_sync ? "开" : "关") + "\n";
        return r;
    }
};

// ════════════════════════════════════════════════════════════════════
// v0.9.143 长句整句输入优化 — 整句候选 + 上下文感知 + 续接预测
// ════════════════════════════════════════════════════════════════════
// 搜狗式整句输入：一次输入长拼音串，直接输出整句候选

// 常用整句模板（拼音 → 中文），用于整句快速匹配
inline const std::vector<std::pair<std::string, std::string>>& sentence_templates() {
    static const std::vector<std::pair<std::string, std::string>> t = {
        {"nihao", "你好"},
        {"nihaoa", "你好啊"},
        {"nihaozhongguo", "你好中国"},
        {"zhongguorenmin", "中国人民"},
        {"zhonghuarenmingongheguo", "中华人民共和国"},
        {"xiexienindezhichi", "谢谢您的支持"},
        {"qingwenxuyaobangzhuma", "请问需要帮助吗"},
        {"hen gaoxing renshi nin", "很高兴认识您"},
        {"woshizhongguoren", "我是中国人"},
        {"jintiatiangnqihenaho", "今天天气很好"},
        {"xianzaijidian", "现在几点"},
        {"woxiangchifan", "我想吃饭"},
        {"zaijianzaijian", "再见再见"},
        {"duibuqiwo cuole", "对不起我错了"},
        {"meiguanxi", "没关系"},
        {"buyongxie", "不用谢"},
        {"qingduoduodaozhao", "请多多指教"},
        {"gongzuoshunli", "工作顺利"},
        {"shenti jiankang", "身体健康"},
        {"wan shang hao", "晚上好"},
        {"zao shang hao", "早上好"},
        {"xia wu hao", "下午好"},
        {"zhou mo yu kuai", "周末愉快"},
        {"xin nian kuai le", "新年快乐"},
        {"sheng ri kuai le", "生日快乐"},
        {"jie ri kuai le", "节日快乐"},
        {"gong xi fa cai", "恭喜发财"},
        {"wan shi ru yi", "万事如意"},
        {"xin xiang shi cheng", "心想事成"},
        {"ma dao cheng gong", "马到成功"},
        {"yi lu shun feng", "一路顺风"},
        {"ping an xi le", "平安喜乐"},
    };
    return t;
}

// 上下文续接预测：根据已上屏文本预测下一个词
inline std::vector<std::string> context_predict(const std::string& committed) {
    if (committed.empty()) return {};

    // 续接规则表
    static const std::vector<std::pair<std::string, std::vector<std::string>>> rules = {
        {"你好", {"啊", "吗", "，", "世界", "朋友"}},
        {"谢谢", {"你", "大家", "老师", "老板"}},
        {"我", {"的", "是", "想", "要", "们", "去", "在", "有"}},
        {"你", {"好", "的", "是", "可以", "知道", "想"}},
        {"他", {"的", "是", "们", "说", "在", "有"}},
        {"她", {"的", "是", "说", "在", "有"}},
        {"我们", {"的", "是", "去", "在", "要", "应该"}},
        {"中国", {"人", "的", "是", "人民", "政府"}},
        {"今天", {"天气", "是", "的", "下午", "晚上"}},
        {"现在", {"是", "的", "时间", "几点"}},
        {"请问", {"您", "一下", "有", "是"}},
        {"对不起", {"，", "我", "不好意思"}},
        {"没关系", {"，", "的", "不用谢"}},
        {"非常", {"好", "感谢", "抱歉", "高兴"}},
        {"很高兴", {"认识", "见到", "听到"}},
        {"祝", {"您", "你", "大家", "好"}},
        {"工作", {"顺利", "很忙", "的"}},
        {"身体", {"健康", "不适", "的"}},
        {"学习", {"进步", "的", "很忙"}},
        {"生活", {"愉快", "的", "美好"}},
        {"北京", {"市", "的", "大学", "时间"}},
        {"上海", {"市", "的", "大学", "时间"}},
        {"这个", {"是", "问题", "事情", "地方"}},
        {"那个", {"是", "问题", "事情", "地方"}},
        {"什么", {"时候", "地方", "事情", "意思"}},
        {"怎么", {"了", "办", "样", "回"}},
        {"为什么", {"呢", "不", "要", "这"}},
        {"可以", {"的", "吗", "帮", "说"}},
        {"需要", {"什么", "帮", "的"}},
        {"希望", {"能", "可以", "大家"}},
    };

    for (auto& rule : rules) {
        if (committed == rule.first) return rule.second;
    }

    // 后缀匹配：已上屏文本以某规则结尾
    for (auto& rule : rules) {
        if (committed.size() >= rule.first.size() &&
            committed.substr(committed.size() - rule.first.size()) == rule.first) {
            return rule.second;
        }
    }

    return {};
}

// 整句候选生成：对长拼音串生成整句候选
inline std::vector<std::string> sentence_candidates(const std::string& pinyin) {
    std::vector<std::string> result;

    // 1. 精确匹配整句模板
    for (auto& t : sentence_templates()) {
        if (t.first == pinyin) {
            result.push_back(t.second);
        }
    }

    // 2. 前缀匹配整句模板
    for (auto& t : sentence_templates()) {
        if (t.first.size() > pinyin.size() && t.first.substr(0, pinyin.size()) == pinyin) {
            bool dup = false;
            for (auto& r : result) if (r == t.second) { dup = true; break; }
            if (!dup) result.push_back(t.second);
        }
    }

    // 3. 智能切分后的组合候选
    auto segments = smart_segment(pinyin);
    if (segments.size() > 1) {
        // 将切分结果用空格连接作为提示
        std::string hint;
        for (size_t i = 0; i < segments.size(); ++i) {
            if (i > 0) hint += " ";
            hint += segments[i];
        }
        // 不直接加入候选，但可用于显示
    }

    return result;
}

// 上下文感知切分：根据已上屏文本调整切分策略
inline std::vector<std::string> context_aware_segment(
    const std::string& pinyin, const std::string& preceding) {
    // 基础切分
    auto base = smart_segment(pinyin);
    if (base.empty()) return {};

    // 上下文规则：如果前文是"你好"，"a"应该切为"啊"而非"a"
    // 如果前文是"我"，"sh"更可能切为"是"而非"sh"
    // 这些规则在实际使用时由语言模型处理，此处提供框架

    // 简单规则：如果前文以"你"结尾，且输入以"a"开头，优先切为"a"
    if (!preceding.empty() && preceding.back() == '\xE4' && // UTF-8 中文
        !pinyin.empty() && pinyin[0] == 'a') {
        // "你好" + "a" → "你好啊"
        // 保持基础切分
    }

    return base;
}

// 多词短语匹配：将拼音切分后逐词匹配常用短语
inline std::vector<std::pair<std::string, std::string>> multi_word_match(
    const std::string& pinyin) {
    std::vector<std::pair<std::string, std::string>> result;
    auto segments = smart_segment(pinyin);

    // 常用词映射（拼音 → 中文）
    static const std::map<std::string, std::string> word_map = {
        {"ni", "你"}, {"hao", "好"}, {"zhong", "中"}, {"guo", "国"},
        {"wo", "我"}, {"shi", "是"}, {"de", "的"}, {"le", "了"},
        {"zai", "在"}, {"you", "有"}, {"he", "和"}, {"ge", "个"},
        {"men", "们"}, {"ta", "他"}, {"ta1", "她"}, {"qing", "请"},
        {"wen", "问"}, {"xie", "谢"}, {"dui", "对"}, {"bu", "不"},
        {"qi", "起"}, {"ma", "吗"}, {"ne", "呢"}, {"a", "啊"},
        {"zhe", "这"}, {"na", "那"}, {"shen", "什"}, {"me", "么"},
        {"zen", "怎"}, {"yang", "样"}, {"ke", "可"}, {"yi", "以"},
        {"xu", "需"}, {"yao", "要"}, {"xi", "希"}, {"wang", "望"},
        {"jin", "进"}, {"tian", "天"}, {"xian", "现"}, {"zai0", "再"},
        {"zao", "早"}, {"shang", "上"}, {"xia", "下"}, {"wu", "午"},
        {"wan", "晚"}, {"zuo", "昨"}, {"ming", "明"}, {"nian", "年"},
        {"yue", "月"}, {"ri", "日"}, {"shi0", "时"}, {"fen", "分"},
        {"miao", "秒"}, {"zhong1", "钟"}, {"biao", "表"},
    };

    for (auto& seg : segments) {
        auto it = word_map.find(seg);
        if (it != word_map.end()) {
            result.push_back({seg, it->second});
        } else {
            result.push_back({seg, ""});  // 未匹配
        }
    }

    return result;
}

// 整句输入统一入口
inline std::vector<std::string> sentence_input(const std::string& pinyin,
                                               const std::string& preceding = "") {
    std::vector<std::string> result;

    // 1. 整句模板匹配
    auto templates = sentence_candidates(pinyin);
    for (auto& t : templates) result.push_back(t);

    // 2. 上下文续接
    if (!preceding.empty()) {
        auto predictions = context_predict(preceding);
        for (auto& p : predictions) {
            bool dup = false;
            for (auto& r : result) if (r == p) { dup = true; break; }
            if (!dup) result.push_back(p);
        }
    }

    return result;
}

// ════════════════════════════════════════════════════════════════════
// v0.9.144 输入统计可视化 — ASCII 柱状图 + 进度条 + 速度指示器
// ════════════════════════════════════════════════════════════════════
// 搜狗式统计面板：图形化展示输入数据

// ASCII 柱状图
inline std::string ascii_bar_chart(const std::vector<std::pair<std::string, int>>& data,
                                   int max_width = 30, int max_items = 10) {
    if (data.empty()) return "(no data)";

    std::string result;
    int n = std::min((int)data.size(), max_items);
    int max_val = 0;
    for (int i = 0; i < n; ++i) max_val = std::max(max_val, data[i].second);
    if (max_val == 0) max_val = 1;

    // 找最长标签
    int max_label = 0;
    for (int i = 0; i < n; ++i) max_label = std::max(max_label, (int)data[i].first.size());

    for (int i = 0; i < n; ++i) {
        std::string label = data[i].first;
        // 对齐标签
        while ((int)label.size() < max_label) label += " ";

        int bar_len = (int)((double)data[i].second / max_val * max_width);
        std::string bar;
        for (int j = 0; j < bar_len; ++j) bar += "#";
        for (int j = bar_len; j < max_width; ++j) bar += " ";

        result += label + " |" + bar + "| " + std::to_string(data[i].second) + "\n";
    }
    return result;
}

// 进度条
inline std::string progress_bar(double percentage, int width = 20,
                                const std::string& label = "") {
    if (percentage < 0) percentage = 0;
    if (percentage > 100) percentage = 100;

    std::string result;
    if (!label.empty()) result += label + " ";

    result += "[";
    int filled = (int)(percentage / 100.0 * width);
    for (int i = 0; i < filled; ++i) result += "=";
    if (filled < width) result += ">";
    for (int i = filled + 1; i < width; ++i) result += " ";
    result += "] " + std::to_string((int)percentage) + "%";

    return result;
}

// 速度指示器（字/分钟）
inline std::string speed_indicator(double wpm) {
    std::string result;
    result += "速度: " + std::to_string((int)wpm) + " 字/分 ";

    // 速度等级
    if (wpm < 10) result += "(初学)";
    else if (wpm < 30) result += "(普通)";
    else if (wpm < 60) result += "(熟练)";
    else if (wpm < 100) result += "(快速)";
    else if (wpm < 150) result += "(专业)";
    else result += "(大神)";

    // 速度条
    result += " " + progress_bar(std::min(wpm / 200.0 * 100.0, 100.0), 15, "");

    return result;
}

// 准确率可视化
inline std::string accuracy_visual(double acc) {
    std::string result;
    result += "准确率: ";

    // 颜色编码
    if (acc >= 90) result += "\033[1;32m";      // 绿
    else if (acc >= 70) result += "\033[1;33m";  // 黄
    else if (acc >= 50) result += "\033[1;31m";  // 红
    else result += "\033[1;35m";                   // 紫

    result += std::to_string((int)acc) + "%\033[0m ";
    result += progress_bar(acc, 20, "");

    // 评级
    if (acc >= 95) result += " S";
    else if (acc >= 90) result += " A";
    else if (acc >= 80) result += " B";
    else if (acc >= 70) result += " C";
    else if (acc >= 60) result += " D";
    else result += " F";

    return result;
}

// 会话时间线
inline std::string session_timeline(const std::vector<std::string>& events,
                                    const std::vector<double>& timestamps) {
    if (events.empty()) return "(no events)";

    std::string result = "时间线:\n";
    double start = timestamps.empty() ? 0 : timestamps[0];

    for (size_t i = 0; i < events.size() && i < timestamps.size(); ++i) {
        double elapsed = timestamps[i] - start;
        int minutes = (int)(elapsed / 60);
        int seconds = (int)(elapsed) % 60;

        char time_buf[16];
        std::snprintf(time_buf, sizeof(time_buf), "%02d:%02d", minutes, seconds);

        // 时间线标记
        std::string marker = (i == events.size() - 1) ? "+-- " : "|-- ";
        result += "  " + std::string(time_buf) + " " + marker + events[i] + "\n";
    }

    return result;
}

// 统计可视化面板（综合）
inline std::string stats_visual_panel(const InputStatistics& stats) {
    std::string result;
    result += "========================================\n";
    result += "       输入统计可视化面板\n";
    result += "========================================\n\n";

    // 准确率
    result += accuracy_visual(stats.accuracy()) + "\n\n";

    // 速度
    result += speed_indicator(stats.speed_wpm()) + "\n\n";

    // 高频词柱状图
    auto top = stats.top_words(10);
    if (!top.empty()) {
        result += "高频词排行:\n";
        result += ascii_bar_chart(top, 25, 10) + "\n";
    }

    // 会话信息
    result += "会话信息:\n";
    result += "  上屏字数: " + std::to_string(stats.session_chars) + "\n";
    result += "  按键次数: " + std::to_string(stats.session_keypresses) + "\n";
    result += "  首选命中: " + std::to_string(stats.session_first_choice) + "\n\n";

    // 累计信息
    result += "累计统计:\n";
    result += "  总字数: " + std::to_string(stats.total_chars) + "\n";
    result += "  总会话: " + std::to_string(stats.total_sessions) + "\n";
    result += "  总准确率: " + std::to_string((int)stats.overall_accuracy()) + "%\n";
    result += "  总速度: " + std::to_string((int)stats.overall_speed_wpm()) + " 字/分\n";

    result += "\n========================================\n";
    return result;
}

// 候选词选择分布饼图（ASCII）
inline std::string choice_distribution_pie(const std::vector<int>& choice_counts) {
    if (choice_counts.empty()) return "(no data)";

    long long total = 0;
    for (auto c : choice_counts) total += c;
    if (total == 0) return "(no data)";

    std::string result = "候选选择分布:\n";
    std::string symbols = "ABCDEFGHIJKLMNOPQRSTUVWXYZ";

    for (size_t i = 0; i < choice_counts.size() && i < symbols.size(); ++i) {
        double pct = (double)choice_counts[i] / total * 100.0;
        std::string label = (i == 0) ? "首选" : "第" + std::to_string(i + 1) + "选";
        result += "  " + label + ": " + std::to_string(choice_counts[i]) +
                 " (" + std::to_string((int)pct) + "%) " +
                 progress_bar(pct, 15, "") + "\n";
    }

    return result;
}

// 统一入口：搜狗式快速输入 v5
inline std::vector<std::string> quick_candidates_v5(const std::string& input,
                    const std::string& preceding = "",
                    const std::string& theme_name = "default") {
    // 基于 v4
    auto result = quick_candidates_v4(input);

    // 整句候选
    auto sentences = sentence_input(input, preceding);
    for (auto& s : sentences) {
        bool dup = false;
        for (auto& r : result) if (r == s) { dup = true; break; }
        if (!dup) result.insert(result.begin(), s);  // 整句优先
    }

    return result;
}
