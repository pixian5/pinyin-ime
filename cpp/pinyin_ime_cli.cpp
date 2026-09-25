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
//   -f, --fuzzy   启用模糊音（平翘舌 z/zh、c/ch、s/sh，前后鼻音 an/ang、en/eng、
//                 in/ing、ian/iang、uan/uang，n/l、f/h、l/r，及键盘邻近纠错）
//   -h, --help    显示本帮助
//
// 已实现（M1 范围）：逐键输入、候选列表、选词上屏、整句首选、简拼（默认）、
//   模糊音（--fuzzy）、会话内自学习、清屏。
// 待实现（见 README「未来优化方向」）：光标移动/逐词分段、用户词库持久化、双拼、
//   中英切换、标点、候选分页。
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
#include <string>
#include <vector>

#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinencoder.h>
#include <libime/pinyin/pinyinime.h>

#ifdef __unix__
#include <termios.h>
#include <unistd.h>
#endif

using namespace libime;

namespace {

// 是否是合法的拼音输入字符（小写字母 + 隔音符 '）。
bool is_pinyin_char(char ch) {
    return (ch >= 'a' && ch <= 'z') || ch == '\'';
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
        << "  -f, --fuzzy   启用模糊音（平翘舌/前后鼻音/n-l/键盘纠错）\n"
        << "  -h, --help    显示本帮助\n"
        << "简拼（首字母输入）默认开启：bjdx → 北京大学\n";
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

// 选中候选并（若已完全上屏）提交：打印、写入会话内自学习上下文、清空。
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
        ctx.learn();  // 会话内自学习（用户模型持久化见 README 待办）
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

// ---- 交互模式（termios raw mode，Linux / BSD）----
struct RawTerm {
#ifdef __unix__
    termios orig_{};
    bool active_ = false;
    bool enable() {
        if (!isatty(STDIN_FILENO)) return false;
        tcgetattr(STDIN_FILENO, &orig_);
        termios raw = orig_;
        raw.c_lflag &= ~static_cast<tcflag_t>(ICANON | ECHO);
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

}  // namespace

int main(int argc, char **argv) {
    // 解析参数：支持 -h/--help、-f/--fuzzy，位置参数为 [lm][dict][nbest]。
    bool fuzzy = false;
    std::vector<std::string> positional;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--fuzzy" || a == "-f") {
            fuzzy = true;
        } else if (a == "--help" || a == "-h") {
            usage(argv[0]);
            return 0;
        } else if (a.size() >= 2 && a[0] == '-') {
            std::cerr << "未知参数: " << a << "（用 --help 查看用法）\n";
            return 2;
        } else {
            positional.push_back(a);
        }
    }
    std::string lmPath = positional.size() > 0 ? positional[0] : "";
    std::string dictPath = positional.size() > 1 ? positional[1] : "";
    int nbest = positional.size() > 2 ? std::atoi(positional[2].c_str()) : 5;
    if (nbest < 1) nbest = 5;

    if (lmPath.empty()) {
        lmPath = find_data({
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
            "/workspace/native-build/opt/libime/share/libime/sc.dict",
            "/usr/share/libime/sc.dict",
            "/usr/lib64/libime/sc.dict",
            "/usr/lib/libime/sc.dict",
            "/usr/lib/aarch64-linux-gnu/libime/sc.dict",
            "/usr/lib/x86_64-linux-gnu/libime/sc.dict",
        }, "系统词典（sc.dict）");
    }

    // 与 libime_bench.cpp 一致：UserLanguageModel 加载系统 LM 作为打分模型。
    PinyinIME ime(std::make_unique<PinyinDictionary>(),
                  std::make_unique<UserLanguageModel>(lmPath.c_str()));
    ime.dict()->load(PinyinDictionary::SystemDict, dictPath.c_str(),
                     PinyinDictFormat::Binary);
    ime.setNBest(static_cast<size_t>(nbest));

    if (fuzzy) {
        ime.setFuzzyFlags(common_fuzzy_flags());
        std::cerr << "pinyin_ime_cli: 模糊音已启用\n";
    }

    PinyinContext ctx(&ime);

    RawTerm raw;
    if (!raw.enable()) {
        // ---- 行模式：每行一段全拼/简拼 → 候选列表 ----
        std::string line;
        while (std::getline(std::cin, line)) {
            ctx.clear();
            for (char ch : line) {
                if (is_pinyin_char(ch)) ctx.type(std::string(1, ch));
            }
            auto cands = collect_candidates(ctx, static_cast<size_t>(nbest));
            for (size_t i = 0; i < cands.size(); ++i) {
                if (i) std::cout << '\t';
                std::cout << cands[i];
            }
            std::cout << '\n';
            std::cout.flush();
        }
        return 0;
    }

    // ---- 交互模式 ----
    auto render = [&]() {
        std::cout << "\r\x1b[2K";  // 清当前行
        std::string pre = ctx.preedit();
        std::cout << (pre.empty() ? "拼音: _" : "拼音: " + pre) << "  |  ";
        const auto &cands = ctx.candidatesToCursor();
        size_t show = std::min<size_t>(cands.size(), static_cast<size_t>(nbest));
        for (size_t i = 0; i < show; ++i) {
            std::cout << (i + 1) << "." << cands[i].toString() << "  ";
        }
        std::cout.flush();
    };

    std::cout << "pinyin_ime_cli 交互模式：输入拼音（支持简拼），数字/空格/回车上屏，退格删除，Esc 清空，Ctrl-C 退出\n";
    render();

    char ch;
    while (read(STDIN_FILENO, &ch, 1) == 1) {
        if (ch == 0x03) {  // Ctrl-C
            std::cout << "\n";
            break;
        }
        if (ch == 0x1b) {  // Esc：清空
            ctx.clear();
        } else if (ch == 0x7f || ch == 0x08) {  // 退格（DEL/BS）
            ctx.backspace();
        } else if (ch == ' ' || ch == '\r' || ch == '\n') {  // 空格/回车：上屏首选
            select_and_commit(ctx, 0);
        } else if (ch >= '1' && ch <= '9') {  // 数字：选第 N 号上屏
            select_and_commit(ctx, static_cast<size_t>(ch - '1'));
        } else if (is_pinyin_char(ch)) {  // 合法拼音字符：输入
            ctx.type(std::string(1, ch));
        }
        render();
    }
    std::cout << "\n";
    return 0;
}