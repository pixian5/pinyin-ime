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

#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinencoder.h>
#include <libime/pinyin/pinyinime.h>
#include <libime/core/prediction.h>
#include <libime/pinyin/shuangpinprofile.h>

#include "emoji_list.h"
#include "pinyin_corrector.h"

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

// 拼音歧义预处理：处理常见的分割歧义（xian→xi'an, jian→ji'an 等）
// 当歧义音节后跟常见上下文词时，插入隔音符帮助解码器正确分割
std::string disambiguate_pinyin(const std::string &input) {
    if (input.empty() || input.find('\'') != std::string::npos)
        return input;  // 已含隔音符，不处理

    std::string result = input;

    // 歧义音节列表：(歧义串, 拆分形式)
    // 这些音节既可以作为完整音节，也可以拆分为两个音节
    struct AmbigEntry {
        const char *full;    // 完整形式 (如 "xian")
        const char *split;   // 拆分形式 (如 "xi'an")
    };
    // 只处理最明确的歧义：xian→xi'an (西安)
    // 其他歧义 (jian→ji'an 等) 误分割率太高，不处理
    static const AmbigEntry ambigTable[] = {
        {"xian", "xi'an"},   // 西安 vs 先/现/显
    };

    // 当歧义音节后跟这些拼音时，倾向于拆分（地名+常见词）
    // 例如 "xianyou" → "xi'anyou" (西安有) 而非 "现有"
    // 但 "xiandai" → "现代" 不拆分
    static const char *splitContexts[] = {
        "you", "shi", "de", "hao", "jiu", "zai", "yao",
        "hai", "bu", "mei", "neng", "hui", "ke", "dao",
        "qi", "ba", "ma", "ne", "le", "ge", "zuo", "you",
        "hen", "duo", "youdouo", "shi", "deshi",
        // 地名常见搭配
        "youhenduo", "youhenduolishiguji",
        "shoudu", "chengshi", "daxue", "shifan",
    };

    // 不拆分的上下文（这些词中完整音节更常见）
    static const char *keepContexts[] = {
        "dai",    // 现代 xiandai
        "zai",    // 现在 xianzai (但 zai 也在 splitContexts... 冲突)
        "shi",    // 现实 xianshi (但 shi 也在 splitContexts... 冲突)
        "xiang",  // 现象 xianxiang
        "lia",    // 现量
        "jin",    // 现金
        "chang",  // 现场
        "ming",   // 鲜明
        "huo",    // 鲜活
        "yan",    // 鲜艳
        "neng",   // 显能
        "shi",    // 显示
        "zhu",    // 显著
        "ran",    // 显然
    };

    // 简化策略：只处理最明确的歧义 case
    // "xian" + 地名上下文 → "xi'an"
    for (const auto &entry : ambigTable) {
        std::string full = entry.full;
        std::string split = entry.split;

        size_t pos = 0;
        while ((pos = result.find(full, pos)) != std::string::npos) {
            // 检查后面跟着什么
            size_t afterPos = pos + full.size();
            if (afterPos >= result.size()) {
                pos = afterPos;
                continue;  // 歧义音节在末尾，不拆分
            }

            // 取后面最多 10 个字符作为上下文
            std::string after = result.substr(afterPos, std::min(size_t(10), result.size() - afterPos));

            // 检查前面是否构成常见词（不拆分）
            // 例如 "shi"+"xian" = 实现/视线/显现, "xian"+"xian" = 现先
            if (pos >= 3) {
                std::string before3 = result.substr(pos >= 3 ? pos - 3 : 0, std::min(pos, size_t(3)));
                std::string before2 = result.substr(pos >= 2 ? pos - 2 : 0, std::min(pos, size_t(2)));
                // 常见前缀词：视线/实现/显现/鲜鲜/现先 → 不拆分
                if (before3 == "shi" || before2 == "xi" || before3 == "xian" ||
                    before3 == "zai" || before3 == "you" || before3 == "zhe") {
                    pos = afterPos;
                    continue;
                }
            }

            // 检查是否在不拆分上下文中
            bool shouldKeep = false;
            for (const char *ctx : keepContexts) {
                if (after.starts_with(ctx)) {
                    shouldKeep = true;
                    break;
                }
            }
            if (shouldKeep) {
                pos = afterPos;
                continue;
            }

            // 检查是否在拆分上下文中
            bool shouldSplit = false;
            for (const char *ctx : splitContexts) {
                if (after.starts_with(ctx)) {
                    shouldSplit = true;
                    break;
                }
            }

            if (shouldSplit) {
                result = result.substr(0, pos) + split + result.substr(afterPos);
                pos = pos + split.size();
            } else {
                pos = afterPos;
            }
        }
    }

    return result;
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
        << "  --quiet         抑制信息性 stderr 输出（配合 --json 使用更干净）\n"
        << "  --json          行模式输出 JSON 数组（方便程序化调用）\n"
        << "  --stats         打印引擎统计（词典/LM/内存）后退出\n"
        << "  --version       显示版本号后退出\n"
        << "  --learn-file F  批量学习：每行 拼音<TAB>中文，训练用户语言模型\n"
        << "  --dump-user     导出用户学习历史（人类可读）后退出\n"
        << "  --batch F       从文件批量处理（每行一段拼音，输出候选）\n"
        << "  --top1          只输出首选候选（方便管道 | 其他工具）\n"
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
        PinyinFuzzyFlag::Z_ZH,       PinyinFuzzyFlag::L_R,
        PinyinFuzzyFlag::AN_ANG,     PinyinFuzzyFlag::EN_ENG,
        PinyinFuzzyFlag::IAN_IANG,   PinyinFuzzyFlag::IN_ING,
        PinyinFuzzyFlag::U_OU,       PinyinFuzzyFlag::UAN_UANG,
        PinyinFuzzyFlag::V_U,        PinyinFuzzyFlag::VE_UE,
        PinyinFuzzyFlag::CommonTypo, PinyinFuzzyFlag::AdvancedTypo,
        PinyinFuzzyFlag::Correction,
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
    if (name == "gb") return ShuangpinBuiltinProfile::GB;
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
    case ShuangpinBuiltinProfile::GB: return "国标";
    case ShuangpinBuiltinProfile::Xiaohe: default: return "小鹤";
    }
}

// 中文标点：半角 ASCII 标点 → 全角中文标点。纯前端（不进拼音解码），
// 只负责「上屏标点」这一件事。返回空串表示该字符无对应中文标点。
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

int main(int argc, char **argv) {
    // 解析参数：支持 -h/--help、-f/--fuzzy、--user-data DIR；位置参数为 [lm][dict][nbest]。
    bool fuzzy = false;
    bool correctPinyin = false;  // --correct：先经 M3 纠错再解码
    bool shuangpin = false;
    bool jsonOutput = false;     // --json：行模式输出 JSON 数组
    bool statsOnly = false;     // --stats：打印引擎统计后退出
    bool quiet = false;          // --quiet：抑制信息性 stderr 输出
    std::string learnFile;       // --learn-file：批量学习
    std::string batchFile;       // --batch：从文件批量处理

    // --batch：从文件批量处理
    bool dumpUser = false;       // --dump-user：导出用户学习历史
    bool top1Only = false;       // --top1：只输出首选候选
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
        } else if (a == "--json") {
            jsonOutput = true;
        } else if (a == "--version") {
            std::cout << "pinyin_ime_cli 0.9.7\n";
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

    if (fuzzy) {
        ime.setFuzzyFlags(common_fuzzy_flags());
        if (!quiet) std::cerr << "pinyin_ime_cli: 模糊音已启用\n";
    }

    // 用户词库 + 自学习历史持久化：简拼歧义大，让用户常用词/新词靠前的关键。
    // user.history 存 UserLanguageModel 的增量历史（不含 34MB 系统 LM），
    // user.dict 存 UserDict（含 learn() 自动造出来的用户词）。
    std::string userHistPath, userDictPath;
    const bool persist = !userDataDir.empty();
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
        std::cout << "  版本: 0.9.7\n";
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
            if (correctPinyin && pinyin_corrector::should_correct(line)) {
                std::string fixed = pinyin_corrector::correct_to_string(line);
                if (!fixed.empty()) line = fixed;
            }
            line = disambiguate_pinyin(line);
            for (char ch : line) {
                if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
            }
            auto cands = collect_candidates(ctx, static_cast<size_t>(nbest));
            if (top1Only) {
                if (!cands.empty()) std::cout << cands[0];
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
                std::cout << '\n';
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

    // 上屏即落盘：崩溃/被 kill 时最多丢「最后一次上屏」的增量（防丢措施，与 addon 对齐）。
    // 成功上屏时把 (文本, 拼音) 压入撤回栈，供 Ctrl-Z 撤回后恢复拼音重新编辑。
    auto commit = [&](size_t idx) {
        if (ctx.empty()) return;
        const std::string preText = ctx.sentence();         // 上屏文本（撤回提示/删行用）
        const std::string prePinyin = preedit_compact(ctx); // 恢复拼音（撤回后重新打字用）
        if (select_and_commit(ctx, idx)) {
            if (!preText.empty()) {
                undoStack.emplace_back(preText, prePinyin);
                if (undoStack.size() > 32) undoStack.erase(undoStack.begin());
            }
            save_user_data();
        }
    };

    RawTerm raw;
    if (!raw.enable()) {
        // ---- 行模式：每行一段全拼/简拼 → 候选列表 ----
        std::string line;
        while (std::getline(std::cin, line)) {
            ctx.clear();
            // --correct：先纠错再解码（C++ 原生，替换行模式每行 fork Python 的桥接）
            if (correctPinyin && pinyin_corrector::should_correct(line)) {
                std::string fixed = pinyin_corrector::correct_to_string(line);
                if (!fixed.empty()) line = fixed;
            }
            line = disambiguate_pinyin(line);
            for (char ch : line) {
                if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
            }
            auto cands = collect_candidates(ctx, static_cast<size_t>(nbest));
            if (top1Only) {
                if (!cands.empty()) std::cout << cands[0];
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
        auto [pre, cursorPos] = ctx.preeditWithCursor();
        if (cursorPos > pre.size()) {
            cursorPos = pre.size();
        }
        std::string preStr = pre;
        if (!preStr.empty() && cursorPos <= preStr.size()) {
            preStr.insert(cursorPos, "▏");
        }
        std::cout << (preStr.empty() ? "拼音: _" : "拼音: " + preStr) << "  |  ";
        const auto &cands = ctx.candidatesToCursor();
        const size_t total = cands.size();
        const size_t maxPage = total == 0 ? 0 : (total - 1) / kPageSize;
        if (page > maxPage) page = maxPage;
        const size_t begin = page * kPageSize;
        const size_t end = std::min<size_t>(total, begin + kPageSize);
        for (size_t i = begin; i < end; ++i) {
            if (i - begin == candFocus) {
                // 焦点候选：* 前缀标记（状态行可读；回归按 "\*2\." 等断言）
                std::cout << "*" << (i - begin + 1) << "." << cands[i].toString() << "  ";
            } else {
                std::cout << (i - begin + 1) << "." << cands[i].toString() << "  ";
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
            } else if (ch == '\t') {  // Tab：仅切回中文，缓冲保留
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
            ctx.clear();
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
                englishMode = true;
                engBuf.clear();
            } else if (ch == 0x7f || ch == 0x08) {  // 退格（DEL/BS）
                ctx.backspace();
                candFocus = 0;
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
                // --correct 交互态：整串纠错后上屏（只对「全拼打错」生效；简拼/无法解析
                // 的输入由 corrector 返回 nullopt 自动跳过，不误伤简拼）。
                if (correctPinyin && !ctx.empty()) {
                    std::string cont;
                    // preedit 音节间可能带空格（显示 "sen me"）、隔音符 xi'an 带 '，都滤掉。
                    for (char c : ctx.preedit()) {
                        if (c != '\'' && c != ' ') cont.push_back(c);
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
                commit(page * kPageSize + candFocus);
            } else if (ch >= '1' && ch <= '9') {  // 数字：选当前页第 N 号上屏
                commit(page * kPageSize + static_cast<size_t>(ch - '1'));
                candFocus = static_cast<size_t>(ch - '1');  // 焦点跟随所选候选
            } else if (ch == '\'') {  // 单引号：空闲智能配对‘/’；打拼音中仍是隔音符（xi'an）
                if (ctx.empty()) {
                    output_line(singleQuoteOpen ? "’" : "‘");
                    singleQuoteOpen = !singleQuoteOpen;
                } else {
                    ctx.type("'");
                }
            } else if (is_pinyin_char(ch)) {  // 合法拼音字符：输入
                ctx.type(std::string(1, ch));
                candFocus = 0;
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
    save_user_data();
    return 0;
}