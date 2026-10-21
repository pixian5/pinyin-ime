// quick_input_test.cpp — 测试搜狗式快速输入功能
#include "quick_input.h"
#include "emoji_list.h"
#include <iostream>
#include <cassert>

int main() {
    using namespace pinyin_ime;
    int pass = 0, fail = 0;

    auto check = [&](const std::string& name, bool cond) {
        if (cond) { ++pass; std::cout << "✅ " << name << "\n"; }
        else      { ++fail; std::cout << "❌ " << name << "\n"; }
    };

    // v 模式符号
    auto [syms1, label1] = v_mode_lookup("1");
    check("v1 圈码: 20 symbols", syms1.size() == 20);
    check("v1 label=圈码", label1 == "圈码");
    check("v1[0]=①", syms1[0] == "①");
    check("v1[9]=⑩", syms1[9] == "⑩");

    auto [syms2, label2] = v_mode_lookup("2");
    check("v2 星号: 20 symbols", syms2.size() == 20);
    check("v2[0]=★", syms2[0] == "★");

    auto [syms3, label3] = v_mode_lookup("3");
    check("v3 箭头: 20 symbols", syms3.size() == 20);
    check("v3[0]=→", syms3[0] == "→");

    auto [syms5, label5] = v_mode_lookup("5");
    check("v5 希腊: 30 symbols", syms5.size() == 30);
    check("v5[0]=α", syms5[0] == "α");

    auto [syms8, label8] = v_mode_lookup("8");
    check("v8 括号: 「", syms8[0] == "「");

    // v 模式检测
    check("is_v_mode(v1)", is_v_mode("v1"));
    check("!is_v_mode(hello)", !is_v_mode("hello"));
    check("!is_v_mode(v)", !is_v_mode("v"));

    // 日期/时间
    std::string date = current_date();
    check("date contains 年", date.find("年") != std::string::npos);
    check("date contains 月", date.find("月") != std::string::npos);
    check("date contains 日", date.find("日") != std::string::npos);
    std::cout << "  current_date() = " << date << "\n";

    std::string time = current_time();
    check("time contains :", time.find(":") != std::string::npos);
    std::cout << "  current_time() = " << time << "\n";

    std::string week = current_week();
    check("week starts with 星期", week.substr(0, 6) == "星期");
    std::cout << "  current_week() = " << week << "\n";

    check("datetime_lookup(date) not empty", !datetime_lookup("date").empty());
    check("datetime_lookup(rq) not empty", !datetime_lookup("rq").empty());
    check("datetime_lookup(time) not empty", !datetime_lookup("time").empty());
    check("datetime_lookup(sj) not empty", !datetime_lookup("sj").empty());
    check("datetime_lookup(week) not empty", !datetime_lookup("week").empty());
    check("datetime_lookup(xq) not empty", !datetime_lookup("xq").empty());
    check("datetime_lookup(hello) empty", datetime_lookup("hello").empty());

    // 计算器
    check("calc: 1+2=3", calculator_eval("1+2") == "3");
    check("calc: 10-3=7", calculator_eval("10-3") == "7");
    check("calc: 4*5=20", calculator_eval("4*5") == "20");
    check("calc: 20/4=5", calculator_eval("20/4") == "5");
    check("calc: 2^10=1024", calculator_eval("2^10") == "1024");
    check("calc: (1+2)*3=9", calculator_eval("(1+2)*3") == "9");
    check("calc: 3.5+1.5=5", calculator_eval("3.5+1.5") == "5");
    check("calc: 100/7 not empty", !calculator_eval("100/7").empty());
    std::cout << "  100/7 = " << calculator_eval("100/7") << "\n";
    check("calc: 10%3=1", calculator_eval("10%3") == "1");
    check("calc: -5+10=5", calculator_eval("-5+10") == "5");

    check("is_calc(1+2)", is_calculator_input("1+2"));
    check("!is_calc(123)", !is_calculator_input("123"));
    check("!is_calc(hello)", !is_calculator_input("hello"));
    check("is_calc(3.14*2)", is_calculator_input("3.14*2"));

    // 数字转中文
    check("num 0 → 零", num_to_chinese_lower(0) == "零");
    check("num 1 → 一", num_to_chinese_lower(1) == "一");
    check("num 10 → 十", num_to_chinese_lower(10) == "十");
    check("num 100 → 一百", num_to_chinese_lower(100) == "一百");
    check("num 123 → 一百二十三", num_to_chinese_lower(123) == "一百二十三");
    check("num 1000 → 一千", num_to_chinese_lower(1000) == "一千");
    std::cout << "  12345 → " << num_to_chinese_lower(12345) << "\n";
    std::cout << "  12345 upper → " << num_to_chinese_upper(12345) << "\n";

    auto nc = number_candidates("123");
    check("number_candidates(123) has 2", nc.size() >= 1);
    check("number_candidates(123)[0] contains 百", nc[0].find("百") != std::string::npos);

    // 表情列表
    auto &emojis = emoji_list();
    check("emoji list > 90", emojis.size() > 90);
    std::cout << "  emoji count = " << emojis.size() << "\n";

    std::cout << "\n=== Results: " << pass << " passed, " << fail << " failed ===\n";
    return fail > 0 ? 1 : 0;
}
