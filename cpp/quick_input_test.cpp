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

    // v0.9.123 扩展 v 模式符号组
    auto [symsa, labela] = v_mode_lookup("a");
    check("va 拼音: 30 symbols", symsa.size() == 30);
    check("va label=拼音", labela == "拼音");
    check("va[0]=ā", symsa[0] == "ā");

    auto [symsb, labelb] = v_mode_lookup("b");
    check("vb 方括: 30 symbols", symsb.size() == 30);
    check("vb[0]=【", symsb[0] == "【");

    auto [symsc, labelc] = v_mode_lookup("c");
    check("vc 棋牌: 30 symbols", symsc.size() == 30);
    check("vc[0]=♠", symsc[0] == "♠");

    auto [symse, labele] = v_mode_lookup("e");
    check("ve 自然: 30 symbols", symse.size() == 30);

    auto [symsy, labely] = v_mode_lookup("y");
    check("vy 生肖: 30 symbols", symsy.size() == 30);
    check("vy[0]=鼠", symsy[0] == "鼠");
    check("vy[11]=猪", symsy[11] == "猪");

    auto [symsz, labelz] = v_mode_lookup("z");
    check("vz 杂项: 30 symbols", symsz.size() == 30);

    // 统计符号组总数
    auto &all_groups = symbol_groups();
    std::cout << "  symbol groups = " << all_groups.size() << "\n";
    check("symbol groups >= 29 (11+18)", all_groups.size() >= 29);

    // v0.9.123 货币转换
    auto cur1 = currency_candidates("123rmb");
    check("currency 123rmb has results", !cur1.empty());
    check("currency 123rmb[0] contains 元", cur1[0].find("元") != std::string::npos);
    std::cout << "  123rmb → " << cur1[0] << " / " << (cur1.size()>1?cur1[1]:"") << "\n";

    auto cur2 = currency_candidates("99$");
    check("currency 99$ has results", !cur2.empty());
    check("currency 99$[0] starts with $", cur2[0].substr(0,1) == "$");
    std::cout << "  99$ → " << cur2[0] << "\n";

    auto cur3 = currency_candidates("100usd");
    check("currency 100usd has results", !cur3.empty());
    std::cout << "  100usd → " << cur3[0];
    if (cur3.size() > 1) std::cout << " / " << cur3[1];
    std::cout << "\n";

    auto cur4 = currency_candidates("50eur");
    check("currency 50eur has results", !cur4.empty());
    check("is_currency(123rmb)", is_currency_input("123rmb"));
    check("!is_currency(hello)", !is_currency_input("hello"));

    // v0.9.123 单位换算
    auto u1 = unit_conversion("1km");
    check("unit 1km has results", !u1.empty());
    check("unit 1km contains 千米", u1[0].find("千米") != std::string::npos);
    std::cout << "  1km → " << u1[0] << "\n";

    auto u2 = unit_conversion("1kg");
    check("unit 1kg has results", !u2.empty());
    std::cout << "  1kg → " << u2[0] << "\n";

    auto u3 = unit_conversion("1mile");
    check("unit 1mile has results", !u3.empty());
    std::cout << "  1mile → " << u3[0] << "\n";

    auto u4 = unit_conversion("100C");
    check("unit 100C has results", !u4.empty());
    std::cout << "  100C → " << u4[0] << "\n";

    auto u5 = unit_conversion("100F");
    check("unit 100F has results", !u5.empty());
    std::cout << "  100F → " << u5[0] << "\n";
    check("is_unit(1km)", is_unit_conversion("1km"));
    check("!is_unit(hello)", !is_unit_conversion("hello"));

    // v0.9.123 百分比转中文
    std::string pct1 = percent_to_chinese("50p");
    check("percent 50p = 百分之五十", pct1 == "百分之五十");
    std::cout << "  50p → " << pct1 << "\n";

    std::string pct2 = percent_to_chinese("100p");
    check("percent 100p = 百分之一百", pct2 == "百分之一百");

    std::string pct3 = percent_to_chinese("12p");
    check("percent 12p = 百分之十二", pct3 == "百分之十二");
    check("is_percent(50p)", is_percent_input("50p"));
    check("!is_percent(hello)", !is_percent_input("hello"));

    // v0.9.123 统一快速输入入口
    auto qc1 = quick_candidates("date");
    check("quick(date) not empty", !qc1.empty());

    auto qc2 = quick_candidates("123rmb");
    check("quick(123rmb) not empty", !qc2.empty());

    auto qc3 = quick_candidates("1km");
    check("quick(1km) not empty", !qc3.empty());

    auto qc4 = quick_candidates("50p");
    check("quick(50p) not empty", !qc4.empty());

    auto qc5 = quick_candidates("1+2");
    check("quick(1+2) = 1+2=3", qc5.size() > 0 && qc5[0] == "1+2=3");

    auto qc6 = quick_candidates("123");
    check("quick(123) has chinese", !qc6.empty() && qc6[0].find("百") != std::string::npos);

    auto qc7 = quick_candidates("hello");
    check("quick(hello) empty", qc7.empty());

    // v0.9.123 扩展日期/时间
    check("datetime(year) not empty", !datetime_lookup("year").empty());
    check("datetime(yn) not empty", !datetime_lookup("yn").empty());
    check("datetime(month) not empty", !datetime_lookup("month").empty());
    check("datetime(ts) not empty", !datetime_lookup("ts").empty());
    check("datetime(hour) not empty", !datetime_lookup("hour").empty());
    check("datetime(noon) not empty", !datetime_lookup("noon").empty());
    std::cout << "  year → " << datetime_lookup("year") << "\n";
    std::cout << "  ts → " << datetime_lookup("ts") << "\n";
    std::cout << "  noon → " << datetime_lookup("noon") << "\n";


    // v0.9.124 i模式快捷短语测试
    std::cout << "\n--- i模式快捷短语 ---\n";
    check("is_i_mode(ih)", is_i_mode("ih"));
    check("is_i_mode(ix)", is_i_mode("ix"));
    check("is_i_mode(izw)", is_i_mode("izw"));
    check("!is_i_mode(hello)", !is_i_mode("hello"));
    check("!is_i_mode(i)", !is_i_mode("i"));
    check("!is_i_mode(i1)", !is_i_mode("i1"));

    auto ip1 = i_mode_lookup("h");
    check("i_mode(h) has 你好", !ip1.empty() && ip1[0] == "你好");
    std::cout << "  ih → " << (ip1.empty() ? "(empty)" : ip1[0]) << "\n";

    auto ip2 = i_mode_lookup("x");
    check("i_mode(x) has 谢谢", !ip2.empty() && ip2[0] == "谢谢");
    std::cout << "  ix → " << (ip2.empty() ? "(empty)" : ip2[0]) << "\n";

    auto ip3 = i_mode_lookup("z");
    check("i_mode(z) has 再见", !ip3.empty() && ip3[0] == "再见");
    std::cout << "  iz → " << (ip3.empty() ? "(empty)" : ip3[0]) << "\n";

    auto ip4 = i_mode_lookup("zw");
    check("i_mode(zw) has 早上好", !ip4.empty() && ip4[0] == "早上好");
    std::cout << "  izw → " << (ip4.empty() ? "(empty)" : ip4[0]) << "\n";

    auto ip5 = i_mode_lookup("ok");
    check("i_mode(ok) has 好的", !ip5.empty() && ip5[0] == "好的");
    std::cout << "  iok → " << (ip5.empty() ? "(empty)" : ip5[0]) << "\n";

    auto ip6 = i_mode_lookup("mt");
    check("i_mode(mt) not empty", !ip6.empty());
    std::cout << "  imt → " << (ip6.empty() ? "(empty)" : ip6[0]) << "\n";

    auto ip7 = i_mode_lookup("ha");
    check("i_mode(ha) has 哈哈", !ip7.empty() && ip7[0] == "哈哈");
    std::cout << "  iha → " << (ip7.empty() ? "(empty)" : ip7[0]) << "\n";

    // v0.9.124 联想词测试
    std::cout << "\n--- 联想词 ---\n";
    auto ap1 = association_lookup("你好");
    check("assoc(你好) not empty", !ap1.empty());
    check("assoc(你好) has 啊", !ap1.empty() && ap1[0] == "啊");
    std::cout << "  你好 → " << (ap1.empty() ? "(empty)" : ap1[0]) << " " << (ap1.size() > 1 ? ap1[1] : "") << "\n";

    auto ap2 = association_lookup("我");
    check("assoc(我) not empty", !ap2.empty());
    check("assoc(我) has 的", !ap2.empty() && ap2[0] == "的");
    std::cout << "  我 → " << (ap2.empty() ? "(empty)" : ap2[0]) << " " << (ap2.size() > 1 ? ap2[1] : "") << "\n";

    auto ap3 = association_lookup("谢谢");
    check("assoc(谢谢) not empty", !ap3.empty());
    std::cout << "  谢谢 → " << (ap3.empty() ? "(empty)" : ap3[0]) << " " << (ap3.size() > 1 ? ap3[1] : "") << "\n";

    auto ap4 = association_lookup("想");
    check("assoc(想) not empty", !ap4.empty());
    check("assoc(想) has 要", !ap4.empty() && ap4[0] == "要");
    std::cout << "  想 → " << (ap4.empty() ? "(empty)" : ap4[0]) << " " << (ap4.size() > 1 ? ap4[1] : "") << "\n";

    auto ap5 = association_lookup("今天");
    check("assoc(今天) not empty", !ap5.empty());
    std::cout << "  今天 → " << (ap5.empty() ? "(empty)" : ap5[0]) << " " << (ap5.size() > 1 ? ap5[1] : "") << "\n";

    auto ap6 = association_lookup("");
    check("assoc(empty) empty", ap6.empty());

    auto ap7 = association_lookup("怎么");
    check("assoc(怎么) not empty", !ap7.empty());
    std::cout << "  怎么 → " << (ap7.empty() ? "(empty)" : ap7[0]) << " " << (ap7.size() > 1 ? ap7[1] : "") << "\n";

    std::cout << "\n=== Results: " << pass << " passed, " << fail << " failed ===\n";
    return fail > 0 ? 1 : 0;
}
