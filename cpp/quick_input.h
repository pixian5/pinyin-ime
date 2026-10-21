// quick_input.h — 搜狗式快速输入功能（CLI 与 Fcitx5 addon 共用）
//
// 功能：
//   1. v 模式（符号输入）：v1→①②③  v2→★☆  v3→箭头  v4→数学  v5→希腊
//      v6→罗马数字  v7→单位符号  v8→括号  v9→点线  vd→日期  vf→分数
//   2. 日期/时间快速输入：date/rq→2026年10月22日  time/sj→14:30:25  week/xq→星期四
//   3. 简易计算器：输入算式→结果作为候选
//   4. 数字转中文大写：123→壹佰贰拾叁
//
// 设计原则：纯函数、无副作用、不依赖外部文件；与 fcitx5 / libime 解耦。
#pragma once

#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <stdexcept>

namespace pinyin_ime {

// ── v 模式符号表 ──────────────────────────────────────────────
struct SymbolGroup {
    std::string key;         // 触发键（"1".."9","d","f" 等）
    std::string label;        // 显示标签
    std::vector<std::string> symbols;
};

inline const std::vector<SymbolGroup> &symbol_groups() {
    static const std::vector<SymbolGroup> groups = {
        {"1", "圈码", {"①","②","③","④","⑤","⑥","⑦","⑧","⑨","⑩",
                       "⑪","⑫","⑬","⑭","⑮","⑯","⑰","⑱","⑲","⑳"}},
        {"2", "星号", {"★","☆","♀","♂","○","●","◇","◆","□","■",
                       "△","▲","▽","▼","⊕","⊗","⊙","◎","※","✦"}},
        {"3", "箭头", {"→","←","↑","↓","↔","↕","⇒","⇐","⇑","⇓",
                       "↗","↘","↖","↙","↦","↪","↩","⇄","⇆","⇵"}},
        {"4", "数学", {"±","×","÷","≈","≠","≤","≥","∞","√","∑",
                       "∏","∫","∂","∇","∝","∈","∉","∪","∩","∀"}},
        {"5", "希腊", {"α","β","γ","δ","ε","ζ","η","θ","ι","κ",
                       "λ","μ","ν","ξ","π","ρ","σ","τ","φ","ψ",
                       "ω","Γ","Δ","Θ","Λ","Ξ","Π","Σ","Φ","Ψ"}},
        {"6", "罗马", {"Ⅰ","Ⅱ","Ⅲ","Ⅳ","Ⅴ","Ⅵ","Ⅶ","Ⅷ","Ⅸ","Ⅹ",
                       "Ⅺ","Ⅻ","ⅰ","ⅱ","ⅲ","ⅳ","ⅴ","ⅵ","ⅵ","ⅸ",
                       "ⅹ","ⅺ","ⅻ","ⅼ","ⅽ","ⅾ","ⅿ","ↀ","ↁ","ↂ"}},
        {"7", "单位", {"℃","℉","￥","‰","§","№","○","◎","●","◇",
                       "◆","□","■","△","▲","▽","▼","¢","£","€"}},
        {"8", "括号", {"「","」","『","』","【","】","〈","〉","《","》",
                       "〔","〕","〖","〗","〘","〙","〚","〛","﹁","﹂"}},
        {"9", "点线", {"·","•","…","—","–","‖","│","┃","┌","┐",
                       "└","┘","├","┤","┬","┴","┼","═","║","╔"}},
        {"d", "日期", {"年","月","日","时","分","秒","周","星期","季度","号"}},
        {"f", "分数", {"½","⅓","⅔","¼","¾","⅕","⅖","⅗","⅘","⅙",
                       "⅚","⅛","⅜","⅝","⅞","¹","²","³","⁴","ₐ"}},
    };
    return groups;
}

// v 模式：给定 v 后跟的子串（如 "1"、"2"），返回符号列表与标签。
// 返回空 vector 表示无匹配。
inline std::pair<std::vector<std::string>, std::string>
v_mode_lookup(const std::string &sub) {
    for (const auto &g : symbol_groups()) {
        if (g.key == sub) {
            return {g.symbols, g.label};
        }
    }
    // 模糊匹配：v 后直接打数字 1-9 或 d/f
    if (sub.size() == 1) {
        for (const auto &g : symbol_groups()) {
            if (g.key[0] == sub[0]) {
                return {g.symbols, g.label};
            }
        }
    }
    return {{}, ""};
}

// 判断是否进入 v 模式：输入串以 'v' 开头且长度 >= 2
inline bool is_v_mode(const std::string &input) {
    return input.size() >= 2 && input[0] == 'v';
}

// ── 日期/时间快速输入 ──────────────────────────────────────────
inline std::string current_date() {
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%d年%d月%d日",
                  lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
    return buf;
}

inline std::string current_date_short() {
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%04d-%02d-%02d",
                  lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
    return buf;
}

inline std::string current_time() {
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%02d:%02d:%02d",
                  lt->tm_hour, lt->tm_min, lt->tm_sec);
    return buf;
}

inline std::string current_week() {
    std::time_t t = std::time(nullptr);
    std::tm *lt = std::localtime(&t);
    static const char *weeks[] = {"星期日","星期一","星期二","星期三",
                                   "星期四","星期五","星期六"};
    return weeks[lt->tm_wday];
}

inline std::string current_datetime() {
    return current_date() + " " + current_time();
}

// 日期/时间快捷词：输入 → 结果（空=不匹配）
inline std::string datetime_lookup(const std::string &input) {
    if (input == "date" || input == "rq") return current_date();
    if (input == "dates" || input == "rqs") return current_date_short();
    if (input == "time" || input == "sj") return current_time();
    if (input == "week" || input == "xq") return current_week();
    if (input == "now" || input == "dt") return current_datetime();
    return "";
}

inline bool is_datetime_trigger(const std::string &input) {
    return !datetime_lookup(input).empty();
}

// ── 简易计算器 ──────────────────────────────────────────────────
// 支持 + - * / × ÷ % ^ ( ) 及小数；输入纯算式时返回结果字符串。
inline bool is_calculator_input(const std::string &input) {
    if (input.empty()) return false;
    bool has_digit = false, has_op = false;
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = input[i];
        if (c >= '0' && c <= '9') { has_digit = true; continue; }
        if (c == '+' || c == '-' || c == '*' || c == '/' ||
            c == '%' || c == '^' || c == '.' ||
            c == '(' || c == ')') { has_op = true; continue; }
        // UTF-8 × (0xC3 0x97) and ÷ (0xC3 0xB7)
        if (c == 0xC3 && i + 1 < input.size()) {
            unsigned char next = input[i + 1];
            if (next == 0x97 || next == 0xB7) { has_op = true; ++i; continue; }
        }
        return false;  // 含非算式字符
    }
    // 必须同时有数字和运算符才算算式（纯数字不算）
    return has_digit && has_op;
}

// 简易表达式求值（支持 + - * / ^ % 和括号、小数）
// 递归下降解析器；失败返回空串。
inline std::string calculator_eval(const std::string &expr) {
    // 预处理：× → *, ÷ → /
    std::string s;
    for (size_t i = 0; i < expr.size(); ++i) {
        unsigned char c = expr[i];
        if (c == 0xC3 && i + 1 < expr.size()) {
            unsigned char next = expr[i + 1];
            if (next == 0x97) { s += '*'; ++i; continue; }
            if (next == 0xB7) { s += '/'; ++i; continue; }
        }
        s += static_cast<char>(c);
    }
    size_t pos = 0;
    auto skip = [&]() { while (pos < s.size() && s[pos] == ' ') ++pos; };

    std::function<double()> parse_expr, parse_term, parse_factor, parse_pow;

    parse_pow = [&]() -> double {
        skip();
        double base = parse_factor();
        skip();
        if (pos < s.size() && s[pos] == '^') {
            ++pos;
            double exp = parse_pow();
            return std::pow(base, exp);
        }
        return base;
    };

    parse_factor = [&]() -> double {
        skip();
        if (pos >= s.size()) throw std::runtime_error("eof");
        if (s[pos] == '(') {
            ++pos;
            double v = parse_expr();
            skip();
            if (pos < s.size() && s[pos] == ')') ++pos;
            return v;
        }
        if (s[pos] == '-') {
            ++pos;
            return -parse_factor();
        }
        if (s[pos] == '+') {
            ++pos;
            return parse_factor();
        }
        size_t start = pos;
        while (pos < s.size() && ((s[pos] >= '0' && s[pos] <= '9') || s[pos] == '.'))
            ++pos;
        if (pos == start) throw std::runtime_error("num");
        return std::stod(s.substr(start, pos - start));
    };

    parse_term = [&]() -> double {
        double v = parse_pow();
        while (true) {
            skip();
            if (pos >= s.size()) break;
            char c = s[pos];
            if (c == '*') { ++pos; v *= parse_pow(); }
            else if (c == '/') { ++pos; double d = parse_pow(); if (d == 0) throw std::runtime_error("div0"); v /= d; }
            else if (c == '%') { ++pos; v = std::fmod(v, parse_pow()); }
            else break;
        }
        return v;
    };

    parse_expr = [&]() -> double {
        double v = parse_term();
        while (true) {
            skip();
            if (pos >= s.size()) break;
            char c = s[pos];
            if (c == '+') { ++pos; v += parse_term(); }
            else if (c == '-') { ++pos; v -= parse_term(); }
            else break;
        }
        return v;
    };

    try {
        pos = 0;
        double result = parse_expr();
        skip();
        if (pos != s.size()) return "";  // 未消费完，不算合法算式
        // 格式化：整数不显示小数点
        if (result == std::floor(result) && std::abs(result) < 1e15) {
            char buf[32];
            std::snprintf(buf, sizeof(buf), "%lld", static_cast<long long>(result));
            return buf;
        }
        char buf[32];
        std::snprintf(buf, sizeof(buf), "%.6g", result);
        return buf;
    } catch (...) {
        return "";
    }
}

// ── 数字转中文大写 ──────────────────────────────────────────────
inline std::string num_to_chinese_upper(long long n) {
    if (n == 0) return "零";
    static const char *digits[] = {"零","壹","贰","叁","肆","伍","陆","柒","捌","玖"};
    static const char *units[] = {"","拾","佰","仟"};
    static const char *big_units[] = {"","万","亿","万亿"};

    bool negative = n < 0;
    if (negative) n = -n;

    std::string result;
    if (negative) result += "负";

    // 分段处理（每4位一段）
    int sections[4] = {0,0,0,0};
    int sc = 0;
    while (n > 0 && sc < 4) {
        sections[sc++] = static_cast<int>(n % 10000);
        n /= 10000;
    }

    bool first_section = true;
    for (int i = sc - 1; i >= 0; --i) {
        int section = sections[i];
        if (section == 0) {
            if (!first_section && i > 0) {
                // 段间零处理
            }
            continue;
        }
        if (!first_section && section < 1000) {
            result += "零";
        }
        bool section_first = true;
        for (int j = 3; j >= 0; --j) {
            int d = (section / static_cast<int>(std::pow(10, j))) % 10;
            if (d == 0) {
                if (!section_first) {
                    // 段内零
                }
                continue;
            }
            if (!section_first && d > 0) {
                // 检查是否需要补零
            }
            result += digits[d];
            if (j > 0) result += units[j];
            section_first = false;
        }
        if (i > 0) result += big_units[i];
        first_section = false;
    }
    return result;
}

inline std::string num_to_chinese_lower(long long n) {
    if (n == 0) return "零";
    static const char *digits[] = {"零","一","二","三","四","五","六","七","八","九"};
    static const char *units[] = {"","十","百","千"};
    static const char *big_units[] = {"","万","亿","万亿"};

    bool negative = n < 0;
    if (negative) n = -n;

    std::string result;
    if (negative) result += "负";

    int sections[4] = {0,0,0,0};
    int sc = 0;
    while (n > 0 && sc < 4) {
        sections[sc++] = static_cast<int>(n % 10000);
        n /= 10000;
    }

    bool first_section = true;
    for (int i = sc - 1; i >= 0; --i) {
        int section = sections[i];
        if (section == 0) continue;
        if (!first_section && section < 1000) result += "零";
        bool section_first = true;
        for (int j = 3; j >= 0; --j) {
            int d = (section / static_cast<int>(std::pow(10, j))) % 10;
            if (d == 0) continue;
            // 段内补零
            if (!section_first) {
                int prev = (section / static_cast<int>(std::pow(10, j + 1))) % 10;
                if (prev == 0) result += "零";
            }
            // 十位为1且是首位时省略"一"（10→十, 12→十二, 10万→十万）
            if (d == 1 && j == 1 && section_first && first_section) {
                result += units[j];
            } else {
                result += digits[d];
                if (j > 0) result += units[j];
            }
            section_first = false;
        }
        if (i > 0) result += big_units[i];
        first_section = false;
    }
    return result;
}

// 判断是否纯数字
inline bool is_pure_number(const std::string &input) {
    if (input.empty()) return false;
    for (char c : input) {
        if (c < '0' || c > '9') return false;
    }
    return true;
}

// 数字输入的中文候选：纯数字 → [大写, 小写]
inline std::vector<std::string> number_candidates(const std::string &input) {
    if (!is_pure_number(input)) return {};
    long long n = std::atoll(input.c_str());
    if (n > 999999999999LL) return {};  // 超出范围
    std::vector<std::string> result;
    std::string lower = num_to_chinese_lower(n);
    std::string upper = num_to_chinese_upper(n);
    if (!lower.empty()) result.push_back(lower);
    if (!upper.empty() && upper != lower) result.push_back(upper);
    return result;
}

} // namespace pinyin_ime
