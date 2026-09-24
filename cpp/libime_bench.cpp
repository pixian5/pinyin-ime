// libime_bench.cpp — 把 libime 的整句拼音结果暴露成行协议，供 pinyinbench 评测。
//
// 协议（与 bench/pinyinbench/engines.py 的 CliEngine 对应）：
//   标准输入：每行一个连续全拼串（如 "nihaoshijie"）
//   标准输出：每行一组候选，\t 分隔，第一个是整句首选
//
// 用法：
//   libime_bench [语言模型路径] [系统词典路径] [nbest]
// 不给参数时自动在常见位置搜索 sc.lm / sc.dict。
//
// 注意：本文件在容器内编译验证，宿主是 macOS 时无法直接编译。
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <memory>
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

}  // namespace

int main(int argc, char **argv) {
    std::string lmPath = argc > 1 ? argv[1] : "";
    std::string dictPath = argc > 2 ? argv[2] : "";
    int nbest = argc > 3 ? std::atoi(argv[3]) : 5;

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
        if (line.empty()) {
            std::cout << "\n";
            std::cout.flush();
            continue;
        }
        ctx.clear();
        for (char ch : line) {
            if ((ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '\'') {
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
