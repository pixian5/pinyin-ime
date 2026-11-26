// 剪贴板历史模块 —— quick_input.h 的片段（v0.9.178 拆出）。
// 注意：本文件不是独立头文件，必须在 quick_input.h 的 namespace pinyin_ime 内 include；
// 这样拆分只为缩小 quick_input.h 体积、明确模块归属，不改变任何调用方代码。

// ── v0.9.136 剪贴板历史 ──────────────────────────────────
// 搜狗式剪贴板历史：记录最近复制的文本，按序号快速粘贴。
struct ClipboardHistory {
    std::vector<std::string> items;
    size_t max_items = 20;

    // 添加条目（去重，最新的放最前）
    void add(const std::string &text) {
        if (text.empty()) return;
        // 去重：移除已存在的相同条目
        items.erase(std::remove(items.begin(), items.end(), text), items.end());
        items.insert(items.begin(), text);
        if (items.size() > max_items) items.resize(max_items);
    }

    // 按序号获取（1-based）
    std::string get(size_t index) const {
        if (index == 0 || index > items.size()) return "";
        return items[index - 1];
    }

    // 清空
    void clear() { items.clear(); }

    // 删除指定序号
    bool remove(size_t index) {
        if (index == 0 || index > items.size()) return false;
        items.erase(items.begin() + index - 1);
        return true;
    }

    // 搜索
    std::vector<std::pair<size_t, std::string>> search(const std::string &keyword) const {
        std::vector<std::pair<size_t, std::string>> results;
        for (size_t i = 0; i < items.size(); ++i) {
            if (items[i].find(keyword) != std::string::npos) {
                results.push_back({i + 1, items[i]});
            }
        }
        return results;
    }

    // 持久化
    bool save_to_file(const std::string &path) const {
        FILE *fp = std::fopen(path.c_str(), "w");
        if (!fp) return false;
        for (auto &item : items) {
            // 转义换行符
            std::string escaped;
            for (char c : item) {
                if (c == '\n') escaped += "\\n";
                else if (c == '\t') escaped += "\\t";
                else escaped += c;
            }
            std::fprintf(fp, "%s\n", escaped.c_str());
        }
        std::fclose(fp);
        return true;
    }

    bool load_from_file(const std::string &path) {
        items.clear();
        FILE *fp = std::fopen(path.c_str(), "r");
        if (!fp) return false;
        char line[4096];
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.empty()) continue;
            // 反转义
            std::string unescaped;
            for (size_t i = 0; i < s.size(); ++i) {
                if (s[i] == '\\' && i + 1 < s.size()) {
                    if (s[i+1] == 'n') { unescaped += '\n'; i++; }
                    else if (s[i+1] == 't') { unescaped += '\t'; i++; }
                    else unescaped += s[i];
                } else {
                    unescaped += s[i];
                }
            }
            items.push_back(unescaped);
        }
        std::fclose(fp);
        return true;
    }

    // 格式化显示
    std::string format_display(size_t max_show = 10) const {
        std::string result;
        size_t n = std::min(items.size(), max_show);
        for (size_t i = 0; i < n; ++i) {
            std::string preview = items[i];
            if (preview.size() > 50) preview = preview.substr(0, 50) + "...";
            result += std::to_string(i + 1) + ". " + preview + "\n";
        }
        return result;
    }
};
