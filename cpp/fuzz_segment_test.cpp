// fuzz_segment_test.cpp — 拼音切分/消歧/候选管线的变异模糊测试（v0.9.193）
//
// 为什么不用 libFuzzer：项目要求零额外依赖、能在任意 g++ 环境跑。
// 这里用固定种子的 xorshift 生成随机输入 + 一组对抗性构造（全同字符、超长串、
// 非法 UTF-8 字节、混合标点），配合 ASan/UBSan 编译即可捕获越界与 UB。
//
// 用法:
//   g++ -std=c++20 -O1 -Icpp cpp/fuzz_segment_test.cpp -o /tmp/fz && /tmp/fz 2000
//   参数：迭代次数（默认 2000）；设置 FUZZ_SEED 可换种子。
//
// 不变量（任何一条被破坏都算失败）：
//   1) 任何输入都不抛异常、不崩溃；
//   2) disambiguate_pinyin 的输出长度不超过输入的 2 倍 + 8（只插分隔符，不做替换）；
//   3) quick_candidates 系列返回的候选条数不超过内部上限（这里用 64 兜底断言）；
//   4) 超长输入（100k 字符）在 200ms 级别内完成，不出现指数级退化。

#include <chrono>
#include <cstring>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

#include "abbr_presets.h"
#include "quick_input.h"

namespace {

unsigned long long rng_state = 0x9E3779B97F4A7C15ULL;
unsigned long long next_rand() {  // xorshift64*
    rng_state ^= rng_state >> 12;
    rng_state ^= rng_state << 25;
    rng_state ^= rng_state >> 27;
    return rng_state * 2685821657736338717ULL;
}
unsigned pick(unsigned n) { return static_cast<unsigned>(next_rand() % n); }

const char *kAscii = "abcdefghijklmnopqrstuvwxyz";
const char *kPunct = " ',-.;:/\\[]{}()!?@#$%^&*_+=<>~`|\"";
const char *kChinese[] = {"你好", "西安", "现在", "北京", "中国", "输入法", "测试", "拼音"};

std::string random_input(size_t max_len) {
    std::string s;
    size_t len = pick(static_cast<unsigned>(max_len + 1));
    for (size_t i = 0; i < len; ++i) {
        switch (pick(10)) {
            case 0: case 1: case 2: case 3: s.push_back(kAscii[pick(26)]); break;
            case 4: s.push_back(static_cast<char>('A' + pick(26))); break;
            case 5: s.push_back(kPunct[pick(static_cast<unsigned>(strlen(kPunct)))]); break;
            case 6: s += kChinese[pick(8)]; break;
            case 7: s.push_back(static_cast<char>(0x80 + pick(0x7F))); break;  // 非法 UTF-8 续字节
            case 8: s += (pick(2) ? "xi'an" : "xian"); break;
            default: s.push_back(static_cast<char>('0' + pick(10))); break;
        }
    }
    return s;
}

int failures = 0;
void expect(bool cond, const char *what, const std::string &input) {
    if (!cond) {
        ++failures;
        if (failures <= 10) {
            std::printf("  ❌ %s（输入长度 %zu）\n", what, input.size());
        }
    }
}

void exercise(const std::string &input) {
    std::string dis;
    try {
        dis = disambiguate_pinyin(input);
    } catch (...) {
        expect(false, "disambiguate_pinyin 抛异常", input);
        return;
    }
    expect(dis.size() <= input.size() * 2 + 8, "消歧输出长度异常膨胀", input);

    // 纯全拼判定必须对同一输入稳定（重复调用结果一致）
    bool seg1 = pinyin_ime::pinyin_fully_segmentable(input);
    bool seg2 = pinyin_ime::pinyin_fully_segmentable(input);
    expect(seg1 == seg2, "pinyin_fully_segmentable 不稳定", input);

    auto c1 = pinyin_ime::quick_candidates(input);
    expect(c1.size() <= 64, "quick_candidates 候选数超上限", input);
    auto c5 = pinyin_ime::quick_candidates_v5(input, "", "default");
    expect(c5.size() <= 64, "quick_candidates_v5 候选数超上限", input);
    auto v11 = pinyin_ime::quick_candidates_v11(input);
    expect(v11.size() <= 64, "quick_candidates_v11 候选数超上限", input);
}

}  // namespace

int main(int argc, char **argv) {
    long iterations = argc > 1 ? std::atol(argv[1]) : 2000;
    if (const char *seed = std::getenv("FUZZ_SEED")) {
        rng_state = std::strtoull(seed, nullptr, 10);
        if (rng_state == 0) rng_state = 0x9E3779B97F4A7C15ULL;
    }

    std::printf("fuzz: %ld 次随机输入（seed=%llu）\n", iterations, rng_state);
    for (long i = 0; i < iterations; ++i) {
        exercise(random_input(200));
    }

    // 对抗性构造：这些不是随机能碰到的形态
    const std::vector<std::string> adversarial = {
        "",
        "a",
        std::string(100000, 'a'),           // 超长同字符
        std::string(100000, ' '),           // 全空格
        [] { std::string s; for (int i = 0; i < 20000; ++i) s += "xi'an"; return s; }(),
        std::string("\x00\x01\x80\xff", 4),  // 控制字符与非法字节
        "nihao" + std::string(5000, ',') + "shijie",
    };
    for (const auto &in : adversarial) {
        auto t0 = std::chrono::steady_clock::now();
        exercise(in);
        auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(
                      std::chrono::steady_clock::now() - t0).count();
        // 超长输入不应出现指数退化；ASan/UBSan 有 5-10 倍开销，阈值相应放宽
#ifdef __SANITIZE_ADDRESS__
        constexpr long long kLongLimit = 2000;
#else
        constexpr long long kLongLimit = 500;
#endif
        expect(ms < kLongLimit, "超长输入耗时超过上限", in);
        std::printf("  adversarial len=%-7zu %lldms\n", in.size(),
                    static_cast<long long>(ms));
    }

    if (failures) {
        std::printf("===== fuzz 失败：%d 处不变量被破坏 =====\n", failures);
        return 1;
    }
    std::printf("===== fuzz 通过：无崩溃、无不变量破坏 =====\n");
    return 0;
}
