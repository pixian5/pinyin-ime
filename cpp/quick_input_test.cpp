// quick_input_test.cpp — 测试搜狗式快速输入功能
#include "quick_input.h"
#include "../fcitx5-addon/wayland_integration.h"
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


    // v0.9.126 自定义快捷词测试
    std::cout << "\n--- 自定义快捷词 ---\n";
    auto cp1 = custom_phrase_lookup("addr");
    check("custom(addr) not empty", !cp1.empty());
    check("custom(addr) has 地址", !cp1.empty() && cp1[0].find("北京") != std::string::npos);
    std::cout << "  addr → " << (cp1.empty() ? "(empty)" : cp1[0]) << "\n";

    auto cp2 = custom_phrase_lookup("todo");
    check("custom(todo) not empty", !cp2.empty());
    check("custom(todo) = 【待办】", !cp2.empty() && cp2[0] == "【待办】");
    std::cout << "  todo → " << (cp2.empty() ? "(empty)" : cp2[0]) << "\n";

    auto cp3 = custom_phrase_lookup("main");
    check("custom(main) not empty", !cp3.empty());
    check("custom(main) has int main", !cp3.empty() && cp3[0].find("int main") != std::string::npos);
    std::cout << "  main → " << (cp3.empty() ? "(empty)" : cp3[0]) << "\n";

    auto cp4 = custom_phrase_lookup("btw");
    check("custom(btw) not empty", !cp4.empty());
    std::cout << "  btw → " << (cp4.empty() ? "(empty)" : cp4[0]) << "\n";

    auto cp5 = custom_phrase_lookup("xyz");
    check("custom(xyz) empty", cp5.empty());

    check("is_custom_phrase(todo)", is_custom_phrase("todo"));
    check("!is_custom_phrase(xyz)", !is_custom_phrase("xyz"));

    // v0.9.126 中英文混输测试
    std::cout << "\n--- 中英文混输 ---\n";
    auto mi1 = mixed_input_lookup("hello");
    check("mixed(hello) not empty", !mi1.empty());
    check("mixed(hello) = hello", !mi1.empty() && mi1[0] == "hello");
    std::cout << "  hello → " << (mi1.empty() ? "(empty)" : mi1[0]) << "\n";

    auto mi2 = mixed_input_lookup("test");
    check("mixed(test) not empty", !mi2.empty());
    check("mixed(test) = test", !mi2.empty() && mi2[0] == "test");
    std::cout << "  test → " << (mi2.empty() ? "(empty)" : mi2[0]) << "\n";

    auto mi3 = mixed_input_lookup("py");  // 前缀匹配 python
    check("mixed(py) has python", !mi3.empty() && mi3[0] == "python");
    std::cout << "  py → " << (mi3.empty() ? "(empty)" : mi3[0]) << "\n";

    auto mi4 = mixed_input_lookup("xyz");
    check("mixed(xyz) empty", mi4.empty());

    auto mi5 = mixed_input_lookup("json");
    check("mixed(json) = json", !mi5.empty() && mi5[0] == "json");
    std::cout << "  json → " << (mi5.empty() ? "(empty)" : mi5[0]) << "\n";

    check("is_mixed_english(hello)", is_mixed_english("hello"));
    check("!is_mixed_english(123)", !is_mixed_english("123"));

    // v0.9.126 错音纠正测试
    std::cout << "\n--- 错音纠正 ---\n";
    auto tc1 = typo_correction_lookup("nihoa");
    check("typo(nihoa) not empty", !tc1.empty());
    check("typo(nihoa) = nihao", !tc1.empty() && tc1[0] == "nihao");
    std::cout << "  nihoa → " << (tc1.empty() ? "(empty)" : tc1[0]) << "\n";

    auto tc2 = typo_correction_lookup("zhonggu");
    check("typo(zhonggu) = zhongguo", !tc2.empty() && tc2[0] == "zhongguo");
    std::cout << "  zhonggu → " << (tc2.empty() ? "(empty)" : tc2[0]) << "\n";

    auto tc3 = typo_correction_lookup("inaho");
    check("typo(inaho) not empty", !tc3.empty());
    std::cout << "  inaho → " << (tc3.empty() ? "(empty)" : tc3[0]) << "\n";

    auto tc4 = typo_correction_lookup("nihao");
    check("typo(nihao) empty (already correct)", tc4.empty());

    // v0.9.126 成语速查测试
    std::cout << "\n--- 成语速查 ---\n";
    auto id1 = idiom_lookup("yffs");
    check("idiom(yffs) has 一帆风顺", !id1.empty() && id1[0] == "一帆风顺");
    std::cout << "  yffs → " << (id1.empty() ? "(empty)" : id1[0]) << "\n";

    auto id2 = idiom_lookup("mdcg");
    check("idiom(mdcg) has 马到成功", !id2.empty() && id2[0] == "马到成功");
    std::cout << "  mdcg → " << (id2.empty() ? "(empty)" : id2[0]) << "\n";

    auto id3 = idiom_lookup("hktt");
    check("idiom(hktt) has 海阔天空", !id3.empty() && id3[0] == "海阔天空");
    std::cout << "  hktt → " << (id3.empty() ? "(empty)" : id3[0]) << "\n";

    auto id4 = idiom_lookup("xxsc");
    check("idiom(xxsc) has 心想事成", !id4.empty() && id4[0] == "心想事成");
    std::cout << "  xxsc → " << (id4.empty() ? "(empty)" : id4[0]) << "\n";

    auto id5 = idiom_lookup("zzz");
    check("idiom(zzz) empty", id5.empty());

    check("is_idiom_input(yffs)", is_idiom_input("yffs"));

    // v0.9.126 繁简转换测试
    std::cout << "\n--- 繁简转换 ---\n";
    std::string trad1 = to_traditional("中国");
    check("trad(中国) contains 國", trad1.find("國") != std::string::npos);
    std::cout << "  中国 → " << trad1 << "\n";

    std::string trad2 = to_traditional("学习");
    check("trad(学习) contains 學", trad2.find("學") != std::string::npos);
    std::cout << "  学习 → " << trad2 << "\n";

    std::string trad3 = to_traditional("电脑");
    check("trad(电脑) contains 電", trad3.find("電") != std::string::npos);
    std::cout << "  电脑 → " << trad3 << "\n";

    std::string trad4 = to_traditional("abc");
    check("trad(abc) = abc", trad4 == "abc");

    // v0.9.126 quick_candidates_v2 统一入口测试
    std::cout << "\n--- quick_candidates_v2 统一入口 ---\n";
    auto qc2_1 = quick_candidates_v2("todo");
    check("qc2(todo) has 【待办】", !qc2_1.empty());
    for (auto &c : qc2_1) std::cout << "  " << c << " ";
    std::cout << "\n";

    auto qc2_2 = quick_candidates_v2("yffs");
    check("qc2(yffs) has 一帆风顺", !qc2_2.empty());
    for (auto &c : qc2_2) std::cout << "  " << c << " ";
    std::cout << "\n";

    auto qc2_3 = quick_candidates_v2("hello");
    check("qc2(hello) has hello", !qc2_3.empty());
    for (auto &c : qc2_3) std::cout << "  " << c << " ";
    std::cout << "\n";
    std::cout << "\n--- v0.9.127 网址/邮箱智能识别 ---\n";
    
    // 网址识别
    auto urls = smart_url_lookup("www.baidu.com");
    check("url(www.baidu.com) not empty", !urls.empty());
    check("url(www.baidu.com) = www.baidu.com", urls[0] == "www.baidu.com");
    std::cout << "  www.baidu.com → ";
    for (auto &u : urls) std::cout << u << " ";
    std::cout << "\n";
    
    auto urls2 = smart_url_lookup("https://github.com");
    check("url(https) not empty", !urls2.empty());
    
    // 邮箱识别
    auto emails = smart_url_lookup("admin@example.com");
    check("email not empty", !emails.empty());
    std::cout << "  admin@example.com → " << emails[0] << "\n";
    
    // 域名补全
    auto domains = smart_url_lookup("baidu");
    check("domain(baidu) not empty", !domains.empty());
    std::cout << "  baidu → ";
    for (auto &d : domains) std::cout << d << " ";
    std::cout << "\n";
    
    auto domains2 = smart_url_lookup("github");
    check("domain(github) not empty", !domains2.empty());
    
    check("url(hello) empty", smart_url_lookup("hello").empty());
    check("is_smart_url(www)", is_smart_url("www.google.com"));
    check("!is_smart_url(nihao)", !is_smart_url("nihao"));
    
    std::cout << "\n--- v0.9.127 拼音分词提示 ---\n";
    
    auto seg = pinyin_segment_hint("nihaozhongguo");
    check("segment(nihaozhongguo) not empty", !seg.empty());
    check("segment has separator", seg.find("'") != std::string::npos);
    std::cout << "  nihaozhongguo → " << seg << "\n";
    
    auto seg2 = pinyin_segment_hint("beijingdaxue");
    check("segment(beijingdaxue) not empty", !seg2.empty());
    std::cout << "  beijingdaxue → " << seg2 << "\n";
    
    auto seg3 = pinyin_segment_hint("shurufa");
    check("segment(shurufa) not empty", !seg3.empty());
    std::cout << "  shurufa → " << seg3 << "\n";
    
    // 短输入不分割
    check("segment(ni) empty", pinyin_segment_hint("ni").empty());
    
    std::cout << "\n--- v0.9.127 词频表 ---\n";
    
    check("freq(你好) = 100", get_word_freq("你好") == 100);
    check("freq(中国) = 95", get_word_freq("中国") == 95);
    check("freq(xyz) = 0", get_word_freq("xyz") == 0);
    
    std::cout << "\n--- v0.9.127 长句断句 ---\n";
    
    auto parts = long_sentence_split("nihaozhongguo");
    check("long_sentence(nihaozhongguo) not empty", !parts.empty());
    check("long_sentence has >= 2 parts", parts.size() >= 2);
    std::cout << "  nihaozhongguo → ";
    for (auto &p : parts) std::cout << p << " ";
    std::cout << "\n";
    
    auto parts2 = long_sentence_split("beijingdaxue");
    check("long_sentence(beijingdaxue) not empty", !parts2.empty());
    std::cout << "  beijingdaxue → ";
    for (auto &p : parts2) std::cout << p << " ";
    std::cout << "\n";
    
    std::cout << "\n--- v0.9.127 quick_candidates_v3 统一入口 ---\n";
    
    auto qc3v3test = quick_candidates_v3("www.baidu.com");
    check("qc3v3(www.baidu.com) not empty", !qc3v3test.empty());
    
    auto qc3v3bbtest = quick_candidates_v3("baidu");
    check("qc3(baidu) not empty", !qc3v3bbtest.empty());
    std::cout << "  qc3(baidu) → ";
    for (auto &c : qc3v3bbtest) std::cout << c << " ";
    std::cout << "\n";


    // ── v0.9.128 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.128: 自定义快捷词配置文件 + Tab英文补全 ──\n";

    // load_custom_phrases_from_file
    {
        // Write a temp config file
        FILE *fp = std::fopen("/tmp/test_custom_phrases.txt", "w");
        if (fp) {
            std::fputs("# test config\n", fp);
            std::fputs("myabbr=我的自定义快捷词\n", fp);
            std::fputs("code=std::cout << \"hello\";\n", fp);
            std::fputs("\n", fp);
            std::fclose(fp);
        }
        auto phrases = load_custom_phrases_from_file("/tmp/test_custom_phrases.txt");
        check("load_custom_phrases count=2", phrases.size() == 2);
        check("load_custom_phrases[0].first=myabbr", phrases[0].first == "myabbr");
        check("load_custom_phrases[0].second=我的自定义快捷词", phrases[0].second == "我的自定义快捷词");
        check("load_custom_phrases[1].first=code", phrases[1].first == "code");
    }

    // merge_custom_phrases
    {
        auto merged = merge_custom_phrases("/tmp/test_custom_phrases.txt");
        check("merge_custom_phrases has builtin+user", merged.size() > 26);
        // User phrase should be in the merged list
        bool found = false;
        for (auto &p : merged) {
            if (p.first == "myabbr") { found = true; break; }
        }
        check("merge_custom_phrases contains user phrase", found);
    }

    // custom_phrase_lookup_ext
    {
        auto result = custom_phrase_lookup_ext("myabbr", "/tmp/test_custom_phrases.txt");
        check("custom_phrase_lookup_ext(myabbr) not empty", !result.empty());
        check("custom_phrase_lookup_ext(myabbr)[0]=我的自定义快捷词",
              !result.empty() && result[0] == "我的自定义快捷词");
    }

    // english_autocomplete
    {
        auto ac = english_autocomplete("hel");
        check("english_autocomplete(hel) not empty", !ac.empty());
        check("english_autocomplete(hel)[0]=hello", !ac.empty() && ac[0] == "hello");
        std::cout << "  autocomplete(hel) → ";
        for (auto &c : ac) std::cout << c << " ";
        std::cout << "\n";
    }
    {
        auto ac = english_autocomplete("comp");
        check("english_autocomplete(comp) has compile", [&]{
            for (auto &c : ac) if (c == "compile") return true;
            return false;
        }());
        std::cout << "  autocomplete(comp) → ";
        for (auto &c : ac) std::cout << c << " ";
        std::cout << "\n";
    }
    {
        auto ac = english_autocomplete("x");
        check("english_autocomplete(x) empty (too short)", ac.empty());
    }
    {
        auto ac = english_autocomplete("json");
        check("english_autocomplete(json) has json", !ac.empty() && ac[0] == "json");
    }
    {
        auto ac = english_autocomplete("py");
        check("english_autocomplete(py) has python", [&]{
            for (auto &c : ac) if (c == "python") return true;
            return false;
        }());
    }

    // quick_candidates_v4
    {
        auto qc4 = quick_candidates_v4("addr");
        check("qc4(addr) not empty", !qc4.empty());
        std::cout << "  qc4(addr) → ";
        for (auto &c : qc4) std::cout << c << " ";
        std::cout << "\n";
    }

    // ── v0.9.129 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.129: 句式模板 + 最近输入记忆 + 智能选词 ──\n";

    // sentence_template_lookup
    {
        auto t = sentence_template_lookup("xw");
        check("sentence_template(xw) not empty", !t.empty());
        check("sentence_template(xw)[0]=请问您需要什么帮助？",
              !t.empty() && t[0] == "请问您需要什么帮助？");
    }
    {
        auto t = sentence_template_lookup("td");
        check("sentence_template(td) not empty", !t.empty());
        check("sentence_template(td)[0]=【待办】",
              !t.empty() && t[0] == "【待办】");
    }
    {
        auto t = sentence_template_lookup("sig");
        check("sentence_template(sig) not empty", !t.empty());
        std::cout << "  sig → " << t[0] << "\n";
    }
    {
        auto t = sentence_template_lookup("ly");
        check("sentence_template(ly) not empty", !t.empty());
        std::cout << "  ly → " << t[0] << "\n";
    }
    {
        auto templates = builtin_sentence_templates();
        check("builtin_sentence_templates count>=30", templates.size() >= 30);
    }

    // RecentHistory
    {
        RecentHistory rh;
        rh.add("你好");
        rh.add("中国");
        rh.add("我们");
        check("RecentHistory size=3", rh.items.size() == 3);
        check("RecentHistory[0]=我们", rh.items[0] == "我们");

        rh.add("你好");  // 重复，移到最前
        check("RecentHistory dedup[0]=你好", rh.items[0] == "你好");
        check("RecentHistory dedup size=3", rh.items.size() == 3);

        auto lookup = rh.lookup();
        check("RecentHistory lookup size=3", lookup.size() == 3);

        auto lookup2 = rh.lookup("你");
        check("RecentHistory lookup(你) has 你好", [&]{
            for (auto &s : lookup2) if (s == "你好") return true;
            return false;
        }());
    }

    // RecentHistory file save/load
    {
        RecentHistory rh;
        rh.add("测试1");
        rh.add("测试2");
        bool saved = rh.save_to_file("/tmp/test_recent.txt");
        check("RecentHistory save", saved);

        RecentHistory rh2;
        bool loaded = rh2.load_from_file("/tmp/test_recent.txt");
        check("RecentHistory load", loaded);
        check("RecentHistory load size=2", rh2.items.size() == 2);
        check("RecentHistory load[0]=测试2", rh2.items[0] == "测试2");
    }

    // context_rank_candidates
    {
        std::vector<std::string> cands = {"过", "国", "果", "郭"};
        auto ranked = context_rank_candidates(cands, "中");
        check("context_rank(中) puts 国 first", ranked[0] == "国");
    }
    {
        std::vector<std::string> cands = {"过", "们", "果"};
        auto ranked = context_rank_candidates(cands, "我");
        check("context_rank(我) puts 们 first", ranked[0] == "们");
    }
    {
        std::vector<std::string> cands = {"过", "好", "果"};
        auto ranked = context_rank_candidates(cands, "你");
        check("context_rank(你) puts 好 first", ranked[0] == "好");
    }

    // ── v0.9.130 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.130: 用户词频学习 ──\n";

    {
        UserFreqTable ft;
        ft.add("nihao", "你好");
        ft.add("nihao", "你好");
        ft.add("nihao", "拟好");
        ft.add("nihao", "你好");
        check("UserFreqTable size=2", ft.entries.size() == 2);

        auto freq = ft.lookup("nihao");
        check("UserFreqTable lookup size=2", freq.size() == 2);
        check("UserFreqTable lookup[0]=你好 (freq=3)", freq[0].first == "你好" && freq[0].second == 3);
        check("UserFreqTable lookup[1]=拟好 (freq=1)", freq[1].first == "拟好" && freq[1].second == 1);
    }
    {
        UserFreqTable ft;
        ft.add("zhongguo", "中国");
        ft.add("zhongguo", "中国");
        ft.add("zhongguo", "中国");

        std::vector<std::string> cands = {"中过", "中国", "种过", "种果"};
        auto ranked = ft.rank_candidates(cands, "zhongguo");
        check("UserFreqTable rank puts 中国 first", ranked[0] == "中国");
    }
    {
        UserFreqTable ft;
        ft.add("shurufa", "输入法");
        ft.add("shurufa", "输入法");
        bool saved = ft.save_to_file("/tmp/test_freq.txt");
        check("UserFreqTable save", saved);

        UserFreqTable ft2;
        bool loaded = ft2.load_from_file("/tmp/test_freq.txt");
        check("UserFreqTable load", loaded);
        check("UserFreqTable load size=1", ft2.entries.size() == 1);
        check("UserFreqTable load count=2", ft2.entries[0].count == 2);
    }
    {
        UserFreqTable ft;
        for (int i = 0; i < 10; ++i) ft.add("test", "测试");
        ft.add("test", "其他");
        auto top = ft.top_entries(5);
        check("UserFreqTable top_entries not empty", !top.empty());
        check("UserFreqTable top[0]=测试 (count=10)", top[0].first == "测试" && top[0].second == 10);
    }

    // ── v0.9.131 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.131: 皮肤/主题切换 ──\n";
    {
        auto t = get_theme("default");
        check("theme(default) name", t.name == "default");
    }
    {
        auto t = get_theme("dark");
        check("theme(dark) name", t.name == "dark");
        check("theme(dark) preedit not empty", !t.preedit_color.empty());
        check("theme(dark) reset not empty", !t.reset.empty());
    }
    {
        auto t = get_theme("matrix");
        check("theme(matrix) name", t.name == "matrix");
        check("theme(matrix) green", t.preedit_color.find("32") != std::string::npos);
    }
    {
        auto t = get_theme("ocean");
        check("theme(ocean) name", t.name == "ocean");
    }
    {
        auto t = get_theme("unknown");
        check("theme(unknown) falls to default", t.name == "default");
    }
    {
        auto themes = list_themes();
        check("list_themes size>=7", themes.size() >= 7);
    }
    // ── v0.9.132 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.132: 云拼音候选词接口 ──\n";
    {
        CloudPinyinConfig cfg;
        cfg.enabled = false;
        CloudPinyinClient client(cfg);
        auto results = client.query("nihao");
        check("cloud disabled returns empty", results.empty());
    }
    {
        CloudPinyinConfig cfg;
        cfg.enabled = true;
        cfg.cache_dir = "/tmp/test_cloud_cache";
        CloudPinyinClient client(cfg);

        // Save cache
        std::vector<CloudCandidate> cands = {{"你好", "nihao", 95, "cloud"}};
        bool saved = client.save_cache("nihao", cands);
        check("cloud save_cache", saved);

        // Load cache
        auto loaded = client.load_cache("nihao");
        check("cloud load_cache size=1", loaded.size() == 1);
        check("cloud load_cache[0].chinese=你好", !loaded.empty() && loaded[0].chinese == "你好");
        check("cloud load_cache[0].confidence=95", !loaded.empty() && loaded[0].confidence == 95);

        // Query with cache
        auto results = client.query("nihao");
        check("cloud query with cache", !results.empty());
    }
    {
        CloudPinyinClient client;
        std::vector<std::string> local = {"你好", "拟好"};
        std::vector<CloudCandidate> cloud = {{"你号", "nihao", 80, "cloud"}};
        auto merged = client.merge_candidates(local, cloud);
        check("cloud merge size=3", merged.size() == 3);
        check("cloud merge[2]=你号", merged[2] == "你号");
    }
    {
        CloudPinyinClient client;
        std::vector<std::string> local = {"你好"};
        std::vector<CloudCandidate> cloud = {{"你好", "nihao", 90, "cloud"}};
        auto merged = client.merge_candidates(local, cloud);
        check("cloud merge dedup size=1", merged.size() == 1);
    }
    // ── v0.9.134 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.134: Wayland 支持 ──\n";
    {
        auto wd = pinyin_ime::wayland::detect_wayland();
        // 环境相关，只检查不崩溃
        check("wayland detect runs", true);
    }
    {
        pinyin_ime::wayland::WaylandInputContext ic;
        check("wayland ic initial inactive", ic.state == pinyin_ime::wayland::TextInputState::Inactive);
        ic.activate();
        check("wayland ic active", ic.state == pinyin_ime::wayland::TextInputState::Active);
        ic.set_preedit("你好", 2);
        check("wayland ic preedit=你好", ic.preedit_string == "你好");
        ic.commit("你好");
        check("wayland ic pending commit", ic.state == pinyin_ime::wayland::TextInputState::PendingCommit);
        ic.deactivate();
        check("wayland ic deactivated", ic.state == pinyin_ime::wayland::TextInputState::Inactive);
    }
    {
        using namespace pinyin_ime::wayland;
        check("wayland purpose(password)", purpose_to_hint(ContentPurpose::Password) == "password");
        check("wayland purpose(number)", purpose_to_hint(ContentPurpose::Number) == "number");
        check("wayland purpose(url)", purpose_to_hint(ContentPurpose::Url) == "url");
        check("wayland purpose(email)", purpose_to_hint(ContentPurpose::Email) == "email");
        check("wayland purpose(terminal)", purpose_to_hint(ContentPurpose::Terminal) == "terminal");
        check("wayland purpose(normal)", purpose_to_hint(ContentPurpose::Normal) == "normal");
    }
    {
        pinyin_ime::wayland::WaylandModuleConfig cfg;
        check("wayland config v3 default", cfg.enable_text_input_v3);
        check("wayland config vk default", cfg.enable_virtual_keyboard);
        check("wayland config repeat_rate", cfg.repeat_rate == 25);
    }
    // ── v0.9.135 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.135: 模糊音 ──\n";
    {
        FuzzyPinyinConfig cfg;
        auto variants = fuzzy_syllable_variants("zai", cfg);
        check("fuzzy zai has zhai", [&]{ for(auto&v:variants) if(v=="zhai") return true; return false; }());
        check("fuzzy zai has zai", [&]{ for(auto&v:variants) if(v=="zai") return true; return false; }());
    }
    {
        FuzzyPinyinConfig cfg;
        auto variants = fuzzy_syllable_variants("ni", cfg);
        check("fuzzy ni has li", [&]{ for(auto&v:variants) if(v=="li") return true; return false; }());
        check("fuzzy ni has ni", [&]{ for(auto&v:variants) if(v=="ni") return true; return false; }());
    }
    {
        FuzzyPinyinConfig cfg;
        auto variants = fuzzy_syllable_variants("lan", cfg);
        check("fuzzy lan has nan", [&]{ for(auto&v:variants) if(v=="nan") return true; return false; }());
        check("fuzzy lan has lang", [&]{ for(auto&v:variants) if(v=="lang") return true; return false; }());
        check("fuzzy lan has nang", [&]{ for(auto&v:variants) if(v=="nang") return true; return false; }());
    }
    {
        FuzzyPinyinConfig cfg;
        auto variants = fuzzy_syllable_variants("shen", cfg);
        check("fuzzy shen has sen", [&]{ for(auto&v:variants) if(v=="sen") return true; return false; }());
        check("fuzzy shen has sheng", [&]{ for(auto&v:variants) if(v=="sheng") return true; return false; }());
    }
    {
        FuzzyPinyinConfig cfg;
        cfg.z_zh = false;
        auto variants = fuzzy_syllable_variants("zai", cfg);
        check("fuzzy z_zh off: no zhai", [&]{ for(auto&v:variants) if(v=="zhai") return false; return true; }());
    }
    {
        FuzzyPinyinConfig cfg;
        auto results = fuzzy_pinyin_expand("ni hao", cfg);
        check("fuzzy expand ni hao not empty", !results.empty());
        check("fuzzy expand ni hao has li hao", [&]{ for(auto&v:results) if(v=="li hao") return true; return false; }());
        check("fuzzy expand ni hao has ni hao", [&]{ for(auto&v:results) if(v=="ni hao") return true; return false; }());
    }
    {
        auto cfg = FuzzyPinyinConfig::parse("z_zh,n_l");
        check("fuzzy parse z_zh", cfg.z_zh);
        check("fuzzy parse n_l", cfg.n_l);
        check("fuzzy parse c_ch off", !cfg.c_ch);
    }
    {
        auto cfg = FuzzyPinyinConfig::parse("all");
        check("fuzzy parse all f_h", cfg.f_h);
        check("fuzzy parse all r_l", cfg.r_l);
    }
    // ── v0.9.136 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.136: 剪贴板历史 ──\n";
    {
        ClipboardHistory ch;
        ch.add("你好世界");
        ch.add("测试文本");
        ch.add("另一段");
        check("clipboard size=3", ch.items.size() == 3);
        check("clipboard[1]=另一段", ch.get(1) == "另一段");
        check("clipboard[2]=测试文本", ch.get(2) == "测试文本");
        check("clipboard[3]=你好世界", ch.get(3) == "你好世界");
    }
    {
        ClipboardHistory ch;
        ch.add("aaa");
        ch.add("bbb");
        ch.add("aaa");  // 重复
        check("clipboard dedup size=2", ch.items.size() == 2);
        check("clipboard dedup[1]=aaa", ch.get(1) == "aaa");
    }
    {
        ClipboardHistory ch;
        ch.max_items = 3;
        ch.add("1"); ch.add("2"); ch.add("3"); ch.add("4");
        check("clipboard max_items", ch.items.size() == 3);
        check("clipboard max[1]=4", ch.get(1) == "4");
    }
    {
        ClipboardHistory ch;
        ch.add("hello world");
        ch.add("foo bar");
        auto results = ch.search("hello");
        check("clipboard search size=1", results.size() == 1);
        check("clipboard search[0].first=2", results[0].first == 2);
    }
    {
        ClipboardHistory ch;
        ch.add("line1");
        ch.add("line2");
        check("clipboard remove", ch.remove(1));
        check("clipboard remove size=1", ch.items.size() == 1);
        check("clipboard remove[1]=line1", ch.get(1) == "line1");
    }
    {
        ClipboardHistory ch;
        ch.add("测试\n换行\t制表");
        ch.save_to_file("/tmp/test_clipboard.txt");
        ClipboardHistory ch2;
        ch2.load_from_file("/tmp/test_clipboard.txt");
        check("clipboard persist size=1", ch2.items.size() == 1);
        check("clipboard persist has newline", ch2.get(1).find('\n') != std::string::npos);
    }
    // ── v0.9.137 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.137: 长句智能切分 ──\n";
    {
        auto seg = smart_segment("nihao");
        check("segment nihao size=2", seg.size() == 2);
        check("segment nihao[0]=ni", seg[0] == "ni");
        check("segment nihao[1]=hao", seg[1] == "hao");
    }
    {
        auto seg = smart_segment("nihaoshijie");
        check("segment nihaoshijie size=4", seg.size() == 4);
        check("segment nihaoshijie[0]=ni", seg[0] == "ni");
        check("segment nihaoshijie[1]=hao", seg[1] == "hao");
        check("segment nihaoshijie[2]=shi", seg[2] == "shi");
        check("segment nihaoshijie[3]=jie", seg[3] == "jie");
    }
    {
        auto seg = smart_segment("zhongguo");
        check("segment zhongguo size=2", seg.size() == 2);
        check("segment zhongguo[0]=zhong", seg[0] == "zhong");
        check("segment zhongguo[1]=guo", seg[1] == "guo");
    }
    {
        auto seg = smart_segment("wangluoyouxi");
        check("segment wangluoyouxi size=4", seg.size() == 4);
    }
    {
        auto seg = smart_segment("woshizhongguoren");
        check("segment woshizhongguoren size=5", seg.size() == 5);
        check("segment woshizhongguoren[0]=wo", seg[0] == "wo");
        check("segment woshizhongguoren[4]=ren", seg[4] == "ren");
    }
    {
        auto s = smart_segment_string("nihao");
        check("segment_string nihao", s == "ni hao");
    }
    {
        auto s = insert_separators("nihaoshijie");
        check("insert_separators nihaoshijie", s == "ni hao shi jie");
    }
    {
        check("is_syllable ni", is_pinyin_syllable("ni"));
        check("is_syllable hao", is_pinyin_syllable("hao"));
        check("is_syllable zhz false", !is_pinyin_syllable("zhz"));
        check("is_syllable chuang", is_pinyin_syllable("chuang"));
    }
    {
        // 零声母测试
        auto seg = smart_segment("yiyao");
        check("segment yiyao size=2", seg.size() == 2);
    }
    // ── v0.9.138 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.138: 自定义短语管理 ──\n";
    {
        UserPhraseManager mgr;
        mgr.add("addr", "我的地址是北京市");
        mgr.add("mail", "user@example.com");
        mgr.add("todo", "【待办事项】");
        check("phrase add count=3", mgr.phrases.size() == 3);
        check("phrase lookup addr", mgr.lookup("addr") == "我的地址是北京市");
        check("phrase lookup mail", mgr.lookup("mail") == "user@example.com");
        check("phrase lookup missing", mgr.lookup("xyz").empty());
    }
    {
        UserPhraseManager mgr;
        mgr.add("addr", "地址1");
        mgr.add("addr", "地址2");  // 更新
        check("phrase update count=1", mgr.phrases.size() == 1);
        check("phrase update value", mgr.lookup("addr") == "地址2");
    }
    {
        UserPhraseManager mgr;
        mgr.add("a", "甲");
        mgr.add("b", "乙");
        check("phrase remove", mgr.remove("a"));
        check("phrase remove count=1", mgr.phrases.size() == 1);
        check("phrase remove missing", !mgr.remove("z"));
    }
    {
        UserPhraseManager mgr;
        mgr.add("addr", "北京市");
        mgr.add("mail", "邮箱地址");
        auto results = mgr.search("地址");
        check("phrase search size=1", results.size() == 1);
    }
    {
        UserPhraseManager mgr("/tmp/test_user_phrases.txt");
        mgr.add("test1", "测试一");
        mgr.add("test2", "测试二");
        mgr.save();
        UserPhraseManager mgr2("/tmp/test_user_phrases.txt");
        check("phrase persist size=2", mgr2.phrases.size() == 2);
        check("phrase persist[0]", mgr2.lookup("test1") == "测试一");
    }
    // ── v0.9.139 测试 ──────────────────────────────────────
    std::cout << "\n── v0.9.139: 输入统计增强 ──\n";
    {
        InputStatistics stats;
        stats.start_session();
        stats.record_commit("你好", true, 5, 2000.0);
        stats.record_commit("世界", true, 4, 1500.0);
        stats.record_commit("测试", false, 6, 3000.0);
        check("stats session_chars=18", stats.session_chars == 18);
        check("stats session_keypresses=15", stats.session_keypresses == 15);
        check("stats session_first=2", stats.session_first_choice == 2);
        check("stats accuracy>0", stats.accuracy() > 0);
    }
    {
        InputStatistics stats;
        stats.start_session();
        stats.record_commit("你好", true, 5, 60000.0);
        // 2 chars in 60s = 2 wpm
        check("stats wpm=6", (int)stats.speed_wpm() == 6);
    }
    {
        InputStatistics stats;
        stats.start_session();
        stats.record_commit("你好", true, 5, 2000.0);
        stats.record_commit("你好", true, 5, 2000.0);
        stats.record_commit("世界", true, 4, 1500.0);
        auto top = stats.top_words(10);
        check("stats top_words size=2", top.size() == 2);
        check("stats top_words[0]=你好", top[0].first == "你好");
        check("stats top_words[0] count=2", top[0].second == 2);
    }
    {
        InputStatistics stats;
        stats.start_session();
        stats.record_commit("测试", true, 3, 1000.0);
        stats.save_to_file("/tmp/test_stats.txt");
        InputStatistics stats2;
        stats2.load_from_file("/tmp/test_stats.txt");
        check("stats persist total_chars=6", stats2.total_chars == 6);
        check("stats persist word_freq", stats2.word_freq.count("测试") > 0);
    }
    {
        InputStatistics stats;
        stats.start_session();
        stats.record_commit("你好", true, 5, 2000.0);
        std::string report = stats.format_report();
        check("stats report has 会话", report.find("会话") != std::string::npos);
        check("stats report has 准确率", report.find("准确率") != std::string::npos);
    }

    // ════════════════════════════════════════════════════════════════════
    // v0.9.140 候选窗 GUI 增强测试
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n--- v0.9.140 候选窗 GUI 增强 ---\n";
    {
        CandidateWindow cw;
        auto rendered = cw.render({"你好", "拟好", "你号"}, "nihao");
        check("cw render non-empty", !rendered.empty());
        check("cw render has pinyin", rendered.find("nihao") != std::string::npos);
        check("cw render has first candidate", rendered.find("你好") != std::string::npos);

        auto inline_r = cw.render_inline({"中国", "中过", "种过"});
        check("cw inline non-empty", !inline_r.empty());
        check("cw inline has 中国", inline_r.find("中国") != std::string::npos);

        auto focused = cw.render_focused({"你好", "拟好", "你号"}, 1);
        check("cw focused non-empty", !focused.empty());

        check("cw total_pages=1 for 3 cands", cw.calc_total_pages(3) == 1);
        check("cw total_pages=2 for 6 cands", cw.calc_total_pages(6) == 2);
        check("cw total_pages=3 for 11 cands", cw.calc_total_pages(11) == 3);

        auto cursor = cw.cursor_blink(true);
        check("cw cursor visible non-empty", !cursor.empty());
        auto status = cw.status_bar("拼音", "双拼:小鹤");
        check("cw status has mode", status.find("拼音") != std::string::npos);
    }

    // ════════════════════════════════════════════════════════════════════
    // v0.9.141 云拼音实际接入测试
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n--- v0.9.141 云拼音实际接入 ---\n";
    {
        auto providers = builtin_cloud_providers();
        check("cloud providers has 3", providers.size() == 3);
        check("cloud provider[0] is local", providers[0].name == "local");
        check("cloud provider[1] is google", providers[1].name == "google");

        CloudPinyinClientV2 client("/tmp/test_cloud_cache");
        check("cloud v2 default providers", client.providers.size() == 3);
        check("cloud v2 disabled by default", client.enabled == false);

        client.enabled = true;
        auto result = client.query("nihao");
        check("cloud v2 query returns empty offline", result.empty());

        client.set_provider("custom", "api.example.com", 443, "/pinyin", "key123");
        check("cloud v2 set_provider works", client.providers[2].host == "api.example.com");

        client.enable_provider("google", true);
        check("cloud v2 enable google", client.providers[1].enabled == true);

        auto merged = client.merge_with_local(
            {"你好", "拟好"},
            {{"中国", "zhongguo", 90, "cloud"}, {"你好", "nihao", 95, "cloud"}});
        check("cloud v2 merge dedup", merged.size() == 3);
        check("cloud v2 merge has 中国", merged[2] == "中国");

        auto list = client.list_providers();
        check("cloud v2 list_providers non-empty", !list.empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // v0.9.142 用户词典云同步测试
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n--- v0.9.142 用户词典云同步 ---\n";
    {
        DictSyncConfig cfg;
        cfg.device_id = "test-device";
        cfg.remote_bucket = "xbz";
        cfg.remote_prefix = "pinyin-ime/dict";
        DictSyncManager mgr(cfg);

        mgr.upsert("nh", "你好");
        mgr.upsert("zg", "中国");
        check("sync upsert 2 entries", mgr.local_entries.size() == 2);
        check("sync version=2", mgr.local_version == 2);

        mgr.upsert("nh", "你好啊");
        check("sync update existing", mgr.local_entries.size() == 2);
        check("sync version=3 after update", mgr.local_version == 3);

        auto exported = mgr.export_sync_data();
        check("sync export has header", exported.find("# pinyin-ime") != std::string::npos);
        check("sync export has device", exported.find("test-device") != std::string::npos);
        check("sync export has 你好", exported.find("你好啊") != std::string::npos);

        DictSyncManager mgr2(cfg);
        int imported = mgr2.import_sync_data(exported);
        check("sync import 2 entries", imported == 2);

        mgr.remove("zg");
        check("sync remove works", mgr.local_entries.size() == 1);

        auto status = mgr.sync_status();
        check("sync status has version", status.find("版本") != std::string::npos);

        // 合并测试
        std::vector<DictSyncEntry> remote = {
            {"zg", "中国", 1000000, "other-device", 1},
            {"nh", "你好", 500000, "old-device", 1}
        };
        auto merged = mgr.merge(remote);
        check("sync merge has 2 entries", merged.size() == 2);
    }

    // ════════════════════════════════════════════════════════════════════
    // v0.9.143 长句整句输入优化测试
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n--- v0.9.143 长句整句输入优化 ---\n";
    {
        auto templates = sentence_templates();
        check("sentence templates non-empty", templates.size() > 10);
        check("sentence template has nihao", [&]{
            for (auto& t : templates) if (t.first == "nihao") return true;
            return false;
        }());

        auto cands = sentence_candidates("nihao");
        check("sentence candidates for nihao non-empty", !cands.empty());
        check("sentence candidates has 你好", [&]{
            for (auto& c : cands) if (c == "你好") return true;
            return false;
        }());

        auto cands2 = sentence_candidates("zhonghuarenmingongheguo");
        check("sentence candidates for PRC", !cands2.empty());

        auto predict = context_predict("你好");
        check("context predict for 你好 non-empty", !predict.empty());
        check("context predict has 啊", [&]{
            for (auto& p : predict) if (p == "啊") return true;
            return false;
        }());

        auto predict2 = context_predict("我");
        check("context predict for 我 has 的", [&]{
            for (auto& p : predict2) if (p == "的") return true;
            return false;
        }());

        auto predict3 = context_predict("xyz_unknown");
        check("context predict unknown returns empty", predict3.empty());

        auto seg = context_aware_segment("nihao", "");
        check("context aware segment non-empty", !seg.empty());

        auto match = multi_word_match("nihao");
        check("multi word match has 2 segments", match.size() == 2);
        check("multi word match[0] pinyin=ni", match[0].first == "ni");
        check("multi word match[0] chinese=你", match[0].second == "你");

        auto si = sentence_input("nihao", "");
        check("sentence input non-empty", !si.empty());

        auto si2 = sentence_input("a", "你好");
        check("sentence input with context", !si2.empty());
    }

    // ════════════════════════════════════════════════════════════════════
    // v0.9.144 输入统计可视化测试
    // ════════════════════════════════════════════════════════════════════
    std::cout << "\n--- v0.9.144 输入统计可视化 ---\n";
    {
        std::vector<std::pair<std::string, int>> data = {
            {"你好", 50}, {"中国", 30}, {"我们", 20}, {"的", 15}, {"是", 10}
        };
        auto chart = ascii_bar_chart(data, 20, 5);
        check("bar chart non-empty", !chart.empty());
        check("bar chart has 你好", chart.find("你好") != std::string::npos);
        check("bar chart has #", chart.find("#") != std::string::npos);

        auto pb = progress_bar(75.0, 20, "测试");
        check("progress bar has 75%", pb.find("75%") != std::string::npos);
        check("progress bar has label", pb.find("测试") != std::string::npos);

        auto pb0 = progress_bar(0.0);
        check("progress bar 0%", pb0.find("0%") != std::string::npos);
        auto pb100 = progress_bar(100.0);
        check("progress bar 100%", pb100.find("100%") != std::string::npos);

        auto si = speed_indicator(45.0);
        check("speed indicator has 45", si.find("45") != std::string::npos);
        check("speed indicator has 熟练", si.find("熟练") != std::string::npos);

        auto si2 = speed_indicator(120.0);
        check("speed indicator 120 has 专业", si2.find("专业") != std::string::npos);

        auto av = accuracy_visual(92.0);
        check("accuracy visual has 92%", av.find("92%") != std::string::npos);
        check("accuracy visual has A grade", av.find("A") != std::string::npos);

        auto av2 = accuracy_visual(45.0);
        check("accuracy visual 45 has F grade", av2.find("F") != std::string::npos);

        std::vector<std::string> events = {"开始输入", "上屏 你好", "上屏 中国"};
        std::vector<double> timestamps = {0.0, 2.5, 5.0};
        auto tl = session_timeline(events, timestamps);
        check("timeline non-empty", !tl.empty());
        check("timeline has 开始", tl.find("开始") != std::string::npos);

        InputStatistics stats;
        stats.start_session();
        stats.record_commit("你好", true, 5, 2000.0);
        stats.record_commit("中国", true, 4, 1500.0);
        stats.record_commit("我们", false, 6, 3000.0);
        auto panel = stats_visual_panel(stats);
        check("visual panel non-empty", !panel.empty());
        check("visual panel has 准确率", panel.find("准确率") != std::string::npos);
        check("visual panel has 速度", panel.find("速度") != std::string::npos);
        check("visual panel has 高频词", panel.find("高频词") != std::string::npos);

        std::vector<int> choices = {80, 10, 5, 3, 2};
        auto pie = choice_distribution_pie(choices);
        check("pie chart non-empty", !pie.empty());
        check("pie chart has 首选", pie.find("首选") != std::string::npos);

        // v5 统一入口
        auto v5 = quick_candidates_v5("nihao");
        check("quick_candidates_v5 non-empty", !v5.empty());
    }

    // ═══ v0.9.145 测试 ═══════════════════════════════════════════
    {
        // smart_pinyin_correct basic
        auto corrections = smart_pinyin_correct("zhonggu");
        check("v145 spc zhonggu has corrections", !corrections.empty());
        bool has_zhongguo = false;
        for (auto& c : corrections) if (c == "zhongguo") has_zhongguo = true;
        check("v145 spc zhonggu→zhongguo", has_zhongguo);

        // smart_pinyin_correct empty
        auto empty_corr = smart_pinyin_correct("");
        check("v145 spc empty input", empty_corr.empty());

        // smart_pinyin_correct h insertion
        auto suzi_corr = smart_pinyin_correct("suzi");
        check("v145 spc suzi has corrections", !suzi_corr.empty());

        // context_rank_candidates_v2 basic
        std::vector<std::string> cands = {"吗", "码", "马", "麻"};
        auto ranked = context_rank_candidates_v2(cands, "好", "你");
        check("v145 crv2 你好吗 first", ranked[0] == "吗");

        // context_rank_candidates_v2 no context
        std::vector<std::string> cands2 = {"a", "b", "c"};
        auto ranked2 = context_rank_candidates_v2(cands2, "", "");
        check("v145 crv2 no context preserves order", ranked2 == cands2);

        // extended_sentence_templates
        auto templates = extended_sentence_templates();
        check("v145 templates > 34", templates.size() > 34);
        bool has_ks = false;
        for (auto& t : templates) {
            if (t.first == "ks" && t.second == "开始") has_ks = true;
        }
        check("v145 templates has ks→开始", has_ks);

        // candidate_quality_score
        int score1 = candidate_quality_score("你好", "nihao");
        check("v145 cqs 你好 positive", score1 > 0);
        int score_de = candidate_quality_score("的", "de");
        check("v145 cqs 的 positive", score_de > 0);

        // quick_candidates_v6 basic
        auto v6 = quick_candidates_v6("xw");
        check("v146 v6 xw non-empty", !v6.empty());
        bool has_template = false;
        for (auto& r : v6) {
            if (r.find("请问") != std::string::npos) has_template = true;
        }
        check("v145 v6 xw has 请问", has_template);

        // quick_candidates_v6 extended templates
        auto v6_ks = quick_candidates_v6("ks");
        check("v145 v6 ks non-empty", !v6_ks.empty());
        bool has_kaishi = false;
        for (auto& r : v6_ks) {
            if (r == "开始") has_kaishi = true;
        }
        check("v145 v6 ks has 开始", has_kaishi);

        // quick_candidates_v6 with context
        auto v6_ctx = quick_candidates_v6("hao", "你", "");
        check("v145 v6 with context non-empty", !v6_ctx.empty());
    }

    // ═══ v0.9.146 测试 ═══════════════════════════════════════════
    {
        // DP分词基础
        auto seg = dp_pinyin_segment("nihao");
        check("v146 dp nihao 2 segments", seg.size() == 2);
        check("v146 dp nihao[0]=ni", seg[0] == "ni");
        check("v146 dp nihao[1]=hao", seg[1] == "hao");

        // DP分词长句
        auto seg2 = dp_pinyin_segment("nihaozhongguo");
        check("v146 dp nihaozhongguo 4 segments", seg2.size() == 4);

        // DP分词超长句
        auto seg3 = dp_pinyin_segment("woshizhongguoren");
        check("v146 dp woshizhongguoren 5 segments", seg3.size() == 5);

        // DP分词提示
        auto hint = dp_pinyin_segment_hint("nihao");
        check("v146 hint has separator", hint.find("'") != std::string::npos);

        // DP分词空输入
        auto seg_empty = dp_pinyin_segment("");
        check("v146 dp empty input", seg_empty.empty());

        // DP分词单音节
        auto seg_one = dp_pinyin_segment("a");
        check("v146 dp single syllable", seg_one.size() == 1 && seg_one[0] == "a");

        // 增强长句分割
        auto groups = long_sentence_split_v2("nihaozhongguo");
        check("v146 long split non-empty", !groups.empty());

        // DP分词 vs 贪心：zhongguo 应该正确分词
        auto seg_zg = dp_pinyin_segment("zhongguo");
        check("v146 dp zhongguo 2 segments", seg_zg.size() == 2);
        check("v146 dp zhongguo[0]=zhong", seg_zg[0] == "zhong");
        check("v146 dp zhongguo[1]=guo", seg_zg[1] == "guo");

        // 超长句20+音节
        auto seg_long = dp_pinyin_segment("jintiantianqihenhaowochumenquwan");
        check("v146 dp long sentence 10+ segments", seg_long.size() >= 10);

        // 有效音节表
        auto& syllables = valid_pinyin_syllables();
        check("v146 syllables has nihao components", syllables.count("ni") && syllables.count("hao"));
        check("v146 syllables has zhong", syllables.count("zhong"));
        check("v146 syllables has guo", syllables.count("guo"));
    }

    // ═══ v0.9.147 测试 ═══════════════════════════════════════════
    {
        // 内置皮肤获取
        auto sogou = get_skin("sogou");
        check("v147 sogou skin name", sogou.name == "sogou");
        check("v147 sogou font_size 14", sogou.font_size == 14);
        check("v147 sogou unicode border", sogou.border_style == "unicode");
        check("v147 sogou highlight_first", sogou.highlight_first == true);

        auto dark = get_skin("dark");
        check("v147 dark skin name", dark.name == "dark");
        check("v147 dark no border", dark.border_style == "none");

        auto matrix = get_skin("matrix");
        check("v147 matrix animation", matrix.enable_animation == true);
        check("v147 matrix speed 50", matrix.animation_speed_ms == 50);

        auto vertical = get_skin("vertical");
        check("v147 vertical style", vertical.window_style == "vertical");
        check("v147 vertical 9 per page", vertical.max_candidates_per_page == 9);

        auto high = get_skin("high");
        check("v147 high font_size 16", high.font_size == 16);

        // 皮肤列表
        auto skins = list_skins();
        check("v147 9 skins total", skins.size() == 9);

        // 默认皮肤
        auto def = get_skin("unknown");
        check("v147 default skin fallback", def.name == "default");

        // 保存和加载皮肤
        SkinConfig custom;
        custom.name = "custom_test";
        custom.description = "测试皮肤";
        custom.font_size = 18;
        custom.candidate_spacing = 4;
        custom.max_candidates_per_page = 7;
        custom.show_index = false;
        custom.window_style = "vertical";
        custom.border_style = "ascii";

        bool saved = save_skin_to_file(custom, "/tmp/test_skin.ini");
        check("v147 save skin file", saved);

        auto loaded = load_skin_from_file("/tmp/test_skin.ini");
        check("v147 load skin name", loaded.name == "custom_test");
        check("v147 load skin font_size 18", loaded.font_size == 18);
        check("v147 load skin spacing 4", loaded.candidate_spacing == 4);
        check("v147 load skin max 7", loaded.max_candidates_per_page == 7);
        check("v147 load skin no index", loaded.show_index == false);
        check("v147 load skin vertical", loaded.window_style == "vertical");
        check("v147 load skin ascii border", loaded.border_style == "ascii");

        // 渲染候选窗
        std::vector<std::string> cands = {"你好", "世界", "中国"};
        auto rendered = render_candidate_window_with_skin(cands, "nihao", get_skin("sogou"));
        check("v147 render non-empty", !rendered.empty());
        check("v147 render has preedit", rendered.find("nihao") != std::string::npos);
        check("v147 render has candidate", rendered.find("你好") != std::string::npos);

        // 渲染空候选
        auto empty_render = render_candidate_window_with_skin({}, "test", get_skin("default"));
        check("v147 render empty candidates", empty_render.empty());
    }

    // ═══ v0.9.148 测试 ═══════════════════════════════════════════
    {
        // 表情分类
        auto cats = emoji_panel_categories();
        check("v148 12 emoji categories", cats.size() == 12);
        check("v148 has 笑脸 category", std::find(cats.begin(), cats.end(), "笑脸") != cats.end());
        check("v148 has 天气 category", std::find(cats.begin(), cats.end(), "天气") != cats.end());

        // 按分类获取表情
        auto faces = emoji_panel_by_category("笑脸");
        check("v148 笑脸 has 20 emojis", faces.size() == 20);
        check("v148 笑脸[0] is 😀", faces[0] == "😀");

        auto weather = emoji_panel_by_category("天气");
        check("v148 天气 has 20 emojis", weather.size() == 20);

        auto nonexistent = emoji_panel_by_category("不存在");
        check("v148 nonexistent category empty", nonexistent.empty());

        // 搜索表情
        auto search_results = emoji_search("笑脸");
        check("v148 search 笑脸 non-empty", !search_results.empty());

        auto search_face = emoji_search("face");
        check("v148 search face non-empty", !search_face.empty());

        auto search_empty = emoji_search("");
        check("v148 search empty query", search_empty.empty());

        auto search_food = emoji_search("food");
        check("v148 search food non-empty", !search_food.empty());

        // 渲染面板
        auto panel = emoji_panel_render(0);
        check("v148 panel render non-empty", !panel.empty());
        check("v148 panel has title", panel.find("表情面板") != std::string::npos);
        check("v148 panel has hint", panel.find("提示") != std::string::npos);

        auto panel2 = emoji_panel_render(5);
        check("v148 panel render cat 5", panel2.find("食物") != std::string::npos);

        // 特殊字符面板
        auto special = special_char_panel();
        check("v148 8 special char categories", special.size() == 8);

        auto special_render = special_char_panel_render();
        check("v148 special panel non-empty", !special_render.empty());
        check("v148 special panel has title", special_render.find("特殊字符面板") != std::string::npos);
        check("v148 special has math", special_render.find("数学符号") != std::string::npos);
    }
    std::cout << "\n=== Results: " << pass << " passed, " << fail << " failed ===\n";
    return fail > 0 ? 1 : 0;
}
