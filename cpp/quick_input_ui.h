// 表情云同步与候选窗模块 —— quick_input.h 的片段（v0.9.178 拆出）。
// 注意：本文件不是独立头文件，必须在 quick_input.h 的 namespace pinyin_ime 内 include；
// 拆分只为缩小 quick_input.h 体积、明确模块归属，不改变任何调用方代码。

// ── v0.9.148 表情/符号可视化面板 ──────────────────────────────
// 表情分类
struct EmojiCategory {
    std::string name;
    std::string keyword;  // 搜索关键词
    std::vector<std::string> emojis;
};

inline const std::vector<EmojiCategory>& emoji_categories() {
    static const std::vector<EmojiCategory> cats = {
        {"笑脸", "face smile happy laugh",
         {"😀","😃","😄","😁","😆","😅","🤣","😂","🙂","🙃",
          "😉","😊","😇","🥰","😍","🤩","😘","😗","☺️","😚"}},
        {"表情", "expression sad cry angry",
         {"😐","😑","😶","😏","😣","😥","😮","🤐","😯","😪",
          "😫","🥱","😴","😌","😛","😜","🤪","😝","🤑","🤗"}},
        {"手势", "gesture hand thumbs ok",
         {"👍","👎","👌","✌️","🤞"," crossed","🤟","🤘","🤙","👈",
          "👉","👆","🖕","👇","☝️","👋","🤚","🖐️","✋","🖖"}},
        {"人物", "person body heart",
         {"❤️","🧡","💛","💚","💙","💜","🖤","🤍","💔","❣️",
          "💕","💞","💓","💗","💖","💘","💝","💟","♥️","💯"}},
        {"动物", "animal cat dog bird fish",
         {"🐱","🐶","🐭","🐹","🐰","🦊","🐻","🐼","🐨","🐯",
          "🦁","🐮","🐷","🐸","🐵","🐔","🐧","🐦","🦆","🦅"}},
        {"食物", "food drink coffee tea cake",
         {"🍎","🍊","🍋","🍌","🍉","🍇","🍓","🫐","🍈","🍒",
          "🍑","🥭","🍍","🥥","🥝","🍅","🥑","🥦","🥕","🌽"}},
        {"活动", "activity sport game music",
         {"⚽","🏀","🏈","⚾","🥎","🎾","🏐","🏉","🥏","🎱",
          "🏓","🏸","🥅","🏒","🏑","🥍","🏏","🥊","🥋","⛳"}},
        {"旅行", "travel car plane train boat",
         {"🚗","🚕","🚙","🚌","🚎","🚐","🚑","🚒","🚓","🚔",
          "🚨","🚲","🚳","🚴","🚵","🏎️","🚂","🚃","🚄","🚅"}},
        {"物品", "object phone computer book gift",
         {"📱","💻","⌨️","🖥️","🖨️","🖱️","🖲️","🕹️","🗜️","💽",
          "💾","💿","📀","📼","📷","📸","📹","🎥","📞","☎️"}},
        {"符号", "symbol sign flag check cross",
         {"✅","❌","❎","✔️","✖️","➕","➖","➗","➰","➿",
          "🔆","⚡","💣","🔥","✨","🎉","🎊","🎈","🎁","🏆"}},
        {"时间", "time clock calendar date",
         {"⏰","⏰","⏳","⌛","📅","📆","🗓️","_clock","🌅","🌄",
          "🌠","🌇","🌆","🌃","🌌","🌉","🌍","🌎","🌏","🌐"}},
        {"天气", "weather sun rain snow cloud",
         {"☀️","🌤️","⛅","🌥️","☁️","🌦️","🌧️","⛈️","🌩️","🌨️",
          "❄️","☃️","⛄","🌬️","💨","🌪️","🌫️","🌈","☔","🌊"}},
    };
    return cats;
}

// 表情面板：列出所有分类
inline std::vector<std::string> emoji_panel_categories() {
    std::vector<std::string> result;
    for (const auto& cat : emoji_categories()) {
        result.push_back(cat.name);
    }
    return result;
}

// 表情面板：获取某分类的表情
inline std::vector<std::string> emoji_panel_by_category(const std::string& cat_name) {
    for (const auto& cat : emoji_categories()) {
        if (cat.name == cat_name) return cat.emojis;
    }
    return {};
}

// 表情面板：搜索表情
inline std::vector<std::pair<std::string, std::string>> emoji_search(const std::string& query) {
    std::vector<std::pair<std::string, std::string>> result;
    if (query.empty()) return result;

    for (const auto& cat : emoji_categories()) {
        // 匹配分类名
        if (cat.name.find(query) != std::string::npos ||
            cat.keyword.find(query) != std::string::npos) {
            for (const auto& e : cat.emojis) {
                result.push_back({e, cat.name});
            }
        }
    }

    if (result.size() > 50) result.resize(50);
    return result;
}

// 表情面板：渲染可视化界面
inline std::string emoji_panel_render(int selected_cat = 0) {
    const auto& cats = emoji_categories();
    if (cats.empty()) return "";

    std::string result = "═══ 表情面板 ═══\n\n";

    // 分类标签栏
    result += "分类: ";
    for (size_t i = 0; i < cats.size(); ++i) {
        if ((int)i == selected_cat) {
            result += "[" + cats[i].name + "] ";
        } else {
            result += cats[i].name + " ";
        }
    }
    result += "\n\n";

    // 选中分类的表情
    if (selected_cat >= 0 && selected_cat < (int)cats.size()) {
        result += cats[selected_cat].name + ":\n";
        for (size_t i = 0; i < cats[selected_cat].emojis.size(); ++i) {
            result += cats[selected_cat].emojis[i] + " ";
            if ((i + 1) % 10 == 0) result += "\n";
        }
        result += "\n\n";
    }

    result += "提示: 输入分类名或关键词搜索，数字键选择表情\n";
    return result;
}

// 特殊字符面板
inline std::vector<std::pair<std::string, std::string>> special_char_panel() {
    return {
        {"数学符号", "± × ÷ ≈ ≠ ≤ ≥ ∞ √ ∑ ∏ ∫ ∂ ∇ ∝ ∈ ∉ ∪ ∩ ∀ ∃"},
        {"箭头符号", "→ ← ↑ ↓ ↔ ↕ ⇒ ⇐ ⇑ ⇓ ↗ ↘ ↖ ↙ ↦ ↪ ↩ ⇄ ⇆ ⇵"},
        {"希腊字母", "α β γ δ ε ζ η θ ι κ λ μ ν ξ π ρ σ τ φ ψ ω"},
        {"罗马数字", "Ⅰ Ⅱ Ⅲ Ⅳ Ⅴ Ⅵ Ⅶ Ⅷ Ⅸ Ⅹ Ⅺ Ⅻ"},
        {"中文标点", "「 」 『 』 【 】 〈 〉 《 》 〔 〕 〖 〗"},
        {"特殊符号", "★ ☆ ♀ ♂ ○ ● ◇ ◆ □ ■ △ ▲ ▽ ▼ ⊕ ⊗ ⊙ ◎ ※ ✦"},
        {"货币符号", "￥ $ € £ ¢ ₩ ₪ ₫ ₱ ﷼"},
        {"单位符号", "℃ ℉ ‰ § № ° ′ ″ ‴"},
    };
}

// 渲染特殊字符面板
inline std::string special_char_panel_render() {
    std::string result = "═══ 特殊字符面板 ═══\n\n";
    for (const auto& [name, chars] : special_char_panel()) {
        result += name + ": " + chars + "\n\n";
    }
    return result;
}

// ── v0.9.149 云拼音多provider框架 ──────────────────────────────
// Provider类型
enum class CloudProviderType {
    LOCAL,      // 本地词典
    HTTP,       // HTTP API
    GRPC,       // gRPC (框架)
    OFFLINE     // 离线降级
};

// Provider配置
struct CloudProviderConfig {
    std::string name;
    CloudProviderType type = CloudProviderType::LOCAL;
    std::string endpoint;         // API地址
    std::string api_key;
    int timeout_ms = 3000;
    int priority = 0;             // 优先级(数字越小优先级越高)
    bool enabled = true;
    int cache_ttl_seconds = 3600; // 缓存TTL
};

// 请求合并去重：相同pinyin的请求只发一次
struct CloudRequestMerger {
    std::unordered_map<std::string, std::vector<CloudCandidate>> pending_cache;
    std::unordered_map<std::string, int64_t> cache_timestamps;

    bool has_cached(const std::string& pinyin, int ttl_seconds) {
        auto it = pending_cache.find(pinyin);
        if (it == pending_cache.end()) return false;
        auto ts_it = cache_timestamps.find(pinyin);
        if (ts_it == cache_timestamps.end()) return false;
        // 简单TTL检查（用time(nullptr)）
        int64_t now = (int64_t)time(nullptr);
        return (now - ts_it->second) < ttl_seconds;
    }

    std::vector<CloudCandidate> get_cached(const std::string& pinyin) {
        auto it = pending_cache.find(pinyin);
        if (it != pending_cache.end()) return it->second;
        return {};
    }

    void set_cached(const std::string& pinyin, const std::vector<CloudCandidate>& cands) {
        pending_cache[pinyin] = cands;
        cache_timestamps[pinyin] = (int64_t)time(nullptr);
    }

    void clear_expired(int ttl_seconds) {
        int64_t now = (int64_t)time(nullptr);
        for (auto it = cache_timestamps.begin(); it != cache_timestamps.end();) {
            if (now - it->second >= ttl_seconds) {
                pending_cache.erase(it->first);
                it = cache_timestamps.erase(it);
            } else {
                ++it;
            }
        }
    }

    size_t cache_size() const { return pending_cache.size(); }
};

// 多provider云拼音管理器
class CloudPinyinManager {
public:
    std::vector<CloudProviderConfig> providers;
    CloudRequestMerger merger;
    std::string cache_dir;

    explicit CloudPinyinManager(const std::string& cache = "") : cache_dir(cache) {}

    // 添加provider
    void add_provider(const CloudProviderConfig& cfg) {
        providers.push_back(cfg);
        // 按优先级排序
        std::sort(providers.begin(), providers.end(),
                  [](const auto& a, const auto& b) { return a.priority < b.priority; });
    }

    // 查询：按优先级尝试各provider
    std::vector<CloudCandidate> query(const std::string& pinyin) {
        if (pinyin.empty()) return {};

        for (const auto& provider : providers) {
            if (!provider.enabled) continue;

            // 检查请求合并缓存
            if (merger.has_cached(pinyin, provider.cache_ttl_seconds)) {
                auto cached = merger.get_cached(pinyin);
                if (!cached.empty()) return cached;
            }

            std::vector<CloudCandidate> result;

            switch (provider.type) {
                case CloudProviderType::LOCAL:
                    result = query_local(pinyin, provider);
                    break;
                case CloudProviderType::HTTP:
                    result = query_http(pinyin, provider);
                    break;
                case CloudProviderType::GRPC:
                    result = {};  // 框架
                    break;
                case CloudProviderType::OFFLINE:
                    result = {};  // 离线降级返回空
                    break;
            }

            if (!result.empty()) {
                merger.set_cached(pinyin, result);
                return result;
            }
        }

        return {};  // 所有provider都失败
    }

    // 批量查询（请求合并）
    std::unordered_map<std::string, std::vector<CloudCandidate>>
    batch_query(const std::vector<std::string>& pinyins) {
        std::unordered_map<std::string, std::vector<CloudCandidate>> results;
        for (const auto& p : pinyins) {
            if (results.count(p)) continue;  // 去重
            results[p] = query(p);
        }
        return results;
    }

    // 获取缓存统计
    std::string cache_stats() const {
        return "缓存条目: " + std::to_string(merger.cache_size());
    }

    // 清理过期缓存
    void cleanup_cache() {
        for (const auto& p : providers) {
            merger.clear_expired(p.cache_ttl_seconds);
        }
    }

private:
    // 本地词典provider
    std::vector<CloudCandidate> query_local(const std::string& pinyin,
                                            const CloudProviderConfig& cfg) {
        (void)cfg;  // 本地缓存不看配置，接口统一而已
        // 从本地缓存文件加载
        if (cache_dir.empty()) return {};

        std::string cache_file = cache_dir + "/cloud_" + pinyin + ".dat";
        FILE* fp = std::fopen(cache_file.c_str(), "r");
        if (!fp) return {};

        std::vector<CloudCandidate> result;
        char line[512];
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.empty()) continue;

            CloudCandidate c;
            size_t tab1 = s.find('\t');
            if (tab1 == std::string::npos) continue;
            c.chinese = s.substr(0, tab1);

            size_t tab2 = s.find('\t', tab1 + 1);
            if (tab2 == std::string::npos) {
                c.confidence = std::atoi(s.substr(tab1 + 1).c_str());
            } else {
                c.confidence = std::atoi(s.substr(tab1 + 1, tab2 - tab1 - 1).c_str());
                c.source = s.substr(tab2 + 1);
            }
            c.pinyin = pinyin;
            result.push_back(c);
        }
        std::fclose(fp);
        return result;
    }

    // HTTP API provider（框架——实际需libcurl）
    std::vector<CloudCandidate> query_http(const std::string& pinyin,
                                           const CloudProviderConfig& cfg) {
        (void)pinyin; (void)cfg;  // 框架代码：预留参数，真实实现用 libcurl
        // 框架代码：实际部署时用libcurl发送HTTP请求
        // POST cfg.endpoint
        // Body: {"pinyin": "...", "api_key": "..."}
        // Response: {"candidates": [{"chinese": "...", "confidence": 90}]}
        //
        // 离线环境下返回空，触发降级
        return {};
    }
};

// 便捷：创建默认多provider配置
inline CloudPinyinManager create_default_cloud_manager(const std::string& cache_dir = "") {
    CloudPinyinManager mgr(cache_dir);

    // Provider 1: 本地缓存（最高优先级）
    mgr.add_provider({"local", CloudProviderType::LOCAL, "", "", 1000, 0, true, 7200});

    // Provider 2: HTTP API（中优先级）
    mgr.add_provider({"http", CloudProviderType::HTTP,
                      "https://pinyin-api.example.com/v1/query", "", 3000, 1, true, 3600});

    // Provider 3: 离线降级（最低优先级）
    mgr.add_provider({"offline", CloudProviderType::OFFLINE, "", "", 0, 2, true, 0});

    return mgr;
}

// ── v0.9.150 用户词典跨设备同步 ──────────────────────────────
// 用户词典条目
struct UserDictEntry {
    std::string pinyin;
    std::string chinese;
    int frequency = 0;
    int64_t timestamp = 0;     // 最后修改时间
    std::string device_id;     // 来源设备
};

// 词典版本追踪
struct DictVersion {
    int64_t version = 0;
    int64_t timestamp = 0;
    std::string device_id;
    int entry_count = 0;
};

// 用户词典管理器（支持CRUD+同步）
class UserDictManager {
public:
    std::vector<UserDictEntry> entries;
    DictVersion version;
    std::string dict_file;
    std::string device_id;

    explicit UserDictManager(const std::string& file = "", const std::string& dev = "local")
        : dict_file(file), device_id(dev) {
        version.device_id = dev;
        if (!file.empty()) load();
    }

    // CRUD: 添加
    bool add(const std::string& pinyin, const std::string& chinese, int freq = 1) {
        // 检查是否已存在
        for (auto& e : entries) {
            if (e.pinyin == pinyin && e.chinese == chinese) {
                e.frequency += freq;
                e.timestamp = (int64_t)time(nullptr);
                e.device_id = device_id;
                bump_version();
                return true;
            }
        }
        UserDictEntry entry;
        entry.pinyin = pinyin;
        entry.chinese = chinese;
        entry.frequency = freq;
        entry.timestamp = (int64_t)time(nullptr);
        entry.device_id = device_id;
        entries.push_back(entry);
        bump_version();
        return true;
    }

    // CRUD: 删除
    bool remove(const std::string& pinyin, const std::string& chinese) {
        auto it = std::find_if(entries.begin(), entries.end(),
            [&](const UserDictEntry& e) {
                return e.pinyin == pinyin && e.chinese == chinese;
            });
        if (it == entries.end()) return false;
        entries.erase(it);
        bump_version();
        return true;
    }

    // CRUD: 更新
    bool update(const std::string& pinyin, const std::string& chinese, int freq) {
        for (auto& e : entries) {
            if (e.pinyin == pinyin && e.chinese == chinese) {
                e.frequency = freq;
                e.timestamp = (int64_t)time(nullptr);
                e.device_id = device_id;
                bump_version();
                return true;
            }
        }
        return false;
    }

    // CRUD: 查询
    std::vector<UserDictEntry> query(const std::string& pinyin) const {
        std::vector<UserDictEntry> result;
        for (const auto& e : entries) {
            if (e.pinyin == pinyin) result.push_back(e);
        }
        // 按频率排序
        std::sort(result.begin(), result.end(),
                  [](const auto& a, const auto& b) { return a.frequency > b.frequency; });
        return result;
    }

    // 保存到文件
    bool save() {
        if (dict_file.empty()) return false;
        FILE* fp = std::fopen(dict_file.c_str(), "w");
        if (!fp) return false;

        std::fprintf(fp, "# pinyin-ime user dict v%lld\n", (long long)version.version);
        std::fprintf(fp, "# device=%s timestamp=%lld entries=%d\n\n",
                     device_id.c_str(), (long long)version.timestamp, (int)entries.size());

        for (const auto& e : entries) {
            std::fprintf(fp, "%s\t%s\t%d\t%lld\t%s\n",
                         e.pinyin.c_str(), e.chinese.c_str(), e.frequency,
                         (long long)e.timestamp, e.device_id.c_str());
        }
        std::fclose(fp);
        return true;
    }

    // 从文件加载
    bool load() {
        if (dict_file.empty()) return false;
        FILE* fp = std::fopen(dict_file.c_str(), "r");
        if (!fp) return false;

        entries.clear();
        char line[512];
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.empty() || s[0] == '#') continue;

            UserDictEntry e;
            size_t pos = 0;
            size_t tab = s.find('\t');
            if (tab == std::string::npos) continue;
            e.pinyin = s.substr(0, tab);
            pos = tab + 1;

            tab = s.find('\t', pos);
            if (tab == std::string::npos) continue;
            e.chinese = s.substr(pos, tab - pos);
            pos = tab + 1;

            tab = s.find('\t', pos);
            if (tab == std::string::npos) {
                e.frequency = std::atoi(s.substr(pos).c_str());
            } else {
                e.frequency = std::atoi(s.substr(pos, tab - pos).c_str());
                pos = tab + 1;
                tab = s.find('\t', pos);
                if (tab == std::string::npos) {
                    e.timestamp = std::atoll(s.substr(pos).c_str());
                } else {
                    e.timestamp = std::atoll(s.substr(pos, tab - pos).c_str());
                    e.device_id = s.substr(tab + 1);
                }
            }
            entries.push_back(e);
        }
        std::fclose(fp);
        version.entry_count = (int)entries.size();
        return true;
    }

    // 三方合并：合并远程词典
    struct MergeResult {
        int added = 0;
        int updated = 0;
        int conflicts = 0;
        int unchanged = 0;
    };

    MergeResult merge(const UserDictManager& remote) {
        MergeResult result;

        for (const auto& re : remote.entries) {
            bool found = false;
            for (auto& le : entries) {
                if (le.pinyin == re.pinyin && le.chinese == re.chinese) {
                    found = true;
                    if (re.timestamp > le.timestamp) {
                        // 远程更新
                        le.frequency = re.frequency;
                        le.timestamp = re.timestamp;
                        le.device_id = re.device_id;
                        result.updated++;
                    } else if (re.timestamp < le.timestamp) {
                        // 本地更新，保留本地
                        result.unchanged++;
                    } else {
                        // 时间戳相同
                        if (re.frequency > le.frequency) {
                            le.frequency = re.frequency;
                            result.updated++;
                        } else {
                            result.unchanged++;
                        }
                    }
                    break;
                }
            }
            if (!found) {
                entries.push_back(re);
                result.added++;
            }
        }

        version.entry_count = (int)entries.size();
        bump_version();
        return result;
    }

    // 导出为同步格式（JSON-like）
    std::string export_sync() const {
        std::string result = "{\n  \"version\": " + std::to_string(version.version) +
                             ",\n  \"device\": \"" + device_id + "\",\n" +
                             "  \"entries\": [\n";
        for (size_t i = 0; i < entries.size(); ++i) {
            result += "    {\"py\": \"" + entries[i].pinyin +
                      "\", \"zh\": \"" + entries[i].chinese +
                      "\", \"freq\": " + std::to_string(entries[i].frequency) +
                      ", \"ts\": " + std::to_string(entries[i].timestamp) +
                      "}\n";
        }
        result += "  ]\n}\n";
        return result;
    }

    // 获取统计
    std::string stats() const {
        return "词条数: " + std::to_string(entries.size()) +
               ", 版本: " + std::to_string(version.version);
    }

private:
    void bump_version() {
        version.version++;
        version.timestamp = (int64_t)time(nullptr);
        version.entry_count = (int)entries.size();
    }
};

// S3/rclone同步框架
struct SyncConfig {
    std::string remote_type = "s3";      // s3/webdav/remote
    std::string remote_path;              // 远程路径
    std::string local_path;               // 本地路径
    std::string rclone_config;            // rclone配置名
    bool auto_sync = false;
    int sync_interval_minutes = 30;
};

class UserDictSyncManager {
public:
    SyncConfig config;
    UserDictManager local_dict;
    UserDictManager remote_dict;

    explicit UserDictSyncManager(const SyncConfig& cfg)
        : config(cfg), local_dict(cfg.local_path, "local"),
          remote_dict("", "remote") {}

    // 推送本地到远程
    bool push() {
        // 框架：实际用rclone copy命令
        // rclone copy local_path remote:remote_path
        return local_dict.save();
    }

    // 从远程拉取
    bool pull() {
        // 框架：实际用rclone copy命令
        // rclone copy remote:remote_path local_path
        return local_dict.load();
    }

    // 同步（三方合并）
    UserDictManager::MergeResult sync() {
        // 1. 拉取远程
        pull();
        // 2. 三方合并
        auto result = local_dict.merge(remote_dict);
        // 3. 推送合并结果
        push();
        return result;
    }
};

// ── v0.9.151 搜狗式候选窗GUI增强 ──────────────────────────────
// 候选窗状态
struct SogouCandidateWindowState {
    std::vector<std::string> candidates;
    std::string preedit;          // 拼音预编辑区
    std::string preedit_raw;      // 原始输入
    int selected_index = 0;       // 当前选中
    int page = 0;                 // 当前页码
    int total_pages = 1;
    int page_size = 5;
    std::string preceding_text;   // 上文
    std::string following_text;   // 下文
};

// 搜狗式候选窗
class SogouCandidateWindow {
public:
    SogouCandidateWindowState state;
    SkinConfig skin;
    bool show_pinyin = true;
    bool show_page_info = true;
    bool show_context = false;

    explicit SogouCandidateWindow(const SkinConfig& sk = get_skin("sogou"))
        : skin(sk) {
        state.page_size = sk.max_candidates_per_page;
    }

    // 设置候选词
    void set_candidates(const std::vector<std::string>& cands,
                        const std::string& preedit = "",
                        const std::string& raw = "") {
        state.candidates = cands;
        state.preedit = preedit;
        state.preedit_raw = raw;
        state.selected_index = 0;
        state.page = 0;
        state.total_pages = std::max(1, (int)((cands.size() + state.page_size - 1) / state.page_size));
    }

    // 翻页
    bool next_page() {
        if (state.page + 1 < state.total_pages) {
            state.page++;
            state.selected_index = state.page * state.page_size;
            return true;
        }
        return false;
    }

    bool prev_page() {
        if (state.page > 0) {
            state.page--;
            state.selected_index = state.page * state.page_size;
            return true;
        }
        return false;
    }

    // 选择候选词
    int select_by_index(int idx) {
        int absolute = state.page * state.page_size + idx;
        if (absolute < (int)state.candidates.size()) {
            state.selected_index = absolute;
            return absolute;
        }
        return -1;
    }

    // 获取当前页候选词
    std::vector<std::string> current_page_candidates() const {
        int start = state.page * state.page_size;
        int end = std::min(start + state.page_size, (int)state.candidates.size());
        std::vector<std::string> result;
        for (int i = start; i < end; ++i) {
            result.push_back(state.candidates[i]);
        }
        return result;
    }

    // 渲染候选窗
    std::string render() const {
        if (state.candidates.empty()) return "";

        std::string result;
        std::string bg = skin.background_color;
        std::string rst = skin.reset_color;

        // 顶部边框
        if (skin.border_style == "unicode") {
            if (!bg.empty()) result += bg;
            result += "\xe2\x94\x8c";
            for (int i = 0; i < 50; ++i) result += "\xe2\x94\x80";
            result += "\xe2\x94\x90";
            if (!bg.empty()) result += rst;
            result += "\n";
        } else if (skin.border_style == "ascii") {
            result += "+";
            for (int i = 0; i < 50; ++i) result += "-";
            result += "+\n";
        }

        // 拼音预编辑区
        if (show_pinyin && !state.preedit.empty()) {
            if (!bg.empty()) result += bg;
            if (skin.border_style == "unicode") result += "\xe2\x94\x82 ";
            else if (skin.border_style == "ascii") result += "| ";
            result += skin.preedit_color + state.preedit + rst;
            if (!bg.empty()) result += rst;
            result += "\n";
        }

        // 上文
        if (show_context && !state.preceding_text.empty()) {
            if (!bg.empty()) result += bg;
            if (skin.border_style == "unicode") result += "\xe2\x94\x82 ";
            else if (skin.border_style == "ascii") result += "| ";
            result += skin.hint_color + "上文: " + state.preceding_text + rst;
            if (!bg.empty()) result += rst;
            result += "\n";
        }

        // 候选词
        auto page_cands = current_page_candidates();
        int start = state.page * state.page_size;

        if (!bg.empty()) result += bg;
        if (skin.border_style == "unicode") result += "\xe2\x94\x82 ";
        else if (skin.border_style == "ascii") result += "| ";

        for (size_t i = 0; i < page_cands.size(); ++i) {
            // 序号
            if (skin.show_index) {
                result += skin.index_color + std::to_string(i + 1) + "." + rst;
                for (int s = 0; s < skin.candidate_spacing; ++s) result += " ";
            }

            // 候选词（高亮选中项）
            int abs_idx = start + i;
            if (abs_idx == state.selected_index && skin.highlight_first) {
                result += skin.focus_color + page_cands[i] + rst;
            } else {
                result += skin.candidate_color + page_cands[i] + rst;
            }

            if (skin.window_style == "vertical") {
                result += "\n";
                if (!bg.empty()) result += bg;
                if (skin.border_style == "unicode") result += "\xe2\x94\x82 ";
                else if (skin.border_style == "ascii") result += "| ";
            } else {
                for (int s = 0; s < skin.candidate_spacing + 2; ++s) result += " ";
            }
        }

        // 页码信息
        if (show_page_info && state.total_pages > 1) {
            result += skin.hint_color + "[" + std::to_string(state.page + 1) +
                      "/" + std::to_string(state.total_pages) + "]" + rst;
        }

        if (!bg.empty()) result += rst;
        result += "\n";

        // 底部边框
        if (!bg.empty()) result += bg;
        if (skin.border_style == "unicode") {
            result += "\xe2\x94\x94";
            for (int i = 0; i < 50; ++i) result += "\xe2\x94\x80";
            result += "\xe2\x94\x98";
        } else if (skin.border_style == "ascii") {
            result += "+";
            for (int i = 0; i < 50; ++i) result += "-";
            result += "+";
        }
        if (!bg.empty()) result += rst;

        return result;
    }

    // 获取选中词
    std::string selected_candidate() const {
        if (state.selected_index < (int)state.candidates.size()) {
            return state.candidates[state.selected_index];
        }
        return "";
    }

    // 候选词总数
    size_t size() const { return state.candidates.size(); }

    // 是否为空
    bool empty() const { return state.candidates.empty(); }
};

// 便捷：创建候选窗并渲染
inline std::string render_candidate_window(
        const std::vector<std::string>& candidates,
        const std::string& preedit = "",
        const std::string& skin_name = "sogou") {
    SogouCandidateWindow cw(get_skin(skin_name));
    cw.set_candidates(candidates, preedit);
    return cw.render();
}
