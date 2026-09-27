// pinyin_corrector.cpp — CHIME 信道 + 音节先验 + 二元转移 的 C++ 移植。
//
// 与 correction/corrector.py 的 correct() 保持一致：整串连续全拼 → 最优
// 「切分 + 纠错」音节路径（二元 Viterbi，状态按 (位置, 最后一个音节) 展开）。
// 数据全部来自 correction_data.h（ASCII 拼音，无敏感文本），无运行时文件依赖。
#include "pinyin_corrector.h"

#include "correction_data.h"

#include <algorithm>
#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <string>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace pinyin_corrector {

namespace data = pinyin_correction_data;

namespace {

// ---------- 1. 音节 Trie（单元编辑距离 ≤ maxDist 的候选生成） ----------

struct TrieNode {
    std::array<int, 26> next{}; // 索引到节点池，-1 表示无
    bool end = false;
    TrieNode() { next.fill(-1); }
};

class SyllableTrie {
public:
    explicit SyllableTrie() {
        pool_.emplace_back(); // root
        for (int i = 0; i < data::kNumSyllables; ++i) {
            insert(data::kSyllables[i]);
        }
    }

    void insert(const char *word) {
        int node = 0;
        for (const char *p = word; *p; ++p) {
            int c = static_cast<int>(*p - 'a');
            if (pool_[node].next[c] < 0) {
                pool_[node].next[c] = static_cast<int>(pool_.size());
                pool_.emplace_back();
            }
            node = pool_[node].next[c];
        }
        pool_[node].end = true;
    }

    // 返回 (音节字符串, 单元编辑距离) 列表，编辑距离 ≤ maxDist。
    std::vector<std::pair<std::string, int>>
    search_within_edit_distance(const std::string &query, int maxDist) const {
        const int m = static_cast<int>(query.size());
        std::vector<std::pair<std::string, int>> results;
        std::string prefix;
        std::vector<int> rootRow(m + 1);
        for (int i = 0; i <= m; ++i) {
            rootRow[i] = i;
        }
        dfs(0, query, m, maxDist, rootRow, prefix, results);
        return results;
    }

private:
    void dfs(int node, const std::string &q, int m, int maxDist,
             const std::vector<int> &prevRow, std::string &prefix,
             std::vector<std::pair<std::string, int>> &results) const {
        if (pool_[node].end && prevRow[m] <= maxDist) {
            results.emplace_back(prefix, prevRow[m]);
        }
        // 剪枝：当前行最小值都超预算，则任何更长后缀只会更远，整棵子树砍掉。
        int rowMin = *std::min_element(prevRow.begin(), prevRow.end());
        if (rowMin > maxDist) {
            return;
        }
        for (int c = 0; c < 26; ++c) {
            int child = pool_[node].next[c];
            if (child < 0) {
                continue;
            }
            char ch = static_cast<char>('a' + c);
            std::vector<int> row(m + 1);
            row[0] = prevRow[0] + 1; // 延长前缀：需删除 query 前 j 个字符
            for (int i = 1; i <= m; ++i) {
                int d = std::min({row[i - 1] + 1, prevRow[i] + 1,
                                  prevRow[i - 1] + (q[static_cast<size_t>(i - 1)] == ch ? 0 : 1)});
                row[i] = d;
            }
            prefix.push_back(ch);
            dfs(child, q, m, maxDist, row, prefix, results);
            prefix.pop_back();
        }
    }

    std::vector<TrieNode> pool_;
};

// ---------- 2. 信道（noisy-channel）代价 ----------

constexpr double COST_INS_DEL = 1.0;
constexpr double COST_SUB_PHONETIC = 0.4;
constexpr double COST_SUB_ADJACENT = 0.6;
constexpr double COST_SUB_DEFAULT = 1.2;
constexpr double CONFUSION_COST = 0.35;

// 键盘物理相邻（QWERTY）。
const std::unordered_map<char, std::string> KEYBOARD_NEIGHBORS = {
    {'q', "was"}, {'w', "qeasd"}, {'e', "wrsdf"}, {'r', "etdfg"}, {'t', "ryfgh"},
    {'y', "tughj"}, {'u', "yihjk"}, {'i', "uojkl"}, {'o', "ipkl"}, {'p', "ol"},
    {'a', "qwsxz"}, {'s', "qweadzxc"}, {'d', "wersfcxv"}, {'f', "ertdgcvb"},
    {'g', "rtyfhvbn"}, {'h', "tyugjbnm"}, {'j', "yuihknm"}, {'k', "uiojlm"},
    {'l', "iopk"}, {'z', "asx"}, {'x', "asdzc"}, {'c', "sdfxv"}, {'v', "dfgcb"},
    {'b', "fghvn"}, {'n', "ghjbm"}, {'m', "hjkn"},
};

bool is_phonetic_swap(char a, char b) {
    if (a > b) std::swap(a, b);
    return (a == 'l' && b == 'n') || (a == 'l' && b == 'r') || (a == 'f' && b == 'h');
}

double sub_cost(char a, char b) {
    if (a == b) return 0.0;
    if (is_phonetic_swap(a, b)) return COST_SUB_PHONETIC;
    auto it = KEYBOARD_NEIGHBORS.find(a);
    if (it != KEYBOARD_NEIGHBORS.end() && it->second.find(b) != std::string::npos) {
        return COST_SUB_ADJACENT;
    }
    return COST_SUB_DEFAULT;
}

double generic_distance(const std::string &query, const std::string &target) {
    size_t m = query.size(), n = target.size();
    std::vector<double> prev(n + 1), cur(n + 1);
    for (size_t j = 0; j <= n; ++j) prev[j] = static_cast<double>(j) * COST_INS_DEL;
    for (size_t i = 1; i <= m; ++i) {
        cur[0] = static_cast<double>(i) * COST_INS_DEL;
        for (size_t j = 1; j <= n; ++j) {
            double d = std::min({cur[j - 1] + COST_INS_DEL, prev[j] + COST_INS_DEL,
                                 prev[j - 1] + sub_cost(query[i - 1], target[j - 1])});
            cur[j] = d;
        }
        std::swap(prev, cur);
    }
    return prev[n];
}

// 平翘舌 / 前后鼻音整音节混淆。
std::vector<std::string> confusion_siblings(const std::string &syl) {
    std::vector<std::string> out;
    const std::pair<const char *, const char *> retro[] = {
        {"zh", "z"}, {"ch", "c"}, {"sh", "s"}};
    for (const auto &[rt, plain] : retro) {
        if (syl.rfind(rt, 0) == 0 && syl.size() >= 2) {
            out.push_back(std::string(plain) + syl.substr(2));
        } else if (syl.rfind(plain, 0) == 0 && syl.size() >= 1) {
            out.push_back(std::string(rt) + syl.substr(1));
        }
    }
    const std::pair<const char *, const char *> nasal[] = {
        {"ing", "in"}, {"eng", "en"}, {"ang", "an"}, {"iang", "ian"}};
    for (const auto &[longf, shortf] : nasal) {
        auto ends = [&](const char *sf) {
            size_t l = std::char_traits<char>::length(sf);
            return syl.size() >= l && syl.compare(syl.size() - l, l, sf) == 0;
        };
        if (ends(longf)) {
            out.push_back(syl.substr(0, syl.size() - std::char_traits<char>::length(longf)) + shortf);
        } else if (ends(shortf)) {
            out.push_back(syl.substr(0, syl.size() - std::char_traits<char>::length(shortf)) + longf);
        }
    }
    return out;
}

double channel_score(const std::string &query, const std::string &target) {
    if (query == target) return 0.0;
    for (const auto &sib : confusion_siblings(query)) {
        if (sib == target) return CONFUSION_COST;
    }
    for (const auto &sib : confusion_siblings(target)) {
        if (sib == query) return CONFUSION_COST;
    }
    return generic_distance(query, target);
}

// ---------- 3. 音节先验 / 二元转移（运行时从固化数据建索引） ----------

struct Model {
    SyllableTrie trie;
    std::unordered_map<std::string, int> sylIndex;   // 音节 → 索引
    std::unordered_map<uint32_t, float> trans;       // key = prev<<16 | cur
    double denomFallback = 0.0;

    Model() : denomFallback(data::kSmooth * data::kDenomFallbackCount) {
        for (int i = 0; i < data::kNumSyllables; ++i) {
            sylIndex.emplace(data::kSyllables[i], i);
        }
        trans.reserve(static_cast<size_t>(data::kNumTrans) * 2);
        for (int i = 0; i < data::kNumTrans; ++i) {
            uint32_t key = (static_cast<uint32_t>(data::kTransPrev[i]) << 16) |
                           static_cast<uint32_t>(data::kTransCur[i]);
            trans.emplace(key, data::kTransCost[i]);
        }
    }

    double priorCost(const std::string &syl) const {
        auto it = sylIndex.find(syl);
        if (it == sylIndex.end()) return 0.42; // prior 兜底
        return data::kSyllablePriors[it->second];
    }

    double transitionCost(const std::string &prev, const std::string &cur) const {
        if (prev.empty()) return 0.0; // 句首
        auto pit = sylIndex.find(prev);
        auto cit = sylIndex.find(cur);
        if (pit == sylIndex.end() || cit == sylIndex.end()) return 0.0;
        uint32_t key = (static_cast<uint32_t>(pit->second) << 16) |
                       static_cast<uint32_t>(cit->second);
        auto tit = trans.find(key);
        double raw;
        if (tit != trans.end()) {
            raw = tit->second;
        } else {
            double d = data::kDenom[pit->second];
            raw = std::log(d / data::kSmooth);
        }
        return data::kLambda * raw;
    }
};

const Model &model() {
    static Model m;
    return m;
}

struct Candidate {
    std::string syl;
    int dist;
    double base; // channel + prior
};

constexpr int CAND_CAP = 16;
constexpr int BEAM_WIDTH = 64;
constexpr int MAX_SYLLABLE_LEN = 6;

// 子串 query 的 base 候选（信道 + 先验，不含转移），按 base 升序、截断 CAND_CAP。
std::vector<Candidate> baseCandidates(const std::string &query, int maxDist) {
    auto raw = model().trie.search_within_edit_distance(query, maxDist);
    std::vector<Candidate> out;
    out.reserve(raw.size());
    for (const auto &[syl, dist] : raw) {
        double base = channel_score(query, syl) + model().priorCost(syl);
        out.push_back({syl, dist, base});
    }
    std::sort(out.begin(), out.end(), [](const Candidate &a, const Candidate &b) {
        return a.base < b.base;
    });
    if (out.size() > CAND_CAP) out.resize(CAND_CAP);
    return out;
}

struct State {
    double cost;
    std::vector<std::string> path;
};

// 状态比较：代价 → 音节数 → 音节字典序（与 Python 的 tie-break 一致）。
bool stateBetter(const State &a, const State &b) {
    if (a.cost != b.cost) return a.cost < b.cost;
    if (a.path.size() != b.path.size()) return a.path.size() < b.path.size();
    return std::lexicographical_compare(a.path.begin(), a.path.end(), b.path.begin(), b.path.end());
}

} // namespace

std::optional<std::vector<std::string>> correct(const std::string &pinyin,
                                                int maxDist) {
    const size_t n = pinyin.size();
    if (n == 0) return std::nullopt;

    // cands[i][k]：子串 [i, i+k) 的 base 候选（k=1..MAX_SYLLABLE_LEN，索引与 k 对齐）。
    std::vector<std::vector<std::vector<Candidate>>> cands(
        n, std::vector<std::vector<Candidate>>(MAX_SYLLABLE_LEN + 1));
    for (size_t i = 0; i < n; ++i) {
        size_t remaining = n - i;
        size_t kmax = std::min<size_t>(remaining, MAX_SYLLABLE_LEN);
        for (size_t k = 1; k <= kmax; ++k) {
            cands[i][k] = baseCandidates(pinyin.substr(i, k), maxDist);
        }
    }

    // best[i][lastSyllable]（lastSyllable 空串表示句首）。
    std::vector<std::unordered_map<std::string, State>> best(n + 1);
    best[0].emplace("", State{0.0, {}});

    for (size_t i = 0; i < n; ++i) {
        auto &states = best[i];
        if (states.empty()) continue;
        // 前沿 beam 剪枝：只留前 BEAM_WIDTH。
        if (states.size() > BEAM_WIDTH) {
            std::vector<std::pair<std::string, State>> ranked(states.begin(), states.end());
            std::sort(ranked.begin(), ranked.end(),
                      [](const auto &a, const auto &b) { return stateBetter(a.second, b.second); });
            std::unordered_map<std::string, State> kept;
            kept.reserve(BEAM_WIDTH);
            for (size_t t = 0; t < BEAM_WIDTH && t < ranked.size(); ++t) {
                kept.emplace(std::move(ranked[t].first), std::move(ranked[t].second));
            }
            best[i] = std::move(kept);
        }
        for (const auto &[prevSyl, state] : best[i]) {
            size_t remaining = n - i;
            size_t kmax = std::min<size_t>(remaining, MAX_SYLLABLE_LEN);
            for (size_t k = 1; k <= kmax; ++k) {
                if (cands[i][k].empty()) continue;
                size_t j = i + k;
                for (const auto &cand : cands[i][k]) {
                    double total = state.cost + cand.base +
                                   model().transitionCost(prevSyl, cand.syl);
                    State next{total, state.path};
                    next.path.push_back(cand.syl);
                    auto it = best[j].find(cand.syl);
                    if (it == best[j].end() || stateBetter(next, it->second)) {
                        best[j][cand.syl] = std::move(next);
                    }
                }
            }
        }
    }

    if (best[n].empty()) return std::nullopt;
    const State *top = nullptr;
    for (const auto &[syl, st] : best[n]) {
        if (top == nullptr || stateBetter(st, *top)) top = &st;
    }
    return top->path;
}

std::string correct_to_string(const std::string &pinyin, int maxDist) {
    auto r = correct(pinyin, maxDist);
    if (!r) return "";
    std::string out;
    for (size_t i = 0; i < r->size(); ++i) {
        if (i) out += '\'';
        out += (*r)[i];
    }
    return out;
}

} // namespace pinyin_corrector