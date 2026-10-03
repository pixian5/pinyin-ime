/*
 * abbr_presets.h — 简拼预设表与 boost 逻辑（CLI 与 fcitx5 addon 共享）
 *
 * libime 的语言模型按词频排序，日常用语（"这个"/"即使"/"随着"）频率高于
 * "中国"/"技术"/"深圳"，导致简拼 zg/js/sz 首选不是用户期望的词。
 * 本表对高频简拼缩写做 boost：若预设词已在候选列表中，移到首位；否则插入。
 *
 * 设计原则：
 *   1) 只收录「无歧义或用户期望明确」的简拼（zg→中国 而非 这个）
 *   2) 不覆盖已有正确行为的简拼（bj→北京 已正确，不加预设）
 *   3) 预设词在候选列表中：移到首位；不在候选中：插入首位
 *   4) 用户自定义简拼（~/.config/pinyin-ime/abbr.txt）优先级高于内置预设
 *   5) 用户自学习数据优先级高于预设（learn() 后 user.history 会覆盖）
 *
 * 优先级：用户自定义 > 内置预设 > 语言模型默认排序
 */
#ifndef ABBR_PRESETS_H
#define ABBR_PRESETS_H

#include <string>
#include <unordered_map>
#include <vector>
#include <fstream>

namespace pinyin_ime {

// ── 内置简拼预设表 ──────────────────────────────────────────
// 返回静态 unordered_map，O(1) 查找
inline const std::unordered_map<std::string, std::string>& abbr_preset_map() {
    static const std::unordered_map<std::string, std::string> map = {
        // 国家/政治
        {"zg",   "中国"},       // 这个→中国
        {"zgr",  "中国人"},
        {"zgh",  "中华人民共和国"},
        {"zgrm", "中国人民"},
        // 技术（js→即使 需 boost）
        {"js",   "技术"},
        {"rj",   "软件"},
        {"kj",   "科技"},
        {"wl",   "网络"},
        {"sd",   "算法"},
        {"dm",   "代码"},
        {"gj",   "工具"},
        {"jk",   "接口"},
        {"cs",   "测试"},
        {"sc",   "生产"},
        {"bb",   "版本"},
        {"az",   "安装"},
        // 城市（sz→随着 需 boost；bj/sh/gz/hz 已正确，不加）
        {"sz",   "深圳"},
        {"cd",   "成都"},
        {"wh",   "武汉"},
        {"cq",   "重庆"},
        {"xa",   "西安"},
        {"nj",   "南京"},
        {"tj",   "天津"},
        // 常见搭配
        {"zl",   "资料"},
        {"jl",   "记录"},
        {"xw",   "新闻"},
        {"bm",   "部门"},
        {"dw",   "单位"},
        {"gr",   "个人"},
        {"qg",   "全国"},
        {"yx",   "邮箱"},
        {"sy",   "使用"},
        {"gn",   "功能"},
        {"xt",   "系统"},
        {"aq",   "安全"},
        {"mm",   "密码"},
        {"zc",   "注册"},
        {"dl",   "登录"},
        {"tc",   "退出"},
        {"xg",   "修改"},
        {"cx",   "查询"},
        {"fx",   "分析"},
        {"bg",   "报告"},
        {"jh",   "计划"},
        {"rw",   "任务"},
        {"jd",   "进度"},
        {"zt",   "状态"},
        {"lx",   "联系"},
        {"dh",   "电话"},
        {"yy",   "应用"},
        {"fw",   "服务"},
        {"zy",   "资源"},
        {"pj",   "配置"},
        {"yh",   "用户"},
        {"qy",   "企业"},
        {"cp",   "产品"},
        {"xq",   "需求"},
        // 简拼扩展（3-4字母）
        {"bjdx", "北京大学"},
        {"qhdx", "清华大学"},
        // v0.9.35: 更多常见简拼
        {"wn",   "我们"},       // 温暖→我们
        {"wx",   "微信"},       // 文学→微信
        {"tt",   "通过"},       // 疼痛→通过
        {"ee",   "而且"},       // 峨峨→而且
        // 重复字母（家人称呼）
        {"mmm",  "妈妈"},
        {"bbb",  "爸爸"},
        {"ggg",  "哥哥"},
        {"jjj",  "姐姐"},
        {"nnn",  "奶奶"},
    };
    return map;
}

// ── 用户自定义简拼文件 ──────────────────────────────────────
// 全局变量：用户可通过 --abbr-file 或 API 设置
inline std::string& abbr_file_path() {
    static std::string path;
    return path;
}

// 加载用户自定义简拼文件（~/.config/pinyin-ime/abbr.txt 或指定路径）
// 格式：每行 "abbr=word"，# 开头为注释。返回空 map 若文件不存在。
inline const std::unordered_map<std::string, std::string>& user_abbr_map() {
    static std::unordered_map<std::string, std::string> umap;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string path = abbr_file_path();
        if (path.empty()) {
            const char *home = getenv("HOME");
            if (home) path = std::string(home) + "/.config/pinyin-ime/abbr.txt";
        }
        if (!path.empty()) {
            std::ifstream f(path);
            if (f) {
                std::string line;
                while (std::getline(f, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    auto pos = line.find('=');
                    if (pos == std::string::npos || pos == 0 || pos == line.size() - 1) continue;
                    umap[line.substr(0, pos)] = line.substr(pos + 1);
                }
            }
        }
    }
    return umap;
}

// ── 简拼判断与 boost ────────────────────────────────────────

// 判断输入是否为简拼（全小写字母，长度 2-6，无隔音符）
inline bool is_abbreviated_pinyin(const std::string &input) {
    if (input.size() < 2 || input.size() > 6) return false;
    for (char c : input) {
        if (c < 'a' || c > 'z') return false;
    }
    return true;
}

// 查找简拼预设词：用户自定义优先，然后内置预设。返回空 string 若无匹配。
inline std::string lookup_abbr(const std::string &input) {
    const auto& umap = user_abbr_map();
    auto uit = umap.find(input);
    if (uit != umap.end()) return uit->second;
    const auto& map = abbr_preset_map();
    auto it = map.find(input);
    if (it != map.end()) return it->second;
    return "";
}

// 对简拼候选做 boost：若输入匹配预设表，将预设词移到首位
inline void boost_abbreviation(const std::string &input, std::vector<std::string> &cands) {
    if (!is_abbreviated_pinyin(input)) return;

    std::string preset_word = lookup_abbr(input);
    if (preset_word.empty()) return;

    // 在候选列表中查找预设词
    for (size_t i = 1; i < cands.size(); ++i) {  // 从 1 开始，跳过当前首选
        if (cands[i] == preset_word) {
            cands.erase(cands.begin() + i);
            cands.insert(cands.begin(), preset_word);
            return;
        }
    }

    // 预设词不在候选列表中：插入首位
    cands.insert(cands.begin(), preset_word);
}

// 计算 boostSwap：返回预设词在候选列表中的位置（0 表示无需交换）
// 用于交互模式：显示时交换 cands[0] 和 cands[boostSwap]
inline size_t compute_boost_swap(const std::string &preedit, size_t total) {
    if (total <= 1 || !is_abbreviated_pinyin(preedit)) return 0;
    std::string pword = lookup_abbr(preedit);
    if (pword.empty()) return 0;
    return 1;  // caller needs to find the actual index by comparing strings
}

} // namespace pinyin_ime

#endif // ABBR_PRESETS_H
