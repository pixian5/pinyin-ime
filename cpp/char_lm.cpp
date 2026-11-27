// char_lm.cpp — 字符 trigram 模型加载与整句打分（v0.9.197）
#include "char_lm.h"

#include <algorithm>
#include <cmath>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <limits>

namespace pinyin_ime {
namespace {

constexpr uint32_t kMagic = 0x4C434950;  // "PICL" 按小端读出的值
constexpr uint16_t kVersion = 2;  // v1=trigram，v2=trigram+4-gram

template <typename T>
bool readValue(std::ifstream &in, T &out) {
    in.read(reinterpret_cast<char *>(&out), sizeof(T));
    return static_cast<bool>(in);
}

uint32_t packBi(uint16_t prev, uint16_t ch) {
    return (static_cast<uint32_t>(prev) << 16) | ch;
}

uint64_t packTri(uint16_t p2, uint16_t p1, uint16_t ch) {
    return (static_cast<uint64_t>(p2) << 32) |
           (static_cast<uint64_t>(p1) << 16) | ch;
}

uint64_t packTriCtx(uint16_t p2, uint16_t p1) {
    return (static_cast<uint64_t>(p2) << 32) |
           (static_cast<uint64_t>(p1) << 16);
}

uint64_t packQuadCtx(uint16_t p3, uint16_t p2, uint16_t p1) {
    return (static_cast<uint64_t>(p3) << 32) |
           (static_cast<uint64_t>(p2) << 16) | p1;
}

uint64_t packQuad(uint16_t p3, uint16_t p2, uint16_t p1, uint16_t ch) {
    return (static_cast<uint64_t>(p3) << 48) |
           (static_cast<uint64_t>(p2) << 32) |
           (static_cast<uint64_t>(p1) << 16) | ch;
}

bool exists(const std::string &p) {
    if (p.empty()) return false;
    std::error_code ec;
    return std::filesystem::is_regular_file(p, ec);
}

}  // namespace

std::unique_ptr<CharLM> CharLM::load(const std::string &path, std::string *err) {
    auto fail = [&](const std::string &msg) -> std::unique_ptr<CharLM> {
        if (err) *err = msg;
        return nullptr;
    };
    std::ifstream in(path, std::ios::binary);
    if (!in) return fail("打不开模型文件：" + path);

    uint32_t magic = 0;
    uint16_t version = 0, vocabN = 0;
    if (!readValue(in, magic) || magic != kMagic) return fail("模型 magic 不对");
    if (!readValue(in, version) || version < 1 || version > kVersion) {
        return fail("模型版本不支持");
    }
    if (!readValue(in, vocabN) || vocabN < 3) return fail("模型词表异常");

    auto lm = std::unique_ptr<CharLM>(new CharLM());
    lm->version_ = version;
    lm->path_ = path;
    lm->vocab_.reserve(vocabN);
    for (uint16_t i = 0; i < vocabN; ++i) {
        uint8_t len = 0;
        if (!readValue(in, len)) return fail("词表读取中断");
        std::string ch(len, '\0');
        if (len && !in.read(ch.data(), len)) return fail("词表字符串读取中断");
        lm->vocab_.push_back(std::move(ch));
    }
    for (uint16_t i = 0; i < vocabN; ++i) {
        lm->ids_.emplace(lm->vocab_[i], i);
    }
    if (!readValue(in, lm->unigramTotal_)) return fail("unigram 总数读取失败");
    lm->unigram_.resize(vocabN);
    for (uint16_t i = 0; i < vocabN; ++i) {
        if (!readValue(in, lm->unigram_[i])) return fail("unigram 读取中断");
    }

    uint32_t n = 0;
    if (!readValue(in, n)) return fail("bigram 上下文数量读取失败");
    lm->biCtx_.resize(n);
    for (auto &e : lm->biCtx_) {
        if (!readValue(in, e.prev) || !readValue(in, e.total)) {
            return fail("bigram 上下文读取中断");
        }
    }
    if (!readValue(in, n)) return fail("bigram 条目数量读取失败");
    lm->biEntry_.resize(n);
    for (auto &e : lm->biEntry_) {
        if (!readValue(in, e.prev) || !readValue(in, e.ch) ||
            !readValue(in, e.count)) {
            return fail("bigram 条目读取中断");
        }
    }

    if (!readValue(in, n)) return fail("trigram 上下文数量读取失败");
    lm->triCtx_.resize(n);
    for (auto &e : lm->triCtx_) {
        if (!readValue(in, e.p2) || !readValue(in, e.p1) ||
            !readValue(in, e.total)) {
            return fail("trigram 上下文读取中断");
        }
    }
    if (!readValue(in, n)) return fail("trigram 条目数量读取失败");
    lm->triEntry_.resize(n);
    for (auto &e : lm->triEntry_) {
        if (!readValue(in, e.p2) || !readValue(in, e.p1) ||
            !readValue(in, e.ch) || !readValue(in, e.count)) {
            return fail("trigram 条目读取中断");
        }
    }

    if (version >= 2) {
        if (!readValue(in, n)) return fail("4-gram 上下文数量读取失败");
        lm->quadCtx_.resize(n);
        for (auto &e : lm->quadCtx_) {
            if (!readValue(in, e.p3) || !readValue(in, e.p2) ||
                !readValue(in, e.p1) || !readValue(in, e.total)) {
                return fail("4-gram 上下文读取中断");
            }
        }
        if (!readValue(in, n)) return fail("4-gram 条目数量读取失败");
        lm->quadEntry_.resize(n);
        for (auto &e : lm->quadEntry_) {
            if (!readValue(in, e.p3) || !readValue(in, e.p2) ||
                !readValue(in, e.p1) || !readValue(in, e.ch) ||
                !readValue(in, e.count)) {
                return fail("4-gram 条目读取中断");
            }
        }
    }

    std::sort(lm->biCtx_.begin(), lm->biCtx_.end(),
              [](const BiCtx &a, const BiCtx &b) { return a.prev < b.prev; });
    std::sort(lm->biEntry_.begin(), lm->biEntry_.end(),
              [](const BiEntry &a, const BiEntry &b) {
                  return packBi(a.prev, a.ch) < packBi(b.prev, b.ch);
              });
    std::sort(lm->triCtx_.begin(), lm->triCtx_.end(),
              [](const TriCtx &a, const TriCtx &b) {
                  return packTriCtx(a.p2, a.p1) < packTriCtx(b.p2, b.p1);
              });
    std::sort(lm->triEntry_.begin(), lm->triEntry_.end(),
              [](const TriEntry &a, const TriEntry &b) {
                  return packTri(a.p2, a.p1, a.ch) < packTri(b.p2, b.p1, b.ch);
              });
    std::sort(lm->quadCtx_.begin(), lm->quadCtx_.end(),
              [](const QuadCtx &a, const QuadCtx &b) {
                  return packQuadCtx(a.p3, a.p2, a.p1) <
                         packQuadCtx(b.p3, b.p2, b.p1);
              });
    std::sort(lm->quadEntry_.begin(), lm->quadEntry_.end(),
              [](const QuadEntry &a, const QuadEntry &b) {
                  return packQuad(a.p3, a.p2, a.p1, a.ch) <
                         packQuad(b.p3, b.p2, b.p1, b.ch);
              });

    if (err) err->clear();
    return lm;
}

std::unique_ptr<CharLM> CharLM::findDefault(const std::string &argv0) {
    std::vector<std::string> candidates;
    if (const char *env = std::getenv("PINYINIME_CHAR_LM")) {
        candidates.emplace_back(env);
    }
    candidates.emplace_back("data/char_trigram.bin");
    candidates.emplace_back("/usr/share/pinyin-ime/char_trigram.bin");
    candidates.emplace_back("/usr/local/share/pinyin-ime/char_trigram.bin");
    if (!argv0.empty()) {
        std::error_code ec;
        auto exe = std::filesystem::absolute(argv0, ec);
        if (!ec) {
            auto dir = exe.parent_path();
            candidates.push_back((dir / "../share/pinyin-ime/char_trigram.bin").string());
            candidates.push_back((dir / "../data/char_trigram.bin").string());
            candidates.push_back((dir / "data/char_trigram.bin").string());
        }
    }
    for (const auto &p : candidates) {
        if (exists(p)) {
            if (auto lm = load(p)) return lm;
        }
    }
    return nullptr;
}

uint16_t CharLM::idOf(std::string_view ch) const {
    auto it = ids_.find(std::string(ch));
    return it == ids_.end() ? 2 : it->second;  // 2 = <unk>
}

float CharLM::logProb(uint16_t c2, uint16_t c1, uint16_t ch) const {
    const double V = static_cast<double>(vocab_.size());
    const double p1 = (static_cast<double>(unigram_[ch]) + 0.5) /
                      (static_cast<double>(unigramTotal_) + 0.5 * V);

    double p2 = p1;
    auto biCtxIt = std::lower_bound(
        biCtx_.begin(), biCtx_.end(), c1,
        [](const BiCtx &e, uint16_t v) { return e.prev < v; });
    if (biCtxIt != biCtx_.end() && biCtxIt->prev == c1) {
        auto biIt = std::lower_bound(
            biEntry_.begin(), biEntry_.end(), packBi(c1, ch),
            [](const BiEntry &e, uint32_t key) {
                return packBi(e.prev, e.ch) < key;
            });
        const uint16_t count =
            (biIt != biEntry_.end() && biIt->prev == c1 && biIt->ch == ch)
                ? biIt->count : 0;
        // 上下文存在但该字没出现过：与训练脚本一致，用 add-0.1 平滑，
        // 不能直接退回 unigram（那样会改变整句分数的刻度）。
        p2 = (static_cast<double>(count) + 0.1) /
             (static_cast<double>(biCtxIt->total) + 0.1 * V);
    }

    double p3 = p2;
    auto triCtxIt = std::lower_bound(
        triCtx_.begin(), triCtx_.end(), packTriCtx(c2, c1),
        [](const TriCtx &e, uint64_t key) {
            return packTriCtx(e.p2, e.p1) < key;
        });
    if (triCtxIt != triCtx_.end() && triCtxIt->p2 == c2 &&
        triCtxIt->p1 == c1) {
        auto triIt = std::lower_bound(
            triEntry_.begin(), triEntry_.end(), packTri(c2, c1, ch),
            [](const TriEntry &e, uint64_t key) {
                return packTri(e.p2, e.p1, e.ch) < key;
            });
        const uint16_t count =
            (triIt != triEntry_.end() && triIt->p2 == c2 &&
             triIt->p1 == c1 && triIt->ch == ch) ? triIt->count : 0;
        p3 = (static_cast<double>(count) + 0.1) /
             (static_cast<double>(triCtxIt->total) + 0.1 * V);
    }

    return static_cast<float>(
        std::log(0.55 * p3 + 0.30 * p2 + 0.15 * p1));
}

float CharLM::logQuadProb(uint16_t p3, uint16_t p2, uint16_t p1,
                           uint16_t ch) const {
    // 4-gram 上下文
    auto qctxIt = std::lower_bound(
        quadCtx_.begin(), quadCtx_.end(), packQuadCtx(p3, p2, p1),
        [](const QuadCtx &e, uint64_t key) {
            return packQuadCtx(e.p3, e.p2, e.p1) < key;
        });
    if (qctxIt != quadCtx_.end() && qctxIt->p3 == p3 &&
        qctxIt->p2 == p2 && qctxIt->p1 == p1) {
        auto qIt = std::lower_bound(
            quadEntry_.begin(), quadEntry_.end(), packQuad(p3, p2, p1, ch),
            [](const QuadEntry &e, uint64_t key) {
                return packQuad(e.p3, e.p2, e.p1, e.ch) < key;
            });
        if (qIt != quadEntry_.end() && qIt->p3 == p3 &&
            qIt->p2 == p2 && qIt->p1 == p1 && qIt->ch == ch &&
            qIt->count > 0) {
            return static_cast<float>(
                std::log(static_cast<double>(qIt->count) /
                         static_cast<double>(qctxIt->total)));
        }
    }

    // 3-gram 回退
    auto tctxIt = std::lower_bound(
        triCtx_.begin(), triCtx_.end(), packTriCtx(p2, p1),
        [](const TriCtx &e, uint64_t key) {
            return packTriCtx(e.p2, e.p1) < key;
        });
    if (tctxIt != triCtx_.end() && tctxIt->p2 == p2 && tctxIt->p1 == p1) {
        auto tIt = std::lower_bound(
            triEntry_.begin(), triEntry_.end(), packTri(p2, p1, ch),
            [](const TriEntry &e, uint64_t key) {
                return packTri(e.p2, e.p1, e.ch) < key;
            });
        if (tIt != triEntry_.end() && tIt->p2 == p2 && tIt->p1 == p1 &&
            tIt->ch == ch && tIt->count > 0) {
            return static_cast<float>(
                std::log(0.4 * static_cast<double>(tIt->count) /
                         static_cast<double>(tctxIt->total)));
        }
    }

    // 2-gram 回退
    auto bctxIt = std::lower_bound(
        biCtx_.begin(), biCtx_.end(), p1,
        [](const BiCtx &e, uint16_t key) { return e.prev < key; });
    if (bctxIt != biCtx_.end() && bctxIt->prev == p1) {
        auto bIt = std::lower_bound(
            biEntry_.begin(), biEntry_.end(), packBi(p1, ch),
            [](const BiEntry &e, uint32_t key) {
                return packBi(e.prev, e.ch) < key;
            });
        if (bIt != biEntry_.end() && bIt->prev == p1 && bIt->ch == ch &&
            bIt->count > 0) {
            return static_cast<float>(
                std::log(0.4 * static_cast<double>(bIt->count) /
                         static_cast<double>(bctxIt->total)));
        }
    }

    // unigram 回退
    const double uni = (static_cast<double>(unigram_[ch]) > 0.1)
                           ? static_cast<double>(unigram_[ch]) : 0.1;
    return static_cast<float>(
        std::log(0.4 * uni / static_cast<double>(unigramTotal_)));
}

float CharLM::score(std::string_view utf8) const {
    float total = 0.0f;
    uint16_t p3 = 0;  // <s>
    uint16_t p2 = 0;
    uint16_t p1 = 0;
    size_t i = 0;
    // 18 字节（约 6 个汉字）以下的短候选仍用 trigram：
    // 4-gram 的跨词上下文在极短文本上容易过拟合（实测“年轻”会被推成“年青”）。
    const bool useQuad = version_ >= 2 && !quadCtx_.empty() &&
                         utf8.size() >= 18;
    while (i < utf8.size()) {
        unsigned char lead = static_cast<unsigned char>(utf8[i]);
        size_t n = 1;
        if ((lead & 0xE0) == 0xC0) n = 2;
        else if ((lead & 0xF0) == 0xE0) n = 3;
        else if ((lead & 0xF8) == 0xF0) n = 4;
        if (i + n > utf8.size()) n = 1;
        uint16_t id = idOf(utf8.substr(i, n));
        total += useQuad ? logQuadProb(p3, p2, p1, id)
                         : logProb(p2, p1, id);
        p3 = p2;
        p2 = p1;
        p1 = id;
        i += n;
    }
    total += useQuad ? logQuadProb(p3, p2, p1, 1)
                     : logProb(p2, p1, 1);  // </s>
    return total;
}
}  // namespace pinyin_ime
