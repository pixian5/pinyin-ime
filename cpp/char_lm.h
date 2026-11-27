// char_lm.h — 字符 trigram 重排序模型（v0.9.197）
//
// 目的：系统 zh_CN.lm 在 nbest 候选之间区分力不足。本模型不参与解码，
// 只对已经生成的整句候选做一个「像不像正常中文」的字符级打分，
// 再与 libime 原始路径分相加后重排。
//
// 模型由 scripts/train_char_lm.py 从 bench/corpus 训练生成，默认查找：
//   1. 环境变量 PINYINIME_CHAR_LM
//   2. ./data/char_trigram.bin
//   3. <可执行文件目录>/../share/pinyin-ime/char_trigram.bin
//   4. /usr/share/pinyin-ime/char_trigram.bin
#ifndef PINYIN_IME_CHAR_LM_H
#define PINYIN_IME_CHAR_LM_H

#include <cctype>
#include <cstdint>
#include <memory>
#include <string>
#include <string_view>
#include <unordered_map>
#include <vector>

namespace pinyin_ime {

// v0.9.197：把拼音串压成小写纯字母，并把 UTF-8 的 ü/Ü 归一成 v，
// 供「候选是否覆盖整段输入」的长度比较使用（输入通常写 lv，候选可能回 lü）。
inline std::string compactPinyin(const std::string &input) {
    std::string out;
    out.reserve(input.size());
    for (size_t i = 0; i < input.size(); ++i) {
        unsigned char c = static_cast<unsigned char>(input[i]);
        if (c == 0xC3 && i + 1 < input.size()) {
            unsigned char n = static_cast<unsigned char>(input[i + 1]);
            if (n == 0xBC || n == 0x9C) { // ü / Ü
                out.push_back('v');
                ++i;
                continue;
            }
        }
        if (std::isalpha(c))
            out.push_back(static_cast<char>(std::tolower(c)));
    }
    return out;
}

class CharLM {
public:
    struct BiCtx {
        uint16_t prev;
        uint16_t total;
    };
    struct BiEntry {
        uint16_t prev;
        uint16_t ch;
        uint16_t count;
    };
    struct TriCtx {
        uint16_t p2;
        uint16_t p1;
        uint16_t total;
    };
    struct TriEntry {
        uint16_t p2;
        uint16_t p1;
        uint16_t ch;
        uint16_t count;
    };
    // v0.9.207：4-gram 上下文/条目；v1 模型没有这两段，加载后为空，
    // score() 自动回退到原 trigram 插值。
    struct QuadCtx {
        uint16_t p3;
        uint16_t p2;
        uint16_t p1;
        uint16_t total;
    };
    struct QuadEntry {
        uint16_t p3;
        uint16_t p2;
        uint16_t p1;
        uint16_t ch;
        uint16_t count;
    };

    // 加载失败返回 nullptr，并在 err（非空）里写明原因；不抛异常。
    static std::unique_ptr<CharLM> load(const std::string &path,
                                        std::string *err = nullptr);

    // 按固定顺序查找默认模型；全部不存在时返回 nullptr。
    static std::unique_ptr<CharLM> findDefault(const std::string &argv0 = "");

    // 整句字符 4-gram（v2）/trigram（v1）对数概率（越高越好，通常为负）。
    // v2 使用 stupid backoff：4-gram 命中用 log(c4/t4)，退回 3/2/1-gram
    // 时乘 0.4；与 scripts/train_char_lm.py 的计数口径一致。
    float score(std::string_view utf8) const;

    // 重排权重：候选最终分 = libime 原分 + alpha * score()。
    // 0.25 来自 clean_corpus（exact + 5-gram 近似去重）上的 dev 调参；
    // 文档级 split 尚未完成，不能把该值当成最终最优。
    static constexpr float kAlpha = 0.25f;

    bool empty() const { return vocab_.empty(); }
    size_t vocabSize() const { return vocab_.size(); }
    const std::string &path() const { return path_; }

private:
    float logProb(uint16_t c2, uint16_t c1, uint16_t ch) const;
    float logQuadProb(uint16_t p3, uint16_t p2, uint16_t p1,
                      uint16_t ch) const;
    uint16_t idOf(std::string_view ch) const;

    std::string path_;
    std::vector<std::string> vocab_;
    std::unordered_map<std::string, uint16_t> ids_;
    std::vector<uint16_t> unigram_;
    uint32_t unigramTotal_ = 0;
    std::vector<BiCtx> biCtx_;
    std::vector<BiEntry> biEntry_;
    std::vector<TriCtx> triCtx_;
    std::vector<TriEntry> triEntry_;
    uint16_t version_ = 1;
    std::vector<QuadCtx> quadCtx_;
    std::vector<QuadEntry> quadEntry_;
};

}  // namespace pinyin_ime

#endif  // PINYIN_IME_CHAR_LM_H
