// nbest_dump.cpp — 把 libime 的 nbest 候选连同内部打分、分词结构导出，供重排序实验使用。
//
// 协议（stdin 每行一个连续拼音，stdout 每行一个候选，制表符分隔）：
//   pinyin <TAB> rank <TAB> score <TAB> wordCount <TAB> words(|分隔) <TAB> costs(|分隔) <TAB> lmScore <TAB> uniSum <TAB> uniMin <TAB> charScore <TAB> text
//
// 与 libime_bench 的区别：libime_bench 只输出候选文本，用于产品口径评测；
// 本工具额外保留 libime 的原始路径分数与分词，便于离线分析“首选为什么错”。
//
// 用法：
//   nbest_dump [语言模型路径] [系统词典路径] [nbest] < bench/holdout/all_pinyin.txt
#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include <libime/core/lattice.h>
#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinencoder.h>
#include <libime/pinyin/pinyinime.h>

#include "char_lm.h"

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
    std::cerr << "nbest_dump: 找不到 " << what << "，请用参数显式指定路径\n";
    std::exit(2);
}

bool is_pinyin_char(char ch) {
    return (ch >= 'a' && ch <= 'z') || (ch >= 'A' && ch <= 'Z') || ch == '\'';
}

}  // namespace

int main(int argc, char **argv) {
    std::string lmPath = argc > 1 ? argv[1] : "";
    std::string dictPath = argc > 2 ? argv[2] : "";
    int nbest = argc > 3 ? std::atoi(argv[3]) : 10;

    if (lmPath.empty()) {
        lmPath = find_data({
            "/usr/lib64/libime/zh_CN.lm",
            "/usr/lib/aarch64-linux-gnu/libime/zh_CN.lm",
            "/usr/lib/x86_64-linux-gnu/libime/zh_CN.lm",
            "/usr/lib/libime/zh_CN.lm",
            "/usr/share/libime/zh_CN.lm",
            "/usr/share/libime/sc.lm",
            "/usr/lib64/libime/sc.lm",
            "/usr/lib/libime/sc.lm",
        }, "语言模型");
    }
    if (dictPath.empty()) {
        dictPath = find_data({
            "/usr/share/libime/sc.dict",
            "/usr/lib64/libime/sc.dict",
            "/usr/lib/libime/sc.dict",
            "/usr/lib/aarch64-linux-gnu/libime/sc.dict",
            "/usr/lib/x86_64-linux-gnu/libime/sc.dict",
        }, "系统词典");
    }

    auto charLM = pinyin_ime::CharLM::findDefault(argv[0]);
    if (charLM) {
        std::cerr << "nbest_dump: char_lm=" << charLM->path()
                  << " vocab=" << charLM->vocabSize() << "\n";
    } else {
        std::cerr << "nbest_dump: 未找到 char_lm，charScore 列恒为 0\n";
    }

    PinyinIME ime(std::make_unique<PinyinDictionary>(),
                  std::make_unique<UserLanguageModel>(lmPath.c_str()));
    ime.dict()->load(PinyinDictionary::SystemDict, dictPath.c_str(),
                     PinyinDictFormat::Binary);
    ime.setNBest(static_cast<size_t>(nbest));
    PinyinContext ctx(&ime);

    std::cout << std::setprecision(9);
    std::string line;
    while (std::getline(std::cin, line)) {
        ctx.clear();
        std::string pinyin;
        for (char ch : line) {
            if (is_pinyin_char(ch)) {
                pinyin.push_back(ch);
                ctx.type(std::string(1, ch));
            }
        }
        if (pinyin.empty()) {
            continue;
        }

        const auto &cands = ctx.candidates();
        const size_t limit = std::min(cands.size(), static_cast<size_t>(nbest));
        for (size_t rank = 0; rank < limit; ++rank) {
            std::string text;
            std::string words;
            std::string costs;
            std::vector<std::string_view> wordViews;
            size_t wordCount = 0;
            for (const auto *node : cands[rank].sentence()) {
                if (!node->word().empty()) {
                    if (!words.empty()) words.push_back('|');
                    words += node->word();
                    text += node->word();
                    if (!costs.empty()) costs.push_back('|');
                    costs += std::to_string(node->cost());
                    wordViews.push_back(node->word());
                    ++wordCount;
                }
            }
            const auto &beginState = ime.model()->beginState();
            float lmScore = ime.model()->wordsScore(beginState, wordViews);
            float uniSum = 0.0f;
            float uniMin = 0.0f;
            for (const auto &word : wordViews) {
                float s = ime.model()->singleWordScore(word);
                uniSum += s;
                if (s < uniMin) uniMin = s;
            }
            float charScore = charLM ? charLM->score(text) : 0.0f;
            std::cout << pinyin << '\t' << rank << '\t' << cands[rank].score()
                      << '\t' << wordCount << '\t' << words << '\t' << costs
                      << '\t' << lmScore << '\t' << uniSum << '\t' << uniMin
                      << '\t' << charScore << '\t' << text << '\n';
        }
    }
    return 0;
}
