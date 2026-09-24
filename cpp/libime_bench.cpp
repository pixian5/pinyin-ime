// libime_bench.cpp — 把 libime 的整句拼音结果暴露成行协议，供 pinyinbench 评测。
//
// 协议（与 bench/pinyinbench/engines.py 的 CliEngine 对应）：
//   标准输入：每行一个连续全拼串（如 "nihaoshijie"）
//   标准输出：每行一组候选，\t 分隔，第一个是整句首选
//
// 用法：
//   libime_bench [语言模型路径] [系统词典路径] [nbest]     # 整句模式
//   libime_bench perkey [语言模型路径] [系统词典路径] [nbest]  # 逐键延迟模式
// 不给参数时自动在常见位置搜索 sc.lm / sc.dict。
// 逐键模式：每行输入仍是一串全拼，但逐字符喂给引擎并计每键耗时，
// 输出每行对应一组 \t 分隔的毫秒数（第 i 个数 = 第 i 个有效键的耗时）。
//
// 注意：本文件在容器内编译验证，宿主是 macOS 时无法直接编译。
#include <cstdlib>
#include <chrono>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <sstream>
#include <string>
#include <vector>

#include <libime/core/lattice.h>
#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinencoder.h>
#include <libime/pinyin/pinyinime.h>

using namespace libime;

namespace {

bool exists(const std::string &p) {
    if (p.empty()) return false;
    std::ifstream f(p);
    return f.good();
}

std::string find_data(const std::vector<std::string> &candidates, const char *what) {
    for (const auto &p : candidates) {
        if (exists(p)) return p;
    }
    std::cerr << "libime_bench: 找不到 " << what << "，请用参数显式指定路径\n";
    std::exit(2);
}

// 逐键延迟计时：用高精度时钟测单次 ctx.type() 的耗时（毫秒）。
using Clock = std::chrono::high_resolution_clock;

double ms_since(Clock::time_point t0) {
    return std::chrono::duration<double, std::milli>(Clock::now() - t0).count();
}

// 是否是合法的拼音输入字符（小写字母 + 隔音符 '）。
bool is_pinyin_char(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '\'';
}

}  // namespace

int main(int argc, char **argv) {
    // 运行模式：argv[1] == "perkey" 进入逐键延迟模式，其余参数整体后移一位。
    // 逐键模式测的是真实输入法「每敲一键、引擎增量更新」的单键耗时，
    // 区别于默认整句模式把整串拼音一次性喂完（那是另一种口径，两者不可比）。
    bool perkey = false;
    int ai = 1;
    if (argc > ai && std::string(argv[ai]) == "perkey") {
        perkey = true;
        ++ai;
    }
    std::string lmPath = argc > ai ? argv[ai++] : "";
    std::string dictPath = argc > ai ? argv[ai++] : "";
    int nbest = argc > ai ? std::atoi(argv[ai++]) : 5;

    if (lmPath.empty()) {
        // Debian 12 的语言模型叫 zh_CN.lm 且装在 triplet 目录下，Ubuntu 上是 sc.lm，都要覆盖
        lmPath = find_data({
            "/usr/lib/aarch64-linux-gnu/libime/zh_CN.lm",
            "/usr/lib/x86_64-linux-gnu/libime/zh_CN.lm",
            "/usr/lib/libime/zh_CN.lm",
            "/usr/share/libime/zh_CN.lm",
            "/usr/share/libime/sc.lm",
            "/usr/lib/libime/sc.lm",
        }, "语言模型（来自 libime-data-language-model）");
    }
    if (dictPath.empty()) {
        dictPath = find_data({
            "/usr/share/libime/sc.dict",
            "/usr/lib/libime/sc.dict",
            "/usr/lib/aarch64-linux-gnu/libime/sc.dict",
            "/usr/lib/x86_64-linux-gnu/libime/sc.dict",
        }, "sc.dict（系统词典，来自 libime-data）");
    }

    // 默认不启用模糊音：基线评测按"完美拼音"条件跑，模糊音单独作为对照实验。
    // 注意：UserLanguageModel 只接受 const char*（另一个重载要 shared_ptr<StaticLanguageModelFile>）
    PinyinIME ime(std::make_unique<PinyinDictionary>(),
                  std::make_unique<UserLanguageModel>(lmPath.c_str()));
    ime.dict()->load(PinyinDictionary::SystemDict, dictPath.c_str(),
                     PinyinDictFormat::Binary);
    ime.setNBest(static_cast<size_t>(nbest));
    PinyinContext ctx(&ime);

    std::string line;
    while (std::getline(std::cin, line)) {
        ctx.clear();

        if (perkey) {
            // 逐键模式：逐字符 type 并计时，输出每键耗时（毫秒，\t 分隔）。
            // 每行输入对应一行输出，保证与评测方严格一一对应；无有效字符则输出空行。
            std::ostringstream os;
            os << std::fixed << std::setprecision(3);
            bool first = true;
            for (char ch : line) {
                if (!is_pinyin_char(ch)) continue;
                auto t0 = Clock::now();
                ctx.type(std::string(1, ch));
                double ms = ms_since(t0);
                if (!first) os << '\t';
                os << ms;
                first = false;
            }
            std::cout << os.str() << "\n";
            std::cout.flush();
            continue;
        }

        // 整句模式（原协议）：整串一次喂完，输出整句首选 + 候选列表。
        for (char ch : line) {
            if (is_pinyin_char(ch)) {
                ctx.type(std::string(1, ch));
            }
        }

        std::string out = ctx.sentence();
        for (const auto &candidate : ctx.candidatesToCursor()) {
            std::string text;
            for (const auto *node : candidate.sentence()) {
                text += node->word();
            }
            if (!text.empty() && text != out) {
                out += "\t" + text;
            }
        }
        std::cout << out << "\n";
        std::cout.flush();
    }
    return 0;
}
