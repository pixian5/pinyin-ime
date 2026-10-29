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
    for (const auto &w : common_english_words()) {
        if (w == input) { result.push_back(w); break; }
    }
    if (result.empty()) {
        for (const auto &w : common_english_words()) {
            if (w.size() > input.size() && w.substr(0, input.size()) == input) {
                result.push_back(w);
                if (result.size() >= 5) break;
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
        {"nihaoa",   "nihao"},
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


} // namespace pinyin_ime
