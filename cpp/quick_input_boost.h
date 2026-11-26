// 搜狗式增强入口模块 —— quick_input.h 的片段（v0.9.178 拆出）。
// 注意：本文件不是独立头文件，必须在 quick_input.h 的 namespace pinyin_ime 内 include；
// 拆分只为缩小 quick_input.h 体积、明确模块归属，不改变任何调用方代码。

// ── v0.9.164 搜狗式用户词频动态学习系统 ──────────────────────
// 核心体验：用户选词后自动提升该词权重，下次输入时优先出现在候选列表前面
// 类似搜狗输入法的"自学习"功能，但更轻量——纯内存+文件持久化

struct UserWordFreqEntry {
    std::string word;      // 候选词
    std::string pinyin;    // 对应拼音
    int freq = 0;          // 用户选择次数
    int64_t last_used = 0; // 最后使用时间戳（秒）
};

class UserWordFreqModel {
private:
    std::unordered_map<std::string, std::vector<UserWordFreqEntry>> table_; // key: pinyin
    std::string persist_path_;
    int max_entries_per_pinyin_ = 20;
    int total_learn_count_ = 0;

    static std::string normalize_pinyin(const std::string& pinyin) {
        std::string result;
        for (char c : pinyin) {
            if (std::isalpha(static_cast<unsigned char>(c))) {
                result += static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            }
        }
        return result;
    }

public:
    UserWordFreqModel() = default;

    void set_persist_path(const std::string& path) {
        persist_path_ = path;
    }

    // 从文件加载
    void load() {
        if (persist_path_.empty()) return;
        std::ifstream ifs(persist_path_);
        if (!ifs.is_open()) return;

        std::string line;
        while (std::getline(ifs, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            std::string py, word;
            int freq;
            int64_t ts;
            if (iss >> py >> word >> freq >> ts) {
                auto& vec = table_[py];
                vec.push_back({word, py, freq, ts});
            }
        }
        for (auto& [key, vec] : table_) {
            std::sort(vec.begin(), vec.end(),
                [](const auto& a, const auto& b) { return a.freq > b.freq; });
            if ((int)vec.size() > max_entries_per_pinyin_) {
                vec.resize(max_entries_per_pinyin_);
            }
        }
    }

    // 保存到文件
    void save() const {
        if (persist_path_.empty()) return;
        auto dir = std::filesystem::path(persist_path_).parent_path();
        std::error_code ec;
        std::filesystem::create_directories(dir, ec);

        std::ofstream ofs(persist_path_);
        if (!ofs.is_open()) return;
        ofs << "# pinyin-ime user word frequency model v0.9.164\n";
        ofs << "# format: pinyin\\tword\\tfreq\\tlast_used\n";
        for (const auto& [py, vec] : table_) {
            for (const auto& e : vec) {
                ofs << py << '\t' << e.word << '\t' << e.freq << '\t' << e.last_used << '\n';
            }
        }
    }

    // 用户选择了某个词，提升其频率
    void learn(const std::string& pinyin, const std::string& word) {
        auto py = normalize_pinyin(pinyin);
        if (py.empty() || word.empty()) return;

        auto& vec = table_[py];
        total_learn_count_++;

        for (auto& e : vec) {
            if (e.word == word) {
                e.freq++;
                e.last_used = std::time(nullptr);
                std::sort(vec.begin(), vec.end(),
                    [](const auto& a, const auto& b) { return a.freq > b.freq; });
                return;
            }
        }

        vec.push_back({word, py, 1, std::time(nullptr)});
        std::sort(vec.begin(), vec.end(),
            [](const auto& a, const auto& b) { return a.freq > b.freq; });

        if ((int)vec.size() > max_entries_per_pinyin_) {
            vec.resize(max_entries_per_pinyin_);
        }
    }

    // 根据用户词频重排候选列表
    std::vector<std::string> rerank_candidates(
        const std::string& pinyin,
        const std::vector<std::string>& candidates) const {

        auto py = normalize_pinyin(pinyin);
        if (py.empty()) return candidates;

        auto it = table_.find(py);
        if (it == table_.end() || it->second.empty()) return candidates;

        auto result = candidates;

        std::unordered_map<std::string, int> freq_map;
        for (const auto& e : it->second) {
            freq_map[e.word] = e.freq;
        }

        std::stable_sort(result.begin(), result.end(),
            [&freq_map](const std::string& a, const std::string& b) {
                int fa = 0, fb = 0;
                auto ia = freq_map.find(a);
                auto ib = freq_map.find(b);
                if (ia != freq_map.end()) fa = ia->second;
                if (ib != freq_map.end()) fb = ib->second;
                if (fa > 0 && fb == 0) return true;
                if (fa == 0 && fb > 0) return false;
                if (fa != fb) return fa > fb;
                return false;
            });

        return result;
    }

    std::string user_first_choice(const std::string& pinyin) const {
        auto py = normalize_pinyin(pinyin);
        auto it = table_.find(py);
        if (it == table_.end() || it->second.empty()) return "";
        return it->second[0].word;
    }

    int total_entries() const {
        int count = 0;
        for (const auto& [k, v] : table_) count += v.size();
        return count;
    }

    int total_learn_count() const { return total_learn_count_; }

    std::vector<UserWordFreqEntry> all_entries() const {
        std::vector<UserWordFreqEntry> result;
        for (const auto& [k, v] : table_) {
            for (const auto& e : v) result.push_back(e);
        }
        return result;
    }

    void clear() {
        table_.clear();
        total_learn_count_ = 0;
    }
};

// ── v0.9.164 搜狗式输入节奏自适应 ──────────────────────────────
struct TypingRhythmTracker {
    std::vector<int64_t> key_timestamps;
    static constexpr size_t MAX_SAMPLES = 30;
    static constexpr double FAST_WPM = 80.0;
    static constexpr double SLOW_WPM = 30.0;

    void record_key() {
        auto now = std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::steady_clock::now().time_since_epoch()).count();
        key_timestamps.push_back(now);
        if (key_timestamps.size() > MAX_SAMPLES) {
            key_timestamps.erase(key_timestamps.begin());
        }
    }

    double estimate_wpm() const {
        if (key_timestamps.size() < 5) return 0.0;
        auto span = key_timestamps.back() - key_timestamps.front();
        if (span == 0) return 0.0;
        double seconds = span / 1000.0;
        double keys = key_timestamps.size();
        return (keys / 5.0) / (seconds / 60.0);
    }

    bool should_rerank_now() const {
        double wpm = estimate_wpm();
        return wpm < FAST_WPM;
    }

    std::string rhythm_description() const {
        double wpm = estimate_wpm();
        if (wpm == 0.0) return "未知";
        if (wpm < SLOW_WPM) return "慢速";
        if (wpm < FAST_WPM) return "正常";
        return "快速";
    }

    void reset() { key_timestamps.clear(); }
};

// ── v0.9.164 搜狗式候选词联想增强 ──────────────────────────────
class AssociationLearner {
private:
    std::unordered_map<std::string, std::unordered_map<std::string, int>> bigram_;
    std::string last_committed_;
    static constexpr int MAX_ASSOCIATIONS = 10;

public:
    void on_commit(const std::string& word) {
        if (!last_committed_.empty() && !word.empty()) {
            bigram_[last_committed_][word]++;
        }
        last_committed_ = word;
    }

    std::vector<std::string> predict_next(const std::string& last_word) const {
        auto it = bigram_.find(last_word);
        if (it == bigram_.end() || it->second.empty()) return {};

        std::vector<std::pair<std::string, int>> sorted;
        for (const auto& [w, c] : it->second) {
            sorted.push_back({w, c});
        }
        std::sort(sorted.begin(), sorted.end(),
            [](const auto& a, const auto& b) { return a.second > b.second; });

        std::vector<std::string> result;
        for (int i = 0; i < std::min((int)sorted.size(), MAX_ASSOCIATIONS); ++i) {
            result.push_back(sorted[i].first);
        }
        return result;
    }

    void save(const std::string& path) const {
        std::ofstream ofs(path);
        if (!ofs.is_open()) return;
        ofs << "# pinyin-ime association model v0.9.164\n";
        for (const auto& [prev, nexts] : bigram_) {
            for (const auto& [word, count] : nexts) {
                ofs << prev << '\t' << word << '\t' << count << '\n';
            }
        }
    }

    void load(const std::string& path) {
        std::ifstream ifs(path);
        if (!ifs.is_open()) return;
        std::string line;
        while (std::getline(ifs, line)) {
            if (line.empty() || line[0] == '#') continue;
            std::istringstream iss(line);
            std::string prev, word;
            int count;
            if (iss >> prev >> word >> count) {
                bigram_[prev][word] = count;
            }
        }
    }

    void reset() {
        bigram_.clear();
        last_committed_.clear();
    }

    int total_associations() const {
        int count = 0;
        for (const auto& [k, v] : bigram_) count += v.size();
        return count;
    }
};

// ── v0.9.164 统一入口 v7：搜狗式智能输入 ──────────────────────
inline std::vector<std::string> quick_candidates_v7(
    const std::string& input,
    const std::string& preceding = "",
    const std::string& prev2 = "",
    const std::string& theme_name = "default",
    const UserWordFreqModel* freq_model = nullptr) {

    auto result = quick_candidates_v6(input, preceding, prev2, theme_name);

    if (freq_model && !result.empty()) {
        result = freq_model->rerank_candidates(input, result);
    }

    return result;
}



// ── v0.9.166 搜狗式年份/数字智能识别 ──────────────────────
// 搜狗输入法特色：输入 "erlingeryiliunian" 自动识别为 "二零二六年"
// 支持 1900-2099 年份范围，以及常见数字组合

inline std::string digit_to_chinese(char d) {
    switch (d) {
        case '0': return "零";
        case '1': return "一";
        case '2': return "二";
        case '3': return "三";
        case '4': return "四";
        case '5': return "五";
        case '6': return "六";
        case '7': return "七";
        case '8': return "八";
        case '9': return "九";
        default: return "";
    }
}

// 拼音数字 → 阿拉伯数字映射
inline const std::unordered_map<std::string, char>& pinyin_digit_map() {
    static const std::unordered_map<std::string, char> m = {
        {"ling", '0'}, {"yi", '1'}, {"er", '2'}, {"san", '3'}, {"si", '4'},
        {"wu", '5'}, {"liu", '6'}, {"qi", '7'}, {"ba", '8'}, {"jiu", '9'},
        {"yao", '1'}, // 电话号码中1常读yao
    };
    return m;
}

// 尝试将拼音串解析为数字序列
// 例如 "erlingeryiliu" → "2026"
inline std::string try_parse_pinyin_to_digits(const std::string& pinyin) {
    if (pinyin.empty() || pinyin.size() < 3) return "";

    auto &digitMap = pinyin_digit_map();
    std::string result;
    size_t i = 0;

    while (i < pinyin.size()) {
        bool matched = false;
        // 尝试匹配 2-4 字符的拼音数字
        for (int len = 4; len >= 2; --len) {
            if (i + len <= pinyin.size()) {
                std::string sub = pinyin.substr(i, len);
                auto it = digitMap.find(sub);
                if (it != digitMap.end()) {
                    result += it->second;
                    i += len;
                    matched = true;
                    break;
                }
            }
        }
        if (!matched) return "";
    }

    // 至少需要 2 位数字才有意义
    if (result.size() < 2) return "";
    return result;
}

// 年份智能识别：检测 "erlingeryiliunian" 等模式
// 返回识别到的中文年份，如 "二零二六年"
inline std::string try_year_recognition(const std::string& pinyin) {
    // 检查是否以 "nian" 结尾
    std::string lower = pinyin;
    for (auto &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    std::string yearPart;
    bool hasNian = false;

    if (lower.size() > 4 && lower.substr(lower.size() - 4) == "nian") {
        yearPart = lower.substr(0, lower.size() - 4);
        hasNian = true;
    } else if (lower.size() > 3 && lower.substr(lower.size() - 3) == "nan") {
        // 简拼：nian → nan
        yearPart = lower.substr(0, lower.size() - 3);
        hasNian = true;
    }

    if (!hasNian) return "";

    // 尝试解析为数字
    std::string digits = try_parse_pinyin_to_digits(yearPart);
    if (digits.empty()) return "";

    // 验证年份范围 (1900-2099)
    if (digits.size() != 4) return "";
    int year = 0;
    for (char d : digits) {
        year = year * 10 + (d - '0');
    }
    if (year < 1900 || year > 2099) return "";

    // 转换为中文数字
    std::string chineseYear;
    for (char d : digits) {
        chineseYear += digit_to_chinese(d);
    }
    return chineseYear + "年";
}

// 数字串智能识别：检测 "yiersan" 等模式 → "一二三"
inline std::string try_number_recognition(const std::string& pinyin) {
    std::string lower = pinyin;
    for (auto &c : lower) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));

    // 尝试解析为数字
    std::string digits = try_parse_pinyin_to_digits(lower);
    if (digits.empty() || digits.size() < 2) return "";

    // 转换为逐位中文数字
    std::string chinese;
    for (char d : digits) {
        chinese += digit_to_chinese(d);
    }
    return chinese;
}

// ── v0.9.166 增强上下文关联规则 ──────────────────────
// 搜狗式：更多常用词搭配，提升整句准确率

inline const std::vector<AssociationRule>& extended_associations_v166() {
    static const std::vector<AssociationRule> rules = {
        // 工作场景
        {"会议", {"室", "时间", "记录", "纪要", "通知", "参加", "开始", "结束", "推迟", "取消"}},
        {"项目", {"的", "进度", "计划", "报告", "管理", "负责", "完成", "启动", "延期", "成果"}},
        {"代码", {"的", "审查", "提交", "合并", "分支", "仓库", "调试", "优化", "重构", "测试"}},
        {"测试", {"的", "用例", "结果", "报告", "覆盖", "通过", "失败", "环境", "数据", "计划"}},
        {"文档", {"的", "内容", "格式", "模板", "编辑", "查看", "下载", "分享", "版本", "更新"}},
        {"系统", {"的", "配置", "状态", "日志", "监控", "更新", "重启", "备份", "恢复", "安全"}},
        {"数据", {"的", "分析", "处理", "存储", "备份", "恢复", "导入", "导出", "同步", "安全"}},
        // 学习场景
        {"学习", {"的", "计划", "方法", "内容", "进度", "目标", "资料", "笔记", "复习", "总结"}},
        {"课程", {"的", "内容", "时间", "安排", "表", "报名", "选修", "必修", "退课", "成绩"}},
        {"考试", {"的", "时间", "地点", "成绩", "题目", "范围", "准备", "复习", "通过", "不及格"}},
        // 生活场景
        {"天气", {"的", "预报", "情况", "怎么样", "好", "不好", "晴", "阴", "雨", "雪"}},
        {"手机", {"的", "号码", "话费", "充值", "欠费", "信号", "屏幕", "电池", "充电", "关机"}},
        {"电脑", {"的", "配置", "性能", "屏幕", "键盘", "鼠标", "硬盘", "内存", "系统", "开机"}},
        {"网络", {"的", "连接", "速度", "信号", "设置", "密码", "安全", "断开", "重连", "问题"}},
        {"快递", {"的", "单号", "查询", "送达", "签收", "退回", "寄出", "到货", "取件", "派送"}},
        // 社交场景
        {"朋友", {"的", "们", "圈", "请", "约", "见", "聊", "聚", "帮忙", "联系"}},
        {"家人", {"的", "们", "好", "健康", "平安", "幸福", "团聚", "关心", "照顾", "陪伴"}},
        {"同事", {"的", "们", "好", "关系", "合作", "帮忙", "聚餐", "开会", "讨论", "交流"}},
        // 时间表达
        {"上午", {"的", "好", "忙", "开", "结束", "安排", "会议", "工作", "学习", "时间"}},
        {"下午", {"的", "好", "忙", "开", "结束", "安排", "会议", "工作", "学习", "时间"}},
        {"晚上", {"的", "好", "忙", "开", "结束", "安排", "吃饭", "休息", "睡觉", "加班"}},
        {"周末", {"的", "好", "快乐", "愉快", "休息", "出去玩", "加班", "在家", "逛街", "聚会"}},
        {"假期", {"的", "好", "快乐", "愉快", "安排", "旅游", "休息", "回家", "出国", "计划"}},
        // 情感表达
        {"开心", {"的", "很", "极了", "，", "！", "呀", "呢", "吧", "，真好", "，太好了"}},
        {"难过", {"的", "很", "极了", "，", "。", "呀", "呢", "吧", "，别担心", "，会好的"}},
        {"辛苦", {"了", "了，谢谢", "了，加油", "了，休息", "了，注意", "了，保重", "了，慢慢来", "了，别累着", "了，辛苦了", "了，好的"}},
        {"加油", {"！", "，你可以", "，努力", "，坚持", "，别放弃", "，慢慢来", "，相信自己", "，一定行", "，好的"}},
    };
    return rules;
}

// ── v0.9.166 增强拼音纠错 ──────────────────────
// 搜狗式：更多常见打错模式

struct EnhancedTypoPattern {
    std::string wrong;    // 错误拼音片段
    std::string correct;  // 正确拼音片段
    std::string desc;     // 描述
};

inline const std::vector<EnhancedTypoPattern>& enhanced_typo_patterns_v166() {
    static const std::vector<EnhancedTypoPattern> patterns = {
        // 声母混淆
        {"zh", "z", "翘舌→平舌"},
        {"ch", "c", "翘舌→平舌"},
        {"sh", "s", "翘舌→平舌"},
        {"z", "zh", "平舌→翘舌"},
        {"c", "ch", "平舌→翘舌"},
        {"s", "sh", "平舌→翘舌"},
        // 韵母混淆
        {"an", "ang", "前鼻音→后鼻音"},
        {"en", "eng", "前鼻音→后鼻音"},
        {"in", "ing", "前鼻音→后鼻音"},
        {"ang", "an", "后鼻音→前鼻音"},
        {"eng", "en", "后鼻音→前鼻音"},
        {"ing", "in", "后鼻音→前鼻音"},
        // n/l 混淆
        {"n", "l", "n→l"},
        {"l", "n", "l→n"},
        // h 遗漏
        {"chu", "chuang", "遗漏ng"},
        {"shu", "shuang", "遗漏ng"},
        // 常见打字错误
        {"ng", "n", "多余g"},
        {"iu", "ou", "iu→ou"},
        {"ui", "ei", "ui→ei"},
        // 键盘相邻误按
        {"a", "s", "a→s键盘相邻"},
        {"e", "r", "e→r键盘相邻"},
        {"i", "o", "i→o键盘相邻"},
        {"u", "i", "u→i键盘相邻"},
    };
    return patterns;
}

// 增强纠错：对输入拼音生成纠错候选
inline std::vector<std::pair<std::string, std::string>> enhanced_correct_v166(const std::string& pinyin) {
    std::vector<std::pair<std::string, std::string>> results;
    auto &patterns = enhanced_typo_patterns_v166();

    // 对每个位置尝试替换
    for (const auto &pat : patterns) {
        size_t pos = pinyin.find(pat.wrong);
        while (pos != std::string::npos) {
            std::string corrected = pinyin;
            corrected.replace(pos, pat.wrong.size(), pat.correct);
            if (corrected != pinyin) {
                results.push_back({corrected, pat.desc});
            }
            pos = pinyin.find(pat.wrong, pos + 1);
        }
    }

    // 去重
    std::sort(results.begin(), results.end());
    results.erase(std::unique(results.begin(), results.end()), results.end());
    return results;
}

// ── v0.9.166 更多句式模板 ──────────────────────
// 搜狗式：常用办公/社交快捷输入

inline const std::vector<std::pair<std::string, std::string>>& extended_templates_v166() {
    static const std::vector<std::pair<std::string, std::string>> templates = {
        // 办公模板
        {"ht", "您好，"},                    // ht → 您好，
        {"hts", "您好，谢谢联系。"},         // hts → 您好，谢谢联系。
        {"qj", "请问您方便吗？"},            // qj → 请问您方便吗？
        {"qjh", "请问您几号方便？"},         // qjh → 请问您几号方便？
        {"qm", "请问您明天有空吗？"},        // qm → 请问您明天有空吗？
        {"qx", "请问需要什么帮助？"},        // qx → 请问需要什么帮助？
        {"fa", "附件请查收，谢谢！"},        // fa → 附件请查收，谢谢！
        {"fy", "烦请查收，谢谢！"},          // fy → 烦请查收，谢谢！
        {"shou", "收到，谢谢！"},            // shou → 收到，谢谢！
        {"wc", "我已经完成了。"},            // wc → 我已经完成了。
        {"wz", "我在处理中。"},              // wz → 我在处理中。
        {"wt", "我稍后处理。"},              // wt → 我稍后处理。
        // 社交模板
        {"zm", "早上好！"},                  // zm → 早上好！
        {"wa", "晚上好！"},                  // wa → 晚上好！
        {"zz", "在的在的"},                  // zz → 在的在的
        {"mm", "没问题"},                    // mm → 没问题
        {"ok", "好的，没问题"},              // ok → 好的，没问题
        {"dd", "等一下"},                    // dd → 等一下
        {"ll", "来了来了"},                  // ll → 来了来了
        // 邮件模板
        {"yj", "您好，\n\n\n\n此致\n敬礼"},  // yj → 邮件格式
        {"hf", "您好，\n\n收到，我会尽快处理。\n\n谢谢！"},  // hf → 回复模板
        // 会议模板
        {"hy", "【会议通知】"},
        {"hys", "【会议纪要】"},
        {"hyt", "【会议提醒】请准时参加。"},
        // 代码/技术
        {"bug", "问题描述：\n复现步骤：\n预期结果：\n实际结果："},
        {"log", "请查看日志："},
        {"fix", "已修复，请验证。"},
    };
    return templates;
}

// ── v0.9.166 统一入口 v8：搜狗式智能输入增强 ──────────────────────
inline std::vector<std::string> quick_candidates_v8(
    const std::string& input,
    const std::string& preceding = "",
    const std::string& prev2 = "",
    const std::string& theme_name = "default",
    const UserWordFreqModel* freq_model = nullptr) {

    std::vector<std::string> result;

    // 1. 年份智能识别（最高优先级）
    std::string yearResult = try_year_recognition(input);
    if (!yearResult.empty()) {
        result.push_back(yearResult);
    }

    // 2. 数字识别
    std::string numResult = try_number_recognition(input);
    if (!numResult.empty() && numResult != yearResult) {
        result.push_back(numResult);
    }

    // 3. 扩展句式模板
    auto &templates = extended_templates_v166();
    for (const auto &tpl : templates) {
        if (input == tpl.first) {
            result.push_back(tpl.second);
        }
    }

    // 4. 调用 v7 获取基础候选
    auto v7Result = quick_candidates_v7(input, preceding, prev2, theme_name, freq_model);

    // 合并去重
    for (const auto &c : v7Result) {
        if (std::find(result.begin(), result.end(), c) == result.end()) {
            result.push_back(c);
        }
    }

    return result;
}


// ── v0.9.167 搜狗式智能缩写扩展 + 中英混输 + 标点配对 + 长句预测 ──

// 搜狗式常用缩写：输入简短拼音首字母组合，展开为常用短语
inline const std::vector<std::pair<std::string, std::string>>& sogou_abbreviations_v167() {
    static const std::vector<std::pair<std::string, std::string>> abbrs = {
        // 日常问候
        {"zr", "早日"}, {"zrh", "早日康复"}, {"srkl", "生日快乐"},
        {"xrkl", "新年快乐"}, {"drkl", "圣诞快乐"}, {"jqkl", "节日快乐"},
        {"wy", "无缘"}, {"wya", "无缘啊"},
        // 工作办公
        {"gz", "工作"}, {"gw", "公文"}, {"hy", "会议"}, {"ht", "话题"},
        {"jd", "进度"}, {"jhr", "计划任务"}, {"rw", "任务"},
        {"xm", "项目"}, {"xx", "信息"}, {"xq", "需求"},
        {"fa", "方案"}, {"wt", "问题"}, {"yj", "意见"},
        {"yf", "月份"}, {"nf", "年份"}, {"bb", "版本"},
        // 社交聊天
        {"lh", "聊天"}, {"pk", "拼课"}, {"pl", "评论"},
        {"zf", "转发"}, {"dz", "点赞"}, {"sc", "收藏"},
        {"gzr", "关注"}, {"fs", "发送"}, {"hd", "回复"},
        // 情感表达
        {"ka", "开心"}, {"nn", "难过"}, {"sh", "生气"},
        {"gx", "感谢"}, {"dl", "道理"}, {"yx", "优秀"},
        // 常用语气
        {"dm", "当然"}, {"kd", "肯定"}, {"yxz", "也许"},
        {"br", "比如"}, {"lh", "例如"}, {"zj", "总结"},
        // 时间日期
        {"jt", "今天"}, {"zt", "昨天"}, {"mt", "明天"},
        {"zst", "上周同"}, {"xzt", "下周同"},
        // 常用长句
        {"wwh", "我希望"}, {"wrd", "我认为"}, {"wjd", "我觉得"},
        {"wsa", "我随便"}, {"wha", "还好啊"},
        {"bky", "不可以"}, {"kyz", "可以啊"},
        {"myy", "没有用"}, {"hys", "还有事"},
        // 网络用语
        {"xswl", "笑死我了"}, {"awsl", "啊我死了"},
        {"yyds", "永远的神"}, {"nsdd", "你说得对"}, {"plmm", "漂亮妹妹"}, {"xjj", "小姐姐"}, {"xjm", "小姐妹"},
        {"u1s1", "有一说一"}, {"xj", "下头"},
        // v0.9.169: 扩展网络用语
        {"bdjw", "不懂就问"}, {"yygq", "阴阳怪气"}, {"ssmy", "是美眉呀"},
        {"kdl", "磕到了"}, {"szd", "是真的"}, {"bhs", "不会吧"},
        {"jrm", "家人们"}, {"xzx", "小仙女"}, {"nmsl", "你没事吧"},
        {"awsl", "啊我死了"}, {"kksk", "磕死磕死"}, {"rnb", "人民币"},
        {"zqsg", "真情实感"},
        // v0.9.170: 四字成语缩写补充
        {"sjmy", "十全十美"},
        {"wmrl", "完美无缺"},
        {"tydiz", "天下第一"},
        {"hylt", "海阔天空"},
        {"qzxh", "千军万马"},
        {"wdyz", "万无一失"},
        {"ybyf", "一败涂地"},
        {"bsyf", "百战百胜"},
        {"yjsy", "一举两得"},
        {"ljsj", "两全其美"},
        {"sjqi", "三思而行"},
        {"swsl", "说一不二"},
        {"yymy", "一言为定"},
        {"yym", "一鸣惊人"},
        {"hyl", "后来居上"},
        {"qyl", "青出于蓝"},
        {"sjl", "水到渠成"},
        {"yjl", "迎刃而解"},
        {"yyqx", "一往情深"},
        {"nbcs", "内部测试"},
        {"pljj", "漂亮姐姐"},
        {"pldd", "漂亮弟弟"},
        {"plgg", "漂亮哥哥"},
        {"plaa", "漂亮阿姨"},
        {"plbb", "漂亮伯伯"}, {"blx", "玻璃心"}, {"yxh", "营销号"},
        {"wzry", "王者荣耀"}, {"hpjy", "和平精英"},
        // v0.9.169: 扩展日常用语
        {"cf", "吃饭"}, {"sj", "睡觉"}, {"xj", "洗脚"}, {"ys", "有时"},
        {"qm", "前面"}, {"hm", "后面"}, {"zl", "整理"}, {"fx", "分享"},
        {"tj", "推荐"}, {"cj", "参加"}, {"bm", "报名"}, {"zf", "支付"},
        {"tk", "退款"}, {"fh", "发货"}, {"sh", "收货"}, {"pj", "评价"},
        {"jd", "进度"}, {"ys", "颜色"}, {"cc", "尺寸"}, {"sl", "数量"},
        // v0.9.169: 扩展工作用语
        {"bg", "报告"}, {"rw", "任务"}, {"kh", "客户"}, {"ht", "合同"},
        {"fp", "发票"}, {"cx", "查询"}, {"tj", "统计"}, {"dc", "导出"},
        {"dr", "导入"}, {"sh", "审核"}, {"sp", "审批"}, {"bh", "编号"},
        {"lxr", "联系人"}, {"dz", "地址"}, {"dh", "电话"}, {"yx", "邮箱"},
        {"gzr", "工作日"}, {"jjr", "节假日"}, {"cq", "长期"}, {"dq", "短期"},
        // v0.9.169: 扩展情感表达
        {"gx", "高兴"}, {"sx", "伤心"}, {"dx", "担心"}, {"ax", "安心"},
        {"jx", "焦心"}, {"lx", "开心"}, {"tx", "贴心"}, {"wx", "温馨"},
        {"hx", "好心"}, {"ex", "恶心"}, {"mx", "满意"}, {"bx", "不满意"},
        // v0.9.169: 扩展常见短语
        {"wsm", "为什么"}, {"zmy", "怎么了"}, {"smh", "什么好"},
        {"bxd", "不知道"}, {"msh", "没什么"}, {"ydy", "一样的"},
        {"bxc", "不存在"}, {"yxy", "有需要"}, {"myy", "没有用"},
        {"hhd", "还好吧"}, {"bkw", "不可无"}, {"ybh", "也不错"},
    };
    return abbrs;
}

// 搜狗式长句预测：根据输入前缀预测完整句子
inline const std::vector<std::pair<std::string, std::string>>& sogou_sentence_predictions_v167() {
    static const std::vector<std::pair<std::string, std::string>> preds = {
        // 常用开场白
        {"ninhao", "您好，请问有什么可以帮您的？"},
        {"dajiahao", "大家好，很高兴见到你们"},
        // 常用结束语
        {"xiexiepz", "谢谢配合，祝您生活愉快"},
        // 常用询问
        {"qingwenny", "请问您需要什么帮助？"},
        {"qingwensm", "请问什么时候方便？"},
        // 常用确认
        {"querenyt", "确认无误，请通知我"},
        {"yijingok", "已经处理好了，请查收"},
        // 常用客套
        {"mafan", "麻烦您了，非常感谢"},
        {"buhaoys", "不好意思，让您久等了"},
        // 工作常用
        {"wancheng", "已完成，请确认"},
        {"gengxin", "已更新，请刷新查看"},
        {"shouddao", "收到了，马上处理"},
        // v0.9.169: 扩展长句预测
        {"wozaiban", "我在办公室，有什么事吗？"},
        {"xianzaimei", "现在没空，稍后联系"},
        {"woyaochifan", "我要去吃饭了，回头聊"},
        {"jintianjia", "今天加班，晚点回去"},
        {"mingtianjian", "明天见，注意安全"},
        {"xiawuhaode", "下午好的，马上处理"},
        {"qingwengen", "请问跟谁对接呢？"},
        {"woyaoquxiao", "我要取消这个订单"},
        {"bangwochaxia", "帮我查一下进度"},
        {"fagejia", "发个价格给我看看"},
        {"woyaoxiadan", "我要下单，请问有货吗？"},
        {"keyikaipiao", "可以开发票吗？"},
        {"woyaojieshao", "我要介绍一下产品"},
        {"youhuiduo", "优惠力度大不大？"},
        {"keyishidian", "可以试一下吗？"},
        {"bangwoxiugai", "帮我修改一下信息"},
        {"woyaotuihuo", "我要退货，怎么操作？"},
        {"youmeiyou", "有没有现货？多久能到？"},
    };
    return preds;
}

// 标点智能配对：输入单个标点，自动配对
inline std::string smart_punctuation_pair(const std::string& input) {
    static const std::unordered_map<std::string, std::string> pairs = {
        {"(", "()" }, {"[", "[]" }, {"{", "{}" },
        {"（", "（）"}, {"【", "【】"}, {"《", "《》"},
        {"\"", "\"\""}, {"'", "''"}, {"`", "``"},
        {"「", "「」"}, {"『", "『』"},
    };
    auto it = pairs.find(input);
    if (it != pairs.end()) return it->second;
    return "";
}

// 中英文智能混输检测：判断输入是否包含英文单词
inline bool has_english_word(const std::string& input) {
    // 检测连续的英文字母（长度>=2）可能是英文单词
    int consecutive = 0;
    for (char c : input) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) {
            consecutive++;
            if (consecutive >= 2) return true;
        } else {
            consecutive = 0;
        }
    }
    return false;
}

// 搜狗式数字+量词智能组合
inline std::string smart_number_with_unit(const std::string& input) {
    // 检测 "数字+量词拼音" 模式，如 "3ge" → "三个", "2tian" → "两天"
    if (input.empty() || !isdigit(input[0])) return "";

    // 提取数字部分
    size_t i = 0;
    std::string numPart;
    while (i < input.size() && isdigit(input[i])) {
        numPart += input[i];
        i++;
    }
    if (i >= input.size()) return "";

    // 提取量词拼音部分
    std::string unitPinyin = input.substr(i);
    if (unitPinyin.empty()) return "";

    static const std::unordered_map<std::string, std::string> units = {
        {"ge", "个"}, {"ren", "人"}, {"tian", "天"}, {"nian", "年"},
        {"yue", "月"}, {"ri", "日"}, {"shi", "时"}, {"fen", "分"},
        {"miao", "秒"}, {"ci", "次"}, {"zhong", "种"}, {"bu", "部"},
        {"ben", "本"}, {"zhang", "张"}, {"tiao", "条"}, {"jian", "件"},
        {"tai", "台"}, {"liang", "辆"}, {"suo", "所"}, {"jia", "家"},
        {"ming", "名"}, {"wei", "位"}, {"zu", "组"}, {"pi", "批"},
        {"du", "度"}, {"li", "里"}, {"mi", "米"}, {"km", "公里"}, {"bei", "倍"}, {"sheng", "升"}, {"ping", "瓶"}, {"he", "盒"}, {"dun", "吨"}, {"ke", "克"}, {"jin", "斤"}, {"bang", "磅"},
    };

    auto it = units.find(unitPinyin);
    if (it == units.end()) return "";

    // 数字转中文
    long long num = std::stoll(numPart);
    if (num < 0 || num > 99999) return "";

    std::string chineseNum;
    if (num == 0) {
        chineseNum = "零";
    } else {
        // 简单数字转中文（不支持大写）
        static const char* digits[] = {"零","一","二","三","四","五","六","七","八","九"};
        if (num < 10) {
            // 2 + 量词 → "两" (中文习惯：两个人、两天、两年)
            if (num == 2) {
                chineseNum = "两";
            } else {
                chineseNum = digits[num];
            }
        } else if (num < 100) {
            int tens = num / 10, ones = num % 10;
            chineseNum = (tens > 1 ? std::string(digits[tens]) : "") + "十";
            if (ones > 0) chineseNum += digits[ones];
        } else if (num < 1000) {
            int hundreds = num / 100;
            int rest = num % 100;
            chineseNum = std::string(digits[hundreds]) + "百";
            if (rest > 0) {
                if (rest < 10) chineseNum += std::string("零") + digits[rest];
                else {
                    int tens = rest / 10, ones = rest % 10;
                    chineseNum += (tens > 1 ? std::string(digits[tens]) : "") + "十";
                    if (ones > 0) chineseNum += digits[ones];
                }
            }
        } else {
            // 大数字直接用阿拉伯数字
            chineseNum = numPart;
        }
    }

    return chineseNum + it->second;
}

// v0.9.167 统一入口：搜狗式智能缩写 + 长句预测 + 标点配对 + 数字量词 + v8
inline std::vector<std::string> quick_candidates_v9(
    const std::string& input,
    const std::string& preceding = "",
    const std::string& prev2 = "",
    const std::string& theme_name = "default",
    const UserWordFreqModel* freq_model = nullptr) {

    std::vector<std::string> result;

    // 1. 搜狗式长句预测（最高优先级）
    for (const auto &pred : sogou_sentence_predictions_v167()) {
        if (input == pred.first) {
            result.push_back(pred.second);
        }
    }

    // 2. 搜狗式缩写扩展
    for (const auto &abbr : sogou_abbreviations_v167()) {
        if (input == abbr.first) {
            result.push_back(abbr.second);
        }
    }

    // 3. 数字+量词智能组合
    std::string numUnit = smart_number_with_unit(input);
    if (!numUnit.empty()) {
        result.push_back(numUnit);
    }

    // 4. 标点智能配对
    std::string punctPair = smart_punctuation_pair(input);
    if (!punctPair.empty()) {
        result.push_back(punctPair);
    }

    // 5. 调用 v8 获取基础候选
    auto v8Result = quick_candidates_v8(input, preceding, prev2, theme_name, freq_model);

    // 合并去重
    for (const auto &c : v8Result) {
        if (std::find(result.begin(), result.end(), c) == result.end()) {
            result.push_back(c);
        }
    }

    return result;
}



    // v0.9.168: boost_v9 — 将 v9 智能功能集成到 CLI 候选流程
    // 在 boost_abbreviation 之后调用，将缩写/预测/标点/量词结果前置到候选列表
    inline void boost_v9(const std::string &input, std::vector<std::string> &cands) {
        if (input.empty()) return;

        // 1. 搜狗式缩写扩展
        auto &abbrs = sogou_abbreviations_v167();
        for (const auto &kv : abbrs) {
            if (kv.first == input) {
                const std::string &abbrResult = kv.second;
                for (size_t i = 1; i < cands.size(); ++i) {
                    if (cands[i] == abbrResult) {
                        cands.erase(cands.begin() + i);
                        cands.insert(cands.begin(), abbrResult);
                        return;
                    }
                }
                cands.insert(cands.begin(), abbrResult);
                return;
            }
        }

        // 2. 长句智能预测
        auto &preds = sogou_sentence_predictions_v167();
        for (const auto &kv : preds) {
            if (kv.first == input) {
                const std::string &predResult = kv.second;
                for (size_t i = 1; i < cands.size(); ++i) {
                    if (cands[i] == predResult) {
                        cands.erase(cands.begin() + i);
                        cands.insert(cands.begin(), predResult);
                        return;
                    }
                }
                cands.insert(cands.begin(), predResult);
                return;
            }
        }

        // 3. 标点智能配对
        std::string punctPair = smart_punctuation_pair(input);
        if (!punctPair.empty()) {
            cands.clear();
            cands.push_back(punctPair);
            return;
        }

        // 4. 数字+量词组合
        std::string numUnit = smart_number_with_unit(input);
        if (!numUnit.empty()) {
            for (size_t i = 1; i < cands.size(); ++i) {
                if (cands[i] == numUnit) {
                    cands.erase(cands.begin() + i);
                    cands.insert(cands.begin(), numUnit);
                    return;
                }
            }
            cands.insert(cands.begin(), numUnit);
            return;
        }
    }



// ═══════════════════════════════════════════════════════════════════════════════
// v0.9.172: 搜狗式增强 — 热门缩写扩展 + 长句预测增强 + 皮肤预设 + 智能联想
// ═══════════════════════════════════════════════════════════════════════════════

// v0.9.172: 搜狗热门缩写扩展（覆盖更多日常/工作/学习/购物/出行场景）
inline const std::vector<std::pair<std::string, std::string>>& sogou_abbreviations_v172() {
    static const std::vector<std::pair<std::string, std::string>> abbrs = {
        // 购物消费
        {"gm", "购买"}, {"xd", "下单"}, {"fh", "发货"}, {"sh", "收货"},
        {"th", "退货"}, {"hx", "好评"}, {"cx", "差评"}, {"zp", "正品"},
        {"yh", "优惠"}, {"qh", "优惠券"}, {"mj", "满减"}, {"zk", "折扣"},
        {"pj", "评价"}, {"pl", "评论"}, {"sc", "收藏"}, {"jg", "价格"},
        // 学习教育
        {"ks", "考试"}, {"kc", "课程"}, {"sj", "试卷"}, {"dt", "答题"},
        {"zy", "作业"}, {"bk", "本科"}, {"ss", "硕士"}, {"bs", "博士"},
        {"jl", "简历"}, {"zs", "证书"}, {"px", "培训"}, {"fx", "复习"},
        // 出行交通
        {"hc", "火车"}, {"fj", "飞机"}, {"dc", "打车"}, {"dt", "地铁"},
        {"gj", "公交"}, {"zy", "自驾"}, {"jd", "酒店"}, {"mp", "门票"},
        {"xc", "行程"}, {"yy", "预约"}, {"qx", "取消"}, {"gb", "改签"},
        // 餐饮美食
        {"cd", "菜单"}, {"dp", "点评"}, {"wm", "外卖"}, {"yd", "预订"},
        {"mt", "美团"}, {"elm", "饿了么"}, {"ps", "配送"}, {"qg", "团购"},
        // 医疗健康
        {"yy", "医院"}, {"ys", "医生"}, {"yz", "药"}, {"jk", "健康"},
        {"ty", "体检"}, {"zz", "症状"}, {"zf", "处方"},
        // 社交网络
        {"pyq", "朋友圈"}, {"wxx", "微信"}, {"wb", "微博"},
        {"dy", "抖音"}, {"xhs", "小红书"}, {"bili", "B站"}, {"zh", "知乎"},
        // 编程开发
        {"api", "API"}, {"bug", "Bug"}, {"pr", "Pull Request"},
        {"mr", "Merge Request"}, {"cr", "Code Review"},
        {"db", "数据库"}, {"sv", "服务"}, {"lg", "日志"},
        // 聊天常用
        {"hm", "好吗"}, {"xk", "辛苦"}, {"kl", "快乐"}, {"bm", "帮忙"},
        {"xy", "需要"}, {"ky", "可以"}, {"my", "没有"}, {"hy", "还有"},
        {"zy", "只有"}, {"sy", "所有"}, {"jy", "建议"},
        // 常用动词
        {"kan", "看"}, {"xie", "写"}, {"shuo", "说"}, {"ting", "听"},
        {"xiang", "想"}, {"zuo", "做"}, {"chi", "吃"}, {"he", "喝"},
        {"mai", "买"}, {"gei", "给"}, {"na", "拿"},
    };
    return abbrs;
}

// v0.9.172: 搜狗长句预测增强（覆盖更多工作/生活/社交场景）
inline const std::vector<std::pair<std::string, std::string>>& sogou_sentence_predictions_v172() {
    static const std::vector<std::pair<std::string, std::string>> preds = {
        // 工作沟通
        {"shouddaox", "收到了，我马上处理一下"},
        {"yijingshancheng", "已经完成了，请查收一下"},
        {"wozhengzaimang", "我正在忙，稍后回复你"},
        {"xianzaifangbian", "现在方便通话吗？"},
        {"womashanglai", "我马上过来，稍等一下"},
        {"qingwenshijian", "请问什么时候有时间？"},
        {"woyaoqingjia", "我要请假，麻烦帮我处理一下"},
        {"jintiankaihui", "今天开会，讨论一下方案"},
        {"xiawuhuiyi", "下午有会议，记得参加"},
        // 日常生活
        {"woyaodingcan", "我要订餐，请问有什么推荐？"},
        {"bangwojiaowai", "帮我叫个外卖，谢谢"},
        {"womennali", "我们在哪里集合？"},
        {"jidianchufa", "几点出发？我提前准备"},
        {"woyaodache", "我要打车，去机场"},
        // 社交聊天
        {"nihaochi", "你好，吃了吗？"},
        {"wozhunbeichuqu", "我准备出去，你呢？"},
        {"yiqiwanba", "一起玩吧，叫上大家"},
        {"mingtianyoukong", "明天有空吗？出来聚聚"},
        {"zhuyishenti", "注意身体，别太累了"},
        {"zhuyianquan", "注意安全，路上小心"},
        {"xiexiebangzhu", "谢谢你的帮助"},
        {"buhaoysdq", "不好意思，打扰一下"},
        // 购物消费
        {"youyouhuima", "有优惠吗？能便宜点吗？"},
        {"keyibaoyouma", "可以包邮吗？"},
        {"woyaokanxia", "我要看一下商品详情"},
        // 学习考试
        {"woyaofuxi", "我要复习，准备考试"},
        {"bangwojiangjie", "帮我讲解一下这道题"},
        {"woyoubudong", "我有不懂的地方，能教我吗？"},
        {"zuoyewancheng", "作业完成了，请查收"},
        {"woyaobaoming", "我要报名，请问怎么操作？"},
    };
    return preds;
}

// v0.9.172: 搜狗皮肤预设 — 模拟搜狗输入法经典皮肤
struct SogouSkinPreset {
    std::string name;
    std::string description;
    SkinConfig skin;
};

inline const std::vector<SogouSkinPreset>& sogou_skin_presets_v172() {
    static const std::vector<SogouSkinPreset> presets = {
        {"sogou_classic", "搜狗经典皮肤（蓝底白字）",
         SkinConfig{"sogou_classic", "搜狗经典", "\033[32m", "\033[37m", "\033[1;33m", "\033[36m", "", "\033[0m",
                    14, 2, 5, true, true, true, "horizontal", "unicode", "\033[44m", false, 100}},
        {"sogou_dark", "搜狗深色皮肤（黑底绿字）",
         SkinConfig{"sogou_dark", "深色", "\033[36m", "\033[32m", "\033[1;37m", "\033[33m", "", "\033[0m",
                    14, 2, 5, true, true, true, "horizontal", "unicode", "\033[40m", false, 100}},
        {"sogou_minimal", "搜狗简约皮肤（无边框）",
         SkinConfig{"sogou_minimal", "简约", "\033[90m", "\033[0m", "\033[1;4m", "\033[90m", "", "\033[0m",
                    14, 3, 5, true, true, true, "horizontal", "none", "", false, 100}},
        {"sogou_vertical", "搜狗竖排皮肤",
         SkinConfig{"sogou_vertical", "竖排", "\033[36m", "\033[0m", "\033[1;31m", "\033[90m", "", "\033[0m",
                    14, 0, 9, true, true, true, "vertical", "unicode", "", false, 100}},
    };
    return presets;
}

// v0.9.172: 根据名称获取搜狗皮肤预设
inline SkinConfig get_sogou_skin_v172(const std::string& name) {
    for (const auto& preset : sogou_skin_presets_v172()) {
        if (preset.name == name) return preset.skin;
    }
    return sogou_skin_presets_v172()[0].skin;
}

// v0.9.172: 智能联想 — 根据上屏词预测下一个词
inline std::vector<std::string> smart_association_v172(const std::string& committed) {
    static const std::unordered_map<std::string, std::vector<std::string>> assoc = {
        {"你好", {"世界", "吗", "啊", "呀"}},
        {"谢谢", {"你", "大家", "老师", "老板"}},
        {"我", {"的", "是", "想", "要", "们"}},
        {"你", {"的", "是", "好", "呢", "们"}},
        {"他", {"的", "是", "们", "说"}},
        {"今天", {"天气", "早上", "下午", "晚上", "工作"}},
        {"明天", {"见", "早上", "下午", "晚上"}},
        {"工作", {"中", "日", "上", "量"}},
        {"吃饭", {"了", "去", "吗", "没"}},
        {"睡觉", {"了", "去", "吧", "了没"}},
        {"开会", {"了", "中", "时间", "地点"}},
        {"收到", {"了", "请", "回复"}},
        {"请问", {"您", "一下", "怎么", "什么"}},
        {"不好意思", {"打扰", "麻烦", "让"}},
        {"非常", {"感谢", "好", "抱歉"}},
        {"马上", {"处理", "回复", "过来", "到"}},
        {"已经", {"完成", "收到", "处理", "发送"}},
        {"麻烦", {"您", "帮", "一下"}},
        {"希望", {"能", "可以", "大家"}},
        {"觉得", {"可以", "不行", "好", "不好"}},
    };
    auto it = assoc.find(committed);
    if (it != assoc.end()) return it->second;
    return {};
}

// v0.9.172: 智能中英文混排检测
struct MixedInputResult {
    std::string chinese_part;
    std::string english_part;
    bool has_mixed;
};

inline MixedInputResult detect_mixed_input_v172(const std::string& input) {
    MixedInputResult result;
    result.has_mixed = false;
    if (input.empty()) return result;

    bool has_digit = false;
    bool has_alpha = false;
    for (char c : input) {
        if (std::isdigit(static_cast<unsigned char>(c))) has_digit = true;
        if (std::isalpha(static_cast<unsigned char>(c))) has_alpha = true;
    }

    if (has_digit && has_alpha) {
        for (char c : input) {
            if (std::isdigit(static_cast<unsigned char>(c))) {
                result.english_part += c;
            } else {
                result.chinese_part += c;
            }
        }
        result.has_mixed = true;
    } else if (has_alpha) {
        result.chinese_part = input;
    } else {
        result.english_part = input;
    }
    return result;
}

// v0.9.172: 搜狗式候选词排序 — 根据词频和上下文重排候选
inline std::vector<std::string> sogou_rank_candidates_v172(
    const std::vector<std::string>& candidates,
    const std::string& input,
    const std::string& preceding = "") {

    if (candidates.empty()) return candidates;
    std::vector<std::string> result = candidates;

    // 1. 检查缩写匹配，优先排列
    auto& abbrs = sogou_abbreviations_v172();
    for (const auto& abbr : abbrs) {
        if (abbr.first == input) {
            auto it = std::find(result.begin(), result.end(), abbr.second);
            if (it != result.end() && it != result.begin()) {
                std::string val = *it;
                result.erase(it);
                result.insert(result.begin(), val);
            }
            break;
        }
    }

    // 2. 检查长句预测
    auto& preds = sogou_sentence_predictions_v172();
    for (const auto& pred : preds) {
        if (pred.first == input) {
            auto it = std::find(result.begin(), result.end(), pred.second);
            if (it != result.end() && it != result.begin()) {
                std::string val = *it;
                result.erase(it);
                result.insert(result.begin(), val);
            }
            break;
        }
    }

    // 3. 基于上下文排序：前一个词是"我"时，优先排列动词
    if (preceding == "我") {
        static const std::vector<std::string> verbs = {
            "想", "要", "是", "在", "有", "去", "来", "做", "看", "说",
            "吃", "喝", "买", "给", "能", "会", "可以", "需要", "觉得"
        };
        for (const auto& verb : verbs) {
            auto it = std::find(result.begin(), result.end(), verb);
            if (it != result.end() && it != result.begin()) {
                std::string val = *it;
                result.erase(it);
                result.insert(result.begin(), val);
                break;
            }
        }
    }

    return result;
}

// v0.9.172: 统一快速输入入口 v10 — 整合 v9 + 搜狗式增强
inline std::vector<std::string> quick_candidates_v10(
    const std::string& input,
    const std::string& preceding = "",
    const std::string& prev2 = "",
    const std::string& theme_name = "default",
    const UserWordFreqModel* freq_model = nullptr) {

    std::vector<std::string> result;

    // 1. v172 新增长句预测
    for (const auto& pred : sogou_sentence_predictions_v172()) {
        if (input == pred.first) {
            result.push_back(pred.second);
        }
    }

    // 2. v172 新增缩写扩展
    for (const auto& abbr : sogou_abbreviations_v172()) {
        if (input == abbr.first) {
            if (std::find(result.begin(), result.end(), abbr.second) == result.end()) {
                result.push_back(abbr.second);
            }
        }
    }

    // 3. 调用 v9 获取基础候选
    auto v9Result = quick_candidates_v9(input, preceding, prev2, theme_name, freq_model);

    // 合并去重
    for (const auto& c : v9Result) {
        if (std::find(result.begin(), result.end(), c) == result.end()) {
            result.push_back(c);
        }
    }

    return result;
}

// v0.9.172: boost_v10 — 将 v10 智能功能集成到 CLI 候选流程
inline void boost_v10(const std::string& input, std::vector<std::string>& cands,
                       const std::string& preceding = "") {
    if (input.empty()) return;

    // 1. v172 缩写扩展
    auto& abbrs = sogou_abbreviations_v172();
    for (const auto& kv : abbrs) {
        if (kv.first == input) {
            const std::string& abbrResult = kv.second;
            for (size_t i = 1; i < cands.size(); ++i) {
                if (cands[i] == abbrResult) {
                    cands.erase(cands.begin() + i);
                    cands.insert(cands.begin(), abbrResult);
                    return;
                }
            }
            cands.insert(cands.begin(), abbrResult);
            return;
        }
    }

    // 2. v172 长句预测
    auto& preds = sogou_sentence_predictions_v172();
    for (const auto& kv : preds) {
        if (kv.first == input) {
            const std::string& predResult = kv.second;
            for (size_t i = 1; i < cands.size(); ++i) {
                if (cands[i] == predResult) {
                    cands.erase(cands.begin() + i);
                    cands.insert(cands.begin(), predResult);
                    return;
                }
            }
            cands.insert(cands.begin(), predResult);
            return;
        }
    }

    // 3. 上下文排序
    if (!preceding.empty()) {
        cands = sogou_rank_candidates_v172(cands, input, preceding);
    }
}


// ═══════════════════════════════════════════════════════════════════════════════
// v0.9.173: 搜狗式智能增强 — 日期时间/计算器/热词/快捷回复/更多缩写/更多长句
// ═══════════════════════════════════════════════════════════════════════════════

// v0.9.173: 日期时间智能输入 — 搜狗式 date/time/week/now 快捷输入
inline std::vector<std::string> smart_datetime_v173(const std::string& input) {
    std::vector<std::string> result;
    if (input.empty()) return result;
    if (input == "date" || input == "rq" || input == "riqi") {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char buf[64];
        strftime(buf, sizeof(buf), "%Y年%m月%d日", t);
        result.push_back(buf);
        strftime(buf, sizeof(buf), "%Y-%m-%d", t);
        result.push_back(buf);
        strftime(buf, sizeof(buf), "%Y/%m/%d", t);
        result.push_back(buf);
        return result;
    }
    if (input == "time" || input == "sj" || input == "shijian") {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char buf[64];
        strftime(buf, sizeof(buf), "%H:%M:%S", t);
        result.push_back(buf);
        strftime(buf, sizeof(buf), "%H点%M分", t);
        result.push_back(buf);
        return result;
    }
    if (input == "week" || input == "xq" || input == "xingqi") {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        const char* weeks[] = {"星期日","星期一","星期二","星期三","星期四","星期五","星期六"};
        result.push_back(weeks[t->tm_wday]);
        const char* ws[] = {"周日","周一","周二","周三","周四","周五","周六"};
        result.push_back(ws[t->tm_wday]);
        return result;
    }
    if (input == "now" || input == "xz" || input == "xianzai") {
        time_t now = time(nullptr);
        struct tm* t = localtime(&now);
        char buf[128];
        strftime(buf, sizeof(buf), "%Y年%m月%d日 %H:%M", t);
        result.push_back(buf);
        const char* weeks[] = {"日","一","二","三","四","五","六"};
        snprintf(buf, sizeof(buf), "%d年%d月%d日 星期%s %02d:%02d",
                 t->tm_year+1900, t->tm_mon+1, t->tm_mday, weeks[t->tm_wday], t->tm_hour, t->tm_min);
        result.push_back(buf);
        return result;
    }
    return result;
}

// v0.9.173: 计算器模式 — 搜狗式直接输入算式得到结果
inline std::vector<std::string> smart_calculator_v173(const std::string& input) {
    std::vector<std::string> result;
    if (input.empty() || input.size() < 3) return result;
    bool has_digit = false, has_op = false;
    for (char c : input) {
        if (c >= '0' && c <= '9') has_digit = true;
        else if (c == '+' || c == '-' || c == '*' || c == '/' || c == '%' || c == '(' || c == ')') has_op = true;
        else if (c == 'x' || c == 'X') has_op = true;
        else return result;
    }
    if (!has_digit || !has_op) return result;
    std::string expr = input;
    for (auto& c : expr) { if (c == 'x' || c == 'X') c = '*'; }
    size_t pos = 0;
    std::function<double()> parse_expr, parse_term, parse_factor;
    parse_factor = [&]() -> double {
        if (pos >= expr.size()) return 0;
        if (expr[pos] == '(') { pos++; double v = parse_expr(); if (pos < expr.size() && expr[pos] == ')') pos++; return v; }
        size_t start = pos;
        while (pos < expr.size() && ((expr[pos] >= '0' && expr[pos] <= '9') || expr[pos] == '.')) pos++;
        if (pos == start) { pos++; return 0; }
        return std::stod(expr.substr(start, pos - start));
    };
    parse_term = [&]() -> double {
        double v = parse_factor();
        while (pos < expr.size() && (expr[pos] == '*' || expr[pos] == '/' || expr[pos] == '%')) {
            char op = expr[pos++]; double r = parse_factor();
            if (op == '*') v *= r; else if (op == '/') { if (r != 0) v /= r; else v = 0; } else if (op == '%') { if (r != 0) v = (int)v % (int)r; else v = 0; }
        }
        return v;
    };
    parse_expr = [&]() -> double {
        double v = parse_term();
        while (pos < expr.size() && (expr[pos] == '+' || expr[pos] == '-')) {
            char op = expr[pos++]; double r = parse_term();
            if (op == '+') v += r; else v -= r;
        }
        return v;
    };
    try {
        pos = 0; double val = parse_expr();
        if (pos == expr.size()) {
            if (val == (long long)val) { result.push_back(std::to_string((long long)val)); }
            else {
                std::ostringstream oss; oss << std::fixed << std::setprecision(6) << val;
                std::string s = oss.str();
                while (!s.empty() && s.back() == '0') s.pop_back();
                if (!s.empty() && s.back() == '.') s.pop_back();
                result.push_back(s);
            }
        }
    } catch (...) {}
    return result;
}

// v0.9.173: 网络热词缩写
inline const std::vector<std::pair<std::string, std::string>>& net_hotwords_v173() {
    static const std::vector<std::pair<std::string, std::string>> hw = {
        {"xswl", "笑死我了"}, {"awsl", "啊我死了"}, {"kksk", "磕死磕死"},
        {"zqsg", "真情实感"}, {"xjm", "小姐妹"}, {"plmm", "漂亮妹妹"},
        {"pljj", "漂亮姐姐"}, {"xjj", "小姐姐"}, {"nsdd", "你说得对"},
        {"u1s1", "有一说一"}, {"yyds", "永远的神"}, {"bdjw", "不懂就问"},
        {"yygq", "阴阳怪气"}, {"kdl", "磕到了"}, {"jrm", "家人们"},
        {"szd", "是真的"}, {"bhys", "不好意思"}, {"ssfd", "瑟瑟发抖"},
        {"yysy", "有一说一"}, {"xs", "笑死"}, {"dd", "懂的都懂"},
        {"wsl", "我死了"}, {"py", "朋友"}, {"jm", "姐妹"},
        {"gz", "关注"}, {"dianzan", "点赞"}, {"shoucang", "收藏"},
        {"zhuanfa", "转发"}, {"pinglun", "评论"}, {"tcl", "太惨了"},
        {"bhs", "不会吧"}, {"amsl", "啊美死了"}, {"xhx", "小可爱"},
    };
    return hw;
}

// v0.9.173: 快捷回复模板
inline const std::vector<std::pair<std::string, std::string>>& quick_replies_v173() {
    static const std::vector<std::pair<std::string, std::string>> rp = {
        {"hao", "好的"}, {"hdy", "好的呀"}, {"msh", "没问题"},
        {"keyi", "可以"}, {"xk", "行"}, {"deng", "等一下"},
        {"ms", "马上"}, {"lj", "立刻"},
        {"bh", "不好"}, {"bkeyi", "不可以"}, {"bx", "不行"},
        {"my", "没有"}, {"bs", "不是"}, {"bd", "不对"},
        {"xx", "谢谢"}, {"xxy", "谢谢你"}, {"xxd", "谢谢大家"},
        {"bhy", "不好意思"}, {"by", "不用谢"}, {"mxy", "没关系"},
        {"dq", "对不起"}, {"nh", "你好"}, {"nhy", "你好呀"},
        {"zao", "早"}, {"zaoa", "早啊"}, {"zsh", "早上好"},
        {"wsh", "晚上好"}, {"xw", "下午好"}, {"hm", "好吗"},
        {"hmb", "好吧"}, {"sdl", "收到了"}, {"sdlr", "收到了，我看看"},
        {"wzh", "我知道了"}, {"wmh", "我会的"}, {"wml", "我来了"},
        {"wwc", "完成了"}, {"wwcr", "完成了，请查收"}, {"mx", "没事"},
        {"mxd", "没事的"}, {"fm", "放心"}, {"zlh", "注意身体"},
        {"zla", "注意安全"}, {"bzl", "别着急"},
    };
    return rp;
}

// v0.9.173: 扩展缩写 — 更多日常/工作/学习/生活场景
inline const std::vector<std::pair<std::string, std::string>>& sogou_abbreviations_v173() {
    static const std::vector<std::pair<std::string, std::string>> abbrs = {
        // 办公文档
        {"wz", "文章"}, {"bt", "标题"}, {"fj", "附件"}, {"zy", "摘要"},
        {"gjz", "关键字"}, {"ml", "目录"}, {"ys", "页数"}, {"zd", "重点"},
        {"xg", "相关"}, {"ck", "参考"}, {"ps", "批注"}, {"dy", "打印"},
        {"fz", "复制"}, {"nt", "内容"}, {"wj", "文件"}, {"wd", "文档"},
        {"bb", "表格"}, {"tupian", "图片"},
        // 邮件通信
        {"yj", "邮件"}, {"sjr", "收件人"}, {"fjr", "发件人"},
        {"cy", "抄送"}, {"msc", "密送"}, {"hy", "回复"},
        // 项目管理
        {"xm", "项目"}, {"rl", "任务"}, {"lc", "流程"}, {"jd", "进度"},
        {"pc", "排期"}, {"dg", "迭代"}, {"lx", "立项"}, {"jx", "结项"},
        // 财务
        {"cw", "财务"}, {"fp", "发票"}, {"fy", "费用"}, {"cb", "成本"},
        {"lr", "利润"}, {"js", "结算"}, {"hk", "汇款"}, {"ye", "余额"},
        // 人力资源
        {"rs", "人事"}, {"zp", "招聘"}, {"xz", "薪资"}, {"jj", "奖金"},
        {"kq", "考勤"}, {"cc", "出差"}, {"nx", "内训"},
        // 客服
        {"kf", "客服"}, {"ts", "投诉"}, {"wt", "问题"}, {"sl", "受理"},
        {"bl", "办理"}, {"hf", "回复"}, {"myd", "满意度"},
        // 情感表达
        {"gx", "高兴"}, {"sx", "伤心"}, {"shengqi", "生气"},
        {"zhaoji", "着急"}, {"danxin", "担心"}, {"fangxin", "放心"},
        {"xihuan", "喜欢"}, {"taoyan", "讨厌"},
        // 天气环境
        {"tq", "天气"}, {"wd", "温度"}, {"sd", "湿度"}, {"fs", "风速"},
        {"yq", "阴晴"}, {"xy", "下雨"}, {"xt", "下雪"}, {"qt", "晴天"},
        {"yt", "阴天"}, {"dt", "多云"},
        // 时间日期
        {"jn", "今年"}, {"qn", "去年"}, {"mn", "明年"},
        {"jy", "本月"}, {"sy", "上月"}, {"xy", "下月"},
        {"jt", "今天"}, {"zt", "昨天"}, {"mt", "明天"},
        {"jz", "本周"}, {"sz", "上周"}, {"xz", "下周"},
        // 方位方向
        {"zb", "左边"}, {"yb", "右边"}, {"qb", "前边"}, {"hb", "后边"},
        {"sb", "上边"}, {"xb", "下边"}, {"zj", "中间"},
        // 程度副词
        {"ch", "很"}, {"fch", "非常"}, {"th", "特别"}, {"jh", "极好"},
        {"ych", "一般"}, {"shd", "稍微"}, {"bd", "比较"},
    };
    return abbrs;
}

// v0.9.173: 扩展长句预测
inline const std::vector<std::pair<std::string, std::string>>& sogou_sentences_v173() {
    static const std::vector<std::pair<std::string, std::string>> preds = {
        // 工作沟通
        {"wofaxangni", "我发给你，请查收一下"},
        {"woyijingfa", "我已经发了，你看一下"},
        {"qingchakanyixia", "请查看一下，有问题随时联系"},
        {"wozhengzaihc", "我正在处理，稍等一下"},
        {"yijingjiao", "已经交了，请确认一下"},
        {"xianzaiyoukong", "现在有空吗？想找你聊聊"},
        {"mingtianfangjia", "明天放假，有什么计划吗？"},
        {"jintianjiaban", "今天加班，晚点回去"},
        {"wobukequl", "我去不了，不好意思"},
        {"bangwoyixia", "帮我一下，谢谢"},
        // 日常生活
        {"woyaomai", "我要买，多少钱？"},
        {"zai nalimai", "在哪里买的？"},
        {"zenmeyang", "怎么样？好用吗？"},
        {"woyaotui", "我要退货，请问怎么操作？"},
        {"keyihuohuan", "可以换货吗？"},
        {"youxianhuo", "有现货吗？多久能到？"},
        {"woyaoyuding", "我要预订，请问还有位置吗？"},
        {"jidiankaishi", "几点开始？我提前到"},
        // 社交聊天
        {"nichisha", "你吃啥？一起呗"},
        {"woyaochulai", "我要出来，你在哪？"},
        {"yiqiqu", "一起去吧，叫上他们"},
        {"zhoumoyoukong", "周末有空吗？出来玩"},
        {"haojiubujian", "好久不见，最近怎么样？"},
        {"zuijinmang", "最近忙什么呢？"},
        {"youkongchulai", "有空出来聚聚"},
        // 学习考试
        {"kaoshizhunbei", "考试准备得怎么样了？"},
        {"woyaochati", "我要查题，帮我看看"},
        {"zuoyejiao", "作业交了吗？"},
        {"kaoshichengji", "考试成绩出来了，你考多少？"},
        // 表达感谢/道歉
        {"tai ganxie", "太感谢了，帮了大忙"},
        {"buhaoysqs", "不好意思，实在是抱歉"},
        {"duibuqq", "对不起啊，下次一定"},
    };
    return preds;
}

// v0.9.173: 统一快速输入入口 v11 — 整合 v10 + 日期时间 + 计算器 + 热词 + 快捷回复 + 扩展缩写/长句
inline std::vector<std::string> quick_candidates_v11(
    const std::string& input,
    const std::string& preceding = "",
    const std::string& prev2 = "",
    const std::string& theme_name = "default",
    const UserWordFreqModel* freq_model = nullptr) {

    std::vector<std::string> result;

    // 1. 日期时间智能输入
    auto dt = smart_datetime_v173(input);
    for (const auto& d : dt) result.push_back(d);
    if (!result.empty()) return result; // 日期时间命中则直接返回

    // 2. 计算器
    auto calc = smart_calculator_v173(input);
    for (const auto& c : calc) result.push_back(c);
    if (!result.empty()) return result;

    // 3. 网络热词
    for (const auto& hw : net_hotwords_v173()) {
        if (input == hw.first) result.push_back(hw.second);
    }

    // 4. 快捷回复
    for (const auto& rp : quick_replies_v173()) {
        if (input == rp.first) {
            if (std::find(result.begin(), result.end(), rp.second) == result.end())
                result.push_back(rp.second);
        }
    }

    // 5. 扩展缩写
    for (const auto& ab : sogou_abbreviations_v173()) {
        if (input == ab.first) {
            if (std::find(result.begin(), result.end(), ab.second) == result.end())
                result.push_back(ab.second);
        }
    }

    // 6. 扩展长句预测
    for (const auto& p : sogou_sentences_v173()) {
        if (input == p.first) {
            if (std::find(result.begin(), result.end(), p.second) == result.end())
                result.push_back(p.second);
        }
    }

    // 7. 调用 v10 获取基础候选
    auto v10Result = quick_candidates_v10(input, preceding, prev2, theme_name, freq_model);
    for (const auto& c : v10Result) {
        if (std::find(result.begin(), result.end(), c) == result.end())
            result.push_back(c);
    }

    return result;
}

// v0.9.173: boost_v11 — 将 v11 智能功能集成到 CLI 候选流程
inline void boost_v11(const std::string& input, std::vector<std::string>& cands,
                       const std::string& preceding = "") {
    if (input.empty()) return;

    // 1. 日期时间
    auto dt = smart_datetime_v173(input);
    if (!dt.empty()) {
        cands.insert(cands.begin(), dt.begin(), dt.end());
        return;
    }

    // 2. 计算器
    auto calc = smart_calculator_v173(input);
    if (!calc.empty()) {
        cands.insert(cands.begin(), calc.begin(), calc.end());
        return;
    }

    // 3. 网络热词
    for (const auto& hw : net_hotwords_v173()) {
        if (hw.first == input) {
            cands.insert(cands.begin(), hw.second);
            return;
        }
    }

    // 4. 快捷回复
    for (const auto& rp : quick_replies_v173()) {
        if (rp.first == input) {
            for (size_t i = 1; i < cands.size(); ++i) {
                if (cands[i] == rp.second) { cands.erase(cands.begin()+i); cands.insert(cands.begin(), rp.second); return; }
            }
            cands.insert(cands.begin(), rp.second);
            return;
        }
    }

    // 5. 扩展缩写
    for (const auto& ab : sogou_abbreviations_v173()) {
        if (ab.first == input) {
            for (size_t i = 1; i < cands.size(); ++i) {
                if (cands[i] == ab.second) { cands.erase(cands.begin()+i); cands.insert(cands.begin(), ab.second); return; }
            }
            cands.insert(cands.begin(), ab.second);
            return;
        }
    }

    // 6. 扩展长句预测
    for (const auto& p : sogou_sentences_v173()) {
        if (p.first == input) {
            for (size_t i = 1; i < cands.size(); ++i) {
                if (cands[i] == p.second) { cands.erase(cands.begin()+i); cands.insert(cands.begin(), p.second); return; }
            }
            cands.insert(cands.begin(), p.second);
            return;
        }
    }

    // 7. 回退到 v10
    boost_v10(input, cands, preceding);
}
