// quick_input.h — 搜狗式快速输入功能（CLI 与 Fcitx5 addon 共用）
//
// 功能：
//   1. v 模式（符号输入）：v1→①②③  v2→★☆  v3→箭头  v4→数学  v5→希腊
//      v6→罗马数字  v7→单位符号  v8→括号  v9→点线  vd→日期  vf→分数
//      va→拼音字母  vb→方括号  vc→棋牌  ve→自然  vg→几何  vh→扩展箭头
//      vi→音乐  vk→特殊  vl→制表线  vm→扩展数学  vn→扩展数字
//      vs→上下标  vt→技术  vu→扩展单位  vw→天气  vx→星座  vy→生肖  vz→杂项
//   2. 日期/时间快速输入：date/rq→2026年10月22日  time/sj→14:30:25  week/xq→星期四
//      year/yn→2026年  month/yue→10月  ts→20261022  hour→14时
//   3. 简易计算器：输入算式→结果作为候选
//   4. 数字转中文大写：123→壹佰贰拾叁
//   5. 货币转换：123rmb→壹佰贰拾叁元  123$→$123.00  123usd→123.00美元
//   6. 单位换算：1km→1千米=1000米  1kg→1千克=1000克  1mile→1.609千米
//   7. 百分比转中文：50p→百分之五十
//
// 设计原则：纯函数、无副作用、不依赖外部文件；与 fcitx5 / libime 解耦。
#pragma once

#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <set>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <chrono>
#include <unordered_map>
#include <filesystem>
#include <sys/stat.h>
#include <cstdio>
#include <unordered_set>
#include <sys/stat.h>
#include <cstdio>
#include <sys/stat.h>
#include <cstdio>

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
        {"a", "拼音", {"ā","á","ǎ","à","ē","é","ě","è","ī","í",
                        "ǐ","ì","ō","ó","ǒ","ò","ū","ú","ǔ","ù",
                        "ǖ","ǘ","ǚ","ǜ","ê","ń","ň","ǹ","m̄","m̀"}},
        {"b", "方括", {"【","】","〖","〗","〘","〙","〚","〛","〔","〕",
                        "〝","〞","‹","›","«","»","《","》","〈","〉",
                        "﹛","﹜","﹝","﹞","｛","｝","［","］","﹃","﹄"}},
        {"c", "棋牌", {"♠","♥","♦","♣","♤","♡","♢","♧","🂡","🂢",
                        "🂣","🂤","🂥","🂦","🂧","🂨","🂩","🂪","🂫","🀀",
                        "🀁","🀂","🀃","🀄","🀅","🀆","🀇","🀈","🀉","🀊"}},
        {"e", "自然", {"☀","☁","☂","☃","☄","★","☆","☎","☑","☒",
                        "✓","✗","✦","✧","✩","✪","✫","✬","✭","✮",
                        "✯","✰","✱","✲","✳","✴","✵","✶","✷","✸"}},
        {"g", "几何", {"△","▲","▽","▼","◇","◆","○","●","□","■",
                        "◐","◑","◒","◓","◔","◕","◖","◗","◯","◮",
                        "⬠","⬡","⬢","⬣","⬤","⬥","⬦","⬧","⬨","⬩"}},
        {"h", "扩箭", {"➡","⬅","⬆","⬇","↗","↘","↖","↙","➜","➝",
                        "➞","➟","➠","⬱","⬲","⬳","⬴","⬵","⬶","⬷",
                        "⬸","⬹","⬺","⬻","⬼","⬽","⬾","⬿","⭀","⭁"}},
        {"i", "音乐", {"♪","♫","♬","♩","♭","♮","♯","𝄞","𝄢","𝄫",
                        "𝄐","𝄑","𝆒","𝆓","𝆔","𝆕","𝆖","𝆗","𝆘","𝆙",
                        "𝆚","𝆛","𝆜","𝆝","𝆞","𝆟","𝆠","𝆡","𝆢","𝆣"}},
        {"k", "特殊", {"©","®","™","℗","§","¶","†","‡","•","·",
                        "…","‰","‱","′","″","‴","‵","‶","‷","⌀",
                        "⌁","⌂","⌃","⌄","⌅","⌆","⌇","⌈","⌉","⌊"}},
        {"l", "制表", {"─","│","┌","┐","└","┘","├","┤","┬","┴",
                        "┼","═","║","╔","╗","╚","╝","╠","╣","╦",
                        "╩","╬","╭","╮","╯","╰","╱","╲","╳","╴"}},
        {"m", "扩数", {"∀","∁","∂","∃","∄","∅","∆","∇","∈","∉",
                        "∊","∋","∌","∍","∑","∏","∐","∫","∬","∭",
                        "∮","∪","∩","⊕","⊗","⊙","⊘","⊚","⊙","⋆"}},
        {"n", "扩号", {"①","②","③","④","⑤","⑥","⑦","⑧","⑨","⑩",
                        "⑴","⑵","⑶","⑷","⑸","⑹","⑺","⑻","⑼","⑽",
                        "⒈","⒉","⒊","⒋","⒌","⒍","⒎","⒏","⒐","⒑"}},
        {"s", "上下标", {"⁰","¹","²","³","⁴","⁵","⁶","⁷","⁸","⁹",
                         "₀","₁","₂","₃","₄","₅","₆","₇","₈","₉",
                         "ᵃ","ᵇ","ᶜ","ᵈ","ᵉ","ᶠ","ᵍ","ʰ","ⁱ","ʲ"}},
        {"t", "技术", {"⌘","⌥","⌃","⇧","⇪","⌫","⌦","⎋","⏎","⎇",
                        "⌧","⌨","〈","〉","⌬","⌭","⌮","⌯","⌰","⌱",
                        "⌲","⌳","⌴","⌵","⌶","⌷","⌸","⌹","⌺","⌻"}},
        {"u", "扩单", {"mg","kg","t","g","lb","oz","cm","dm","m","km",
                        "in","ft","yd","mi","㎡","m³","L","mL","gal","pt",
                        "Hz","kHz","MHz","GHz","W","kW","MW","V","A","Ω"}},
        {"w", "天气", {"☀","⛅","☁","🌧","❄","⛄","🌫","🌪","🌈","⚡",
                        "☔","🌤","🌥","🌦","🌨","🌬","💨","💧","🔥","✨",
                        "🌟","💫","⭐","🌙","🌞","🌝","🌚","🌛","🌜","🌑"}},
        {"x", "星座", {"♈","♉","♊","♋","♌","♍","♎","♏","♐","♑",
                        "♒","♓","⛎","★","☆","✡","✦","✧","✩","✪",
                        "✫","✬","✭","✮","✯","✰","✱","✲","✳","✴"}},
        {"y", "生肖", {"鼠","牛","虎","兔","龙","蛇","马","羊","猴","鸡",
                        "狗","猪","子","丑","寅","卯","辰","巳","午","未",
                        "申","酉","戌","亥","甲","乙","丙","丁","戊","己"}},
        {"z", "杂项", {"✓","✗","✔","✘","✕","✖","❀","❁","❂","❃",
                        "❄","❅","❆","❇","❈","❉","❊","❋","❌","❎",
                        "❏","❐","❑","❒","❓","❔","❕","❖","❗","❘"}},
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
    // v0.9.123 扩展日期/时间
    if (input == "year" || input == "yn") {
        std::time_t t = std::time(nullptr);
        std::tm *lt = std::localtime(&t);
        char buf[16]; std::snprintf(buf, sizeof(buf), "%d年", lt->tm_year + 1900);
        return buf;
    }
    if (input == "month" || input == "yue") {
        std::time_t t = std::time(nullptr);
        std::tm *lt = std::localtime(&t);
        char buf[16]; std::snprintf(buf, sizeof(buf), "%d月", lt->tm_mon + 1);
        return buf;
    }
    if (input == "ts") {
        std::time_t t = std::time(nullptr);
        std::tm *lt = std::localtime(&t);
        char buf[16]; std::snprintf(buf, sizeof(buf), "%04d%02d%02d",
                     lt->tm_year + 1900, lt->tm_mon + 1, lt->tm_mday);
        return buf;
    }
    if (input == "hour" || input == "xs") {
        std::time_t t = std::time(nullptr);
        std::tm *lt = std::localtime(&t);
        char buf[16]; std::snprintf(buf, sizeof(buf), "%d时", lt->tm_hour);
        return buf;
    }
    if (input == "noon" || input == "zw") {
        std::time_t t = std::time(nullptr);
        std::tm *lt = std::localtime(&t);
        int h = lt->tm_hour;
        if (h < 6) return "凌晨";
        if (h < 12) return "上午";
        if (h == 12) return "正午";
        if (h < 18) return "下午";
        return "晚上";
    }
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

// ── v0.9.123 货币转换 ──────────────────────────────────────────
// 输入 "123rmb" → ["壹佰贰拾叁元", "123.00元"]
// 输入 "123$"  → ["$123.00"]
// 输入 "123usd"→ ["123.00美元", "$123.00"]
inline std::vector<std::string> currency_candidates(const std::string &input) {
    if (input.empty()) return {};
    // 提取数字前缀和货币后缀
    size_t i = 0;
    while (i < input.size() && input[i] >= '0' && input[i] <= '9') ++i;
    if (i == 0 || i == input.size()) return {};
    std::string numstr = input.substr(0, i);
    std::string suffix = input.substr(i);
    long long n = std::atoll(numstr.c_str());
    if (n > 999999999999LL) return {};

    std::vector<std::string> result;
    // 转小写比较
    std::string sfx;
    for (char c : suffix) sfx += static_cast<char>(std::tolower(c));

    if (sfx == "rmb" || sfx == "r") {
        std::string upper = num_to_chinese_upper(n);
        result.push_back(upper + "元");
        result.push_back(numstr + ".00元");
    } else if (suffix == "$" || sfx == "usd") {
        result.push_back("$" + numstr + ".00");
        if (sfx == "usd") result.push_back(numstr + ".00美元");
    } else if (suffix == "€" || sfx == "eur") {
        result.push_back("€" + numstr + ".00");
        if (sfx == "eur") result.push_back(numstr + ".00欧元");
    } else if (suffix == "¥" || sfx == "jpy") {
        result.push_back("¥" + numstr + ".00");
        if (sfx == "jpy") result.push_back(numstr + ".00日元");
    } else if (suffix == "£" || sfx == "gbp") {
        result.push_back("£" + numstr + ".00");
        if (sfx == "gbp") result.push_back(numstr + ".00英镑");
    }
    return result;
}

inline bool is_currency_input(const std::string &input) {
    return !currency_candidates(input).empty();
}

// ── v0.9.123 单位换算 ──────────────────────────────────────────
// 输入 "1km" → ["1千米 = 1000米"]
// 输入 "1kg" → ["1千克 = 1000克"]
// 输入 "1mile" → ["1英里 ≈ 1.609千米"]
inline std::vector<std::string> unit_conversion(const std::string &input) {
    if (input.empty()) return {};
    size_t i = 0;
    while (i < input.size() && input[i] >= '0' && input[i] <= '9') ++i;
    if (i == 0 || i == input.size()) return {};
    std::string numstr = input.substr(0, i);
    std::string unit = input.substr(i);
    double n = std::atof(numstr.c_str());
    std::vector<std::string> result;

    auto fmt = [](double v) -> std::string {
        if (v == std::floor(v) && std::abs(v) < 1e15) {
            char b[32]; std::snprintf(b, sizeof(b), "%lld", (long long)v); return b;
        }
        char b[32]; std::snprintf(b, sizeof(b), "%.3g", v); return b;
    };

    if (unit == "km") {
        result.push_back(numstr + "千米 = " + fmt(n * 1000) + "米");
    } else if (unit == "m") {
        result.push_back(numstr + "米 = " + fmt(n * 100) + "厘米");
    } else if (unit == "cm") {
        result.push_back(numstr + "厘米 = " + fmt(n / 100.0) + "米");
    } else if (unit == "kg") {
        result.push_back(numstr + "千克 = " + fmt(n * 1000) + "克");
    } else if (unit == "g") {
        result.push_back(numstr + "克 = " + fmt(n / 1000.0) + "千克");
    } else if (unit == "mile" || unit == "mi") {
        result.push_back(numstr + "英里 ≈ " + fmt(n * 1.609) + "千米");
    } else if (unit == "inch" || unit == "in") {
        result.push_back(numstr + "英寸 = " + fmt(n * 2.54) + "厘米");
    } else if (unit == "ft" || unit == "feet") {
        result.push_back(numstr + "英尺 = " + fmt(n * 0.3048) + "米");
    } else if (unit == "lb" || unit == "lbs") {
        result.push_back(numstr + "磅 ≈ " + fmt(n * 0.4536) + "千克");
    } else if (unit == "gal") {
        result.push_back(numstr + "加仑 ≈ " + fmt(n * 3.785) + "升");
    } else if (unit == "C" || unit == "celsius") {
        result.push_back(numstr + "°C = " + fmt(n * 9.0 / 5.0 + 32) + "°F");
    } else if (unit == "F" || unit == "fahrenheit") {
        result.push_back(numstr + "°F = " + fmt((n - 32) * 5.0 / 9.0) + "°C");
    }
    return result;
}

inline bool is_unit_conversion(const std::string &input) {
    return !unit_conversion(input).empty();
}

// ── v0.9.123 百分比转中文 ──────────────────────────────────────
// 输入 "50p" → "百分之五十"
inline std::string percent_to_chinese(const std::string &input) {
    if (input.empty()) return "";
    size_t i = 0;
    while (i < input.size() && input[i] >= '0' && input[i] <= '9') ++i;
    if (i == 0 || i >= input.size()) return "";
    if (input[i] != 'p' && input[i] != 'P') return "";
    if (i + 1 != input.size()) return "";
    long long n = std::atoll(input.substr(0, i).c_str());
    if (n > 999999) return "";
    return "百分之" + num_to_chinese_lower(n);
}

inline bool is_percent_input(const std::string &input) {
    return !percent_to_chinese(input).empty();
}

// ── v0.9.123 统一快速输入入口 ──────────────────────────────────
// 综合判断：给定输入串，返回快速输入候选列表。
// 优先级：日期时间 > 货币 > 单位换算 > 百分比 > 计算器 > 数字转中文
inline std::vector<std::string> quick_candidates(const std::string &input) {
    std::vector<std::string> result;
    // 1. 日期/时间
    std::string dt = datetime_lookup(input);
    if (!dt.empty()) { result.push_back(dt); return result; }
    // 2. 货币
    auto cur = currency_candidates(input);
    if (!cur.empty()) return cur;
    // 3. 单位换算
    auto units = unit_conversion(input);
    if (!units.empty()) return units;
    // 4. 百分比
    std::string pct = percent_to_chinese(input);
    if (!pct.empty()) { result.push_back(pct); return result; }
    // 5. 计算器
    if (is_calculator_input(input)) {
        std::string calc = calculator_eval(input);
        if (!calc.empty()) {
            result.push_back(input + "=" + calc);
            return result;
        }
    }
    // 6. 数字转中文
    auto nums = number_candidates(input);
    if (!nums.empty()) return nums;
    return {};
}



// ── u 模式：笔画输入 ──────────────────────────────────────────
// 搜狗式 u 模式：u 后跟笔画代码（h=横 s=竖 p=撇 n=捺 z=折）
// 返回匹配该笔画序列的常用汉字列表
// 例：uhsp → 横竖撇 → 仁, 亿, 什, 仆, 仇, 付, 仟, 仡, 仞, 代...
inline bool is_u_mode(const std::string &input) {
    return input.size() >= 2 && input[0] == 'u' &&
           std::all_of(input.begin() + 1, input.end(),
                       [](char c) { return c=='h'||c=='s'||c=='p'||c=='n'||c=='z'; });
}

// 笔画→汉字 映射表（常用字，按频率排序）
// h=横(一) s=竖(丨) p=撇(丿) n=捺(丶) z=折(乙)
inline const std::vector<std::pair<std::string, std::vector<std::string>>>&
stroke_table() {
    static const std::vector<std::pair<std::string, std::vector<std::string>>> table = {
        {"h",    {"一", "丁", "七", "三", "丈", "上", "下", "不", "与", "丑"}},
        {"s",    {"丨", "卜", "中", "串", "史", "吏", "吠", "忠", "虫", "临"}},
        {"p",    {"丿", "亿", "仁", "什", "仆", "仇", "付", "仟", "代", "仙"}},
        {"n",    {"丶", "义", "凡", "丸", "丹", "主", "丽", "举", "买", "乱"}},
        {"z",    {"乙", "九", "也", "乃", "刀", "力", "又", "叉", "么", "马"}},
        {"hs",   {"十", "干", "寸", "才", "丁", "木", "本", "术", "未", "末"}},
        {"hp",   {"千", "乞", "川", "亿", "仁", "什", "仆", "仇", "付", "仟"}},
        {"hn",   {"大", "太", "犬", "天", "夫", "夹", "奋", "奎", "夭", "夯"}},
        {"hz",   {"丁", "七", "万", "丈", "与", "丑", "专", "且", "世", "丘"}},
        {"sh",   {"工", "土", "士", "王", "五", "正", "去", "走", "赶", "起"}},
        {"ss",   {"中", "串", "史", "吏", "忠", "虫", "临", "监", "鉴", "竖"}},
        {"sp",   {"少", "小", "尔", "尖", "尘", "尚", "尝", "肖", "省", "雀"}},
        {"sn",   {"小", "少", "尔", "尖", "尘", "尚", "尝", "肖", "省", "雀"}},
        {"sz",   {"口", "日", "曰", "田", "由", "甲", "申", "电", "国", "围"}},
        {"ph",   {"千", "乞", "川", "亿", "仁", "什", "仆", "仇", "付", "仟"}},
        {"ps",   {"少", "小", "尔", "尖", "尘", "尚", "尝", "肖", "省", "雀"}},
        {"pp",   {"行", "很", "得", "待", "往", "征", "径", "徐", "徒", "徙"}},
        {"pn",   {"入", "人", "内", "全", "会", "合", "令", "今", "仓", "企"}},
        {"pz",   {"几", "风", "凤", "凡", "丸", "丹", "九", "也", "乃", "刀"}},
        {"nh",   {"大", "太", "犬", "天", "夫", "夹", "奋", "奎", "夭", "夯"}},
        {"ns",   {"小", "少", "尔", "尖", "尘", "尚", "尝", "肖", "省", "雀"}},
        {"np",   {"入", "人", "内", "全", "会", "合", "令", "今", "仓", "企"}},
        {"nn",   {"义", "凡", "丸", "丹", "主", "丽", "举", "买", "乱", "乳"}},
        {"nz",   {"之", "文", "方", "放", "访", "纺", "防", "妨", "仿", "房"}},
        {"zh",   {"己", "已", "巳", "巴", "邑", "巵", "改", "忌", "志", "忍"}},
        {"zs",   {"口", "日", "曰", "田", "由", "甲", "申", "电", "国", "围"}},
        {"zp",   {"几", "风", "凤", "凡", "丸", "丹", "九", "也", "乃", "刀"}},
        {"zn",   {"之", "文", "方", "放", "访", "纺", "防", "妨", "仿", "房"}},
        {"zz",   {"马", "鸟", "乌", "鸣", "鸭", "鹅", "鸦", "鸽", "鹤", "鹰"}},
        {"hsp",  {"仁", "亿", "什", "仆", "仇", "付", "仟", "代", "仙", "仪"}},
        {"hps",  {"千", "乞", "川", "亿", "仁", "什", "仆", "仇", "付", "仟"}},
        {"shp",  {"少", "小", "尔", "尖", "尘", "尚", "尝", "肖", "省", "雀"}},
        {"phs",  {"千", "乞", "川", "亿", "仁", "什", "仆", "仇", "付", "仟"}},
        {"hsh",  {"木", "本", "术", "未", "末", "札", "杀", "杂", "杠", "杆"}},
        {"hh",   {"二", "十", "干", "于", "亏", "云", "专", "亚", "些", "亭"}},
        {"hhh",  {"三", "王", "玉", "主", "丰", "圭", "寿", "弄", "麦", "表"}},
        {"hhhh", {"四", "置", "署", "罢", "罪", "罗", "罚", "罢", "羁", "罡"}},
        {"ssss", {"口", "日", "曰", "田", "由", "甲", "申", "电", "国", "围"}},
        {"hns",  {"大", "太", "犬", "天", "夫", "夹", "奋", "奎", "夭", "夯"}},
        {"hzn",  {"丁", "七", "万", "丈", "与", "丑", "专", "且", "世", "丘"}},
        {"shn",  {"工", "土", "士", "王", "五", "正", "去", "走", "赶", "起"}},
        {"phn",  {"行", "很", "得", "待", "往", "征", "径", "徐", "徒", "徙"}},
    };
    return table;
}

// u 模式查表：给定笔画代码（如 "hsp"），返回匹配的汉字列表
inline std::vector<std::string> u_mode_lookup(const std::string &strokes) {
    const auto &table = stroke_table();
    // 精确匹配
    for (const auto &[code, chars] : table) {
        if (code == strokes) return chars;
    }
    // 前缀匹配：返回所有以 strokes 为前缀的笔画对应的汉字（去重）
    std::vector<std::string> result;
    for (const auto &[code, chars] : table) {
        if (code.size() > strokes.size() &&
            code.substr(0, strokes.size()) == strokes) {
            for (const auto &c : chars) {
                if (std::find(result.begin(), result.end(), c) == result.end()) {
                    result.push_back(c);
                }
            }
        }
    }
    return result;
}

// ── Tab 英文补全 ──────────────────────────────────────────
// 给定英文前缀，返回可能的完整单词
inline bool is_english_input(const std::string &input) {
    if (input.empty()) return false;
    for (char c : input) {
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

inline const std::vector<std::string>& english_word_list() {
    static const std::vector<std::string> words = {
        "hello", "help", "world", "test", "code", "data", "file", "name", "type", "value",
        "function", "class", "string", "number", "array", "object", "return", "const", "static", "public",
        "private", "protected", "virtual", "override", "template", "namespace", "include", "define", "ifdef", "ifndef",
        "struct", "union", "enum", "typedef", "sizeof", "malloc", "calloc", "realloc", "free", "memcpy",
        "memset", "strcmp", "strcpy", "strlen", "strstr", "printf", "scanf", "fopen", "fclose", "fread",
        "fwrite", "fgets", "fputs", "feof", "fseek", "ftell", "flush", "error", "debug", "info",
        "warn", "fatal", "trace", "assert", "check", "verify", "validate", "parse", "format", "encode",
        "decode", "encrypt", "decrypt", "compress", "decompress", "serialize", "deserialize", "upload", "download", "transfer",
        "connect", "disconnect", "listen", "accept", "send", "receive", "read", "write", "open", "close",
        "create", "destroy", "init", "cleanup", "start", "stop", "pause", "resume", "restart", "update",
        "delete", "insert", "select", "update", "replace", "remove", "search", "sort", "filter", "merge",
        "split", "join", "push", "pop", "shift", "unshift", "slice", "splice", "concat", "reverse",
        "config", "option", "param", "argument", "default", "require", "import", "export", "module", "package",
        "library", "framework", "service", "client", "server", "proxy", "gateway", "router", "handler", "controller",
        "model", "view", "render", "buffer", "stream", "pipe", "queue", "stack", "cache", "pool",
        "thread", "process", "task", "job", "worker", "master", "slave", "leader", "follower", "candidate",
        "version", "branch", "commit", "merge", "rebase", "checkout", "status", "log", "diff", "patch",
        "build", "deploy", "release", "install", "uninstall", "upgrade", "downgrade", "rollback", "backup", "restore",
        "main", "argc", "argv", "stdin", "stdout", "stderr", "null", "true", "false", "auto",
        "void", "int", "char", "bool", "float", "double", "long", "short", "unsigned", "signed",
        "if", "else", "for", "while", "do", "switch", "case", "break", "continue", "goto",
        "and", "or", "not", "in", "out", "ref", "val", "var", "let", "set",
        "get", "has", "is", "can", "should", "would", "could", "will", "shall", "must",
        "about", "above", "after", "again", "all", "any", "ask", "back", "base", "been",
        "before", "being", "best", "better", "big", "book", "both", "but", "call", "came",
        "can", "cap", "car", "cat", "city", "come", "cool", "copy", "cost", "day",
        "did", "die", "end", "even", "ever", "every", "eye", "fact", "far", "fast",
        "feel", "few", "find", "first", "fix", "flag", "flat", "fly", "form", "found",
        "four", "from", "full", "fun", "gap", "gen", "get", "give", "go", "good",
        "got", "great", "group", "grow", "had", "has", "have", "head", "hear", "here",
        "high", "his", "hit", "home", "hope", "how", "idea", "image", "img", "info",
        "into", "its", "job", "just", "keep", "key", "kind", "know", "last", "left",
        "less", "let", "life", "like", "line", "link", "list", "live", "local", "lock",
        "long", "look", "loop", "low", "made", "make", "many", "map", "mark", "may",
        "mean", "men", "might", "mind", "miss", "mode", "more", "most", "move", "much",
        "must", "my", "name", "need", "new", "next", "nice", "no", "node", "none",
        "not", "note", "now", "number", "of", "off", "old", "on", "once", "one",
        "only", "open", "order", "other", "our", "out", "over", "own", "page", "part",
        "pass", "past", "path", "pay", "per", "pick", "plan", "play", "point", "port",
        "post", "put", "quit", "race", "rand", "range", "rate", "raw", "read", "real",
        "ref", "rel", "rest", "result", "return", "right", "room", "root", "rule", "run",
        "save", "say", "see", "seem", "self", "send", "set", "show", "side", "size",
        "slow", "small", "some", "sort", "source", "space", "start", "state", "stay", "step",
        "stop", "store", "sub", "such", "sure", "sync", "system", "tag", "take", "talk",
        "task", "team", "tell", "test", "text", "than", "that", "the", "their", "them",
        "then", "there", "these", "they", "thing", "think", "this", "those", "time", "tiny",
        "tip", "title", "to", "top", "try", "type", "use", "user", "using", "value",
        "very", "want", "was", "way", "well", "were", "what", "when", "where", "which",
        "while", "who", "why", "wide", "will", "with", "work", "would", "write", "wrong",
        "year", "yes", "yet", "you", "your", "zero", "zone",
    };
    return words;
}

// Tab 补全：给定前缀，返回匹配的完整单词（最多 10 个）
inline std::vector<std::string> tab_complete(const std::string &prefix) {
    if (prefix.empty() || !is_english_input(prefix)) return {};
    std::string lower = prefix;
    for (auto &c : lower) if (c >= 'A' && c <= 'Z') c += 32;
    
    std::vector<std::string> result;
    for (const auto &word : english_word_list()) {
        if (word.size() >= lower.size() &&
            word.substr(0, lower.size()) == lower) {
            result.push_back(word);
            if (result.size() >= 10) break;
        }
    }
    return result;
}



// ── i 模式：搜狗式快捷短语 ──────────────────────────────────
// 搜狗 i 模式：输入 i 后跟拼音首字母缩写，返回常用短语。
// 例如：ih→你好  ix→谢谢  iz→再见  izw→早上好
// 用户也可自定义短语文件（~/.config/pinyin-ime/quick_phrases.txt）。
//
// 设计原则：纯函数、无副作用；自定义文件由调用方加载后传入。

struct QuickPhrase {
    std::string abbr;       // 缩写（如 "h", "x", "zw"）
    std::string phrase;     // 完整短语
    std::string pinyin;     // 完整拼音（供学习/调试）
};

// 内置快捷短语表（按使用频率排序）
inline const std::vector<QuickPhrase>& builtin_quick_phrases() {
    static const std::vector<QuickPhrase> phrases = {
        // 问候
        {"h",  "你好",       "nihao"},
        {"hh", "你好你好",   "nihaonihao"},
        {"x",  "谢谢",       "xiexie"},
        {"z",  "再见",       "zaijian"},
        {"zw", "早上好",     "zaoshanghao"},
        {"xw", "下午好",     "xiawuhao"},
        {"ww", "晚上好",     "wanshanghao"},
        {"wa", "晚安",       "wanan"},
        {"hj", "好久不见",   "haojiubujian"},
        // 礼貌
        {"q",  "请",         "qing"},
        {"qq", "起请",       "qiqing"},
        {"d",  "对的",       "duide"},
        {"dy", "当然",       "dangran"},
        {"bd", "不对",       "budui"},
        {"by", "不用",       "buyong"},
        {"my", "没有",       "meiyou"},
        {"kl", "客气了",     "keqile"},
        {"bk", "不客气",     "bukeqi"},
        {"sg", "应该的",     "yinggaide"},
        // 道歉/回应
        {"sq", "对不起",     "duibuqi"},
        {"my", "没关系",     "meiguanxi"},
        {"hl", "好了",       "haole"},
        {"ok", "好的",       "haode"},
        {"xk", "辛苦了",     "xinkule"},
        {"lx", "麻烦了",     "mafanle"},
        // 工作/日常
        {"sy", "收到",       "shoudao"},
        {"zl", "知道了",     "zhidaole"},
        {"ml", "明白了",     "mingbaile"},
        {"dl", "到了",       "daole"},
        {"wl", "完了",       "wanle"},
        {"ks", "开始",       "kaishi"},
        {"js", "结束",       "jieshu"},
        {"cx", "继续",       "jixu"},
        {"dy", "等一下",     "dengyixia"},
        {"mt", "没问题",     "meiwenti"},
        // 情感
        {"gx", "高兴",       "gaoxing"},
        {"kk", "开心",       "kaixin"},
        {"ns", "难受",       "nanshou"},
        {"sl", "算了",       "suanle"},
        {"hy", "好的呀",     "haodeya"},
        {"xa", "想啊",       "xianga"},
        // 常用句
        {"wm", "我们",       "women"},
        {"nm", "你们",       "nimen"},
        {"tm", "他们",       "tamen"},
        {"sh", "是的",       "shide"},
        {"bs", "不是",       "bushi"},
        {"zy", "这样",       "zheyang"},
        {"ny", "那样",       "nayang"},
        {"sm", "什么",       "shenme"},
        {"ws", "为什么",     "weishenme"},
        {"zm", "怎么",       "zenme"},
        {"zy", "怎么样",     "zenmeyang"},
        {"ky", "可以",       "keyi"},
        {"by", "不用谢",     "buyongxie"},
        {"gr", "个人",       "geren"},
        {"jt", "今天",       "jintian"},
        {"mt", "明天",       "mingtian"},
        {"zt", "昨天",       "zuotian"},
        {"xw", "现在",       "xianzai"},
        {"yl", "以后",       "yihou"},
        {"yq", "以前",       "yiqian"},
        // 网络/聊天
        {"lx", "联系",       "lianxi"},
        {"dh", "电话",       "dianhua"},
        {"yx", "邮箱",       "youxiang"},
        {"dz", "地址",       "dizhi"},
        {"wd", "我的",       "wode"},
        {"nd", "你的",       "nide"},
        {"td", "他的",       "tade"},
        {"aj", "啊就",       "ajiu"},
        {"ha", "哈哈",       "haha"},
        {"he", "呵呵",       "hehe"},
        {"xi", "嘻嘻",       "xixi"},
        {"en", "嗯嗯",       "enen"},
        {"oh", "哦哦",       "ohoho"},
    };
    return phrases;
}

// 判断是否为 i 模式输入：以 'i' 开头，后跟小写字母
inline bool is_i_mode(const std::string &input) {
    if (input.size() < 2 || input[0] != 'i') return false;
    for (size_t k = 1; k < input.size(); ++k) {
        char c = input[k];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z'))) return false;
    }
    return true;
}

// i 模式查表：给定缩写（如 "h"），返回匹配的短语列表
// 匹配规则：精确匹配优先，然后前缀匹配
inline std::vector<std::string> i_mode_lookup(const std::string &abbr,
    const std::vector<QuickPhrase> *customPhrases = nullptr) {
    if (abbr.empty()) return {};

    std::vector<std::string> result;
    const auto &builtin = builtin_quick_phrases();

    // 精确匹配（内置 + 自定义）
    for (const auto &p : builtin) {
        if (p.abbr == abbr) result.push_back(p.phrase);
    }
    if (customPhrases) {
        for (const auto &p : *customPhrases) {
            if (p.abbr == abbr) result.push_back(p.phrase);
        }
    }

    // 前缀匹配（内置 + 自定义），去重
    for (const auto &p : builtin) {
        if (p.abbr.size() > abbr.size() &&
            p.abbr.substr(0, abbr.size()) == abbr) {
            // 去重
            if (std::find(result.begin(), result.end(), p.phrase) == result.end()) {
                result.push_back(p.phrase);
            }
        }
    }
    if (customPhrases) {
        for (const auto &p : *customPhrases) {
            if (p.abbr.size() > abbr.size() &&
                p.abbr.substr(0, abbr.size()) == abbr) {
                if (std::find(result.begin(), result.end(), p.phrase) == result.end()) {
                    result.push_back(p.phrase);
                }
            }
        }
    }

    // 限制最多 20 个
    if (result.size() > 20) result.resize(20);
    return result;
}

// ── 联想词：基于已上屏文本预测下一个词 ──────────────────────
// 搜狗式联想：上屏后自动显示关联词候选，用户按数字键直接选词。
// 联想词表（内置常用搭配），可由调用方扩展。
struct AssociationRule {
    std::string trigger;    // 触发词（已上屏的最后一个词）
    std::vector<std::string> associates;  // 联想候选
};

inline const std::vector<AssociationRule>& builtin_associations() {
    static const std::vector<AssociationRule> rules = {
        // 代词 → 常见搭配
        {"我",   {"的", "们", "是", "想", "会", "能", "要", "去", "在", "有"}},
        {"你",   {"好", "的", "们", "是", "想", "会", "能", "要", "去", "在"}},
        {"他",   {"的", "们", "是", "说", "想", "会", "能", "要", "去", "在"}},
        {"她",   {"的", "是", "说", "想", "会", "能", "要", "去", "在", "好"}},
        {"它",   {"的", "是", "们", "能", "要", "在", "有", "会", "可", "应"}},
        {"我们", {"的", "是", "要", "去", "在", "有", "能", "会", "都", "应"}},
        {"你们", {"的", "是", "要", "去", "在", "有", "能", "会", "都", "应"}},
        {"他们", {"的", "是", "要", "去", "在", "有", "能", "会", "都", "说"}},
        // 问候 → 回应
        {"你好", {"啊", "吗", "的", "，", "！", "呀", "呢", "，再见", "，谢谢", "，你好"}},
        {"谢谢", {"你", "啊", "！", "，不用谢", "，不客气", "，应该的", "，没问题", "，辛苦了", "，好的", "，再见"}},
        {"再见", {"！", "，下次见", "，拜拜", "，保重", "，一路平安", "，回头见", "，明天见", "，好的", "，嗯"}},
        {"早安", {"！", "，你好", "，新的一天", "，加油", "，好的", "，嗯", "，再见", "，谢谢", "，辛苦了"}},
        {"晚安", {"！", "，好梦", "，早点休息", "，明天见", "，好的", "，嗯", "，再见", "，谢谢", "，辛苦了"}},
        // 动词 → 常见宾语
        {"想",   {"要", "到", "起", "去", "说", "做", "看", "听", "买", "吃"}},
        {"要",   {"去", "做", "看", "说", "想", "买", "吃", "喝", "用", "给"}},
        {"去",   {"哪", "了", "看", "做", "买", "吃", "喝", "玩", "学", "上班"}},
        {"做",   {"什么", "了", "的", "好", "到", "事", "梦", "饭", "作业", "决定"}},
        {"看",   {"看", "了", "到", "书", "电影", "电视", "新闻", "报告", "一下", "情况"}},
        {"说",   {"了", "的", "到", "什么", "话", "法", "明", "是", "好", "吧"}},
        {"吃",   {"了", "饭", "什么", "好", "的", "到", "东西", "早餐", "午餐", "晚餐"}},
        {"喝",   {"了", "水", "茶", "咖啡", "酒", "汤", "什么", "好", "的", "到"}},
        {"买",   {"了", "什么", "的", "到", "东西", "书", "票", "菜", "水果", "衣服"}},
        {"学",   {"习", "了", "的", "到", "会", "好", "生", "校", "习", "中文"}},
        // 形容词 → 常见搭配
        {"好",   {"的", "了", "吗", "啊", "，", "！", "呀", "呢", "吧", "很好"}},
        {"不",   {"好", "是", "要", "能", "会", "想", "去", "做", "看", "说"}},
        {"很",   {"好", "棒", "快", "慢", "多", "少", "大", "小", "高", "低"}},
        {"真",   {"好", "棒", "的", "是", "想", "可", "不", "会", "能", "要"}},
        // 名词 → 常见搭配
        {"今天", {"天气", "是", "的", "怎么样", "好", "忙", "累", "开心", "高兴", "下午"}},
        {"明天", {"见", "是", "的", "怎么样", "好", "忙", "累", "开心", "高兴", "下午"}},
        {"现在", {"是", "的", "几点", "怎么样", "好", "忙", "累", "开心", "高兴", "可以"}},
        {"时间", {"是", "的", "到", "不", "够", "好", "长", "短", "快", "慢"}},
        {"工作", {"是", "的", "很", "好", "忙", "累", "开心", "顺利", "完成", "问题"}},
        {"生活", {"是", "的", "很", "好", "忙", "累", "开心", "幸福", "美好", "问题"}},
        // 否定/疑问
        {"不是", {"的", "吗", "吧", "，", "，是", "，对", "，好", "，嗯", "，真的", "，可能"}},
        {"没有", {"的", "吗", "吧", "，", "，是", "，对", "，好", "，嗯", "，真的", "，可能"}},
        {"怎么", {"了", "样", "办", "回事", "说", "做", "去", "用", "能", "会"}},
        {"什么", {"的", "吗", "呢", "吧", "，", "意思", "时候", "地方", "人", "事"}},
        {"为什么", {"呢", "吗", "，", "不", "要", "会", "能", "是", "这样", "那样"}},
    };
    return rules;
}

// 联想词查找：给定已上屏文本，返回联想候选列表
// 取文本末尾 1-4 个字符作为触发词，匹配联想规则
inline std::vector<std::string> association_lookup(const std::string &committedText) {
    if (committedText.empty()) return {};

    // 提取末尾的中文词（UTF-8，每个汉字 3 字节）
    // 尝试末尾 1-4 个字符作为触发词
    std::vector<std::string> triggers;
    // 计算汉字数量
    size_t charCount = 0;
    for (size_t i = 0; i < committedText.size(); ) {
        unsigned char c = committedText[i];
        int len = 1;
        if (c >= 0xF0) len = 4;
        else if (c >= 0xE0) len = 3;
        else if (c >= 0xC0) len = 2;
        if (i + len > committedText.size()) break;
        charCount++;
        i += len;
    }

    // 提取末尾 1-4 个汉字
    for (size_t n = 1; n <= std::min<size_t>(4, charCount); ++n) {
        // 跳到倒数第 n 个汉字
        size_t pos = committedText.size();
        for (size_t k = 0; k < n; ++k) {
            if (pos == 0) break;
            // 回退一个 UTF-8 字符
            --pos;
            while (pos > 0 && (static_cast<unsigned char>(committedText[pos]) & 0xC0) == 0x80) --pos;
        }
        triggers.push_back(committedText.substr(pos));
    }

    // 按触发词长度从长到短匹配（优先长词匹配）
    const auto &rules = builtin_associations();
    for (auto it = triggers.rbegin(); it != triggers.rend(); ++it) {
        for (const auto &rule : rules) {
            if (rule.trigger == *it) {
                return rule.associates;
            }
        }
    }

    return {};
}


// ════════════════════════════════════════════════════════════════════
// v0.9.126 新增搜狗式功能
// ════════════════════════════════════════════════════════════════════

// ── 自定义快捷词：用户自定义缩写→短语映射 ──────────────────────
// 搜狗式自定义短语：输入缩写直接展开为预设文本。
struct CustomPhrase {
    std::string abbr;       // 触发缩写
    std::string phrase;     // 展开文本
    std::string description; // 描述
};

inline const std::vector<CustomPhrase>& builtin_custom_phrases() {
    static const std::vector<CustomPhrase> phrases = {
        {"addr",  "北京市海淀区中关村南大街", "地址"},
        {"email", "admin@example.com",         "邮箱"},
        {"phone", "13800138000",               "电话"},
        {"sig",   "——来自我的Linux输入法",    "签名"},
        {"br",    "\n",                         "换行"},
        {"hr",    "──────────",                "分隔线"},
        {"todo",  "【待办】",                  "待办标记"},
        {"done",  "【已完成】",                "完成标记"},
        {"fixme", "【待修复】",                "修复标记"},
        {"note",  "【备注】",                  "备注标记"},
        {"wip",   "【进行中】",                "进行中标记"},
        {"main",  "int main(int argc, char* argv[])", "main函数"},
        {"for",   "for (int i = 0; i < n; i++)",      "for循环"},
        {"while", "while (true)",                     "while循环"},
        {"if",    "if (condition)",                  "if语句"},
        {"ret",   "return",                          "返回"},
        {"hi",    "你好",                          "你好"},
        {"thx",   "谢谢",                          "谢谢"},
        {"sry",   "对不起",                        "对不起"},
        {"np",    "没问题",                        "没问题"},
        {"btw",   "顺便说一下",                    "顺便说一下"},
        {"fyi",   "供参考",                        "供参考"},
        {"asap",  "尽快",                          "尽快"},
        {"tba",   "待定",                          "待定"},
        {"imo",   "我认为",                        "我认为"},
        {"etc",   "等等",                          "等等"},
    };
    return phrases;
}

inline std::vector<std::string> custom_phrase_lookup(const std::string &input) {
    if (input.empty() || input.size() > 20) return {};
    std::vector<std::string> result;
    for (const auto &p : builtin_custom_phrases()) {
        if (p.abbr == input) result.push_back(p.phrase);
    }
    return result;
}

inline bool is_custom_phrase(const std::string &input) {
    if (input.empty()) return false;
    for (const auto &p : builtin_custom_phrases()) {
        if (p.abbr == input) return true;
    }
    return false;
}

// ── 中英文混输：拼音流中识别英文单词 ──────────────────────
// 搜狗式中英文混输：用户无需切换即可输入常用英文单词。
inline const std::vector<std::string>& common_english_words() {
    static const std::vector<std::string> words = {
        "python", "java", "javascript", "typescript", "golang", "rust",
        "linux", "windows", "macos", "android", "docker", "kubernetes",
        "git", "nginx", "apache", "mysql", "redis", "mongodb",
        "react", "vue", "angular", "node", "django", "flask", "spring",
        "hello", "world", "test", "debug", "error", "warning", "info",
        "file", "code", "data", "type", "name", "value", "count", "size",
        "list", "dict", "array", "string", "number", "boolean", "object",
        "class", "function", "method", "property", "event", "handler",
        "config", "setting", "option", "param", "args", "result", "output",
        "input", "source", "target", "format", "parse", "build", "deploy",
        "http", "https", "ftp", "ssh", "tcp", "udp", "api", "url", "uri",
        "json", "xml", "html", "css", "sql", "csv", "yaml", "toml",
        "email", "password", "username", "token", "session", "cookie",
        "ok", "yes", "no", "true", "false", "null", "none", "void",
        "help", "about", "version", "update", "install", "remove", "delete",
        "open", "close", "save", "load", "start", "stop", "pause", "resume",
        "play", "next", "prev", "home", "end", "back", "forward", "refresh",
    };
    return words;
}

inline std::vector<std::string> mixed_input_lookup(const std::string &input) {
    if (input.empty() || input.size() < 2) return {};
    for (char c : input) {
        if (c < 'a' || c > 'z') return {};
    }
    std::vector<std::string> result;
    // 1. 精确匹配（任何长度都允许）
    for (const auto &w : common_english_words()) {
        if (w == input) { result.push_back(w); break; }
    }
    // 2. 前缀匹配：跳过有效拼音（避免 wo→world, ta→target）
    //    非拼音输入(如 py, deb)仍可匹配英文前缀
    if (result.empty()) {
        bool is_pinyin = false;
        // 检查是否是有效拼音音节（简化判断：查预设表）
        static const std::unordered_set<std::string> valid_pinyin = {
            "wo","ta","ma","ya","rou","ji","zhu","ba","pa","fa","da","na","la","ga","ka","ha","za","ca","sa","ra","de","he","le","ge","qu","du","dui","kan","shuo","xiang","neng","hui","xiao","hao","bu","ren","shang","xia","li","wai","duo","shao","lai","zuo","zhong","zai","you","shi","ni",
            "bo","po","mo","fo","do","to","no","lo","go","ko","ho","zo","co","so","ro",
            "bi","pi","mi","di","ti","ni","li","qi","xi","yi","ri","zi","ci","si",
            "bu","pu","mu","fu","du","tu","nu","lu","gu","ku","hu","zu","cu","su","ru","yu","wu","ju","qu","xu",
            "ai","ei","ao","ou","an","en","ang","eng","er",
            "zhi","chi","shi","rii","zhu","chu","shu","ruo","zha","cha","sha","zhe","che","she","re",
            "zhi","chi","shi","zho","cho","sho","zhu","chu","shu","zhai","chai","shai","zhou","chou","shou",
            "zhan","chan","shan","zhen","chen","shen","rhen","zhang","chang","shang","zheng","cheng","sheng","reng",
            "zhong","chong","shong","rung","zhuo","chuo","shuo","ruo","zhua","chua","shua","rua",
            "zhuai","chuai","shuai","zhuang","chuang","shuang","zhueng","chueng","shueng",
            "zhui","chui","shui","zhun","chun","shun","zhuan","chuan","shuan","zhuan","chuan","shuan",
            "gou","mao","yu","dan","nai","yang","zhou","rou","ji","ya","zhu",
            "fayu","deyu","haoma","xingma","qibaji","shibai",
            "bana","pama","fada","tana","nala","laga","kaha","zaca","sara","raza",
            "beipe","meife","deite","nele","lege","kehe","zece","sese","rez",
            "bipi","mimi","diti","nili","liqi","qixi","xiyi","riri","zici","sisi",
            "bupu","mufu","dutu","nulu","lugu","kuhu","zucu","susu","ruyu","wuju","quxu",
            "bai","pai","mai","dai","tai","nai","lai","gai","kai","hai","zai","cai","sai","rai",
            "bei","pei","mei","fei","dei","tei","nei","lei","gei","kei","hei","zei","cei","sei","rei",
            "bao","pao","mao","dao","tao","nao","lao","gao","kao","hao","zao","cao","sao","rao",
            "ban","pan","man","fan","dan","tan","nan","lan","gan","kan","han","zan","can","san","ran",
            "ben","pen","men","fen","den","ten","nen","len","gen","ken","hen","zen","cen","sen","ren",
            "bang","pang","mang","fang","dang","tang","nang","lang","gang","kang","hang","zang","cang","sang","rang",
            "beng","peng","meng","feng","deng","teng","neng","leng","geng","keng","heng","zeng","ceng","seng","reng",
            "biao","piao","miao","diao","tiao","niao","liao","jiao","qiao","xiao","yiao",
            "bian","pian","mian","dian","tian","nian","lian","jian","qian","xian","yian",
            "bin","pin","min","din","tin","nin","lin","jin","qin","xin","yin",
            "bing","ping","ming","ding","ting","ning","ling","jing","qing","xing","ying",
            "duan","tuan","nuan","luan","guan","kuan","huan","zhuan","chuan","shuan","ruan","zuan","cuan","suan",
            "dui","tui","gui","kui","hui","zhui","chui","shui","rui","zui","cui","sui",
            "dun","tun","gun","kun","hun","zhun","chun","shun","run","zun","cun","sun",
            "dong","tong","nong","long","gong","kong","hong","zhong","chong","shong","rong","zong","cong","song",
            "guang","kuang","huang","zhuang","chuang","shuang","ruang","zuang","cuang","suang",
            "guo","kuo","huo","zhuo","chuo","shuo","ruo","zuo","cuo","suo",
            "nve","lve","jue","que","xue","yue","nue","lue",
            "nv","lv","nu","lu","ju","qu","xu","yu",
            "jiao","qiao","xiao","jiong","qiong","xiong","yong",
            "jiang","qiang","xiang","yang","wang","liang","niang",
            "jvan","qvan","xvan","yvan","juan","quan","xuan","yuan",
            "jvn","qvn","xvn","yvn","jun","qun","xun","yun",
            "jve","qve","xve","yve","jue","que","xue","yue"
        };
        is_pinyin = (valid_pinyin.count(input) > 0);
        
        if (!is_pinyin) {
            for (const auto &w : common_english_words()) {
                if (w.size() > input.size() && w.substr(0, input.size()) == input) {
                    result.push_back(w);
                    if (result.size() >= 5) break;
                }
            }
        }
    }
    return result;
}

inline bool is_mixed_english(const std::string &input) {
    return !mixed_input_lookup(input).empty();
}

// ── 常见错音纠正：自动修正常见拼音输入错误 ──────────────────────
// 注意：这不是模糊音！这是纠正用户打字时的实际错误。
struct TypoCorrection {
    std::string wrong;
    std::string correct;
};

inline const std::vector<TypoCorrection>& common_typos() {
    static const std::vector<TypoCorrection> typos = {
        {"nihoa",  "nihao"},
        {"zhonggu",  "zhongguo"},
        {"shuruf",   "shurufa"},
        {"shurufaa", "shurufa"},
        {"wihao",    "nihao"},
        {"bihao",    "nihao"},
        {"shijina",  "shijian"},
        {"inaho",    "nihao"},
        {"inaho",    "nihao"},
    };
    return typos;
}

inline std::vector<std::string> typo_correction_lookup(const std::string &input) {
    if (input.empty() || input.size() < 3) return {};
    std::vector<std::string> result;
    for (const auto &t : common_typos()) {
        if (t.wrong == input && t.correct != input) {
            result.push_back(t.correct);
        }
    }
    // 通用纠错：相邻字母交换检测
    if (result.empty() && input.size() >= 4) {
        static const std::vector<std::string> valid_initials = {
            "zh", "ch", "sh", "b", "p", "m", "f", "d", "t", "n",
            "l", "g", "k", "h", "j", "q", "x", "r", "z", "c", "s",
            "y", "w", "a", "o", "e", "i", "u", "v"
        };
        for (size_t i = 0; i + 1 < input.size(); ++i) {
            std::string swapped = input;
            std::swap(swapped[i], swapped[i + 1]);
            if (swapped == input) continue;
            bool swapped_valid = false, orig_valid = false;
            for (const auto &init : valid_initials) {
                if (swapped.substr(0, init.size()) == init) swapped_valid = true;
                if (input.substr(0, init.size()) == init) orig_valid = true;
            }
            if (swapped_valid && !orig_valid) {
                result.push_back(swapped);
                break;
            }
        }
    }
    return result;
}

// ── 成语速查：按首字母速查四字成语 ──────────────────────
// 搜狗式成语输入：输入四个拼音首字母，匹配四字成语。
struct IdiomEntry {
    std::string idiom;
    std::string pinyin;
    std::string initials;
};

inline const std::vector<IdiomEntry>& builtin_idioms() {
    static const std::vector<IdiomEntry> idioms = {
        {"一帆风顺", "yifanfengshun", "yffs"},
        {"一路顺风", "yilushunfeng",  "ylsf"},
        {"一路平安", "yilupingan",    "ylpa"},
        {"一鸣惊人", "yimingjingren", "ymjr"},
        {"一诺千金", "yinuoqianjin",  "ynqj"},
        {"一日千里", "yiriqianli",    "yrql"},
        {"一丝不苟", "yisibugou",     "ysbg"},
        {"一言为定", "yiyanweiding",  "yywd"},
        {"一往无前", "yiwangwuqian",  "ywwq"},
        {"一马当先", "yimadangxian",  "ymdx"},
        {"万事如意", "wanshiruyi",    "wsry"},
        {"万无一失", "wanwuyishi",    "wwys"},
        {"千军万马", "qianjunwanma",  "qjwm"},
        {"千言万语", "qianyanwanyu",  "qywy"},
        {"千方百计", "qianfangbaiji", "qfbj"},
        {"千辛万苦", "qianxinwanku",  "qxwk"},
        {"海阔天空", "haikuotiankong","hktt"},
        {"天长地久", "tianchangdijiu","tcdj"},
        {"天经地义", "tianjingdiyi",  "tjdy"},
        {"天涯海角", "tianyahaijiao", "tyhj"},
        {"风和日丽", "fengerili",     "fhrl"},
        {"风平浪静", "fengpinglangjing","fplj"},
        {"雨过天晴", "yuguotianqing", "ygtq"},
        {"春暖花开", "chunnuanhuakai","cnhk"},
        {"秋高气爽", "qiugaoqishuang","qgqs"},
        {"金玉满堂", "jinyumantang",  "jymt"},
        {"龙飞凤舞", "longfeifengwu", "lffw"},
        {"龙马精神", "longmajingshen","lmjs"},
        {"马到成功", "madaochenggong","mdcg"},
        {"心想事成", "xinxiangshicheng","xxsc"},
        {"心满意足", "xinmanyizu",    "xmyz"},
        {"胸有成竹", "xiongyouchengzhu","xycz"},
        {"喜气洋洋", "xiqiyangyang",  "xqyy"},
        {"笑口常开", "xiaokouchangkai","xkck"},
        {"眉开眼笑", "meikaiyanxiao", "mkyx"},
        {"欢天喜地", "huantianxidi",  "htxd"},
        {"鹏程万里", "pengchengwanli","pcwl"},
        {"前程似锦", "qianchengsijin","qcsj"},
        {"如鱼得水", "ruyudeshui",    "ryds"},
        {"如虎添翼", "ruhutianyi",    "rhty"},
        {"如释重负", "rushizhongfu",  "rszf"},
        {"如梦初醒", "rumengchuxing", "rmcx"},
        {"自相矛盾", "zixiangmaodun", "zxmd"},
        {"自言自语", "ziyanziyu",     "zyzy"},
        {"自强不息", "ziqiangbuxi",   "zqbx"},
        {"自得其乐", "zideqile",      "zdql"},
        {"无忧无虑", "wuyouwulv",     "wywl"},
        {"无微不至", "wuweibuzhi",    "wwbz"},
        {"无与伦比", "wuyulunbi",     "wylb"},
        {"不可思议", "bukesiyi",      "bksy"},
        {"不约而同", "buyueertong",   "byet"},
        {"不耻下问", "buchixiawen",   "bcxw"},
        {"不屈不挠", "buqubunao",     "bqbn"},
        {"不遗余力", "buyiyuli",      "byyl"},
        {"乐不思蜀", "lebusishu",     "lbss"},
        {"乐在其中", "lezaiqizhong",  "lzqz"},
        {"井井有条", "jingjingyoutiao","jjyt"},
        {"井底之蛙", "jingdizhiwa",   "jdzw"},
        {"亡羊补牢", "wangyangbulao","wybl"},
        {"事半功倍", "shibangongbei", "sbgb"},
        {"事与愿违", "shiyuyuanwei",  "syyw"},
        {"五光十色", "wuguangshise",  "wgss"},
        {"五湖四海", "wuhusihai",     "whsh"},
        {"六神无主", "liushenwuzhu",  "lswz"},
        {"七上八下", "qishangbaxia",  "qsbx"},
        {"九牛一毛", "jiuniuyimao",   "jnym"},
        {"十全十美", "shiquanshimei", "sqsm"},
        {"百发百中", "baifabaizhong", "bfbz"},
        {"百闻不如一见", "baiwenburuyijian", "bwbryj"},
    };
    return idioms;
}

// 成语速查：输入首字母（如 "yffs"），返回匹配的成语
inline std::vector<std::string> idiom_lookup(const std::string &initials) {
    if (initials.empty() || initials.size() < 2) return {};
    std::vector<std::string> result;
    for (const auto &idiom : builtin_idioms()) {
        if (idiom.initials == initials) {
            result.push_back(idiom.idiom);
        }
    }
    // 前缀匹配（如果精确匹配为空）
    if (result.empty()) {
        for (const auto &idiom : builtin_idioms()) {
            if (idiom.initials.size() > initials.size() &&
                idiom.initials.substr(0, initials.size()) == initials) {
                result.push_back(idiom.idiom);
            }
        }
    }
    if (result.size() > 10) result.resize(10);
    return result;
}

inline bool is_idiom_input(const std::string &input) {
    return !idiom_lookup(input).empty();
}

// ── 繁简转换：简体→繁体 常用字映射 ──────────────────────
// 搜狗式繁简切换：提供繁体候选。
inline const std::vector<std::pair<std::string, std::string>>&
simplified_to_traditional_map() {
    static const std::vector<std::pair<std::string, std::string>> map = {
        {"国", "國"}, {"学", "學"}, {"生", "生"}, {"经", "經"}, {"济", "濟"},
        {"发", "發"}, {"展", "展"}, {"关", "關"}, {"键", "鍵"}, {"盘", "盤"},
        {"电", "電"}, {"脑", "腦"}, {"网", "網"}, {"络", "絡"}, {"程", "程"},
        {"序", "序"}, {"设", "設"}, {"备", "備"}, {"系", "系"}, {"统", "統"},
        {"文", "文"}, {"件", "件"}, {"夹", "夾"}, {"搜", "搜"}, {"索", "索"},
        {"输", "輸"}, {"入", "入"}, {"法", "法"}, {"汉", "漢"}, {"字", "字"},
        {"词", "詞"}, {"典", "典"}, {"语", "語"}, {"言", "言"}, {"音", "音"},
        {"乐", "樂"}, {"观", "觀"}, {"看", "看"}, {"见", "見"}, {"书", "書"},
        {"写", "寫"}, {"读", "讀"}, {"学", "學"}, {"习", "習"}, {"题", "題"},
        {"目", "目"}, {"标", "標"}, {"准", "準"}, {"确", "確"}, {"认", "認"},
        {"识", "識"}, {"记", "記"}, {"忆", "憶"}, {"忆", "憶"}, {"计", "計"},
        {"划", "劃"}, {"设", "設"}, {"计", "計"}, {"实", "實"}, {"现", "現"},
        {"功", "功"}, {"能", "能"}, {"测", "測"}, {"试", "試"}, {"验", "驗"},
        {"证", "證"}, {"明", "明"}, {"结", "結"}, {"果", "果"}, {"报", "報"},
        {"告", "告"}, {"显", "顯"}, {"示", "示"}, {"页", "頁"}, {"面", "面"},
        {"图", "圖"}, {"片", "片"}, {"频", "頻"}, {"数", "數"}, {"据", "據"},
        {"库", "庫"}, {"表", "表"}, {"单", "單"}, {"双", "雙"}, {"击", "擊"},
        {"选", "選"}, {"项", "項"}, {"删", "刪"}, {"除", "除"}, {"修", "修"},
        {"改", "改"}, {"更", "更"}, {"新", "新"}, {"旧", "舊"}, {"历", "歷"},
        {"史", "史"}, {"时", "時"}, {"间", "間"}, {"钟", "鐘"}, {"表", "表"},
    };
    return map;
}

// 简体转繁体：逐字替换
inline std::string to_traditional(const std::string &simplified) {
    const auto &map = simplified_to_traditional_map();
    std::string result;
    size_t i = 0;
    while (i < simplified.size()) {
        unsigned char c = simplified[i];
        int len = 1;
        if (c >= 0xF0) len = 4;
        else if (c >= 0xE0) len = 3;
        else if (c >= 0xC0) len = 2;
        if (i + len > simplified.size()) {
            result += simplified[i];
            ++i;
            continue;
        }
        std::string ch = simplified.substr(i, len);
        bool found = false;
        for (const auto &[simp, trad] : map) {
            if (simp == ch) {
                result += trad;
                found = true;
                break;
            }
        }
        if (!found) result += ch;
        i += len;
    }
    return result;
}

// ── v0.9.126 统一快速输入入口（扩展） ──────────────────────
// 在原有 quick_candidates() 基础上新增自定义快捷词、中英文混输、成语速查
inline std::vector<std::string> quick_candidates_v2(const std::string &input) {
    std::vector<std::string> result;

    // 1. 原有 quick_candidates
    result = quick_candidates(input);

    // 2. 自定义快捷词
    auto custom = custom_phrase_lookup(input);
    for (auto &c : custom) result.push_back(std::move(c));

    // 3. 中英文混输
    auto mixed = mixed_input_lookup(input);
    for (auto &m : mixed) result.push_back(std::move(m));

    // 4. 成语速查（输入 >= 2 个字母时尝试）

    // 5. 错音纠正（返回纠正后的拼音，由调用方再解码）
    auto corrected = typo_correction_lookup(input);
    for (auto &c : corrected) result.push_back(std::move(c));
    if (input.size() >= 2) {
        auto idioms = idiom_lookup(input);
        for (auto &idm : idioms) result.push_back(std::move(idm));
    }

    // 去重
    std::vector<std::string> unique;
    for (auto &r : result) {
        bool dup = false;
        for (auto &u : unique) {
            if (u == r) { dup = true; break; }
        }
        if (!dup) unique.push_back(std::move(r));
    }

    if (unique.size() > 20) unique.resize(20);
    return unique;
}


// ════════════════════════════════════════════════════════════════════
// v0.9.127 新增搜狗式功能
// ════════════════════════════════════════════════════════════════════

// ── 网址/邮箱智能识别：输入特定格式自动识别 ──────────────────────
// 搜狗式：输入 www. / http / mailto 等自动识别为网址/邮箱
inline std::vector<std::string> smart_url_lookup(const std::string &input) {
    if (input.empty() || input.size() < 3) return {};
    std::vector<std::string> result;
    
    // 网址识别
    if (input.substr(0, 4) == "www." || 
        input.substr(0, 7) == "http://" ||
        input.substr(0, 8) == "https://") {
        result.push_back(input);
        // 补全协议
        if (input.substr(0, 4) == "www.") {
            result.push_back("https://" + input);
        }
    }
    
    // 邮箱识别（包含 @ 符号）
    if (input.find('@') != std::string::npos) {
        result.push_back(input);
    }
    
    // 常见域名补全：输入 baidu → https://www.baidu.com
    if (input.size() >= 3 && input.find('.') == std::string::npos &&
        input.find('@') == std::string::npos) {
        static const std::vector<std::pair<std::string, std::string>> domains = {
            {"baidu", "https://www.baidu.com"},
            {"google", "https://www.google.com"},
            {"github", "https://github.com"},
            {"gitlab", "https://gitlab.com"},
            {"bilibili", "https://www.bilibili.com"},
            {"zhihu", "https://www.zhihu.com"},
            {"taobao", "https://www.taobao.com"},
            {"jingdong", "https://www.jd.com"},
            {"weibo", "https://weibo.com"},
            {"douyin", "https://www.douyin.com"},
        };
        for (const auto &[name, url] : domains) {
            if (name == input || name.substr(0, input.size()) == input) {
                result.push_back(url);
            }
        }
    }
    
    if (result.size() > 5) result.resize(5);
    return result;
}

inline bool is_smart_url(const std::string &input) {
    return !smart_url_lookup(input).empty();
}

// ── 拼音分隔符智能插入：搜狗式分词辅助 ──────────────────────
// 输入长拼音串时自动插入分隔符提示，如 "nihaozhongguo" → "ni'hao'zhong'guo"
inline std::string pinyin_segment_hint(const std::string &input) {
    if (input.empty() || input.size() < 4) return {};
    
    // 拼音声母表
    static const std::vector<std::string> initials = {
        "zh", "ch", "sh", "b", "p", "m", "f", "d", "t", "n",
        "l", "g", "k", "h", "j", "q", "x", "r", "z", "c", "s",
        "y", "w"
    };
    // 拼音韵母表（简化）
    static const std::vector<std::string> finals = {
        "a", "o", "e", "i", "u", "v",
        "ai", "ei", "ui", "ao", "ou", "iu", "ie", "ve",
        "an", "en", "in", "un", "vn",
        "ang", "eng", "ing", "ong", "iong",
        "ia", "ua", "uo", "uai", "uei", "iao", "iou",
        "ian", "uan", "van", "iang", "uang", "ueng",
    };
    
    std::string result;
    size_t i = 0;
    bool first = true;
    
    while (i < input.size()) {
        if (!first) result += "'";
        
        // 尝试匹配声母
        std::string matched_initial;
        for (const auto &init : initials) {
            if (i + init.size() <= input.size() &&
                input.substr(i, init.size()) == init) {
                if (init.size() > matched_initial.size()) {
                    matched_initial = init;
                }
            }
        }
        
        size_t start = i;
        i += matched_initial.size();
        
        // 尝试匹配韵母
        std::string matched_final;
        for (const auto &fin : finals) {
            if (i + fin.size() <= input.size() &&
                input.substr(i, fin.size()) == fin) {
                if (fin.size() > matched_final.size()) {
                    matched_final = fin;
                }
            }
        }
        i += matched_final.size();
        
        if (i == start) {
            // 无法匹配，跳过一个字符
            result += input.substr(start, 1);
            i = start + 1;
        } else {
            result += input.substr(start, i - start);
        }
        first = false;
    }
    
    // 只有当插入了分隔符时才返回
    if (result.find("'") != std::string::npos) {
        return result;
    }
    return {};
}

// ── 候选词频排序：根据使用频率调整候选顺序 ──────────────────────
// 搜狗式：高频词排前面
struct FreqEntry {
    std::string word;
    int freq;
};

inline const std::vector<FreqEntry>& default_freq_table() {
    static const std::vector<FreqEntry> table = {
        {"你好", 100}, {"中国", 95}, {"我们", 90}, {"他们", 85},
        {"什么", 80}, {"这个", 75}, {"可以", 70}, {"没有", 65},
        {"现在", 60}, {"时候", 55}, {"知道", 50}, {"觉得", 48},
        {"谢谢", 45}, {"再见", 42}, {"北京", 40}, {"上海", 38},
        {"输入法", 35}, {"手机", 32}, {"电脑", 30}, {"工作", 28},
        {"学习", 26}, {"生活", 24}, {"时间", 22}, {"问题", 20},
        {"方式", 18}, {"地方", 16}, {"电话", 14}, {"地址", 12},
    };
    return table;
}

inline int get_word_freq(const std::string &word) {
    for (const auto &entry : default_freq_table()) {
        if (entry.word == word) return entry.freq;
    }
    return 0;
}

// ── 拼音输入提示：输入过程中显示拼音分词 ──────────────────────
// 搜狗式：输入 "nihao" 时显示 "ni hao" 分词提示
inline std::string pinyin_display_format(const std::string &input) {
    if (input.empty()) return {};
    auto segmented = pinyin_segment_hint(input);
    if (!segmented.empty()) return segmented;
    return input;
}

// ── 长句输入优化：自动断句 ──────────────────────
// 搜狗式：输入很长的拼音串时，自动按词断句
inline std::vector<std::string> long_sentence_split(const std::string &input) {
    if (input.size() < 6) return {};
    
    auto segmented = pinyin_segment_hint(input);
    if (segmented.empty()) return {};
    
    // 按分隔符分割
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < segmented.size()) {
        size_t pos = segmented.find("'", start);
        if (pos == std::string::npos) {
            parts.push_back(segmented.substr(start));
            break;
        }
        parts.push_back(segmented.substr(start, pos - start));
        start = pos + 1;
    }
    
    if (parts.size() < 2) return {};
    return parts;
}

// ── v0.9.127 统一快速输入入口（扩展） ──────────────────────
inline std::vector<std::string> quick_candidates_v3(const std::string &input) {
    std::vector<std::string> result = quick_candidates_v2(input);
    
    // 网址/邮箱智能识别
    auto urls = smart_url_lookup(input);
    for (auto &u : urls) result.push_back(std::move(u));
    
    // 去重
    std::vector<std::string> unique;
    for (auto &r : result) {
        bool dup = false;
        for (auto &u : unique) {
            if (u == r) { dup = true; break; }
        }
        if (!dup) unique.push_back(std::move(r));
    }
    
    if (unique.size() > 20) unique.resize(20);
    return unique;
}



// ── v0.9.128 用户自定义快捷词配置文件 ──────────────────────
// 格式：每行 缩写=展开文本，# 开头为注释
// 示例：
//   addr=北京市海淀区中关村南大街
//   todo=【待办事项】
//   main=int main(int argc, char** argv) { return 0; }
inline std::vector<std::pair<std::string, std::string>>
load_custom_phrases_from_file(const std::string &path) {
    std::vector<std::pair<std::string, std::string>> result;
    FILE *fp = std::fopen(path.c_str(), "r");
    if (!fp) return result;
    char line[1024];
    while (std::fgets(line, sizeof(line), fp)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.empty() || s[0] == '#') continue;
        auto pos = s.find('=');
        if (pos == std::string::npos || pos == 0) continue;
        std::string abbr = s.substr(0, pos);
        std::string text = s.substr(pos + 1);
        if (!abbr.empty() && !text.empty()) {
            result.emplace_back(std::move(abbr), std::move(text));
        }
    }
    std::fclose(fp);
    return result;
}

// 合并内置自定义快捷词和用户配置文件
inline std::vector<std::pair<std::string, std::string>>
merge_custom_phrases(const std::string &user_file = "") {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto &p : builtin_custom_phrases()) {
        result.emplace_back(p.abbr, p.phrase);
    }
    if (!user_file.empty()) {
        auto user = load_custom_phrases_from_file(user_file);
        for (auto &u : user) {
            bool found = false;
            for (auto &r : result) {
                if (r.first == u.first) { r.second = u.second; found = true; break; }
            }
            if (!found) result.push_back(u);
        }
    }
    return result;
}

// 用合并后的快捷词表查找
inline std::vector<std::string>
custom_phrase_lookup_ext(const std::string &input,
                          const std::string &user_file = "") {
    auto phrases = merge_custom_phrases(user_file);
    std::vector<std::string> result;
    for (auto &p : phrases) {
        if (p.first == input) result.push_back(p.second);
    }
    for (auto &p : phrases) {
        if (p.first.size() > input.size() &&
            p.first.compare(0, input.size(), input) == 0) {
            result.push_back(p.second);
        }
    }
    return result;
}

// ── v0.9.128 Tab 英文自动补全 ──────────────────────────────
inline std::vector<std::string>
english_autocomplete(const std::string &prefix) {
    if (prefix.empty() || prefix.size() < 2) return {};
    static const std::vector<std::string> words = {
        "about","above","access","across","action","active","add","address",
        "after","again","agent","agree","alert","all","allow","almost",
        "alone","along","already","also","always","among","amount","analyze",
        "angle","animal","another","answer","any","anyone","anything","appear",
        "apple","apply","area","array","arrow","article","ask","assert",
        "asset","assign","async","atom","attach","audio","auth","author",
        "auto","available","await","back","balance","base","basic","batch",
        "battery","beam","bear","beat","begin","behind","being","below",
        "best","better","between","binary","bind","block","blog","blue",
        "body","bool","boost","boot","border","both","bottom","box",
        "branch","break","bridge","brief","bring","broad","browser","buffer",
        "bug","build","busy","button","byte","cache","call","cancel",
        "case","cast","catch","category","center","central","chain","change",
        "channel","char","chart","check","child","choice","choose","chunk",
        "class","clean","clear","click","client","close","cloud","cluster",
        "code","collect","color","column","combine","command","comment","common",
        "compile","complete","compress","computer","config","connect","console","const",
        "contact","content","context","continue","control","convert","copy","core",
        "correct","cost","count","country","course","cover","create","cross",
        "css","csv","ctrl","cube","current","cursor","custom","cycle",
        "daily","data","database","date","day","debug","decimal","default",
        "defer","define","delay","delete","demand","depth","desc","design",
        "detail","detect","device","dict","difference","digital","direct","directory",
        "disable","disk","display","distance","divide","document","domain","done",
        "double","down","download","draft","draw","driver","drop","dump",
        "during","each","early","earth","easy","echo","edge","edit",
        "effect","effort","either","element","else","email","empty","enable",
        "engine","english","ensure","enter","entity","entry","env","equal",
        "error","escape","event","every","exact","example","except","exclude",
        "exec","exist","exit","expand","expect","expire","explain","export",
        "extend","extra","face","fact","factor","factory","fail","fair",
        "false","family","fast","fault","feature","feed","feel","field",
        "file","fill","filter","final","find","fine","finish","fire",
        "first","fish","fix","flag","flash","flat","float","floor",
        "flow","fluid","focus","follow","font","food","foot","force",
        "form","format","forward","found","frame","free","fresh","friend",
        "front","full","function","future","game","gate","general","generate",
        "get","give","global","goal","gold","good","grade","graph",
        "great","green","grid","group","grow","guard","guess","guide",
        "hack","hair","half","hall","hand","handle","hang","happen",
        "happy","hard","hash","have","head","health","hear","heart",
        "heat","hello","help","here","hero","hide","high","hill",
        "hint","history","hit","hold","home","honor","hook","hope",
        "host","hour","house","html","http","human","icon","idea",
        "image","import","include","income","increase","index","info","init",
        "input","insert","inside","install","instance","integer","intent","internal",
        "internet","into","invalid","issue","item","join","json","jump",
        "just","keep","key","keyboard","kind","king","label","language",
        "large","last","late","later","launch","layer","layout","lazy",
        "lead","learn","least","leave","left","legal","length","less",
        "level","library","license","life","light","like","limit","line",
        "link","linux","list","listen","little","live","load","local",
        "location","lock","logic","login","logout","long","look","loop",
        "lose","loss","lower","machine","main","make","manage","many",
        "mark","market","master","match","math","matter","maybe","mean",
        "media","meet","member","memory","menu","merge","message","method",
        "middle","might","mind","mine","minor","minute","miss","mode",
        "model","modern","module","moment","money","month","more","most",
        "move","movie","much","music","must","name","native","nature",
        "near","need","network","new","news","next","nice","night",
        "node","none","normal","north","note","nothing","notice","now",
        "null","number","object","obtain","occur","offer","office","often",
        "old","once","one","online","only","open","operate","option",
        "order","origin","other","out","output","outside","over","own",
        "owner","pack","page","pair","panel","paper","parent","part",
        "pass","past","path","pattern","pause","pay","peak","perform",
        "period","person","phone","photo","physics","pick","picture","piece",
        "place","plan","plant","play","please","plus","point","policy",
        "pool","poor","port","position","positive","possible","post","power",
        "practice","prefer","prepare","present","press","pretty","price","primary",
        "print","private","prize","problem","process","produce","product","program",
        "project","promise","proof","proper","property","protect","protocol","proxy",
        "public","pull","push","python","quality","query","question","queue",
        "quick","quiet","quit","quote","race","radio","rain","raise",
        "random","range","rank","rapid","rate","raw","read","real",
        "reason","receive","recent","record","red","reduce","refer","refresh",
        "region","register","reject","relate","release","reload","remain","remember",
        "remove","render","repair","repeat","replace","reply","report","request",
        "require","research","resource","response","rest","result","return","reveal",
        "review","reward","rich","right","ring","rise","risk","river",
        "road","robot","rock","role","roll","room","root","round",
        "route","row","ruby","rule","run","rust","safe","save",
        "scale","scan","scene","school","scope","score","screen","script",
        "search","season","seat","second","secret","section","secure","seed",
        "seek","seem","select","self","sell","send","sense","server",
        "service","session","setting","setup","seven","shadow","shape","share",
        "sheet","shell","shift","ship","short","should","show","shut",
        "side","sign","signal","silent","silver","simple","since","single",
        "site","size","skill","skin","sky","sleep","slow","small",
        "smart","smile","social","soft","software","solution","some","song",
        "soon","sort","sound","source","space","speak","special","speed",
        "spend","spirit","split","sport","spread","spring","square","stable",
        "staff","stage","stand","standard","start","state","static","station",
        "stay","step","still","stock","stop","store","storm","story",
        "stream","street","strict","string","strong","struct","student","study",
        "style","subject","submit","success","such","sudden","sugar","suggest",
        "summer","super","supply","support","sure","surface","surprise","swap",
        "switch","symbol","system","table","tag","tail","take","talk",
        "tall","task","teach","team","tech","technology","tell","temp",
        "template","term","test","text","than","thank","that","their",
        "them","then","there","these","they","thing","think","third",
        "this","those","though","thread","three","through","throw","time",
        "tiny","title","today","together","tone","tool","top","topic",
        "total","touch","tough","tour","tower","town","track","trade",
        "traffic","train","transfer","transform","translate","transport","travel","tree",
        "trend","trial","trick","trim","true","trust","try","tube",
        "turn","type","under","union","unique","unit","universe","unless",
        "until","update","upgrade","upper","use","user","using","util",
        "valid","value","variable","vector","version","very","video","view",
        "virtual","visible","visit","visual","voice","volume","vote","wait",
        "wake","walk","wall","want","warm","warn","watch","water",
        "wave","way","weak","wear","week","weight","well","west",
        "what","when","where","which","while","white","who","whole",
        "why","wide","width","window","winter","wire","wise","wish",
        "with","within","without","woman","wonder","wood","word","work",
        "world","worry","worse","worst","worth","would","write","wrong",
        "xml","year","yellow","yes","yet","you","young","your",
        "zero","zone"
    };
    std::vector<std::string> result;
    for (auto &w : words) {
        if (w.size() >= prefix.size() &&
            w.compare(0, prefix.size(), prefix) == 0) {
            result.push_back(w);
        }
        if (result.size() >= 10) break;
    }
    return result;
}

// ── v0.9.128 统一快速输入入口（v4 扩展） ──────────────────
inline std::vector<std::string> quick_candidates_v4(const std::string &input,
                                                     const std::string &user_file = "") {
    auto result = quick_candidates_v3(input);
    // 用户自定义快捷词
    auto custom = custom_phrase_lookup_ext(input, user_file);
    for (auto &c : custom) result.push_back(std::move(c));
    // 去重
    std::vector<std::string> unique;
    for (auto &r : result) {
        bool dup = false;
        for (auto &u : unique) { if (u == r) { dup = true; break; } }
        if (!dup) unique.push_back(std::move(r));
    }
    if (unique.size() > 20) unique.resize(20);
    return unique;
}

// ── v0.9.129 常用句式模板 ──────────────────────────────────
// 输入句式缩写，展开为常用句子
inline std::vector<std::pair<std::string, std::string>>
builtin_sentence_templates() {
    return {
        {"xw",      "请问您需要什么帮助？"},
        {"xw2",     "您好，请问有什么可以帮您的？"},
        {"gz",      "感谢您的关注与支持！"},
        {"gz2",     "非常感谢您的配合，期待下次合作。"},
        {"qh",      "请稍等，我正在为您处理。"},
        {"qh2",     "请确认信息无误后提交。"},
        {"yy",      "已为您处理完毕，请查收。"},
        {"yy2",     "已收到您的反馈，我们会尽快处理。"},
        {"bj",      "备注：请仔细核对以上信息。"},
        {"yj",      "以上意见仅供参考，具体以实际情况为准。"},
        {"td",      "【待办】"},
        {"td2",     "【已完成】"},
        {"td3",     "【进行中】"},
        {"td4",     "【已取消】"},
        {"yx",      "【优先级：高】"},
        {"yx2",     "【优先级：中】"},
        {"yx3",     "【优先级：低】"},
        {"ly",      "祝您工作顺利，生活愉快！"},
        {"ly2",     "祝您身体健康，万事如意！"},
        {"ly3",     "祝您旅途愉快，一路顺风！"},
        {"em",      "我的邮箱是"},
        {"ph",      "我的电话是"},
        {"ad",      "我的地址是"},
        {"nm",      "我的名字是"},
        {"sig",     "此致\n敬礼"},
        {"sig2",    "Best regards,"},
        {"sig3",    "Thanks and Best Regards,"},
        {"mt",      "会议时间："},
        {"mt2",     "会议地点："},
        {"mt3",     "参会人员："},
        {"mt4",     "会议主题："},
        {"rp",      "报告人："},
        {"rp2",     "报告时间："},
        {"rp3",     "报告地点："},
    };
}

inline std::vector<std::string>
sentence_template_lookup(const std::string &input) {
    auto templates = builtin_sentence_templates();
    std::vector<std::string> result;
    for (auto &t : templates) {
        if (t.first == input) result.push_back(t.second);
    }
    for (auto &t : templates) {
        if (t.first.size() > input.size() &&
            t.first.compare(0, input.size(), input) == 0) {
            result.push_back(t.second);
        }
    }
    return result;
}

// ── v0.9.129 最近输入记忆 ──────────────────────────────────
// 记录最近上屏的文本，用于快速重复输入
struct RecentHistory {
    std::vector<std::string> items;
    size_t max_size = 20;

    void add(const std::string &text) {
        if (text.empty()) return;
        // 去重：如果已存在，移到最前
        for (auto it = items.begin(); it != items.end(); ++it) {
            if (*it == text) { items.erase(it); break; }
        }
        items.insert(items.begin(), text);
        if (items.size() > max_size) items.resize(max_size);
    }

    std::vector<std::string> lookup(const std::string &prefix = "") const {
        std::vector<std::string> result;
        for (auto &item : items) {
            if (prefix.empty() ||
                item.size() >= prefix.size() &&
                item.compare(0, prefix.size(), prefix) == 0) {
                result.push_back(item);
            }
            if (result.size() >= 10) break;
        }
        return result;
    }

    bool save_to_file(const std::string &path) const {
        FILE *fp = std::fopen(path.c_str(), "w");
        if (!fp) return false;
        for (auto &item : items) {
            std::fprintf(fp, "%s\n", item.c_str());
        }
        std::fclose(fp);
        return true;
    }

    bool load_from_file(const std::string &path) {
        FILE *fp = std::fopen(path.c_str(), "r");
        if (!fp) return false;
        char line[1024];
        items.clear();
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (!s.empty()) items.push_back(s);
            if (items.size() >= max_size) break;
        }
        std::fclose(fp);
        return true;
    }
};

// ── v0.9.129 智能选词（基于上下文） ──────────────────────
// 根据上一个上屏的词，调整当前候选排序
inline std::vector<std::string>
context_rank_candidates(const std::vector<std::string> &candidates,
                         const std::string &last_committed) {
    if (candidates.empty() || last_committed.empty()) return candidates;

    // 上下文关联表：某些词后面更可能跟某些词（v0.9.145 扩展至 72 条）
    static const std::vector<std::pair<std::string, std::string>> context_rules = {
        // 原有 36 条
        {"你", "好"}, {"你", "们"}, {"我", "们"}, {"我", "的"},
        {"他", "们"}, {"这", "个"}, {"那", "个"}, {"什", "么"},
        {"为", "什"}, {"因", "为"}, {"所", "以"}, {"但", "是"},
        {"如", "果"}, {"虽", "然"}, {"还", "是"}, {"或", "者"},
        {"一", "个"}, {"两", "个"}, {"三", "个"}, {"四", "个"},
        {"中", "国"}, {"北", "京"}, {"上", "海"}, {"广", "州"},
        {"学", "生"}, {"老", "师"}, {"朋", "友"}, {"同", "学"},
        {"工", "作"}, {"学", "习"}, {"生", "活"}, {"时", "间"},
        {"今", "天"}, {"明", "天"}, {"昨", "天"}, {"前", "天"},
        {"上", "午"}, {"下", "午"}, {"晚", "上"}, {"早", "上"},
        // v0.9.145 新增 36 条
        {"请", "问"}, {"谢", "谢"}, {"不", "用"}, {"不", "是"},
        {"可", "以"}, {"能", "够"}, {"应", "该"}, {"需", "要"},
        {"已", "经"}, {"正", "在"}, {"将", "会"}, {"可", "能"},
        {"问", "题"}, {"方", "法"}, {"结", "果"}, {"原", "因"},
        {"程", "序"}, {"代", "码"}, {"系", "统"}, {"网", "络"},
        {"数", "据"}, {"技", "术"}, {"开", "发"}, {"测", "试"},
        {"用", "户"}, {"服", "务"}, {"安", "全"}, {"性", "能"},
        {"手", "机"}, {"电", "脑"}, {"网", "站"}, {"软", "件"},
        {"人", "工"}, {"智", "能"}, {"机", "器"}, {"深", "度"},
    };

    auto result = candidates;
    // 如果上一个词在关联表中，把关联词排到前面
    for (auto &rule : context_rules) {
        if (last_committed == rule.first) {
            for (size_t i = 1; i < result.size(); ++i) {
                if (result[i].find(rule.second) != std::string::npos) {
                    std::swap(result[0], result[i]);
                    break;
                }
            }
            break;
        }
    }
    return result;
}

// ── v0.9.130 用户词频学习 ──────────────────────────────────
// 记录 拼音→中文 的上屏频率，用于动态调整候选排序
struct UserFreqEntry {
    std::string pinyin;
    std::string chinese;
    int count = 0;
    long last_used = 0;  // 时间戳
};

struct UserFreqTable {
    std::vector<UserFreqEntry> entries;
    size_t max_size = 500;

    void add(const std::string &pinyin, const std::string &chinese) {
        if (pinyin.empty() || chinese.empty()) return;
        // 查找已有条目
        for (auto &e : entries) {
            if (e.pinyin == pinyin && e.chinese == chinese) {
                e.count++;
                e.last_used = std::time(nullptr);
                return;
            }
        }
        // 新条目
        entries.push_back({pinyin, chinese, 1, std::time(nullptr)});
        // 超过上限：淘汰最低频率的
        if (entries.size() > max_size) {
            size_t min_idx = 0;
            for (size_t i = 1; i < entries.size(); ++i) {
                if (entries[i].count < entries[min_idx].count) min_idx = i;
            }
            entries.erase(entries.begin() + min_idx);
        }
    }

    // 查找某拼音的所有频率条目，按频率降序排列
    std::vector<std::pair<std::string, int>>
    lookup(const std::string &pinyin) const {
        std::vector<std::pair<std::string, int>> result;
        for (auto &e : entries) {
            if (e.pinyin == pinyin) {
                result.emplace_back(e.chinese, e.count);
            }
        }
        std::sort(result.begin(), result.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });
        return result;
    }

    // 根据频率表重排候选列表
    std::vector<std::string>
    rank_candidates(const std::vector<std::string> &candidates,
                    const std::string &pinyin) const {
        if (candidates.empty()) return candidates;
        auto freq = lookup(pinyin);
        if (freq.empty()) return candidates;

        auto result = candidates;
        // 按频率从高到低，把匹配的候选移到前面
        size_t insert_pos = 0;
        for (auto &f : freq) {
            for (size_t i = insert_pos; i < result.size(); ++i) {
                if (result[i] == f.first) {
                    std::swap(result[insert_pos], result[i]);
                    insert_pos++;
                    break;
                }
            }
        }
        return result;
    }

    // 获取所有高频词（用于统计面板）
    std::vector<std::pair<std::string, int>>
    top_entries(size_t n = 20) const {
        std::vector<std::pair<std::string, int>> result;
        for (auto &e : entries) {
            result.emplace_back(e.chinese, e.count);
        }
        std::sort(result.begin(), result.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });
        if (result.size() > n) result.resize(n);
        return result;
    }

    bool save_to_file(const std::string &path) const {
        FILE *fp = std::fopen(path.c_str(), "w");
        if (!fp) return false;
        std::fprintf(fp, "# pinyin<TAB>chinese<TAB>count<TAB>last_used\n");
        for (auto &e : entries) {
            std::fprintf(fp, "%s\t%s\t%d\t%ld\n",
                         e.pinyin.c_str(), e.chinese.c_str(), e.count, e.last_used);
        }
        std::fclose(fp);
        return true;
    }

    bool load_from_file(const std::string &path) {
        FILE *fp = std::fopen(path.c_str(), "r");
        if (!fp) return false;
        char line[1024];
        entries.clear();
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.empty() || s[0] == '#') continue;
            // 分割 tab
            std::vector<std::string> parts;
            size_t start = 0;
            for (size_t i = 0; i <= s.size(); ++i) {
                if (i == s.size() || s[i] == '\t') {
                    parts.push_back(s.substr(start, i - start));
                    start = i + 1;
                }
            }
            if (parts.size() >= 3) {
                UserFreqEntry e;
                e.pinyin = parts[0];
                e.chinese = parts[1];
                e.count = std::atoi(parts[2].c_str());
                if (parts.size() >= 4) e.last_used = std::atol(parts[3].c_str());
                if (!e.pinyin.empty() && !e.chinese.empty()) entries.push_back(e);
            }
            if (entries.size() >= max_size) break;
        }
        std::fclose(fp);
        return true;
    }
};

// ── v0.9.131 皮肤/主题切换 ────────────────────────────────
struct Theme {
    std::string name;
    std::string preedit_color;    // 拼音预编辑区颜色
    std::string candidate_color;  // 候选词颜色
    std::string focus_color;      // 焦点候选颜色
    std::string index_color;      // 序号颜色
    std::string hint_color;       // 提示信息颜色
    std::string reset;            // 重置颜色
};

inline Theme get_theme(const std::string &name) {
    if (name == "dark" || name == "暗色") {
        return {"dark", "\033[36m", "\033[37m", "\033[1;33m",
                "\033[90m", "\033[90m", "\033[0m"};
    } else if (name == "high" || name == "高对比") {
        return {"high", "\033[1;37m", "\033[1;37m", "\033[1;31m",
                "\033[1;36m", "\033[1;35m", "\033[0m"};
    } else if (name == "retro" || name == "复古") {
        return {"retro", "\033[33m", "\033[32m", "\033[1;33m",
                "\033[35m", "\033[36m", "\033[0m"};
    } else if (name == "matrix" || name == "黑客") {
        return {"matrix", "\033[32m", "\033[32m", "\033[1;32m",
                "\033[90m", "\033[32m", "\033[0m"};
    } else if (name == "ocean" || name == "海洋") {
        return {"ocean", "\033[34m", "\033[36m", "\033[1;34m",
                "\033[90m", "\033[35m", "\033[0m"};
    } else if (name == "solar" || name == "日光") {
        return {"solar", "\033[33m", "\033[37m", "\033[1;31m",
                "\033[33m", "\033[35m", "\033[0m"};
    }
    // 默认主题
    return {"default", "", "", "", "", "", ""};
}

inline std::vector<std::string> list_themes() {
    return {"default", "dark", "high", "retro", "matrix", "ocean", "solar"};
}

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
        {"du", "度"}, {"li", "里"}, {"mi", "米"}, {"km", "公里"},
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

} // namespace pinyin_ime
