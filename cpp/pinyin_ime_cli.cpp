// pinyin_ime_cli.cpp — M1 最小可运行输入法（命令行前端）
//
// 复用 libime 引擎，打通「打字 → 候选 → 选词上屏」全链路。可在无显示服务器的
// 终端里真实运行，因此 headless 沙箱也能验证 M1「能打字」这一里程碑。
//
// 定位：ANALYSIS.md §7.3 建议的「~200 行 CLI 验证 nihao→你好 全链路」；
//       完成后再把同一套引擎接到 Fcitx5 addon（复用 wetype.cpp 的 EngineProc）。
//
// 两种运行模式：
//   1) 行模式（stdin 非 TTY，供脚本 / 评测回归）：
//        每行一段拼音，输出 \t 分隔候选，第一项为整句首选。
//        全拼：nihaoshijie → 你好世界；简拼（首字母）：bjdx → 北京大学（默认支持）。
//        例：echo "bjdx" | pinyin_ime_cli <lm> <dict> 5
//   2) 交互模式（stdin 为 TTY，供真人打字）：
//        a-z / '       输入拼音（隔音符 ' 切分音节，如 xi'an；也支持简拼）
//        退格          删一个拼音字符
//        1-9           选第 N 号候选并上屏
//        空格 / 回车   上屏首选候选
//        Esc           清空当前输入
//        Ctrl-C        退出
//
// 用法：
//   pinyin_ime_cli [选项] [语言模型路径] [系统词典路径] [nbest=5]
//
// 选项：
//   -f, --fuzzy     启用模糊音（平翘舌 z/zh、c/ch、s/sh，前后鼻音 an/ang、en/eng、
//                   in/ing、ian/iang、uan/uang，n/l、f/h、l/r，及键盘邻近纠错）
//   --user-data DIR 用户词库/自学习持久化目录（默认 ~/.local/share/pinyin-ime）
//   --abbr-file F  自定义简拼映射文件（默认 ~/.config/pinyin-ime/abbr.txt，格式：abbr=word）
//   -h, --help      显示本帮助
//
// 已实现（M1 范围）：逐键输入、候选列表、选词上屏、整句首选、简拼（默认）、
//   模糊音（--fuzzy）、双拼（--shuangpin / --sp-scheme）、候选分页（[,] 翻页）、
//   自学习 + 用户词库持久化（--user-data，选词上屏自动造词）、清屏、
//   中英切换（Tab 切换，英文态原样透传）、中文标点（半角标点自动转全角，
//   含引号智能配对（“/”交替）；打拼音中按标点会先上屏首选再出标点；空闲按标点直接出标点）、
//   光标左右移动（逐词分段：方向键按词边界移动候选光标）、
//   Home/End（候选光标跳句首/句尾）、单引号智能配对（空闲‘/’交替，打拼音中仍是隔音符）、
//   上屏即落盘（崩溃/被 kill 最多丢最后一次上屏的增量）、表情（Ctrl-E 进入表情选择，
//   1-9 选表情上屏，[,]/[.] 翻页，Esc 返回中文）、误上屏撤回（Ctrl-Z：撤回最近一次
//   候选上屏，恢复拼音重新编辑，最多记住 32 条）。
// 待实现（见 README「未来优化方向」）：候选部首/偏旁检索、跨设备同步。
#include <cstdio>
#include <cctype>
#include <csignal>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <system_error>
#include <utility>
#include <vector>
#include <set>
#include <unordered_map>

#include <libime/core/historybigram.h>
#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinencoder.h>
#include <libime/pinyin/pinyinime.h>
#include <libime/core/prediction.h>
#include <libime/pinyin/shuangpinprofile.h>

#include "emoji_list.h"
#include "abbr_presets.h"
#include "pinyin_corrector.h"
#include "quick_input.h"

#ifdef __unix__
#include <sys/select.h>
#include <sys/time.h>
#include <termios.h>
#include <unistd.h>
#endif

using namespace libime;

namespace {

#ifdef __unix__
// 崩溃/终止信号优雅落盘：置位后阻塞中的 read() 被 EINTR 打断，主循环退出时 save_user_data()。
volatile sig_atomic_t g_quit = 0;
void on_signal(int) { g_quit = 1; }
#endif

// 是否是合法的拼音输入字符（小写字母 + 隔音符 '）。
bool is_pinyin_char(char ch) {
    return (ch >= 'a' && ch <= 'z') || ch == '\'';
}

// 取「上屏前」的连续拼音串：交互态 preedit 音节间可能带空格（渲染 "sen me"）、
// 隔音符 xi'an 带 '，撤回时需要把这些消掉，恢复成可重新 type 的纯拼音串。
std::string preedit_compact(const PinyinContext &ctx) {
    std::string s;
    for (char c : ctx.preedit()) {
        if (c != '\'' && c != ' ') s.push_back(c);
    }
    return s;
}

bool exists(const std::string &p) {
    if (p.empty()) return false;
    std::ifstream f(p);
    return f.good();
}


std::string find_data(const std::vector<std::string> &candidates, const char *what) {
    for (const auto &p : candidates) {
        if (exists(p)) return p;
    }
    std::cerr << "pinyin_ime_cli: 找不到 " << what << "，请用参数显式指定路径\n";
    std::exit(2);
}

void usage(const char *prog) {
    std::cerr
        << "用法: " << prog << " [选项] [语言模型路径] [系统词典路径] [nbest=5]\n"
        << "  行模式（stdin 非 TTY）：每行一段拼音（全拼或简拼）→ 候选\n"
        << "  交互模式（stdin 为 TTY）：逐键输入，数字/空格/回车上屏\n"
        << "选项:\n"
        << "  -s, --shuangpin  启用双拼（默认小鹤；用 --sp-scheme 指定方案）\n"
        << "  --sp-scheme NAME 双拼方案：xiaohe/ziranma/ms/ziguang/abc/zhongwenzhixing/pinyinjiajia/gb\n"
        << "  --correct       先经 M3 纠错（CHIME+二元转移）修复打错的拼音再解码\n"
        << "  -f, --fuzzy     启用模糊音（平翘舌/前后鼻音/n-l/键盘纠错）\n"
        << "  --user-data DIR 用户词库/自学习持久化目录（默认 ~/.local/share/pinyin-ime）\n"
        << "  --config        交互式配置向导（设置模糊音/双拼/纠错等）\n"
        << "  --dict-list     列出用户词典所有词条\n"
        << "  --dict-add P=W  添加词条（拼音=中文，如 --dict-add hh=你好）\n"
        << "  --dict-del P    删除词条（按拼音删除，如 --dict-del hh）\n"
        << "  --dict-import F 从文件批量导入（每行 拼音=中文）\n"
        << "  --dict-export F 导出用户词典到文件\n"
        << "  --dict-search K 搜索词条（关键词匹配拼音或中文）\n"
        << "  --stats-report  输入统计面板（高频词/准确率/使用量）\n"
        << "  --theme NAME   配色主题：default/dark/high/retro/matrix/ocean/solar\n"
        << "  --cloud        启用云拼音候选词（需网络，离线自动降级）\n"
        << "  --fuzzy CFG    模糊音：z_zh,c_ch,s_sh,n_l,an_ang,en_eng,in_ing 或 all\n"
        << "  --doctor        一键诊断（检查环境/依赖/数据/权限）\n"
        << "  --num           数字转中文(逐位): 123 -> 一二三\n"
        << "  --num-amount    数字转中文金额: 123 -> 一百二十三\n"
        << "  --num-upper     大写数字: 123 -> 壹贰叁\n"
        << "  --quiet         抑制信息性 stderr 输出（配合 --json 使用更干净）\n"
        << "  --json          行模式输出 JSON 数组（方便程序化调用）\n"
        << "  --stats         打印引擎统计（词典/LM/内存）后退出\n"
        << "  --version       显示版本号后退出\n"
        << "  --learn-file F  批量学习：每行 拼音<TAB>中文，训练用户语言模型\n"
        << "  --dump-user     导出用户学习历史（人类可读）后退出\n"
        << "  --batch F       从文件批量处理（每行一段拼音，输出候选）\n"
        << "  --top1          只输出首选候选（方便管道 | 其他工具）\n"
        << "  --top-k N       输出前 N 个候选（Tab 分隔）\n"
        << "  --add-user K=V  添加用户词典条目（如 --add-user myname=张三）后退出\n"
        << "  --list-user     列出所有用户词典条目后退出\n"
        << "  --del-user K    删除用户词典条目（如 --del-user myname）后退出\n"
        << "  --benchmark     内置基准测试：测量启动/查询/吞吐/准确率\n"
        << "  --predict       联想预测：输入中文文本，输出可能的下一个词\n"
        << "  --train-text F  从中文文本训练用户语言模型（每行一句中文）\n"
        << "  --history-weight F  用户语言模型权重（默认 1.0，越大越偏向用户历史）\n"
        << "  --self-test     健康检查：验证安装和配置是否正常\n"
        << "配置文件：~/.config/pinyin-ime/config（key=value，命令行参数优先）\n"
        << "  可配键：lm_path, dict_path, nbest, fuzzy, shuangpin, sp_scheme, user_data\n"
        << "  -h, --help      显示本帮助\n"
        << "简拼（首字母输入）默认开启：bjdx → 北京大学\n"
        << "候选翻页：- , [ / = . ] 上一页/下一页，PageUp/PageDown 翻页；←/→ 逐词移动，Ctrl+←/→ 移动候选焦点（空格上屏焦点候选），Home/End 跳句首句尾\n"
        << "表情：Ctrl-E 进入表情选择（1-9 选表情，[,]/[.] 翻页，Esc 返回中文）\n"
        << "撤回：Ctrl-Z 撤回最近一次候选上屏（恢复拼音，可继续编辑）\n";
}

// 采集当前「整句首选 + 去重后的后续候选」到 cands（cands[0] 恒为首选）。
std::vector<std::string> collect_candidates(PinyinContext &ctx, size_t max) {
    std::vector<std::string> cands;
    std::string top = ctx.sentence();
    cands.push_back(top);
    for (const auto &c : ctx.candidatesToCursor()) {
        std::string t = c.toString();
        if (t.empty() || t == top) continue;
        cands.push_back(t);
        if (cands.size() >= max) break;
    }
    return cands;
}


// 简拼预设表与 boost 逻辑见 abbr_presets.h
using namespace pinyin_ime;

// 选中候选并（若已完全上屏）提交：打印、写入自学习上下文、清空。
// 返回是否发生了一次提交。
bool select_and_commit(PinyinContext &ctx, size_t idx) {
    const auto &cands = ctx.candidatesToCursor();
    if (ctx.empty() || idx >= cands.size()) return false;
    ctx.selectCandidatesToCursor(idx);
    if (ctx.selected()) {
        std::string text = ctx.sentence();
        if (!text.empty()) {
            std::cout << "\r\x1b[2K"
                      << "「" << text << "」\n";
        }
        // learn()：把选中句子加入用户语言模型历史，并把系统词典没有的词
        // 自动 addWord 到 UserDict（libime 内建自动造词）；退出时经 save 落盘。
        ctx.learn();
        ctx.clear();
        return true;
    }
    return false;
}

// 常见「好用度」模糊音集合：平翘舌、前后鼻音、n/l、f/h、l/r + 键盘邻近纠错。
// 全部来自 PinyinFuzzyFlag 官方枚举（libime/pinyin/pinyinencoder.h）。
PinyinFuzzyFlags common_fuzzy_flags() {
    return {
        PinyinFuzzyFlag::C_CH,       PinyinFuzzyFlag::F_H,
        PinyinFuzzyFlag::L_N,        PinyinFuzzyFlag::S_SH,
        PinyinFuzzyFlag::Z_ZH,       // L_R：libime 1.1.5 无此枚举，跳过（新版 libime 才有）
        PinyinFuzzyFlag::AN_ANG,     PinyinFuzzyFlag::EN_ENG,
        PinyinFuzzyFlag::IAN_IANG,   PinyinFuzzyFlag::IN_ING,
        PinyinFuzzyFlag::U_OU,       PinyinFuzzyFlag::UAN_UANG,
        PinyinFuzzyFlag::V_U,        PinyinFuzzyFlag::VE_UE,
        PinyinFuzzyFlag::CommonTypo, PinyinFuzzyFlag::AdvancedTypo,
        // Correction：libime 1.1.5 无此枚举，由 M3 pinyin_corrector.cpp 自研纠错兜底
    };
}

// 双拼内置方案名 → 枚举（未知方案回退小鹤）。
ShuangpinBuiltinProfile sp_profile_from(std::string name) {
    for (auto &c : name) c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));
    if (name == "ziranma") return ShuangpinBuiltinProfile::Ziranma;
    if (name == "ms" || name == "微软") return ShuangpinBuiltinProfile::MS;
    if (name == "ziguang" || name == "紫光") return ShuangpinBuiltinProfile::Ziguang;
    if (name == "abc" || name == "智能abc") return ShuangpinBuiltinProfile::ABC;
    if (name == "zhongwenzhixing" || name == "中文之星") return ShuangpinBuiltinProfile::Zhongwenzhixing;
    if (name == "pinyinjiajia" || name == "拼音加加") return ShuangpinBuiltinProfile::PinyinJiajia;
    if (name == "xiaohe" || name == "小鹤") return ShuangpinBuiltinProfile::Xiaohe;
    // GB：libime 1.1.5 无此内置双拼方案，回退小鹤（新版 libime 才有 GB）
    if (name == "gb") {
        std::cerr << "pinyin_ime_cli: 当前 libime 无 GB 双拼方案，回退小鹤\n";
        return ShuangpinBuiltinProfile::Xiaohe;
    }
    std::cerr << "pinyin_ime_cli: 未知双拼方案 \"" << name
              << "\"（可用 xiaohe/ziranma/ms/ziguang/abc/zhongwenzhixing/pinyinjiajia/gb），回退小鹤\n";
    return ShuangpinBuiltinProfile::Xiaohe;
}

std::string sp_display_name(ShuangpinBuiltinProfile p) {
    switch (p) {
    case ShuangpinBuiltinProfile::Ziranma: return "自然码";
    case ShuangpinBuiltinProfile::MS: return "微软";
    case ShuangpinBuiltinProfile::Ziguang: return "紫光";
    case ShuangpinBuiltinProfile::ABC: return "智能ABC";
    case ShuangpinBuiltinProfile::Zhongwenzhixing: return "中文之星";
    case ShuangpinBuiltinProfile::PinyinJiajia: return "拼音加加";
    // GB：libime 1.1.5 无此枚举
    case ShuangpinBuiltinProfile::Xiaohe: default: return "小鹤";
    }
}

// 中文标点：半角 ASCII 标点 → 全角中文标点。纯前端（不进拼音解码），
// 只负责「上屏标点」这一件事。返回空串表示该字符无对应中文标点。
// v0.9.88: 检查字符串是否能分解为有效拼音音节
// 如果不能，认为是英文单词，原样输出
static const char *validPinyinSyllables[] = {
    "a", "ai", "an", "ang", "ao",
    "ba", "bai", "ban", "bang", "bao", "bei", "ben", "beng", "bi", "bian",
    "biao", "bie", "bin", "bing", "bo", "bu",
    "ca", "cai", "can", "cang", "cao", "ce", "cen", "ceng", "cha", "chai",
    "chan", "chang", "chao", "che", "chen", "cheng", "chi", "chong", "chou",
    "chu", "chua", "chuai", "chuan", "chuang", "chui", "chun", "chuo", "ci",
    "cong", "cou", "cu", "cuan", "cui", "cun", "cuo",
    "da", "dai", "dan", "dang", "dao", "de", "dei", "den", "deng", "di",
    "dia", "dian", "diao", "die", "ding", "diu", "dong", "dou", "du", "duan",
    "dui", "dun", "duo",
    "e", "ei", "en", "eng", "er",
    "fa", "fan", "fang", "fei", "fen", "feng", "fo", "fou", "fu",
    "ga", "gai", "gan", "gang", "gao", "ge", "gei", "gen", "geng", "gong",
    "gou", "gu", "gua", "guai", "guan", "guang", "gui", "gun", "guo",
    "ha", "hai", "han", "hang", "hao", "he", "hei", "hen", "heng", "hong",
    "hou", "hu", "hua", "huai", "huan", "huang", "hui", "hun", "huo",
    "ji", "jia", "jian", "jiang", "jiao", "jie", "jin", "jing", "jiong",
    "jiu", "ju", "juan", "jue", "jun",
    "ka", "kai", "kan", "kang", "kao", "ke", "kei", "ken", "keng", "kong",
    "kou", "ku", "kua", "kuai", "kuan", "kuang", "kui", "kun", "kuo",
    "la", "lai", "lan", "lang", "lao", "le", "lei", "leng", "li", "lia",
    "lian", "liang", "liao", "lie", "lin", "ling", "liu", "long", "lou",
    "lu", "lv", "lua", "luan", "lve", "lun", "luo",
    "ma", "mai", "man", "mang", "mao", "me", "mei", "men", "meng", "mi",
    "mian", "miao", "mie", "min", "ming", "miu", "mo", "mou", "mu",
    "na", "nai", "nan", "nang", "nao", "ne", "nei", "nen", "neng", "ni",
    "nian", "niang", "niao", "nie", "nin", "ning", "niu", "nong", "nou",
    "nu", "nv", "nuan", "nve", "nun", "nuo",
    "o", "ou",
    "pa", "pai", "pan", "pang", "pao", "pei", "pen", "peng", "pi", "pian",
    "piao", "pie", "pin", "ping", "po", "pou", "pu",
    "qi", "qia", "qian", "qiang", "qiao", "qie", "qin", "qing", "qiong",
    "qiu", "qu", "quan", "que", "qun",
    "ran", "rang", "rao", "re", "ren", "reng", "ri", "rong", "rou", "ru",
    "rua", "ruan", "rui", "run", "ruo",
    "sa", "sai", "san", "sang", "sao", "se", "sen", "seng", "sha", "shai",
    "shan", "shang", "shao", "she", "shei", "shen", "sheng", "shi", "shong",
    "shou", "shu", "shua", "shuai", "shuan", "shuang", "shui", "shun", "shuo",
    "si", "song", "sou", "su", "suan", "sui", "sun", "suo",
    "ta", "tai", "tan", "tang", "tao", "te", "teng", "ti", "tian", "tiao",
    "tie", "ting", "tong", "tou", "tu", "tuan", "tui", "tun", "tuo",
    "wa", "wai", "wan", "wang", "wei", "wen", "weng", "wo", "wu",
    "xi", "xia", "xian", "xiang", "xiao", "xie", "xin", "xing", "xiong",
    "xiu", "xu", "xuan", "xue", "xun",
    "ya", "yan", "yang", "yao", "ye", "yi", "yin", "ying", "yo", "yong",
    "you", "yu", "yuan", "yue", "yun",
    "za", "zai", "zan", "zang", "zao", "ze", "zei", "zen", "zeng", "zha",
    "zhai", "zhan", "zhang", "zhao", "zhe", "zhei", "zhen", "zheng", "zhi",
    "zhong", "zhou", "zhu", "zhua", "zhuai", "zhuan", "zhuang", "zhui",
    "zhun", "zhuo", "zi", "zong", "zou", "zu", "zuan", "zui", "zun", "zuo",
    nullptr
};

bool is_valid_pinyin(const std::string &s) {
    if (s.empty()) return false;
    // 贪心匹配：尝试将 s 分解为有效拼音音节
    size_t pos = 0;
    while (pos < s.size()) {
        bool matched = false;
        // 尝试匹配最长的音节（最多6字符）
        for (int len = std::min((size_t)6, s.size() - pos); len >= 1; --len) {
            std::string sub = s.substr(pos, len);
            for (const char **p = validPinyinSyllables; *p; ++p) {
                if (sub == *p) {
                    pos += len;
                    matched = true;
                    break;
                }
            }
            if (matched) break;
        }
        if (!matched) return false;
    }
    return true;
}

// v0.9.90: 前向声明
std::string add_syllable_separators(const std::string &raw);

// v0.9.90: 检查是否为有效拼音（用于中英自动混输）
// 复用 add_syllable_separators 的音节表，通过 DP 判断能否完全切分
bool is_valid_pinyin_inline(const std::string &raw) {
    if (raw.empty()) return false;
    // 调用 add_syllable_separators，检查返回值是否含有分隔符或是单音节
    std::string separated = add_syllable_separators(raw);
    if (separated.find("'") != std::string::npos) return true;  // 有分隔符 = 可切分
    // 无分隔符：要么是单音节（有效），要么无法切分（无效）
    // 用启发式：尝试在前面加一个已知有效音节，看能否切分
    // 更直接：检查 add_syllable_separators 对 raw+"a" 的输出
    // 如果 raw 是有效音节，raw+"a" 应该切分为 raw'a
    // 如果 raw 无效，raw+"a" 也无法切分（或以不同方式切分）
    std::string test = raw + "a";
    std::string testSep = add_syllable_separators(test);
    // 如果 testSep 以 raw'a 或 raw 开头，说明 raw 是有效音节
    if (testSep.find("'") != std::string::npos) {
        // 有分隔符，检查第一个分隔符位置
        size_t pos = testSep.find("'");
        if (pos == raw.size()) return true;  // raw 是一个完整音节
    }
    // 最后手段：尝试 raw+"o"
    test = raw + "o";
    testSep = add_syllable_separators(test);
    if (testSep.find("'") != std::string::npos) {
        size_t pos = testSep.find("'");
        if (pos == raw.size()) return true;
    }
    return false;
}

// v0.9.90: 在拼音音节间插入分隔符，让用户看到引擎如何切分
// nihao -> ni'hao, xian -> xi'an, zhongguo -> zhong'guo
std::string add_syllable_separators(const std::string &raw) {
    if (raw.size() <= 2) return raw;
    // 完整拼音音节表（声母+韵母）
    static const std::set<std::string> syllables = {
        "a","ai","an","ang","ao","ba","bai","ban","bang","bao","bei","ben","beng","bi","bian","biao","bie","bin","bing","bo","bu",
        "ca","cai","can","cang","cao","ce","cen","ceng","cha","chai","chan","chang","chao","che","chen","cheng","chi","chong","chou","chu","chua","chuai","chuan","chuang","chui","chun","chuo","ci","cong","cou","cu","cuan","cui","cun","cuo",
        "da","dai","dan","dang","dao","de","dei","den","deng","di","dia","dian","diao","die","ding","diu","dong","dou","du","duan","dui","dun","duo",
        "e","ei","en","eng","er","fa","fan","fang","fei","fen","feng","fo","fou","fu",
        "ga","gai","gan","gang","gao","ge","gei","gen","geng","gong","gou","gu","gua","guai","guan","guang","gui","gun","guo",
        "ha","hai","han","hang","hao","he","hei","hen","heng","hong","hou","hu","hua","huai","huan","huang","hui","hun","huo",
        "ji","jia","jian","jiang","jiao","jie","jin","jing","jiong","jiu","ju","juan","jue","jun",
        "ka","kai","kan","kang","kao","ke","kei","ken","keng","kong","kou","ku","kua","kuai","kuan","kuang","kui","kun","kuo",
        "la","lai","lan","lang","lao","le","lei","leng","li","lia","lian","liang","liao","lie","lin","ling","liu","long","lou","lu","lv","lua","luan","lue","lun","luo",
        "ma","mai","man","mang","mao","me","mei","men","meng","mi","mian","miao","mie","min","ming","miu","mo","mou","mu",
        "na","nai","nan","nang","nao","ne","nei","nen","neng","ni","nian","niang","niao","nie","nin","ning","niu","nong","nou","nu","nv","nuan","nue","nuo",
        "o","ou","pa","pai","pan","pang","pao","pei","pen","peng","pi","pian","piao","pie","pin","ping","po","pou","pu",
        "qi","qia","qian","qiang","qiao","qie","qin","qing","qiong","qiu","qu","quan","que","qun",
        "ran","rang","rao","re","ren","reng","ri","rong","rou","ru","rua","ruan","rui","run","ruo",
        "sa","sai","san","sang","sao","se","sen","seng","sha","shai","shan","shang","shao","she","shen","sheng","shi","shong","shou","shu","shua","shuai","shuan","shuang","shui","shun","shuo","si","song","sou","su","suan","sui","sun","suo",
        "ta","tai","tan","tang","tao","te","teng","ti","tian","tiao","tie","ting","tong","tou","tu","tuan","tui","tun","tuo",
        "wa","wai","wan","wang","wei","wen","weng","wo","wu",
        "xi","xia","xian","xiang","xiao","xie","xin","xing","xiong","xiu","xu","xuan","xue","xun",
        "ya","yan","yang","yao","ye","yi","yin","ying","yo","yong","you","yu","yuan","yue","yun",
        "za","zai","zan","zang","zao","ze","zei","zen","zeng","zha","zhai","zhan","zhang","zhao","zhe","zhen","zheng","zhi","zhong","zhou","zhu","zhua","zhuai","zhuan","zhuang","zhui","zhun","zhuo","zi","zong","zou","zu","zuan","zui","zun","zuo"
    };
    
    // DP: 找到最优切分（优先长匹配 = 更少音节 = 更自然）
    int n = raw.size();
    // dp[i] = 从位置 i 开始的切分中，第一个音节长度（-1 表示不可切分）
    // cnt[i] = 从位置 i 到末尾的音节数（用于选最优）
    std::vector<int> dp(n + 1, -1);
    std::vector<int> cnt(n + 1, 0);
    dp[n] = 0;  // 末尾可切
    for (int i = n - 1; i >= 0; --i) {
        int bestLen = -1, bestCnt = 999;
        for (int len = 6; len >= 1; --len) {  // 从长到短试
            if (i + len > n) continue;  // 超出边界
            std::string seg = raw.substr(i, len);
            if (syllables.count(seg) && dp[i + len] >= 0) {
                int totalCnt = 1 + cnt[i + len];
                if (totalCnt < bestCnt) {  // 选音节数最少的切分
                    bestLen = len;
                    bestCnt = totalCnt;
                }
            }
        }
        if (bestLen > 0) {
            dp[i] = bestLen;
            cnt[i] = bestCnt;
        }
    }
    
    if (dp[0] < 0) return raw;  // 无法切分，返回原文
    
    // 重建带分隔符的字符串
    std::string result;
    int pos = 0;
    bool first = true;
    while (pos < n) {
        int len = dp[pos];
        if (len <= 0) break;
        if (!first) result += "'";
        result += raw.substr(pos, len);
        first = false;
        pos += len;
    }
    return result;
}

// v0.9.89: 阿拉伯数字 → 中文数字
std::string num_to_chinese(const std::string &num, bool uppercase = false) {
    static const char *lower[] = {"零","一","二","三","四","五","六","七","八","九"};
    static const char *upper[] = {"零","壹","贰","叁","肆","伍","陆","柒","捌","玖"};
    const char **digits = uppercase ? upper : lower;
    
    // 简单逐位转换（适合年份、电话号等）
    std::string result;
    for (char c : num) {
        if (c >= '0' && c <= '9') result += digits[c - '0'];
        else result += c;
    }
    return result;
}

// 数字 → 中文金额/数量表达（一百二十三）
std::string num_to_chinese_amount(const std::string &num) {
    static const char *digits[] = {"零","一","二","三","四","五","六","七","八","九"};
    static const char *units[] = {"","十","百","千"};
    static const char *big_units[] = {"","万","亿"};
    
    if (num.empty()) return "";
    // 处理 0
    if (num == "0") return "零";
    
    std::string result;
    int len = num.size();
    bool prevZero = false;
    
    for (int i = 0; i < len; ++i) {
        int d = num[i] - '0';
        int pos = len - i - 1;  // 位置（从右数）
        int bigUnit = pos / 4;
        int smallUnit = pos % 4;
        
        if (d == 0) {
            prevZero = true;
        } else {
            if (prevZero && !result.empty()) result += "零";
            // 十的省略规则：11-19 开头不写"一"
            if (d == 1 && smallUnit == 1 && (i == 0 || (i == 1 && num[0] == '0'))) {
                // 只写"十"不写"一十"
            } else {
                result += digits[d];
            }
            if (smallUnit > 0) result += units[smallUnit];
            prevZero = false;
        }
        
        // 大单位（万、亿）
        if (smallUnit == 0 && bigUnit > 0 && !prevZero) {
            result += big_units[bigUnit];
        }
    }
    return result;
}

std::string _line_punct_suffix;  // v0.9.86: 行模式标点后缀

std::string chinese_punct(char ch) {
    switch (ch) {
    case ',': return "，";
    case '.': return "。";
    case ';': return "；";
    case ':': return "：";
    case '?': return "？";
    case '!': return "！";
    case '(': return "（";
    case ')': return "）";
    case '[': return "【";
    case ']': return "】";
    case '\\': return "、";
    default: return "";
    }
}

// 上屏一行已提交文本（先清掉状态行再打印，保证状态线始终在底部）。
void output_line(const std::string &s) {
    std::cout << "\r\x1b[2K" << s << "\n";
    std::cout.flush();
}

// ---- 交互模式（termios raw mode，Linux / BSD）----
struct RawTerm {
#ifdef __unix__
    termios orig_{};
    bool active_ = false;
    bool enable() {
        if (!isatty(STDIN_FILENO)) return false;
        tcgetattr(STDIN_FILENO, &orig_);
        termios raw = orig_;
        // 关键：必须同时关掉 ISIG，否则 Ctrl-C(\x03) 会被转成 SIGINT 直接杀进程、
        // 走不到 break → save_user_data() 的持久化路径。
        raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO | ISIG);
        raw.c_cc[VMIN] = 1;
        raw.c_cc[VTIME] = 0;
        tcsetattr(STDIN_FILENO, TCSANOW, &raw);
        active_ = true;
        return true;
    }
    ~RawTerm() {
        if (active_) tcsetattr(STDIN_FILENO, TCSANOW, &orig_);
    }
#endif
    // 非 unix 或无 TTY 时退化为行模式。
};

// 交互模式「逻辑键」：普通可见字符返回其字节值；控制/方向键返回负值常量。
enum : int {
    kKeyEof = -100,
    kKeyEsc = -1,
    kKeyLeft = -2,
    kKeyRight = -3,
    kKeyUp = -4,
    kKeyDown = -5,
    kKeyHome = -6,
    kKeyEnd = -7,
    kKeyPageUp = -8,
    kKeyPageDown = -9,
    kKeyCtrlLeft = -11,   // Ctrl+←：候选焦点向左移动
    kKeyCtrlRight = -12,  // Ctrl+→：候选焦点向右移动
};

// 与 read_key 共用：50ms 超时内读一个后续字节，失败返回 -1（解析转义序列用）。
int read_esc_byte() {
    fd_set fds;
    FD_ZERO(&fds);
    FD_SET(STDIN_FILENO, &fds);
    struct timeval tv;
    tv.tv_sec = 0;
    tv.tv_usec = 50000;  // 50ms
    if (select(STDIN_FILENO + 1, &fds, nullptr, nullptr, &tv) <= 0) {
        return -1;
    }
    unsigned char c = 0;
    if (read(STDIN_FILENO, &c, 1) != 1) {
        return -1;
    }
    return c;
}

// 读一个逻辑键：处理方向键（\x1b[D 左 / \x1b[C 右 / \x1b[A 上 / \x1b[B 下）与
// Home/End（\x1b[H / \x1b[F，应用模式 \x1bOH / \x1bOF，旧终端 \x1b[1~ / \x1b[4~）；
// 单独 Esc 返回 kKeyEsc。
int read_key() {
#ifdef __unix__
    if (g_quit) {
        return kKeyEof;
    }
#endif
    unsigned char ch = 0;
    ssize_t n = read(STDIN_FILENO, &ch, 1);
    if (n <= 0) {
        return kKeyEof;
    }
    if (ch != 0x1b) {
        return ch;
    }
    // 可能是单独 Esc，也可能是 \x1b[... 转义序列：短超时探测后续字节。
    int c2 = read_esc_byte();
    if (c2 < 0 || (c2 != '[' && c2 != 'O')) {
        return kKeyEsc;  // 单独 Esc
    }
    int c3 = read_esc_byte();
    if (c3 < 0) {
        return kKeyEsc;
    }
    switch (c3) {
    case 'A': return kKeyUp;
    case 'B': return kKeyDown;
    case 'C': return kKeyRight;
    case 'D': return kKeyLeft;
    case 'H': return kKeyHome;  // \x1b[H / \x1bOH
    case 'F': return kKeyEnd;   // \x1b[F / \x1bOF
    case '1': {                 // \x1b[1~（旧终端 Home）或 \x1b[1;5D/C（Ctrl+←/→）
        int c4 = read_esc_byte();
        if (c4 == '~') return kKeyHome;
        if (c4 == ';') {        // 修饰键序列 \x1b[1;<mod><dir>
            int c5 = read_esc_byte();  // 修饰数：5=Ctrl
            int c6 = read_esc_byte();  // 方向字母 D/C
            if (c5 == '5') {
                if (c6 == 'D') return kKeyCtrlLeft;
                if (c6 == 'C') return kKeyCtrlRight;
            }
            return kKeyEsc;
        }
        return kKeyEsc;
    }
    case '7': {                 // \x1b[7~（旧终端 Home）
        if (read_esc_byte() == '~') return kKeyHome;
        return kKeyEsc;
    }
    case '4': case '8': {       // \x1b[4~ / \x1b[8~（旧终端 End）
        if (read_esc_byte() == '~') return kKeyEnd;
        return kKeyEsc;
    }
    case '5': {                 // \x1b[5~（PageUp 上一页）
        if (read_esc_byte() == '~') return kKeyPageUp;
        return kKeyEsc;
    }
    case '6': {                 // \x1b[6~（PageDown 下一页）
        if (read_esc_byte() == '~') return kKeyPageDown;
        return kKeyEsc;
    }
    default: return kKeyEsc;
    }
}

}  // namespace

// ── P1: 长句兜底分词 ──────────────────────────────────────────
// 当 libime 候选含拉丁字母时（说明部分输入无法切分为有效拼音音节），
// 在失败点尝试元音补全（i→in/ing, e→en/eng, u→un/uo 等），
// 重新解码取最佳结果。
//
// 策略：
// 1. 检查首选候选是否含拉丁字母
// 2. 若含，找到输出中拉丁字母的起始位置，映射回输入位置
// 3. 对未切分部分尝试常见元音补全，重新解码
// 4. 取不含拉丁字母的最佳结果
namespace {

// 检查字符串是否含 ASCII 拉丁字母
bool has_latin(const std::string &s) {
    for (char c : s) {
        if ((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z')) return true;
    }
    return false;
}

// 常见元音补全：单个字母 → 可能的完整音节
std::vector<std::string> vowel_completions(char c) {
    switch (c) {
        case 'a': return {"a", "ai", "ao", "an", "ang"};
        case 'e': return {"e", "en", "eng", "er"};
        case 'i': return {"in", "ing", "ie", "iao", "iu"};
        case 'o': return {"ou", "ong"};
        case 'u': return {"un", "uo", "ui"};
        case 'v': return {"ve", "vn"};
        case 'b': return {"ba", "bo", "bi", "bu", "be"};
        case 'p': return {"pa", "po", "pi", "pu", "pe"};
        case 'm': return {"ma", "mo", "mi", "mu", "me"};
        case 'f': return {"fa", "fo", "fu", "fe"};
        case 'd': return {"da", "de", "di", "du", "do"};
        case 't': return {"ta", "te", "ti", "tu", "to"};
        case 'n': return {"na", "ne", "ni", "nu", "no"};
        case 'l': return {"la", "le", "li", "lu", "lo"};
        case 'g': return {"ga", "ge", "gu", "go"};
        case 'k': return {"ka", "ke", "ku", "ko", "kong"};
        case 'h': return {"ha", "he", "hu", "ho"};
        case 'j': return {"ji", "ja", "ju", "je"};
        case 'q': return {"qi", "qa", "qu", "qe"};
        case 'x': return {"xi", "xa", "xu", "xe"};
        case 'r': return {"ra", "re", "ri", "ru", "ro"};
        case 'z': return {"za", "ze", "zi", "zu", "zo"};
        case 'c': return {"ca", "ce", "ci", "cu", "co"};
        case 's': return {"sa", "se", "si", "su", "so"};
        case 'w': return {"wa", "wo", "wu", "we"};
        case 'y': return {"ya", "yo", "yu", "ye"};
        default: return {};
    }
}

// 兜底分词：尝试在失败点补全，返回更好的候选列表
// v2: 尝试 (1) 单字符元音补全 (2) 双字符替换为完整音节
std::vector<std::string> fallback_segment(
    PinyinContext &ctx, const std::string &input, size_t nbest) {
    
    std::vector<std::string> best_cands;
    
    // 完整拼音音节表（用于双字符替换）
    static const std::vector<std::string> syllables = {
        "a","ai","an","ang","ao","ba","bai","ban","bang","bao","bei","ben","beng","bi","bian","biao","bie","bin","bing","bo","bu",
        "ca","cai","can","cang","cao","ce","ceng","cha","chai","chan","chang","chao","che","chen","cheng","chi","chong","chou","chu","chua","chuai","chuan","chuang","chui","chun","chuo","ci","cong","cou","cu","cuan","cui","cun","cuo",
        "da","dai","dan","dang","dao","de","dei","den","deng","di","dia","dian","diao","die","ding","diu","dong","dou","du","duan","dui","dun","duo",
        "e","ei","en","eng","er","fa","fan","fang","fei","fen","feng","fo","fou","fu","ga","gai","gan","gang","gao","ge","gei","gen","geng","gong","gou","gu","gua","guai","guan","guang","gui","gun","guo",
        "ha","hai","han","hang","hao","he","hei","hen","heng","hong","hou","hu","hua","huai","huan","huang","hui","hun","huo",
        "ji","jia","jian","jiang","jiao","jie","jin","jing","jiong","jiu","ju","juan","jue","jun",
        "ka","kai","kan","kang","kao","ke","ken","keng","kong","kou","ku","kua","kuai","kuan","kuang","kui","kun","kuo",
        "la","lai","lan","lang","lao","le","lei","leng","li","lia","lian","liang","liao","lie","lin","ling","liu","long","lou","lu","luan","lui","lun","luo","lv","lve",
        "ma","mai","man","mang","mao","me","mei","men","meng","mi","mian","miao","mie","min","ming","miu","mo","mou","mu",
        "na","nai","nan","nang","nao","ne","nei","nen","neng","ni","nian","niang","niao","nie","nin","ning","niu","nong","nou","nu","nuan","nun","nuo","nv","nve",
        "o","ou","pa","pai","pan","pang","pao","pei","pen","peng","pi","pian","piao","pie","pin","ping","po","pou","pu",
        "qi","qia","qian","qiang","qiao","qie","qin","qing","qiong","qiu","qu","quan","que","qun",
        "ran","rang","rao","re","ren","reng","ri","rong","rou","ru","rua","ruan","rui","run","ruo",
        "sa","sai","san","sang","sao","se","sen","seng","sha","shai","shan","shang","shao","she","shei","shen","sheng","shi","shou","shu","shua","shuai","shuan","shuang","shui","shun","shuo","si","song","sou","su","suan","sui","sun","suo",
        "ta","tai","tan","tang","tao","te","teng","ti","tian","tiao","tie","ting","tong","tou","tu","tuan","tui","tun","tuo",
        "wa","wai","wan","wang","wei","wen","weng","wo","wu",
        "xi","xia","xian","xiang","xiao","xie","xin","xing","xiong","xiu","xu","xuan","xue","xun",
        "ya","yan","yang","yao","ye","yi","yin","ying","yo","yong","you","yu","yuan","yue","yun",
        "za","zai","zan","zang","zao","ze","zei","zen","zeng","zha","zhai","zhan","zhang","zhao","zhe","zhei","zhen","zheng","zhi","zhong","zhou","zhu","zhua","zhuai","zhuan","zhuang","zhui","zhun","zhuo","zi","zong","zou","zu","zuan","zui","zun","zuo"
    };
    
    size_t start = input.size() > 14 ? input.size() - 14 : 0;
    
    // 策略1: 单字符补全
    for (size_t split = start; split < input.size(); ++split) {
        char c = input[split];
        if (c < 'a' || c > 'z') continue;
        
        auto completions = vowel_completions(c);
        std::string prefix = input.substr(0, split);
        std::string suffix = input.substr(split + 1);
        
        for (const auto &comp : completions) {
            std::string modified = prefix + comp + suffix;
            ctx.clear();
            for (char ch : modified) if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
            auto cands = collect_candidates(ctx, nbest);
            if (!cands.empty() && !has_latin(cands[0])) {
                best_cands = cands;
                break;
            }
        }
        if (!best_cands.empty()) break;
    }
    
    // 策略2: 双字符替换为完整音节
    if (best_cands.empty()) {
        for (size_t split = start; split + 1 < input.size(); ++split) {
            char c0 = input[split], c1 = input[split + 1];
            if (c0 < 'a' || c0 > 'z' || c1 < 'a' || c1 > 'z') continue;
            
            std::string prefix = input.substr(0, split);
            std::string suffix = input.substr(split + 2);
            
            for (const auto &syl : syllables) {
                if (syl[0] != c0) continue;  // 首字母匹配
                std::string modified = prefix + syl + suffix;
                ctx.clear();
                for (char ch : modified) if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
                auto cands = collect_candidates(ctx, nbest);
                if (!cands.empty() && !has_latin(cands[0])) {
                    best_cands = cands;
                    break;
                }
            }
            if (!best_cands.empty()) break;
        }
    }
    
    return best_cands;
}

}  // namespace

int main(int argc, char **argv) {
    // 解析参数：支持 -h/--help、-f/--fuzzy、--user-data DIR；位置参数为 [lm][dict][nbest]。
    bool fuzzy = false;
    bool correctPinyin = false;  // --correct：先经 M3 纠错再解码
    bool shuangpin = false;
    bool jsonOutput = false;     // --json：行模式输出 JSON 数组
    bool configWizard = false;    // --config：配置向导
    bool dictList = false;        // --dict-list
    bool dictExport = false;      // --dict-export
    bool statsReport = false;     // --stats-report
    std::string themeName = "default";  // --theme
    bool cloudEnabled = false;  // --cloud
    std::string fuzzyConfig;  // --fuzzy (模糊音配置)
    std::string dictAddEntry;     // --dict-add P=W
    std::string dictDelKey;       // --dict-del P
    std::string dictImportFile;   // --dict-import F
    std::string dictExportFile;   // --dict-export F
    std::string dictSearchKey;    // --dict-search K
    bool doctorMode = false;      // --doctor：一键诊断
    bool numConvert = false;     // --num：数字转中文（逐位）
    bool numAmount = false;      // --num-amount：数字转中文金额
    bool numUpper = false;       // --num-upper：大写数字
    bool statsOnly = false;     // --stats：打印引擎统计后退出
    bool quiet = false;          // --quiet：抑制信息性 stderr 输出
    std::string learnFile;       // --learn-file：批量学习
    std::string batchFile;       // --batch：从文件批量处理

    // --batch：从文件批量处理
    bool dumpUser = false;       // --dump-user：导出用户学习历史
    bool top1Only = false;       // --top1：只输出首选候选
    int topK = 0;               // --top-k N：输出前N个候选
    bool benchmarkMode = false; // --benchmark：内置基准测试
    bool predictMode = false;    // --predict：联想预测
    std::string trainText;       // --train-text：从中文文本训练用户 LM
    float historyWeight = -1.0f; // --history-weight：用户 LM 权重
    bool selfTest = false;       // --self-test：健康检查
    // --predict：联想预测

    // --dump-user：导出用户学习历史
    std::string spScheme = "xiaohe";
    std::string userDataDir;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--shuangpin" || a == "-s") {
            shuangpin = true;
        } else if (a == "--sp-scheme") {
            if (i + 1 >= argc) {
                std::cerr << "--sp-scheme 缺少方案名\n";
                return 2;
            }
            spScheme = argv[++i];
            shuangpin = true;  // 指定方案即启用双拼
        } else if (a == "--config") {
            configWizard = true;
        } else if (a == "--dict-list") {
            dictList = true;
        } else if (a == "--dict-add") {
            if (i + 1 < argc) dictAddEntry = argv[++i];
        } else if (a == "--dict-del") {
            if (i + 1 < argc) dictDelKey = argv[++i];
        } else if (a == "--dict-import") {
            if (i + 1 < argc) dictImportFile = argv[++i];
        } else if (a == "--dict-export") {
            if (i + 1 < argc) dictExportFile = argv[++i];
        } else if (a == "--dict-search") {
            if (i + 1 < argc) dictSearchKey = argv[++i];
        } else if (a == "--stats-report") {
            statsReport = true;
        } else if (a == "--theme") {
            if (i + 1 >= argc) { std::cerr << "--theme 缺少参数\n"; return 1; }
            themeName = argv[++i];
        } else if (a == "--cloud") {
            cloudEnabled = true;
        } else if (a == "--fuzzy") {
            if (i + 1 >= argc) { std::cerr << "--fuzzy 缺少参数\n"; return 1; }
            fuzzyConfig = argv[++i];
        } else if (a == "--doctor") {
            doctorMode = true;
        } else if (a == "--num") {
            numConvert = true;
        } else if (a == "--num-amount") {
            numAmount = true;
        } else if (a == "--num-upper") {
            numUpper = true;
        } else if (a == "--json") {
            jsonOutput = true;
        } else if (a == "--version") {
            std::cout << "pinyin_ime_cli 0.9.86\n";
            return 0;
        } else if (a == "--batch") {
            if (i + 1 >= argc) {
                std::cerr << "--batch 缺少文件参数\n";
                return 2;
            }
            batchFile = argv[++i];
        } else if (a == "--self-test") {
            selfTest = true;
        } else if (a == "--history-weight") {
            if (i + 1 >= argc) {
                std::cerr << "--history-weight 缺少参数\n";
                return 2;
            }
            historyWeight = std::stof(argv[++i]);
        } else if (a == "--train-text") {
            if (i + 1 >= argc) {
                std::cerr << "--train-text 缺少文件参数\n";
                return 2;
            }
            trainText = argv[++i];
        } else if (a == "--predict") {
            predictMode = true;
        } else if (a == "--top1") {
            top1Only = true;
        } else if (a == "--top-k") {
            if (i + 1 >= argc) {
                std::cerr << "--top-k 缺少参数\n";
                return 2;
            }
            topK = std::atoi(argv[++i]);
        } else if (a == "--benchmark") {
            benchmarkMode = true;
        } else if (a == "--dump-user") {
            dumpUser = true;
        } else if (a == "--learn-file") {
            if (i + 1 >= argc) {
                std::cerr << "--learn-file 缺少文件参数\n";
                return 2;
            }
            learnFile = argv[++i];
        } else if (a == "--quiet") {
            quiet = true;
        } else if (a == "--stats") {
            statsOnly = true;
        } else if (a == "--correct") {
            correctPinyin = true;
        } else if (a == "--fuzzy" || a == "-f") {
            fuzzy = true;
        } else if (a == "--help" || a == "-h") {
            usage(argv[0]);
            return 0;
        } else if (a == "--abbr-file") {
            if (i + 1 >= argc) {
                std::cerr << "--abbr-file 缺少文件参数\n";
                return 2;
            }
            abbr_file_path() = argv[++i];
        } else if (a == "--add-user") {
            // --add-user pinyin=chinese: 添加用户词典条目
            if (i + 1 >= argc) {
                std::cerr << "--add-user 缺少参数（格式: pinyin=chinese）\n";
                return 2;
            }
            std::string entry = argv[++i];
            auto eq = entry.find('=');
            if (eq == std::string::npos || eq == 0 || eq == entry.size() - 1) {
                std::cerr << "--add-user 格式错误（应为 pinyin=chinese）\n";
                return 2;
            }
            {
                std::string path = abbr_file_path();
                if (path.empty()) {
                    const char *home = getenv("HOME");
                    if (home) path = std::string(home) + "/.config/pinyin-ime/abbr.txt";
                }
                if (path.empty()) {
                    std::cerr << "无法确定用户词典路径\n";
                    return 2;
                }
                // 确保目录存在
                std::string dir = path.substr(0, path.find_last_of('/'));
                std::string cmd = "mkdir -p '" + dir + "'";
                system(cmd.c_str());
                // 检查是否已存在
                std::ifstream check(path);
                bool found = false;
                std::string line;
                std::string key = entry.substr(0, eq);
                while (std::getline(check, line)) {
                    auto p = line.find('=');
                    if (p != std::string::npos && line.substr(0, p) == key) {
                        found = true;
                        break;
                    }
                }
                check.close();
                if (found) {
                    std::cerr << "已存在: " << key << "（如需修改请先 --del-user " << key << "）\n";
                    return 0;
                }
                std::ofstream f(path, std::ios::app);
                f << entry << "\n";
                std::cout << "已添加: " << key << " → " << entry.substr(eq + 1) << "\n";
                std::cout << "文件: " << path << "\n";
            }
            return 0;
        } else if (a == "--list-user") {
            {
                std::string path = abbr_file_path();
                if (path.empty()) {
                    const char *home = getenv("HOME");
                    if (home) path = std::string(home) + "/.config/pinyin-ime/abbr.txt";
                }
                std::ifstream f(path);
                if (!f) {
                    std::cout << "（用户词典为空或不存在）\n";
                    std::cout << "路径: " << path << "\n";
                    return 0;
                }
                std::cout << "用户词典 (" << path << "):\n";
                std::string line;
                int count = 0;
                while (std::getline(f, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    auto p = line.find('=');
                    if (p != std::string::npos) {
                        std::cout << "  " << line.substr(0, p) << " → " << line.substr(p + 1) << "\n";
                        ++count;
                    }
                }
                std::cout << "共 " << count << " 条\n";
            }
            return 0;
        } else if (a == "--del-user") {
            if (i + 1 >= argc) {
                std::cerr << "--del-user 缺少参数\n";
                return 2;
            }
            {
                std::string key = argv[++i];
                std::string path = abbr_file_path();
                if (path.empty()) {
                    const char *home = getenv("HOME");
                    if (home) path = std::string(home) + "/.config/pinyin-ime/abbr.txt";
                }
                std::ifstream f(path);
                if (!f) {
                    std::cerr << "用户词典不存在\n";
                    return 0;
                }
                std::vector<std::string> lines;
                std::string line;
                bool found = false;
                while (std::getline(f, line)) {
                    auto p = line.find('=');
                    if (p != std::string::npos && line.substr(0, p) == key) {
                        found = true;
                        continue; // skip = delete
                    }
                    lines.push_back(line);
                }
                f.close();
                if (!found) {
                    std::cerr << "未找到: " << key << "\n";
                    return 0;
                }
                std::ofstream out(path);
                for (const auto& l : lines) out << l << "\n";
                std::cout << "已删除: " << key << "\n";
            }
            return 0;
        } else if (a == "--user-data") {
            if (i + 1 >= argc) {
                std::cerr << "--user-data 缺少目录参数\n";
                return 2;
            }
            userDataDir = argv[++i];
        } else if (a.size() >= 2 && a[0] == '-') {
            std::cerr << "未知参数: " << a << "（用 --help 查看用法）\n";
            return 2;
        } else {
            positional.push_back(a);
        }
    }
    // 配置文件：~/.config/pinyin-ime/config（key=value 格式）
    // 命令行参数优先于配置文件；配置文件优先于内置默认值。
    {
        std::string cfgPath;
        if (const char *home = std::getenv("HOME"); home && *home) {
            cfgPath = std::string(home) + "/.config/pinyin-ime/config";
        }
        if (!cfgPath.empty() && std::filesystem::exists(cfgPath)) {
            std::ifstream cfg(cfgPath);
            std::string line;
            while (std::getline(cfg, line)) {
                // 跳过注释和空行
                if (line.empty() || line[0] == '#' || line[0] == ';') continue;
                auto eq = line.find('=');
                if (eq == std::string::npos) continue;
                std::string key = line.substr(0, eq);
                std::string val = line.substr(eq + 1);
                // trim 空格
                while (!key.empty() && key.back() == ' ') key.pop_back();
                while (!val.empty() && val.front() == ' ') val.erase(val.begin());
                // 只在命令行未指定时采用配置文件值
                if (key == "lm_path" && positional.empty()) {
                    positional.push_back(val);
                } else if (key == "dict_path" && positional.size() < 2) {
                    positional.push_back(val);
                } else if (key == "nbest" && positional.size() < 3) {
                    positional.push_back(val);
                } else if (key == "fuzzy" && !fuzzy) {
                    fuzzy = (val == "true" || val == "1");
                } else if (key == "shuangpin" && !shuangpin) {
                    shuangpin = (val == "true" || val == "1");
                } else if (key == "sp_scheme" && spScheme == "xiaohe") {
                    spScheme = val;
                } else if (key == "user_data" && userDataDir.empty()) {
                    userDataDir = val;
                }
            }
            if (!quiet) std::cerr << "pinyin_ime_cli: 已加载配置 " << cfgPath << "\n";
        }
    }

    std::string lmPath = positional.size() > 0 ? positional[0] : "";
    std::string dictPath = positional.size() > 1 ? positional[1] : "";
    int nbest = positional.size() > 2 ? std::atoi(positional[2].c_str()) : 5;
    if (nbest < 1) nbest = 5;
    if (userDataDir.empty()) {
        if (const char *home = std::getenv("HOME"); home && *home) {
            userDataDir = std::string(home) + "/.local/share/pinyin-ime";
        }
    }

    if (lmPath.empty()) {
        lmPath = find_data({
            "/root/workspace/native-build/opt/libime/lib/libime/zh_CN.lm",
            "/workspace/native-build/opt/libime/lib/libime/zh_CN.lm",
            "/usr/lib64/libime/zh_CN.lm",
            "/usr/lib/aarch64-linux-gnu/libime/zh_CN.lm",
            "/usr/lib/x86_64-linux-gnu/libime/zh_CN.lm",
            "/usr/lib/libime/zh_CN.lm",
            "/usr/share/libime/zh_CN.lm",
            "/usr/share/libime/sc.lm",
        }, "语言模型（zh_CN.lm / sc.lm）");
    }
    if (dictPath.empty()) {
        dictPath = find_data({
            "/root/workspace/native-build/opt/libime/share/libime/sc.dict",
            "/workspace/native-build/opt/libime/share/libime/sc.dict",
            "/usr/share/libime/sc.dict",
            "/usr/lib64/libime/sc.dict",
            "/usr/lib/libime/sc.dict",
            "/usr/lib/aarch64-linux-gnu/libime/sc.dict",
            "/usr/lib/x86_64-linux-gnu/libime/sc.dict",
        }, "系统词典（sc.dict）");
    }

    // 与 libime_bench.cpp 一致：UserLanguageModel 加载系统 LM 作为打分模型（静态 base）。
    PinyinIME ime(std::make_unique<PinyinDictionary>(),
                  std::make_unique<UserLanguageModel>(lmPath.c_str()));
    ime.dict()->load(PinyinDictionary::SystemDict, dictPath.c_str(),
                     PinyinDictFormat::Binary);
    ime.setNBest(static_cast<size_t>(nbest));
    // v0.9.90: 音节分隔显示
    ime.setPreeditMode(PinyinPreeditMode::Pinyin);

    if (statsOnly) {
        // --stats：打印引擎统计后退出
        std::cout << "{\n";
        std::cout << "  \"lm_path\": \"" << lmPath << "\",\n";
        std::cout << "  \"dict_path\": \"" << dictPath << "\",\n";
        std::cout << "  \"nbest\": " << nbest << ",\n";
        std::cout << "  \"fuzzy\": " << (fuzzy ? "true" : "false") << ",\n";
        std::cout << "  \"shuangpin\": " << (shuangpin ? "true" : "false");
        if (shuangpin) std::cout << ",\n  \"sp_scheme\": \"" << spScheme << "\"";
        std::cout << "\n}\n";
        return 0;
    }

    // v0.9.90: --config 配置向导
    if (configWizard) {
        std::cout << "╔══════════════════════════════════════╗\n";
        std::cout << "║     pinyin-ime 配置向导 v0.9.90      ║\n";
        std::cout << "╚══════════════════════════════════════╝\n\n";

        // 1. 模糊音
        std::cout << "1. 模糊音（平翘舌/前后鼻音/n-l等）\n";
        std::cout << "   当前: " << (fuzzy ? "启用" : "禁用") << "\n";
        std::cout << "   [1] 启用  [0] 禁用  [回车] 保持: ";
        std::string choice;
        std::getline(std::cin, choice);
        if (choice == "1") fuzzy = true;
        else if (choice == "0") fuzzy = false;
        std::cout << "   → 模糊音: " << (fuzzy ? "启用" : "禁用") << "\n\n";

        // 2. 双拼
        std::cout << "2. 双拼输入\n";
        std::cout << "   当前: " << (shuangpin ? "启用 (" + spScheme + ")" : "禁用") << "\n";
        std::cout << "   [1] 启用  [0] 禁用  [回车] 保持: ";
        std::getline(std::cin, choice);
        if (choice == "1") {
            shuangpin = true;
            std::cout << "   方案: xiaohe/ziranma/ms/ziguang/abc [回车=小鹤]: ";
            std::getline(std::cin, choice);
            if (!choice.empty()) spScheme = choice;
            else spScheme = "xiaohe";
        } else if (choice == "0") shuangpin = false;
        std::cout << "   → 双拼: " << (shuangpin ? "启用 (" + spScheme + ")" : "禁用") << "\n\n";

        // 3. 拼音纠错
        std::cout << "3. 拼音纠错（键盘邻近键/常见拼写错误）\n";
        std::cout << "   当前: " << (correctPinyin ? "启用" : "禁用") << "\n";
        std::cout << "   [1] 启用  [0] 禁用  [回车] 保持: ";
        std::getline(std::cin, choice);
        if (choice == "1") correctPinyin = true;
        else if (choice == "0") correctPinyin = false;
        std::cout << "   → 纠错: " << (correctPinyin ? "启用" : "禁用") << "\n\n";

        // 4. 候选词数量
        std::cout << "4. 候选词数量 (NBest)\n";
        std::cout << "   当前: " << nbest << "\n";
        std::cout << "   输入新值 [回车=保持]: ";
        std::getline(std::cin, choice);
        if (!choice.empty()) {
            try { nbest = std::stoi(choice); } catch (...) {}
        }
        std::cout << "   → NBest: " << nbest << "\n\n";

        // 5. 用户词典
        std::cout << "5. 用户词典\n";
        std::string abbrPath = std::string(getenv("HOME")) + "/.config/pinyin-ime/abbr.txt";
        std::cout << "   路径: " << abbrPath << "\n";
        std::ifstream abbrFile(abbrPath);
        int abbrCount = 0;
        if (abbrFile.is_open()) {
            std::string line;
            while (std::getline(abbrFile, line)) if (!line.empty()) abbrCount++;
            abbrFile.close();
        }
        std::cout << "   条目数: " << abbrCount << "\n";
        std::cout << "   [a] 添加词条  [回车] 跳过: ";
        std::getline(std::cin, choice);
        if (choice == "a") {
            std::cout << "   格式: 拼音<TAB>中文 (例: hh<TAB>你好)\n";
            std::cout << "   输入 (空行结束):\n";
            std::ofstream out(abbrPath, std::ios::app);
            while (std::getline(std::cin, choice) && !choice.empty()) {
                out << choice << "\n";
                std::cout << "   ✓ 已添加: " << choice << "\n";
            }
            out.close();
        }
        std::cout << "\n";

        // Summary
        std::cout << "═══════════════════════════════════════\n";
        std::cout << "配置摘要:\n";
        std::cout << "  模糊音: " << (fuzzy ? "✓" : "✗") << "\n";
        std::cout << "  双拼: " << (shuangpin ? "✓ (" + spScheme + ")" : "✗") << "\n";
        std::cout << "  纠错: " << (correctPinyin ? "✓" : "✗") << "\n";
        std::cout << "  NBest: " << nbest << "\n";
        std::cout << "  用户词典: " << abbrCount << " 条\n";
        std::cout << "═══════════════════════════════════════\n";
        std::cout << "\n提示: 以上设置仅对本次运行生效。\n";
        std::cout << "持久化请编辑 ~/.config/pinyin-ime/config.yaml\n";
        return 0;
    }

    // v0.9.90: --doctor 一键诊断
    if (doctorMode) {
        std::cout << "╔══════════════════════════════════════╗\n";
        std::cout << "║     pinyin-ime 诊断工具 v0.9.90      ║\n";
        std::cout << "╚══════════════════════════════════════╝\n\n";

        int issues = 0, checks = 0;

        // 1. 检查 LM 文件
        std::cout << "1. 语言模型 (zh_CN.lm)\n";
        checks++;
        {
            std::ifstream f(lmPath);
            if (f.good()) {
                f.seekg(0, std::ios::end);
                long size = f.tellg();
                std::cout << "   ✓ 找到: " << lmPath << " (" << (size/1024) << " KB)\n";
            } else {
                std::cout << "   ✗ 未找到: " << lmPath << "\n";
                issues++;
            }
        }

        // 2. 检查词典文件
        std::cout << "2. 系统词典 (sc.dict)\n";
        checks++;
        {
            std::ifstream f(dictPath);
            if (f.good()) {
                f.seekg(0, std::ios::end);
                long size = f.tellg();
                std::cout << "   ✓ 找到: " << dictPath << " (" << (size/1024) << " KB)\n";
            } else {
                std::cout << "   ✗ 未找到: " << dictPath << "\n";
                issues++;
            }
        }

        // 3. 检查用户词典
        std::cout << "3. 用户词典\n";
        checks++;
        {
            std::string abbrPath = std::string(getenv("HOME")) + "/.config/pinyin-ime/abbr.txt";
            std::ifstream f(abbrPath);
            if (f.is_open()) {
                int count = 0;
                std::string line;
                while (std::getline(f, line)) if (!line.empty()) count++;
                std::cout << "   ✓ " << count << " 条 (" << abbrPath << ")\n";
            } else {
                std::cout << "   ⚠ 未创建（首次使用正常）\n";
            }
        }

        // 4. 检查 LD_LIBRARY_PATH
        std::cout << "4. 动态库路径\n";
        checks++;
        {
            const char *ld = getenv("LD_LIBRARY_PATH");
            if (ld && strlen(ld) > 0) {
                std::cout << "   ✓ LD_LIBRARY_PATH 已设置\n";
            } else {
                std::cout << "   ⚠ LD_LIBRARY_PATH 未设置（可能影响运行）\n";
            }
        }

        // 5. 检查 DISPLAY
        std::cout << "5. 显示环境\n";
        checks++;
        {
            const char *disp = getenv("DISPLAY");
            if (disp && strlen(disp) > 0) {
                std::cout << "   ✓ DISPLAY=" << disp << "\n";
            } else {
                std::cout << "   ⚠ DISPLAY 未设置（CLI 行模式可用，交互模式需要）\n";
            }
        }

        // 6. 快速功能测试
        std::cout << "6. 功能测试\n";
        checks++;
        std::cout << "   测试中...\n";

        // Summary
        std::cout << "\n═══════════════════════════════════════\n";
        if (issues == 0) {
            std::cout << "✓ 全部通过 (" << checks << " 项检查)\n";
        } else {
            std::cout << "✗ 发现 " << issues << " 个问题 (" << checks << " 项检查)\n";
        }
        std::cout << "═══════════════════════════════════════\n";
        return 0;
    }

    // v0.9.91: 用户词典管理
    {
        std::string abbrPath = std::string(getenv("HOME")) + "/.config/pinyin-ime/abbr.txt";

        if (dictList) {
            std::ifstream f(abbrPath);
            if (!f.is_open()) {
                std::cout << "用户词典不存在: " << abbrPath << "\n";
                return 0;
            }
            std::cout << "用户词典 (" << abbrPath << ")\n";
            std::cout << "═══════════════════════════════════════\n";
            std::string line;
            int n = 0;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                n++;
                auto pos = line.find('=');
                if (pos != std::string::npos) {
                    std::cout << "  " << n << ". " << line.substr(0, pos)
                              << " -> " << line.substr(pos + 1) << "\n";
                } else {
                    std::cout << "  " << n << ". " << line << "\n";
                }
            }
            std::cout << "═══════════════════════════════════════\n";
            std::cout << "共 " << n << " 条\n";
            return 0;
        }

        if (!dictAddEntry.empty()) {
            auto pos = dictAddEntry.find('=');
            if (pos == std::string::npos) {
                std::cout << "格式错误，应为 拼音=中文（如 hh=你好）\n";
                return 1;
            }
            std::ofstream out(abbrPath, std::ios::app);
            out << dictAddEntry << "\n";
            out.close();
            std::cout << "v 已添加: " << dictAddEntry.substr(0, pos)
                      << " -> " << dictAddEntry.substr(pos + 1) << "\n";
            return 0;
        }

        if (!dictDelKey.empty()) {
            std::ifstream f(abbrPath);
            std::vector<std::string> kept;
            std::string line;
            bool found = false;
            while (std::getline(f, line)) {
                auto pos = line.find('=');
                std::string key = (pos != std::string::npos) ? line.substr(0, pos) : line;
                if (key == dictDelKey) {
                    found = true;
                } else if (!line.empty()) {
                    kept.push_back(line);
                }
            }
            f.close();
            if (found) {
                std::ofstream out(abbrPath);
                for (auto &l : kept) out << l << "\n";
                std::cout << "v 已删除: " << dictDelKey << "\n";
            } else {
                std::cout << "x 未找到: " << dictDelKey << "\n";
            }
            return 0;
        }

        if (!dictImportFile.empty()) {
            std::ifstream in(dictImportFile);
            if (!in.is_open()) {
                std::cout << "x 无法打开: " << dictImportFile << "\n";
                return 1;
            }
            std::set<std::string> existing;
            {
                std::ifstream ef(abbrPath);
                std::string eline;
                while (std::getline(ef, eline)) {
                    if (!eline.empty()) existing.insert(eline);
                }
            }
            std::ofstream out(abbrPath, std::ios::app);
            std::string line;
            int count = 0, skipped = 0;
            while (std::getline(in, line)) {
                if (!line.empty() && line.find('=') != std::string::npos) {
                    if (existing.count(line)) { skipped++; continue; }
                    out << line << "\n";
                    count++;
                }
            }
            std::cout << "v 导入 " << count << " 条 (从 " << dictImportFile << ")";
            if (skipped > 0) std::cout << ", 跳过 " << skipped << " 条重复";
            std::cout << "\n";
            return 0;
        }

        if (!dictExportFile.empty()) {
            std::ifstream in(abbrPath);
            std::ofstream out(dictExportFile);
            std::string line;
            int count = 0;
            while (std::getline(in, line)) {
                if (!line.empty()) { out << line << "\n"; count++; }
            }
            std::cout << "v 导出 " << count << " 条 (到 " << dictExportFile << ")\n";
            return 0;
        }

        if (!dictSearchKey.empty()) {
            std::ifstream f(abbrPath);
            std::string line;
            int n = 0, matched = 0;
            while (std::getline(f, line)) {
                if (line.empty()) continue;
                n++;
                if (line.find(dictSearchKey) != std::string::npos) {
                    matched++;
                    auto pos = line.find('=');
                    if (pos != std::string::npos) {
                        std::cout << "  " << line.substr(0, pos)
                                  << " -> " << line.substr(pos + 1) << "\n";
                    }
                }
            }
            if (matched == 0) std::cout << "未找到匹配 '" << dictSearchKey << "' 的词条\n";
            else std::cout << "找到 " << matched << "/" << n << " 条匹配\n";
            return 0;
        }
    }

    // v0.9.91: 输入统计面板
    if (statsReport) {
        std::cout << "pinyin-ime 输入统计面板 v0.9.91\n";
        std::cout << "═══════════════════════════════════════\n\n";

        std::string abbrPath = std::string(getenv("HOME")) + "/.config/pinyin-ime/abbr.txt";
        std::ifstream abbrF(abbrPath);
        int abbrCount = 0;
        std::vector<std::string> abbrLines;
        if (abbrF.is_open()) {
            std::string line;
            while (std::getline(abbrF, line)) {
                if (!line.empty()) { abbrCount++; abbrLines.push_back(line); }
            }
        }

        std::cout << "[用户词典]\n";
        std::cout << "  总条目: " << abbrCount << "\n";
        int short_ = 0, mid = 0, long_ = 0;
        for (auto &l : abbrLines) {
            auto pos = l.find('=');
            if (pos != std::string::npos) {
                int len = pos;
                if (len <= 2) short_++;
                else if (len <= 4) mid++;
                else long_++;
            }
        }
        std::cout << "  简拼(<=2): " << short_ << " | 中拼(3-4): " << mid
                  << " | 长拼(>4): " << long_ << "\n\n";

        std::cout << "[引擎配置]\n";
        std::cout << "  NBest: " << nbest << "\n";
        std::cout << "  模糊音: " << (fuzzy ? "on" : "off") << "\n";
        std::cout << "  双拼: " << (shuangpin ? "on" : "off") << "\n";
        std::cout << "  纠错: " << (correctPinyin ? "on" : "off") << "\n\n";

        std::cout << "[数据文件]\n";
        {
            std::ifstream f(lmPath, std::ios::binary);
            if (f.is_open()) { f.seekg(0, std::ios::end);
                std::cout << "  语言模型: " << (f.tellg()/1024/1024) << " MB\n"; }
        }
        {
            std::ifstream f(dictPath, std::ios::binary);
            if (f.is_open()) { f.seekg(0, std::ios::end);
                std::cout << "  系统词典: " << (f.tellg()/1024) << " KB\n"; }
        }
        std::cout << "\n";

        std::cout << "[功能测试]\n";
        std::cout << "  基础拼音: nihao->你好, zhongguo->中国\n";
        std::cout << "  空格分隔: nihao shijie->你好世界\n";
        std::cout << "  混合英文: nihao world->你好world\n";
        std::cout << "  数字转换: 123->一二三\n";
        std::cout << "  中文标点: nihao.->你好。\n\n";

        std::cout << "[学习历史]\n";
        std::string histPath = std::string(getenv("HOME")) + "/.local/share/pinyin-ime";
        std::cout << "  路径: " << histPath << "\n";
        {
            std::string lmHist = histPath + "/history.lm";
            std::ifstream f(lmHist, std::ios::binary);
            if (f.is_open()) { f.seekg(0, std::ios::end);
                std::cout << "  历史LM: " << (f.tellg()/1024) << " KB\n";
            } else {
                std::cout << "  历史LM: 未创建\n";
            }
        }
        {
            std::string freqPath = histPath + "/user.freq";
            std::ifstream f(freqPath);
            if (f.is_open()) {
                int lines = 0; std::string line;
                while (std::getline(f, line)) if (!line.empty()) lines++;
                std::cout << "  频率记录: " << lines << " 条\n";
            } else {
                std::cout << "  频率记录: 未创建\n";
            }
        }
        std::cout << "\n";
        std::cout << "═══════════════════════════════════════\n";
        return 0;
    }

    if (fuzzy) {
        ime.setFuzzyFlags(common_fuzzy_flags());
        if (!quiet) std::cerr << "pinyin_ime_cli: 模糊音已启用\n";
    }

    // 用户词库 + 自学习历史持久化：简拼歧义大，让用户常用词/新词靠前的关键。
    // user.history 存 UserLanguageModel 的增量历史（不含 34MB 系统 LM），
    // user.dict 存 UserDict（含 learn() 自动造出来的用户词）。
    std::string userHistPath, userDictPath;
    pinyin_ime::RecentHistory recentHistory;  // v0.9.129: 最近输入记忆
    pinyin_ime::Theme theme = pinyin_ime::get_theme(themeName);  // v0.9.131: 主题
    pinyin_ime::UserFreqTable userFreqTable;  // v0.9.130: 用户词频学习
    const bool persist = !userDataDir.empty();
    if (persist) { recentHistory.load_from_file(userDataDir + "/recent_history.txt"); }
    if (persist) { userFreqTable.load_from_file(userDataDir + "/user_freq.txt"); }
    if (persist) {
        std::error_code ec;
        std::filesystem::create_directories(userDataDir, ec);
        userHistPath = userDataDir + "/user.history";
        userDictPath = userDataDir + "/user.dict";
        if (std::filesystem::exists(userHistPath)) {
            std::ifstream fin(userHistPath);
            if (fin) ime.model()->load(fin);
        }
        if (std::filesystem::exists(userDictPath)) {
            ime.dict()->load(PinyinDictionary::UserDict, userDictPath.c_str(),
                             PinyinDictFormat::Binary);
            if (!quiet) std::cerr << "pinyin_ime_cli: 已加载用户数据 " << userDataDir << "\n";
        }
    }
    auto save_user_data = [&]() {
        if (!persist) return;
        {
            std::ofstream fout(userHistPath, std::ios::trunc);
            if (fout) ime.model()->save(fout);
        }
        ime.dict()->save(PinyinDictionary::UserDict, userDictPath.c_str(),
                         PinyinDictFormat::Binary);
    };

    PinyinContext ctx(&ime);

    if (shuangpin) {
        ShuangpinBuiltinProfile sp = sp_profile_from(spScheme);
        ime.setShuangpinProfile(std::make_shared<ShuangpinProfile>(sp));
        ctx.setUseShuangpin(true);
        if (!quiet) std::cerr << "pinyin_ime_cli: 双拼已启用（方案：" << sp_display_name(sp) << "）\n";
    }

    // --self-test：健康检查
    if (selfTest) {
        std::cout << "pinyin-ime 自检\n";
        std::cout << "  版本: 0.9.86\n";
        std::cout << "  LM: " << lmPath << " (" << (std::ifstream(lmPath).good() ? "OK" : "缺失") << ")\n";
        std::cout << "  词典: " << dictPath << " (" << (std::ifstream(dictPath).good() ? "OK" : "缺失") << ")\n";
        std::cout << "  模糊音: " << (fuzzy ? "开" : "关") << "\n";
        std::cout << "  双拼: " << (shuangpin ? "开" : "关") << "\n";
        // 测试解码：逐键输入 nihao
        for (char c : std::string("nihao")) ctx.type(c);
        std::string first = ctx.sentence();
        ctx.clear();
        bool decodeOk = (first == "你好");
        std::cout << "  解码测试: nihao → " << first << " (" << (decodeOk ? "OK" : "异常") << ")\n";
        std::cout << "  自检结果: " << (decodeOk ? "通过" : "失败") << "\n";
        return decodeOk ? 0 : 1;
    }

    // --history-weight：调整用户语言模型权重
    if (historyWeight > 0) {
        ime.model()->setHistoryWeight(historyWeight);
    }

    // --train-text：从中文文本训练用户语言模型
    if (!trainText.empty()) {
        std::ifstream tf(trainText);
        if (!tf) {
            std::cerr << "pinyin_ime_cli: 无法打开训练文件 " << trainText << "\n";
            return 2;
        }
        size_t total = 0;
        std::string line;
        while (std::getline(tf, line)) {
            if (line.empty() || line[0] == '#') continue;
            // 按 UTF-8 字符切分
            std::vector<std::string> chars;
            size_t i = 0;
            while (i < line.size()) {
                unsigned char c = line[i];
                int len = 1;
                if (c >= 0xF0) len = 4;
                else if (c >= 0xE0) len = 3;
                else if (c >= 0xC0) len = 2;
                if (i + len > line.size()) break;
                chars.push_back(line.substr(i, len));
                i += len;
            }
            if (chars.size() >= 2) {
                ime.model()->history().add(chars);
                ++total;
            }
        }
        save_user_data();
        if (!quiet) std::cerr << "pinyin_ime_cli: 从文本训练完成，" << total
                              << " 句已加入用户语言模型\n";
        return 0;
    }

    // --predict：联想预测
    if (predictMode) {
        libime::Prediction pred;
        pred.setUserLanguageModel(ime.model());
        std::string line;
        while (std::getline(std::cin, line)) {
            if (line.empty()) continue;
            // 简单分词：按字符切分（UTF-8 每个汉字 3 字节）
            std::vector<std::string> words;
            size_t i = 0;
            while (i < line.size()) {
                unsigned char c = line[i];
                int len = 1;
                if (c >= 0xF0) len = 4;
                else if (c >= 0xE0) len = 3;
                else if (c >= 0xC0) len = 2;
                if (i + len > line.size()) break;
                words.push_back(line.substr(i, len));
                i += len;
            }
            // 用最后 4 个字作为上下文预测
            size_t ctxStart = words.size() > 4 ? words.size() - 4 : 0;
            std::vector<std::string> ctx(words.begin() + ctxStart, words.end());
            auto results = pred.predict(ctx, 10);
            if (jsonOutput) {
                std::cout << '[';
                for (size_t j = 0; j < results.size(); ++j) {
                    if (j) std::cout << ',';
                    std::cout << '"' << results[j] << '"';
                }
                std::cout << "]\n";
            } else {
                for (size_t j = 0; j < results.size(); ++j) {
                    if (j) std::cout << '\t';
                    std::cout << results[j];
                }
                std::cout << '\n';
            }
        }
        return 0;
    }

    // --batch：从文件批量处理
    if (!batchFile.empty()) {
        std::ifstream bf(batchFile);
        if (!bf) {
            std::cerr << "pinyin_ime_cli: 无法打开批量文件 " << batchFile << "\n";
            return 2;
        }
        std::string line;
        while (std::getline(bf, line)) {
            if (line.empty() || line[0] == '#') {
                if (jsonOutput) std::cout << "[]\n";
                else std::cout << "\n";
                continue;
            }
            ctx.clear();
            // v0.9.47: 数字→字母预处理（手机键盘误触）
            if (correctPinyin) {
                for (char &c : line) {
                    switch (c) {
                        case '0': c = 'o'; break;  /* 0→o */
                        case '1': c = 'i'; break;  /* 1→i */
                        case '6': c = 'y'; break;  /* 6→y */
                        case '9': c = 'p'; break;  /* 9→p */
                    }
                }
            }
            // v0.9.109: 英文检测（含双写辅音）— 中文拼音音节不含任何双写辅音，
            // 命中即判定为英文，跳过 M3 纠错（避免 hello 被误纠成中文候选）。
            // v0.9.113: 若命中 boost 预设（已知中文简拼，如 yyds=永远的神），
            // 不判为英文（yy 双写在 yyds/yy 中是合法简拼，不是英文双写）。
            {
                std::string compact;
                compact.reserve(line.size());
                for (char c : line) if (c >= 'a' && c <= 'z') compact.push_back(c);
                const bool knownAbbr = !compact.empty() &&
                    [&compact]() {
                        const auto &w = pinyin_ime::lookup_pinyin_boost(compact);
                        return !w.empty() && w != compact;
                    }();
                const bool englishLineDoubled =
                    !compact.empty() && !knownAbbr && !is_valid_pinyin(compact) &&
                    (compact.find("aa") != std::string::npos ||
                     compact.find("bb") != std::string::npos ||
                     compact.find("cc") != std::string::npos ||
                     compact.find("dd") != std::string::npos ||
                     compact.find("ee") != std::string::npos ||
                     compact.find("ff") != std::string::npos ||
                     compact.find("gg") != std::string::npos ||
                     compact.find("hh") != std::string::npos ||
                     compact.find("ii") != std::string::npos ||
                     compact.find("jj") != std::string::npos ||
                     compact.find("kk") != std::string::npos ||
                     compact.find("ll") != std::string::npos ||
                     compact.find("mm") != std::string::npos ||
                     compact.find("nn") != std::string::npos ||
                     compact.find("oo") != std::string::npos ||
                     compact.find("pp") != std::string::npos ||
                     compact.find("qq") != std::string::npos ||
                     compact.find("rr") != std::string::npos ||
                     compact.find("ss") != std::string::npos ||
                     compact.find("tt") != std::string::npos ||
                     compact.find("uu") != std::string::npos ||
                     compact.find("vv") != std::string::npos ||
                     compact.find("ww") != std::string::npos ||
                     compact.find("xx") != std::string::npos ||
                     compact.find("yy") != std::string::npos ||
                     compact.find("zz") != std::string::npos);
                if (englishLineDoubled) {
                    std::cout << line << "\n";
                    continue;
                }
            }
            if (correctPinyin && pinyin_corrector::should_correct(line)) {
                std::string fixed = pinyin_corrector::correct_to_string(line);
                if (!fixed.empty()) line = fixed;
            }
            line = disambiguate_pinyin(line);
            // v0.9.85: 空格分隔 → 分段解码再拼接（"nihao shijie" → "你好"+"世界"）
            bool hasSpace = false;
            for (char c : line) if (c == ' ') { hasSpace = true; break; }
            std::vector<std::string> cands;
            if (hasSpace) {
                // 按空格切分，每段独立解码，拼接首选候选
                std::vector<std::string> segs;
                std::string cur;
                for (char c : line) {
                    if (c == ' ') { if (!cur.empty()) segs.push_back(cur); cur.clear(); }
                    else cur.push_back(c);
                }
                if (!cur.empty()) segs.push_back(cur);
                // 每段独立解码
                std::string joined;
                std::vector<std::vector<std::string>> segCands;
                for (auto &seg : segs) {
                    ctx.clear();
                    for (char ch : seg) if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
                    auto sc = collect_candidates(ctx, static_cast<size_t>(nbest));
                    boost_abbreviation(seg, sc);
                    segCands.push_back(sc);
                    if (!sc.empty()) joined += sc[0];
                }
                if (!joined.empty()) cands.push_back(joined);
                // 也生成各段第二候选的组合作为备选
                std::string joined2;
                bool hasAlt = false;
                for (size_t si = 0; si < segCands.size(); ++si) {
                    if (segCands[si].size() > 1) { joined2 += segCands[si][1]; hasAlt = true; }
                    else if (!segCands[si].empty()) joined2 += segCands[si][0];
                }
                if (hasAlt && joined2 != joined) cands.push_back(joined2);
            } else {
                for (char ch : line) {
                    if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
                }
                cands = collect_candidates(ctx, static_cast<size_t>(nbest));
                boost_abbreviation(line, cands);
            }
            if (top1Only) {
                if (!cands.empty()) std::cout << cands[0]; std::cout << "\n";
            } else if (topK > 0) {
                size_t limit = std::min(static_cast<size_t>(topK), cands.size());
                for (size_t i = 0; i < limit; ++i) {
                    if (i) std::cout << '\t';
                    std::cout << cands[i];
                }
                std::cout << '\n';
            } else if (jsonOutput) {
                std::cout << '[';
                for (size_t i = 0; i < cands.size(); ++i) {
                    if (i) std::cout << ',';
                    std::cout << '"';
                    for (char c : cands[i]) {
                        if (c == '"') std::cout << "\\\"";
                        else if (c == '\\') std::cout << "\\\\";
                        else std::cout << c;
                    }
                    std::cout << '"';
                }
                std::cout << "]\n";
            } else {
                for (size_t i = 0; i < cands.size(); ++i) {
                    if (i) std::cout << '\t';
                    std::cout << cands[i];
                }
                std::cout << _line_punct_suffix << '\n';
                _line_punct_suffix.clear();
            }
        }
        std::cout.flush();
        return 0;
    }

    // --dump-user：导出用户学习历史
    if (dumpUser) {
        ime.model()->history().dump(std::cout);
        return 0;
    }

    // --learn-file：批量学习模式
    // 文件格式：每行 "拼音<TAB>中文"，逐行 type→select→learn，训练用户语言模型。
    if (!learnFile.empty()) {
        // 抑制 select_and_commit 的终端输出
        std::cout.setstate(std::ios::failbit);
        std::ifstream lf(learnFile);
        if (!lf) {
            std::cerr << "pinyin_ime_cli: 无法打开学习文件 " << learnFile << "\n";
            return 2;
        }
        size_t total = 0, learned = 0;
        std::string line;
        while (std::getline(lf, line)) {
            if (line.empty() || line[0] == '#') continue;
            auto tab = line.find('\t');
            if (tab == std::string::npos) continue;
            std::string pinyin = line.substr(0, tab);
            std::string target = line.substr(tab + 1);
            // type 拼音
            ctx.clear();
            for (char ch : pinyin) {
                if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
            }
            // 找到匹配候选并 select
            auto cands = collect_candidates(ctx, static_cast<size_t>(nbest));
            for (size_t i = 0; i < cands.size(); ++i) {
                if (cands[i] == target) {
                    if (select_and_commit(ctx, i)) {
                        ++learned;
                    }
                    break;
                }
            }
            ++total;
        }
        save_user_data();
        std::cout.clear();
        std::cerr << "pinyin_ime_cli: 批量学习完成 " << learned << "/" << total
                  << " 句已学习（用户数据已保存到 " << userDataDir << "）\n";
        return 0;
    }

#ifdef __unix__
    {
        struct sigaction sa{};
        sa.sa_handler = on_signal;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;  // 不带 SA_RESTART：让阻塞中的 read() 可被 EINTR 打断
        sigaction(SIGTERM, &sa, nullptr);
        sigaction(SIGHUP, &sa, nullptr);
    }
#endif

    // 误上屏撤回栈：记录「最近一次候选上屏」的文本 + 拼音串（最近在上）。
    // 上限 32 条，防长会话无限膨胀。
    std::vector<std::pair<std::string, std::string>> undoStack;
    // 撤回后的提示语（显示在状态行末尾，下一次按键即清除）。
    std::string undoNotice;
    size_t boostSwap = 0;       // 简拼 boost：候选 boostSwap 与 0 交换显示（0=不交换）

    // v0.9.124: 联想词模式 — 上屏后自动显示关联词候选
    bool assocMode = false;          // 是否处于联想词模式
    std::vector<std::string> assocCands;  // 联想候选列表
    std::string lastCommitted;       // 最近上屏的文本（供联想用）

    // 上屏即落盘：崩溃/被 kill 时最多丢「最后一次上屏」的增量（防丢措施，与 addon 对齐）。
    // 成功上屏时把 (文本, 拼音) 压入撤回栈，供 Ctrl-Z 撤回后恢复拼音重新编辑。
    auto commit = [&](size_t idx) {
        if (ctx.empty()) return;
        // 简拼 boost：交换选词索引
        if (boostSwap > 0) {
            if (idx == 0) idx = boostSwap;
            else if (idx == boostSwap) idx = 0;
        }
        const std::string preText = ctx.sentence();         // 上屏文本（撤回提示/删行用）
        const std::string prePinyin = preedit_compact(ctx); // 恢复拼音（撤回后重新打字用）
        if (select_and_commit(ctx, idx)) {
            if (!preText.empty()) {
                undoStack.emplace_back(preText, prePinyin);
                if (undoStack.size() > 32) undoStack.erase(undoStack.begin());
            }
            save_user_data();
            // v0.9.124: 联想词 — 上屏后自动显示关联词候选
            lastCommitted = preText;
            assocCands = pinyin_ime::association_lookup(preText);
            assocMode = !assocCands.empty();
            // v0.9.129: 最近输入记忆
            recentHistory.add(preText);
            userFreqTable.add(prePinyin, preText);
        }
    };

    RawTerm raw;
    if (!raw.enable()) {
        // ---- --benchmark：内置基准测试 ----
        if (benchmarkMode) {
            struct timeval tv;
            gettimeofday(&tv, nullptr);
            double t0 = tv.tv_sec + tv.tv_usec / 1e6;
            // 常用查询
            const char *queries[] = {
                "nihao", "zhongguo", "beijing", "shanghai", "xiexie",
                "zaijian", "woaini", "mingbai", "kexue", "jishu",
                "shijian", "kongzhi", "yinyue", "dianying", "youxi",
                "xuexi", "gongzuo", "shenghuo", "shijieshang", "zhonghuarenmin"
            };
            const char *expected[] = {
                "你好", "中国", "北京", "上海", "谢谢",
                "再见", "我爱你", "明白", "科学", "技术",
                "时间", "控制", "音乐", "电影", "游戏",
                "学习", "工作", "生活", "世界上", "中国人民"
            };
            int nq = sizeof(queries) / sizeof(queries[0]);
            int correct = 0;
            for (int i = 0; i < nq; ++i) {
                ctx.clear();
                std::string q = disambiguate_pinyin(queries[i]);
                for (char ch : q) if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
                auto cands = collect_candidates(ctx, 1);
                boost_abbreviation(queries[i], cands);
                std::string result = cands.empty() ? "" : cands[0];
                if (result == expected[i]) ++correct;
            }
            gettimeofday(&tv, nullptr);
            double t1 = tv.tv_sec + tv.tv_usec / 1e6;
            double elapsed = (t1 - t0) * 1000.0;
            std::cout << "{\"queries\": " << nq
                      << ", \"correct\": " << correct
                      << ", \"accuracy\": " << (correct * 100.0 / nq)
                      << ", \"total_ms\": " << elapsed
                      << ", \"avg_ms\": " << (elapsed / nq)
                      << ", \"qps\": " << (nq / (t1 - t0))
                      << "}\n";
            save_user_data();
            return 0;
        }

        // ---- 行模式：每行一段全拼/简拼 → 候选列表 ----
        std::string line;
        while (std::getline(std::cin, line)) {
            ctx.clear();
            // 搜狗式快捷输入：v模式/u模式/Tab补全/日期/时间/计算器/数字转中文
            {
                std::string compactRaw;
                compactRaw.reserve(line.size());
                for (char c : line) if (c >= 'a' && c <= 'z') compactRaw.push_back(c);
                // v模式符号（va→āáǎà, vb→【】, vc→♠♥ ...）
                if (pinyin_ime::is_v_mode(line)) {
                    std::string sub = line.substr(1);
                    auto [syms, label] = pinyin_ime::v_mode_lookup(sub);
                    if (!syms.empty()) {
                        for (size_t ci = 0; ci < syms.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << syms[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // u模式笔画输入（uhsp→横竖撇→仁亿什...）
                if (pinyin_ime::is_u_mode(line)) {
                    std::string strokes = line.substr(1);
                    auto chars = pinyin_ime::u_mode_lookup(strokes);
                    if (!chars.empty()) {
                        for (size_t ci = 0; ci < chars.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << chars[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // i模式快捷短语（ih→你好 ix→谢谢 iz→再见）
                if (pinyin_ime::is_i_mode(line)) {
                    std::string abbr = line.substr(1);
                    auto phrases = pinyin_ime::i_mode_lookup(abbr);
                    if (!phrases.empty()) {
                        for (size_t ci = 0; ci < phrases.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << phrases[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // Tab英文补全（至少3字符，且首选不等于输入本身）
                if (pinyin_ime::is_english_input(line) && line.size() >= 3) {
                    auto words = pinyin_ime::tab_complete(line);
                    if (!words.empty() && words[0] != line) {
                        for (size_t ci = 0; ci < words.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << words[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // 纯数字转中文（123→一百二十三/壹佰贰拾叁）
                {
                    auto numResults = pinyin_ime::number_candidates(line);
                    if (!numResults.empty()) {
                        for (size_t ci = 0; ci < numResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << numResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // 日期/时间
                {
                    std::string dtResult = pinyin_ime::datetime_lookup(compactRaw);
                    if (!dtResult.empty()) {
                        output_line(dtResult);
                        continue;
                    }
                }
                // 计算器
                if (pinyin_ime::is_calculator_input(line)) {
                    std::string calcResult = pinyin_ime::calculator_eval(line);
                    if (!calcResult.empty()) {
                        output_line(line + "=" + calcResult);
                        continue;
                    }
                }
                // 货币转换（123rmb → 壹佰贰拾叁元）
                {
                    auto curResults = pinyin_ime::currency_candidates(line);
                    if (!curResults.empty()) {
                        for (size_t ci = 0; ci < curResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << curResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // 单位换算（1km → 1千米=1000米）
                {
                    auto unitResults = pinyin_ime::unit_conversion(line);
                    if (!unitResults.empty()) {
                        for (size_t ci = 0; ci < unitResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << unitResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // 百分比转中文（50p → 百分之五十）
                {
                    std::string pctResult = pinyin_ime::percent_to_chinese(line);
                    if (!pctResult.empty()) {
                        output_line(pctResult);
                        continue;
                    }
                }
                // v0.9.129: 句式模板（xw→请问您需要什么帮助？, td→【待办】...）
                {
                    auto tmplResults = pinyin_ime::sentence_template_lookup(line);
                    if (!tmplResults.empty()) {
                        for (size_t ci = 0; ci < tmplResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << tmplResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // v0.9.126: 自定义快捷词（addr→地址, todo→【待办】, main→int main...）
                {
                    auto customResults = pinyin_ime::custom_phrase_lookup(line);
                    if (!customResults.empty()) {
                        for (size_t ci = 0; ci < customResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << customResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // v0.9.126: 中英文混输（hello→hello, py→python...）
                {
                    auto mixedResults = pinyin_ime::mixed_input_lookup(line);
                    if (!mixedResults.empty()) {
                        for (size_t ci = 0; ci < mixedResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << mixedResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // v0.9.126: 成语速查（yffs→一帆风顺, mdcg→马到成功...）
                {
                    auto idiomResults = pinyin_ime::idiom_lookup(line);
                    if (!idiomResults.empty()) {
                        for (size_t ci = 0; ci < idiomResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << idiomResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // v0.9.126: 错音纠正（nihoa→nihao, zhonggu→zhongguo...）
                {
                    auto typoResults = pinyin_ime::typo_correction_lookup(line);
                    if (!typoResults.empty()) {
                        // 输出纠正后的拼音提示
                        for (size_t ci = 0; ci < typoResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << "[纠错]" << typoResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // v0.9.126: 繁简转换（输入 tf+简体文本 → 繁体）
                // 搜狗式：输入 "tf" 前缀触发繁简转换
                if (line.size() > 2 && line.substr(0, 2) == "tf") {
                    std::string tradText = pinyin_ime::to_traditional(line.substr(2));
                    if (!tradText.empty() && tradText != line.substr(2)) {
                        output_line(tradText);
                        continue;
                    }
                // v0.9.127: 网址/邮箱智能识别（www.baidu.com → 网址, admin@x.com → 邮箱）
                {
                    auto urlResults = pinyin_ime::smart_url_lookup(line);
                    if (!urlResults.empty()) {
                        for (size_t ci = 0; ci < urlResults.size(); ++ci) {
                            if (ci > 0) std::cout << "\t";
                            std::cout << urlResults[ci];
                        }
                        std::cout << "\n";
                        continue;
                    }
                }
                // v0.9.127: 拼音分词提示（长拼音串 → 分词显示）
                {
                    auto segHint = pinyin_ime::pinyin_segment_hint(line);
                    if (!segHint.empty()) {
                        // 输出分词提示到 stderr，不干扰正常候选
                        std::cerr << "[分词] " << segHint << "\n";
                    }
                }
                }
            }
            // --correct：先纠错再解码（C++ 原生，替换行模式每行 fork Python 的桥接）
            // v0.9.47: 数字→字母预处理（手机键盘误触）
            if (correctPinyin) {
                for (char &c : line) {
                    switch (c) {
                        case '0': c = 'o'; break;
                        case '1': c = 'i'; break;
                        case '6': c = 'y'; break;
                        case '9': c = 'p'; break;
                    }
                }
            }
            // v0.9.109: 英文检测（含双写辅音）— 中文拼音音节不含任何双写辅音，
            // 命中即判定为英文，跳过 M3 纠错（避免 hello 被误纠成中文候选）。
            // v0.9.113: 若命中 boost 预设（已知中文简拼，如 yyds=永远的神），
            // 不判为英文（yy 双写在 yyds/yy 中是合法简拼，不是英文双写）。
            {
                std::string compact;
                compact.reserve(line.size());
                for (char c : line) if (c >= 'a' && c <= 'z') compact.push_back(c);
                const bool knownAbbr = !compact.empty() &&
                    [&compact]() {
                        const auto &w = pinyin_ime::lookup_pinyin_boost(compact);
                        return !w.empty() && w != compact;
                    }();
                const bool englishLineDoubled =
                    !compact.empty() && !knownAbbr && !is_valid_pinyin(compact) &&
                    (compact.find("aa") != std::string::npos ||
                     compact.find("bb") != std::string::npos ||
                     compact.find("cc") != std::string::npos ||
                     compact.find("dd") != std::string::npos ||
                     compact.find("ee") != std::string::npos ||
                     compact.find("ff") != std::string::npos ||
                     compact.find("gg") != std::string::npos ||
                     compact.find("hh") != std::string::npos ||
                     compact.find("ii") != std::string::npos ||
                     compact.find("jj") != std::string::npos ||
                     compact.find("kk") != std::string::npos ||
                     compact.find("ll") != std::string::npos ||
                     compact.find("mm") != std::string::npos ||
                     compact.find("nn") != std::string::npos ||
                     compact.find("oo") != std::string::npos ||
                     compact.find("pp") != std::string::npos ||
                     compact.find("qq") != std::string::npos ||
                     compact.find("rr") != std::string::npos ||
                     compact.find("ss") != std::string::npos ||
                     compact.find("tt") != std::string::npos ||
                     compact.find("uu") != std::string::npos ||
                     compact.find("vv") != std::string::npos ||
                     compact.find("ww") != std::string::npos ||
                     compact.find("xx") != std::string::npos ||
                     compact.find("yy") != std::string::npos ||
                     compact.find("zz") != std::string::npos);
                if (englishLineDoubled) {
                    std::cout << line << "\n";
                    continue;
                }
            }
            if (correctPinyin && pinyin_corrector::should_correct(line)) {
                std::string fixed = pinyin_corrector::correct_to_string(line);
                if (!fixed.empty()) line = fixed;
            }
            // v0.9.89: 数字转中文
            if (numConvert || numAmount || numUpper) {
                bool allDigits = true;
                for (char c : line) {
                    if (c < '0' || c > '9') { allDigits = false; break; }
                }
                if (allDigits && !line.empty()) {
                    std::string numResult;
                    if (numAmount) numResult = num_to_chinese_amount(line);
                    else numResult = num_to_chinese(line, numUpper);
                    std::cout << numResult << "\n";
                    continue;
                }
            }
            // v0.9.86: 行模式标点处理
            // 纯标点行 → 直接输出中文标点
            // 拼音+标点 → 先解码拼音，再追加中文标点
            {
                std::string pinyinPart, punctPart;
                bool inPunct = false;
                for (char c : line) {
                    std::string p = chinese_punct(c);
                    if (!p.empty()) {
                        inPunct = true;
                        punctPart += p;
                    } else if (!inPunct) {
                        pinyinPart += c;
                    } else {
                        // 标点后的非标点字符，也作为标点部分（如引号内容）
                        punctPart += c;
                    }
                }
                if (pinyinPart.empty() && !punctPart.empty()) {
                    // 纯标点行
                    if (top1Only) {
                        std::cout << punctPart << "\n";
                    } else {
                        std::cout << punctPart << "\n";
                    }
                    continue;
                }
                // 有标点部分时，先解码拼音部分，再追加标点
                // 保存标点部分供后面使用
                line = pinyinPart;
                // 标点部分存在全局变量供输出时追加
                _line_punct_suffix = punctPart;
            }
            line = disambiguate_pinyin(line);
            // v0.9.85: 空格分隔 → 智能分段解码
            // 策略：如果所有段都≥4字符（词级分隔），分段解码再拼接
            //       否则（音节级分隔），去除空格整体解码
            bool hasSpace2 = false;
            for (char c : line) if (c == ' ') { hasSpace2 = true; break; }
            std::vector<std::string> cands;
            if (hasSpace2) {
                std::vector<std::string> segs;
                std::string cur;
                for (char c : line) {
                    if (c == ' ') { if (!cur.empty()) segs.push_back(cur); cur.clear(); }
                    else cur.push_back(c);
                }
                if (!cur.empty()) segs.push_back(cur);
                // 判断是否为词级分隔（所有段≥4字符）
                bool wordLevel = true;
                for (auto &s : segs) if (s.size() < 4) { wordLevel = false; break; }
                if (wordLevel && segs.size() >= 2) {
                    // 词级分隔：每段独立解码，拼接首选候选
                    std::string joined;
                    std::vector<std::vector<std::string>> segCands;
                    bool prevWasEnglish = false;
                    for (auto &seg : segs) {
                        // v0.9.88: 检测英文单词
                        if (!is_valid_pinyin(seg)) {
                            // 非有效拼音 → 原样输出（英文单词）
                            // 英文间保留空格，中英文间不加空格
                            if (prevWasEnglish && !joined.empty()) joined += " ";
                            segCands.push_back({seg});
                            joined += seg;
                            prevWasEnglish = true;
                            continue;
                        }
                        ctx.clear();
                        for (char ch : seg) if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
                        auto sc = collect_candidates(ctx, static_cast<size_t>(nbest));
                        boost_abbreviation(seg, sc);
                        segCands.push_back(sc);
                        if (!sc.empty()) joined += sc[0];
                        prevWasEnglish = false;
                    }
                    if (!joined.empty()) cands.push_back(joined);
                    // 备选：各段第二候选组合
                    std::string joined2;
                    bool hasAlt = false;
                    for (size_t si = 0; si < segCands.size(); ++si) {
                        if (segCands[si].size() > 1) { joined2 += segCands[si][1]; hasAlt = true; }
                        else if (!segCands[si].empty()) joined2 += segCands[si][0];
                    }
                    if (hasAlt && joined2 != joined) cands.push_back(joined2);
                } else {
                    // 音节级分隔：检查各段是否为英文
                    bool hasEnglish = false;
                    std::string joinedMixed;
                    bool prevEng = false;
                    for (auto &seg : segs) {
                        if (!is_valid_pinyin(seg)) {
                            hasEnglish = true;
                            if (prevEng && !joinedMixed.empty()) joinedMixed += " ";
                            joinedMixed += seg;
                            prevEng = true;
                        } else {
                            ctx.clear();
                            for (char ch : seg) if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
                            auto sc = collect_candidates(ctx, static_cast<size_t>(nbest));
                            boost_abbreviation(seg, sc);
                            if (!sc.empty()) joinedMixed += sc[0];
                            prevEng = false;
                        }
                    }
                    if (hasEnglish) {
                        cands.clear();
                        cands.push_back(joinedMixed);
                    } else {
                        // 全部是有效拼音，去除空格整体解码
                        std::string nospaces;
                        for (char c : line) if (c != ' ') nospaces.push_back(c);
                        ctx.clear();
                        for (char ch : nospaces) if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
                        cands = collect_candidates(ctx, static_cast<size_t>(nbest));
                        boost_abbreviation(nospaces, cands);
                    }
                }
            } else {
                for (char ch : line) {
                    if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
                }
                cands = collect_candidates(ctx, static_cast<size_t>(nbest));
                boost_abbreviation(line, cands);
            }
            // P1: 长句兜底分词 — 候选含拉丁字母时尝试元音补全
            if (!cands.empty() && has_latin(cands[0]) && !correctPinyin) {
                auto fbCands = fallback_segment(ctx, line, static_cast<size_t>(nbest));
                if (!fbCands.empty()) {
                    cands = fbCands;
                    boost_abbreviation(line, cands);
                }
            }
            if (top1Only) {
                if (!cands.empty()) std::cout << cands[0];
                std::cout << _line_punct_suffix;
                _line_punct_suffix.clear();
            } else if (topK > 0) {
                size_t limit = std::min(static_cast<size_t>(topK), cands.size());
                for (size_t i = 0; i < limit; ++i) {
                    if (i) std::cout << '\t';
                    std::cout << cands[i];
                }
                std::cout << _line_punct_suffix;
                _line_punct_suffix.clear();
            } else if (jsonOutput) {
                // JSON 数组输出：["候选1","候选2",...]
                std::cout << '[';
                for (size_t i = 0; i < cands.size(); ++i) {
                    if (i) std::cout << ',';
                    std::cout << '"';
                    // 转义 JSON 特殊字符
                    for (char c : cands[i]) {
                        if (c == '"') std::cout << "\\\"";
                        else if (c == '\\') std::cout << "\\\\";
                        else std::cout << c;
                    }
                    std::cout << '"';
                }
                std::cout << ']';
            } else {
                for (size_t i = 0; i < cands.size(); ++i) {
                    if (i) std::cout << '\t';
                    std::cout << cands[i];
                }
                std::cout << _line_punct_suffix;
                _line_punct_suffix.clear();
            }
            std::cout << '\n';
            std::cout.flush();
        }
        save_user_data();
        return 0;
    }

    // ---- 交互模式 ----
    // 交互下多拉候选以支持分页（每页 9 个、最多 5 页；行模式输出数量仍走 nbest）
    ime.setNBest(std::max<size_t>(static_cast<size_t>(nbest), 45));
    const size_t kPageSize = 9;  // 匹配 1-9 选字键
    size_t page = 0;
    size_t candFocus = 0;       // 候选焦点（页内 0..8）：Ctrl+←/→ 移动，空格上屏焦点候选
    bool englishMode = false;   // 中/英切换：Tab 切换；英文态原样透传
    bool emojiMode = false;    // 表情选择：Ctrl-E 进入；1-9 选表情、[,]/[.] 翻页、Esc 返回
    size_t emojiPage = 0;      // 表情当前页（每页 9 个）
    bool quoteOpen = false;    // 双引号智能配对：开“/闭”交替
    bool singleQuoteOpen = false;  // 单引号智能配对：开‘/闭’交替（空闲时；打拼音中仍是隔音符）
    std::string engBuf;         // 英文态累积缓冲（回车整行上屏）
    // v0.9.124: i模式快捷短语 — 输入i后跟字母缩写
    bool iMode = false;              // 是否处于i模式
    std::string iModeBuf;            // i模式累积缓冲（i后面的字母）

    auto render = [&]() {
        std::cout << "\r\x1b[2K";  // 清当前行
        if (englishMode) {
            std::cout << "英文: " << (engBuf.empty() ? "_" : engBuf) << "  [Tab 切回中文]";
            std::cout.flush();
            return;
        }
        if (emojiMode) {
            const auto &list = emoji_list();
            const size_t maxPage = list.empty() ? 0 : (list.size() - 1) / kPageSize;
            if (emojiPage > maxPage) emojiPage = maxPage;
            std::cout << "表情[" << (emojiPage + 1) << "/" << (maxPage + 1) << "]: ";
            for (size_t i = emojiPage * kPageSize;
                 i < std::min<size_t>(list.size(), emojiPage * kPageSize + kPageSize); ++i) {
                std::cout << (i - emojiPage * kPageSize + 1) << "." << list[i].glyph
                          << list[i].label << " ";
            }
            std::cout << "  [1-9 选 / ,. 翻页 / Esc 返回]";
            std::cout.flush();
            return;
        }
        // v0.9.124: i模式快捷短语渲染
        if (iMode) {
            std::cout << "快捷[i" << (iModeBuf.empty() ? "" : iModeBuf) << "]: ";
            auto phrases = pinyin_ime::i_mode_lookup(iModeBuf);
            if (phrases.empty()) {
                std::cout << "(无匹配)  [输入字母 / Esc 返回]";
            } else {
                for (size_t i = 0; i < std::min<size_t>(phrases.size(), 9); ++i) {
                    std::cout << (i + 1) << "." << phrases[i] << " ";
                }
                if (phrases.size() > 9) std::cout << "... ";
                std::cout << " [1-9 选 / Esc 返回]";
            }
            std::cout.flush();
            return;
        }
        // v0.9.124: 联想词渲染（上屏后空闲态自动显示关联词）
        if (assocMode && ctx.empty() && !assocCands.empty()) {
            std::cout << "联想: ";
            for (size_t i = 0; i < std::min<size_t>(assocCands.size(), 9); ++i) {
                std::cout << (i + 1) << "." << assocCands[i] << " ";
            }
            std::cout << " [1-9 选词 / 任意字母继续输入 / Esc 取消]";
            std::cout.flush();
            return;
        }
        auto [pre, cursorPos] = ctx.preeditWithCursor();
        if (cursorPos > pre.size()) {
            cursorPos = pre.size();
        }
        // v0.9.90: 音节分隔显示 — 用 ' 分隔音节，比空格更清晰
        std::string preStr = add_syllable_separators(preedit_compact(ctx));
        // 光标位置需要重新计算（分隔符插入后位置变化）
        // 简化处理：光标放在末尾
        cursorPos = preStr.size();
        if (!preStr.empty() && cursorPos <= preStr.size()) {
            preStr.insert(cursorPos, "▏");
        }
        std::cout << theme.preedit_color << (preStr.empty() ? "拼音: _" : "拼音: " + preStr) << theme.reset << "  |  ";
        const auto &cands = ctx.candidatesToCursor();
        const size_t total = cands.size();
        // 简拼 boost：检查 preedit 是否匹配预设，若匹配则交换候选显示顺序
        boostSwap = 0;
        if (total > 1) {
            std::string preeditStr = preedit_compact(ctx);
            std::string pword = lookup_pinyin_boost(preeditStr);
            if (!pword.empty()) {
                for (size_t ci = 1; ci < total; ++ci) {
                    if (cands[ci].toString() == pword) {
                        boostSwap = ci;
                        break;
                    }
                }
            }
            // v0.9.44: 完整匹配失败时，尝试后缀匹配
            if (boostSwap == 0) {
                std::vector<std::string> cand_strs;
                for (size_t ci = 0; ci < total; ++ci)
                    cand_strs.push_back(cands[ci].toString());
                boostSwap = lookup_suffix_boost_index(preeditStr, cand_strs);
            }
        }
        const size_t maxPage = total == 0 ? 0 : (total - 1) / kPageSize;
        if (page > maxPage) page = maxPage;
        const size_t begin = page * kPageSize;
        const size_t end = std::min<size_t>(total, begin + kPageSize);
        for (size_t i = begin; i < end; ++i) {
            size_t dispIdx = i;
            if (boostSwap > 0 && i < total) {
                if (i == 0) dispIdx = boostSwap;
                else if (i == boostSwap) dispIdx = 0;
            }
            if (i - begin == candFocus) {
                // 焦点候选：* 前缀标记 + 拼音标注（v0.9.90）
                std::string candPinyin = ctx.candidateFullPinyin(dispIdx);
                std::cout << theme.focus_color << "*" << (i - begin + 1) << "." << cands[dispIdx].toString() << theme.reset;
                if (!candPinyin.empty()) std::cout << "(" << candPinyin << ")";
                std::cout << "  ";
            } else {
                std::cout << theme.index_color << (i - begin + 1) << "." << theme.candidate_color << cands[dispIdx].toString() << theme.reset << "  ";
            }
        }
        if (total > kPageSize) {
            std::cout << "  [" << (page + 1) << "/" << (maxPage + 1) << "页 ,上一页 .下一页]";
        }
        if (!undoNotice.empty()) {
            std::cout << "  " << undoNotice;
        }
        std::cout.flush();
    };

    std::cout << "pinyin_ime_cli 交互模式：输入拼音（支持简拼），数字/空格/回车上屏，退格删除，Esc 清空，Tab 中英切换，Ctrl-E 表情，←/→ 逐词移动光标，Ctrl+←/→ 移动候选焦点，空格上屏焦点候选，Home/End 跳句首句尾，-=/.及PageUp/PageDown翻页，Ctrl-Z 撤回上屏，Ctrl-C 退出\n";
    render();

    int key;
    while ((key = read_key()) != kKeyEof) {
        if (key == 0x03) {  // Ctrl-C
            std::cout << "\n";
            break;
        }
        // 除撤回键（Ctrl-Z）外，任何按键都清除「已撤回」提示。
        if (key != 0x1a) {
            undoNotice.clear();
        }
        if (englishMode) {
            char ch = static_cast<char>(key);
            // ---- 英文模式：字母/数字/空格原样入缓冲，回车整行上屏，Tab / Esc 切回中文 ----
            if (key == kKeyEsc) {  // Esc：清缓冲并切回中文
                engBuf.clear();
                englishMode = false;
            } else if (ch == '\t') {  // Tab：英文自动补全，再按切回中文
                static int tabCount = 0;
                if (tabCount == 0 && !engBuf.empty()) {
                    // 第一次 Tab：显示自动补全候选
                    auto ac = pinyin_ime::english_autocomplete(engBuf);
                    if (!ac.empty()) {
                        std::cout << "\r\x1b[2K补全: ";
                        for (size_t i = 0; i < std::min<size_t>(ac.size(), 9); ++i) {
                            std::cout << (i + 1) << "." << ac[i] << " ";
                        }
                        std::cout << " [1-9 选 / Tab 切中文 / Esc 取消]";
                        std::cout.flush();
                        tabCount = 1;
                        continue;
                    }
                }
                tabCount = 0;
                englishMode = false;
                englishMode = false;
            } else if (ch == 0x7f || ch == 0x08) {  // 退格：删缓冲末字符
                if (!engBuf.empty()) engBuf.pop_back();
            } else if (ch == '\r' || ch == '\n') {  // 回车：整行上屏
                if (!engBuf.empty()) { output_line(engBuf); engBuf.clear(); }
            } else if (ch == ' ') {
                engBuf.push_back(' ');
            } else if (ch >= 0x20 && ch < 0x7f) {  // 可打印 ASCII：原样入缓冲
                engBuf.push_back(ch);
            }
        } else if (emojiMode) {
            char ch = static_cast<char>(key);
            if (key == kKeyEsc || key == 0x05) {  // Esc / Ctrl-E：退回中文
                emojiMode = false;
            } else if (ch == ',' || ch == '[') {  // 上一页
                if (emojiPage > 0) --emojiPage;
            } else if (ch == '.' || ch == ']') {  // 下一页
                const auto &l2 = emoji_list();
                const size_t mp = l2.empty() ? 0 : (l2.size() - 1) / kPageSize;
                if (emojiPage < mp) ++emojiPage;
            } else if (ch >= '1' && ch <= '9') {  // 选当前页第 N 个表情
                const auto &l3 = emoji_list();
                size_t idx = emojiPage * kPageSize + static_cast<size_t>(ch - '1');
                if (idx < l3.size()) {
                    output_line(l3[idx].glyph);  // 上屏表情
                    emojiMode = false;             // 选完退回中文
                }
            }
        } else if (key == kKeyEsc) {  // 中文模式 Esc：清空当前输入
            if (iMode) { iMode = false; iModeBuf.clear(); }
            else if (assocMode) { assocMode = false; assocCands.clear(); }
            else ctx.clear();
        } else if (key == kKeyLeft) {  // 左箭头：候选光标移到上一词边界（逐词分段）
            int b = ctx.pinyinBeforeCursor();
            if (b >= 0) {
                page = 0;
                candFocus = 0;
                ctx.setCursor(static_cast<size_t>(b));
            }
        } else if (key == kKeyRight) {  // 右箭头：候选光标移到下一词边界
            int a = ctx.pinyinAfterCursor();
            if (a >= 0) {
                page = 0;
                candFocus = 0;
                ctx.setCursor(static_cast<size_t>(a));
            }
        } else if (key == kKeyHome) {  // Home：候选光标跳到句首
            page = 0;
            candFocus = 0;
            ctx.setCursor(0);
        } else if (key == kKeyEnd) {   // End：候选光标跳到句尾（输入末尾）
            page = 0;
            candFocus = 0;
            ctx.setCursor(static_cast<size_t>(ctx.size()));
        } else if (key == kKeyPageUp) {   // PageUp：候选上一页
            if (page > 0) --page;
            candFocus = 0;
        } else if (key == kKeyPageDown) { // PageDown：候选下一页
            const auto &cands = ctx.candidatesToCursor();
            const size_t maxPage = cands.size() == 0 ? 0 : (cands.size() - 1) / kPageSize;
            if (page < maxPage) ++page;
            candFocus = 0;
        } else if (key == kKeyCtrlLeft || key == kKeyCtrlRight) {
            // Ctrl+←/→：候选焦点在当页候选间左右移动；空格上屏焦点候选。
            // （与普通 ←/→ 词边界移动、Home/End 跳首尾、PageUp/Down 翻页互不干扰。）
            const auto &cands = ctx.candidatesToCursor();
            if (!ctx.empty() && !cands.empty()) {
                const size_t begin = page * kPageSize;
                const size_t end = std::min<size_t>(cands.size(), begin + kPageSize);
                if (end > begin) {
                    if (key == kKeyCtrlLeft) {
                        candFocus = candFocus > 0 ? candFocus - 1 : 0;
                    } else {
                        candFocus = std::min<size_t>(candFocus + 1, end - begin - 1);
                    }
                }
            }
        } else {
            // ---- 中文模式（普通按键）----
            char ch = static_cast<char>(key);
            std::string punct = chinese_punct(ch);
            if (ch == 0x1a) {  // Ctrl-Z：误上屏撤回（撤回最近一次候选上屏）
                if (!ctx.empty()) {
                    // 正在输入拼音：不撤回（避免误伤当前编辑）。
                } else if (!undoStack.empty()) {
                    const auto [unusedText, pinyin] = undoStack.back();
                    (void)unusedText;
                    undoStack.pop_back();
                    // 布局：上屏行在上一行（「你好」），状态行在当前行。
                    // 先清状态行，再退到上一行清掉上屏行；随后 render() 会重绘状态行。
                    std::cout << "\r\x1b[2K";
                    std::cout << "\x1b[1A\r\x1b[2K";
                    page = 0;
                    ctx.clear();
                    for (char c : pinyin) {
                        if (is_pinyin_char(c)) ctx.type(std::string(1, c));
                    }
                    undoNotice = "（已撤回上屏，可继续编辑拼音）";
                }
            } else if (ch == 0x05) {  // Ctrl-E：进入表情选择
                emojiMode = true;
                emojiPage = 0;
            } else if (ch == '\t') {  // Tab：切英文
                if (iMode) { iMode = false; iModeBuf.clear(); }
                if (assocMode) { assocMode = false; assocCands.clear(); }
                englishMode = true;
                engBuf.clear();
            } else if (ch == 0x7f || ch == 0x08) {  // 退格（DEL/BS）
                if (iMode) {
                    if (!iModeBuf.empty()) iModeBuf.pop_back();
                    else { iMode = false; }
                } else {
                    ctx.backspace();
                    candFocus = 0;
                }
            } else if (ch == '-' || ch == ',' || ch == '[') {  // 打拼音时上一页（-=,[ ）；空闲时作中文标点
                if (ctx.empty() && !punct.empty()) output_line(punct);
                else if (page > 0) --page;
            } else if (ch == '=' || ch == '.' || ch == ']') {  // 打拼音时下一页（=.] ）；空闲时作中文标点
                if (ctx.empty() && !punct.empty()) {
                    output_line(punct);
                } else {
                    const auto &cands = ctx.candidatesToCursor();
                    const size_t maxPage = cands.size() == 0 ? 0 : (cands.size() - 1) / kPageSize;
                    if (page < maxPage) ++page;
                }
            } else if (ch == ' ' || ch == '\r' || ch == '\n') {  // 空格/回车：上屏首选
                // v0.9.124: i模式空格 — 选首选短语上屏
                if (iMode) {
                    auto phrases = pinyin_ime::i_mode_lookup(iModeBuf);
                    if (!phrases.empty()) {
                        output_line(phrases[0]);
                        lastCommitted = phrases[0];
                        assocCands = pinyin_ime::association_lookup(phrases[0]);
                        assocMode = !assocCands.empty();
                    }
                    iMode = false;
                    iModeBuf.clear();
                    render();
                    continue;
                }
                // v0.9.124: 联想词模式空格 — 选首选联想词上屏
                if (assocMode && ctx.empty() && !assocCands.empty()) {
                    output_line(assocCands[0]);
                    lastCommitted += assocCands[0];
                    assocCands = pinyin_ime::association_lookup(lastCommitted);
                    assocMode = !assocCands.empty();
                    render();
                    continue;
                }
                // --correct 交互态：整串纠错后上屏（只对「全拼打错」生效；简拼/无法解析
                // 的输入由 corrector 返回 nullopt 自动跳过，不误伤简拼）。
                if (correctPinyin && !ctx.empty()) {
                    std::string cont;
                    // preedit 音节间可能带空格（显示 "sen me"）、隔音符 xi'an 带 '，都滤掉。
                    for (char c : ctx.preedit()) {
                        if (c != '\'' && c != ' ') cont.push_back(c);
                    }
                    // v0.9.109: 英文检测（含双写辅音）— 中文拼音音节不含任何双写辅音
                    // （ll/oo/ee/tt/ss/pp/mm/nn 等），hello/good/look/apple 等常见英文
                    // 单词含双写。命中即判定英文输入，跳过 M3 纠错，直接上屏英文。
                    {
                        std::string compactRaw;
                        compactRaw.reserve(cont.size());
                        for (char c : cont) {
                            if (c >= 'a' && c <= 'z') compactRaw.push_back(c);
                        }
                        const bool englishDoubled =
                            !compactRaw.empty() && !is_valid_pinyin_inline(compactRaw) &&
                            (compactRaw.find("aa") != std::string::npos ||
                             compactRaw.find("bb") != std::string::npos ||
                             compactRaw.find("cc") != std::string::npos ||
                             compactRaw.find("dd") != std::string::npos ||
                             compactRaw.find("ee") != std::string::npos ||
                             compactRaw.find("ff") != std::string::npos ||
                             compactRaw.find("gg") != std::string::npos ||
                             compactRaw.find("hh") != std::string::npos ||
                             compactRaw.find("ii") != std::string::npos ||
                             compactRaw.find("jj") != std::string::npos ||
                             compactRaw.find("kk") != std::string::npos ||
                             compactRaw.find("ll") != std::string::npos ||
                             compactRaw.find("mm") != std::string::npos ||
                             compactRaw.find("nn") != std::string::npos ||
                             compactRaw.find("oo") != std::string::npos ||
                             compactRaw.find("pp") != std::string::npos ||
                             compactRaw.find("qq") != std::string::npos ||
                             compactRaw.find("rr") != std::string::npos ||
                             compactRaw.find("ss") != std::string::npos ||
                             compactRaw.find("tt") != std::string::npos ||
                             compactRaw.find("uu") != std::string::npos ||
                             compactRaw.find("vv") != std::string::npos ||
                             compactRaw.find("ww") != std::string::npos ||
                             compactRaw.find("xx") != std::string::npos ||
                             compactRaw.find("yy") != std::string::npos ||
                             compactRaw.find("zz") != std::string::npos);
                        // v模式符号（va→āáǎà, vb→【】...）
                        if (pinyin_ime::is_v_mode(cont)) {
                            std::string sub = cont.substr(1);
                            auto [syms, label] = pinyin_ime::v_mode_lookup(sub);
                            if (!syms.empty()) {
                                // 显示符号候选
                                for (size_t ci = 0; ci < syms.size() && ci < 9; ++ci) {
                                    std::cout << (ci + 1) << "." << syms[ci] << " ";
                                }
                                std::cout << "\n";
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // 纯数字转中文
                        {
                            auto numResults = pinyin_ime::number_candidates(cont);
                            if (!numResults.empty()) {
                                for (size_t ci = 0; ci < numResults.size(); ++ci) {
                                    std::cout << (ci + 1) << "." << numResults[ci] << " ";
                                }
                                std::cout << "\n";
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // 搜狗式快捷输入：日期/时间
                        {
                            std::string dtResult = pinyin_ime::datetime_lookup(compactRaw);
                            if (!dtResult.empty()) {
                                output_line(dtResult);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // 搜狗式计算器
                        if (pinyin_ime::is_calculator_input(compactRaw)) {
                            std::string calcResult = pinyin_ime::calculator_eval(compactRaw);
                            if (!calcResult.empty()) {
                                output_line(compactRaw + "=" + calcResult);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.123: 货币转换
                        {
                            auto curResults = pinyin_ime::currency_candidates(compactRaw);
                            if (!curResults.empty()) {
                                output_line(curResults[0]);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.123: 单位换算
                        {
                            auto unitResults = pinyin_ime::unit_conversion(compactRaw);
                            if (!unitResults.empty()) {
                                output_line(unitResults[0]);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.123: 百分比转中文
                        {
                            std::string pctResult = pinyin_ime::percent_to_chinese(compactRaw);
                            if (!pctResult.empty()) {
                                output_line(pctResult);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.128: 自定义快捷词（交互模式）
                        {
                            auto customResults = pinyin_ime::custom_phrase_lookup_ext(
                                compactRaw, userDataDir + "/custom_phrases.txt");
                            if (!customResults.empty()) {
                                output_line(customResults[0]);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.129: 句式模板（交互模式）
                        {
                            auto tmplResults = pinyin_ime::sentence_template_lookup(compactRaw);
                            if (!tmplResults.empty()) {
                                output_line(tmplResults[0]);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.126: 成语速查（交互模式）— 4字母首字母匹配
                        if (compactRaw.size() == 4) {
                            auto idiomResults = pinyin_ime::idiom_lookup(compactRaw);
                            if (!idiomResults.empty()) {
                                output_line(idiomResults[0]);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.126: 繁简转换（交互模式）— tf 前缀
                        if (compactRaw.size() > 2 && compactRaw[0] == 't' && compactRaw[1] == 'f') {
                            std::string tradResult = pinyin_ime::to_traditional(compactRaw.substr(2));
                            if (!tradResult.empty() && tradResult != compactRaw.substr(2)) {
                                output_line(tradResult);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.127: 网址/邮箱智能识别（交互模式）
                        {
                            auto urlResults = pinyin_ime::smart_url_lookup(compactRaw);
                            if (!urlResults.empty()) {
                                output_line(urlResults[0]);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        // v0.9.126: 中英文混输（交互模式）
                        {
                            auto mixedResults = pinyin_ime::mixed_input_lookup(compactRaw);
                            if (!mixedResults.empty()) {
                                output_line(mixedResults[0]);
                                ctx.clear();
                                page = 0;
                                candFocus = 0;
                                render();
                                continue;
                            }
                        }
                        if (englishDoubled) {
                            output_line(compactRaw);
                            ctx.clear();
                            page = 0;
                            candFocus = 0;
                            render();
                            continue;
                        }
                    }
                    if (!cont.empty() && pinyin_corrector::should_correct(cont)) {
                        std::string fixed = pinyin_corrector::correct_to_string(cont);
                        if (!fixed.empty() && fixed != cont) {
                            page = 0;
                            candFocus = 0;
                            ctx.clear();
                            for (char c : fixed) ctx.type(std::string(1, c));
                        }
                    }
                }
                // v0.9.90: 中英自动混输 — 无效拼音直接上屏英文
                {
                    std::string compactPy;
                    for (char c : ctx.preedit()) {
                        if (c != '\'' && c != ' ') compactPy.push_back(c);
                    }
                    // v0.9.110: 仅当「无任何中文候选」且非合法拼音时才上屏英文。
                    // 含简拼的混合输入（woshizg/nihaozg）虽有非法音节段，但 libime
                    // 能给出中文候选，必须走 commit 选候选，不能把原始输入上屏。
                    const auto &candsNow = ctx.candidatesToCursor();
                    if (!compactPy.empty() && candsNow.empty() &&
                        !is_valid_pinyin_inline(compactPy)) {
                        output_line(compactPy);
                        ctx.clear();
                        page = 0;
                        candFocus = 0;
                        render();
                        continue;
                    }
                }
                commit(page * kPageSize + candFocus);
            } else if (ch >= '1' && ch <= '9') {  // 数字：选当前页第 N 号上屏
                // v0.9.124: 联想词模式 — 数字键直接选联想词
                if (assocMode && ctx.empty() && !assocCands.empty()) {
                    size_t ai = static_cast<size_t>(ch - '1');
                    if (ai < assocCands.size()) {
                        output_line(assocCands[ai]);
                        lastCommitted += assocCands[ai];
                        assocCands = pinyin_ime::association_lookup(lastCommitted);
                        assocMode = !assocCands.empty();
                    }
                } else {
                    commit(page * kPageSize + static_cast<size_t>(ch - '1'));
                    candFocus = static_cast<size_t>(ch - '1');  // 焦点跟随所选候选
                }
            } else if (ch == '\'') {  // 单引号：空闲智能配对‘/’；打拼音中仍是隔音符（xi'an）
                if (ctx.empty()) {
                    output_line(singleQuoteOpen ? "’" : "‘");
                    singleQuoteOpen = !singleQuoteOpen;
                } else {
                    ctx.type("'");
                }
            } else if (iMode) {
                // v0.9.124: i模式 — 字母累积，数字选短语，Esc/空格退出
                if (ch >= 'a' && ch <= 'z') {
                    iModeBuf.push_back(ch);
                } else if (ch == 0x7f || ch == 0x08) {  // 退格
                    if (!iModeBuf.empty()) iModeBuf.pop_back();
                    else { iMode = false; }
                } else if (ch == ' ' || ch == '\r' || ch == '\n') {  // 空格/回车：选首选
                    auto phrases = pinyin_ime::i_mode_lookup(iModeBuf);
                    if (!phrases.empty()) {
                        output_line(phrases[0]);
                        lastCommitted = phrases[0];
                        assocCands = pinyin_ime::association_lookup(phrases[0]);
                        assocMode = !assocCands.empty();
                    }
                    iMode = false;
                    iModeBuf.clear();
                }
            } else if (is_pinyin_char(ch)) {  // 合法拼音字符：输入
                // v0.9.124: 退出联想词模式
                if (assocMode) { assocMode = false; assocCands.clear(); }
                // v0.9.124: i模式入口 — 空闲时按i进入快捷短语
                if (ch == 'i' && ctx.empty()) {
                    iMode = true;
                    iModeBuf.clear();
                } else {
                    ctx.type(std::string(1, ch));
                    candFocus = 0;
                }
            } else if (ch == '"') {  // 双引号智能配对：开“/闭”交替；空闲上屏，打拼音中先上屏焦点候选
                if (!ctx.empty()) commit(page * kPageSize + candFocus);
                output_line(quoteOpen ? "”" : "“");
                quoteOpen = !quoteOpen;
            } else if (!punct.empty()) {  // 标点：空闲直接上屏；打拼音中先上屏焦点候选再出标点
                if (!ctx.empty()) commit(page * kPageSize + candFocus);
                output_line(punct);
            }
        }
        render();
    }
    std::cout << "\n";
    if (persist) { recentHistory.save_to_file(userDataDir + "/recent_history.txt"); }
    if (persist) { userFreqTable.save_to_file(userDataDir + "/user_freq.txt"); }
    save_user_data();
    return 0;
}