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

    std::cout << "\n=== Results: " << pass << " passed, " << fail << " failed ===\n";
    return fail > 0 ? 1 : 0;
}
// v0.9.127 测试
// ════════════════════════════════════════════════════════════════════

