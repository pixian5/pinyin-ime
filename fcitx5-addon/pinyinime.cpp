/*
 * pinyin-ime — Fcitx5 addon 实现。
 * 说明见 pinyinime.h。
 */
#include "pinyinime.h"

#include <fcitx-utils/key.h>
#include <fcitx-utils/keysym.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/macros.h>

#include <fcitx/addonfactory.h>
#include <fcitx/addonmanager.h>
#include <fcitx/candidatelist.h>
#include <fcitx/inputpanel.h>
#include <fcitx/text.h>
#include <fcitx/userinterface.h>

#include <algorithm>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

#include <libime/pinyin/pinyinencoder.h>

#include "emoji_list.h"
#include "pinyin_corrector.h"
#include "abbr_presets.h"
#include "quick_input.h"

namespace fcitx {

namespace {

constexpr char kConfFile[] = "conf/pinyinime.conf";

// 把路径里开头的 ~ 展开成 $HOME。
std::string expandHome(const std::string &path) {
    if (!path.empty() && path[0] == '~' &&
        (path.size() == 1 || path[1] == '/')) {
        if (const char *home = std::getenv("HOME"); home && *home) {
            return std::string(home) + path.substr(1);
        }
    }
    return path;
}

bool fileExists(const std::string &path) {
    if (path.empty()) {
        return false;
    }
    std::error_code ec;
    return std::filesystem::is_regular_file(path, ec);
}

// 从候选里找第一个存在的文件；找不到返回空串并告警（列出试过的路径）。
std::string findData(const std::vector<std::string> &candidates,
                     const char *what) {
    for (const auto &p : candidates) {
        if (fileExists(p)) {
            return p;
        }
    }
    FCITX_WARN() << "pinyinime: cannot find " << what;
    for (const auto &p : candidates) {
        FCITX_WARN() << "pinyinime:   tried " << p;
    }
    return "";
}

// pinyin-ime addon disambiguation patch
// Insert after findData function (line 69), before commonFuzzyFlags


// 常见「好用度」模糊音集合（与 cpp/pinyin_ime_cli.cpp 保持一致）。
// Note: libime's fuzzyCost has been modified from log10(0.5) to log10(0.1)
// to ensure exact pinyin matches rank higher than fuzzy matches.
// Without this, common words like 限定 (via ian↔iang fuzzy of xiang) can
// outrank the exact match 想订, producing wrong candidates.
libime::PinyinFuzzyFlags buildFuzzyFlags(const PinyinIMEConfig &cfg) {
    using libime::PinyinFuzzyFlag;
    libime::PinyinFuzzyFlags flags;
    // 细粒度模糊音（每对独立开关，默认全关）
    if (cfg.fuzzyZ_ZH.value())      flags |= PinyinFuzzyFlag::Z_ZH;
    if (cfg.fuzzyC_CH.value())      flags |= PinyinFuzzyFlag::C_CH;
    if (cfg.fuzzyS_SH.value())      flags |= PinyinFuzzyFlag::S_SH;
    if (cfg.fuzzyL_N.value())       flags |= PinyinFuzzyFlag::L_N;
    // L_R：libime 1.1.5 无此枚举，跳过（新版 libime 支持 l/r 模糊）
    if (cfg.fuzzyF_H.value())       flags |= PinyinFuzzyFlag::F_H;
    if (cfg.fuzzyAN_ANG.value())    flags |= PinyinFuzzyFlag::AN_ANG;
    if (cfg.fuzzyEN_ENG.value())    flags |= PinyinFuzzyFlag::EN_ENG;
    if (cfg.fuzzyIN_ING.value())    flags |= PinyinFuzzyFlag::IN_ING;
    if (cfg.fuzzyIAN_IANG.value())  flags |= PinyinFuzzyFlag::IAN_IANG;
    if (cfg.fuzzyU_OU.value())      flags |= PinyinFuzzyFlag::U_OU;
    if (cfg.fuzzyUAN_UANG.value())  flags |= PinyinFuzzyFlag::UAN_UANG;
    if (cfg.fuzzyV_U.value())       flags |= PinyinFuzzyFlag::V_U;
    if (cfg.fuzzyVE_UE.value())     flags |= PinyinFuzzyFlag::VE_UE;
    // CommonTypo + AdvancedTypo 始终包含在 fuzzy 模式中
    if (cfg.enableFuzzy.value()) {
        flags |= PinyinFuzzyFlag::CommonTypo;
        flags |= PinyinFuzzyFlag::AdvancedTypo;
    }
    return flags;
}

// 双拼方案名 → libime 枚举（与 CLI sp_profile_from 保持一致）。

libime::ShuangpinBuiltinProfile shuangpinProfileFrom(std::string name) {

    for (auto &c : name) {

        c = static_cast<char>(::tolower(static_cast<unsigned char>(c)));

    }

    if (name == "ziranma") return libime::ShuangpinBuiltinProfile::Ziranma;

    if (name == "ms" || name == "微软") return libime::ShuangpinBuiltinProfile::MS;

    if (name == "ziguang" || name == "紫光") return libime::ShuangpinBuiltinProfile::Ziguang;

    if (name == "abc" || name == "智能abc") return libime::ShuangpinBuiltinProfile::ABC;

    if (name == "zhongwenzhixing" || name == "中文之星") return libime::ShuangpinBuiltinProfile::Zhongwenzhixing;

    if (name == "pinyinjiajia" || name == "拼音加加") return libime::ShuangpinBuiltinProfile::PinyinJiajia;

    if (name == "xiaohe" || name == "小鹤") return libime::ShuangpinBuiltinProfile::Xiaohe;

    // GB：libime 1.1.5 无此内置方案，回退小鹤
    if (name == "gb") return libime::ShuangpinBuiltinProfile::Xiaohe;

    FCITX_WARN() << "pinyinime: unknown shuangpin scheme \"" << name
                 << "\", falling back to Xiaohe";

    return libime::ShuangpinBuiltinProfile::Xiaohe;

}

// US 布局下把 (KeySym, Shift) 还原成屏幕上会出现的 ASCII 字符。
// 兼容「符号已折叠进 keysym」（如 Shift+/ → FcitxKey_question）与
// 「基键 + Shift 状态」（如 FcitxKey_1 + Shift → !）两套前端约定；非打印键返回空串。
std::string asciiFromKey(KeySym sym, bool shifted) {
    if (sym >= FcitxKey_a && sym <= FcitxKey_z) {
        return std::string(1, static_cast<char>(shifted ? sym - 32 : sym));
    }
    if (sym >= FcitxKey_A && sym <= FcitxKey_Z) {
        return std::string(1, static_cast<char>(sym));
    }
    switch (sym) {
    case FcitxKey_exclam:     return "!";
    case FcitxKey_quotedbl:   return "\"";
    case FcitxKey_colon:      return ":";
    case FcitxKey_question:   return "?";
    case FcitxKey_parenleft:  return "(";
    case FcitxKey_parenright: return ")";
    case FcitxKey_less:       return "<";
    case FcitxKey_greater:    return ">";
    default: break;
    }
    switch (sym) {
    case FcitxKey_0: return shifted ? ")" : "0";
    case FcitxKey_1: return shifted ? "!" : "1";
    case FcitxKey_2: return shifted ? "@" : "2";
    case FcitxKey_3: return shifted ? "#" : "3";
    case FcitxKey_4: return shifted ? "$" : "4";
    case FcitxKey_5: return shifted ? "%" : "5";
    case FcitxKey_6: return shifted ? "^" : "6";
    case FcitxKey_7: return shifted ? "&" : "7";
    case FcitxKey_8: return shifted ? "*" : "8";
    case FcitxKey_9: return shifted ? "(" : "9";
    case FcitxKey_space:      return " ";
    case FcitxKey_apostrophe: return shifted ? "\"" : "'";
    case FcitxKey_comma:      return shifted ? "<" : ",";
    case FcitxKey_minus:      return shifted ? "_" : "-";
    case FcitxKey_period:     return shifted ? ">" : ".";
    case FcitxKey_slash:      return shifted ? "?" : "/";
    case FcitxKey_semicolon:  return shifted ? ":" : ";";
    case FcitxKey_equal:      return shifted ? "+" : "=";
    case FcitxKey_bracketleft:  return shifted ? "{" : "[";
    case FcitxKey_backslash:    return shifted ? "|" : "\\";
    case FcitxKey_bracketright: return shifted ? "}" : "]";
    default: return "";
    }
}

// 半角 ASCII 标点 → 全角中文标点；非标点返回空串。
std::string fullwidthPunct(const std::string &ascii) {
    if (ascii.size() != 1) {
        return "";
    }
    switch (ascii[0]) {
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

// UTF-8 字符串的「字符数」（按首字节统计，非字节数）。
// 用于撤回时告诉应用要按多少个 BackSpace 删除刚上屏的文本（xterm/编辑器按字符删）。
size_t utf8Len(const std::string &s) {
    size_t n = 0;
    for (unsigned char c : s) {
        if ((c & 0xC0) != 0x80) {
            ++n;
        }
    }
    return n;
}

} // namespace

// v0.9.90: 检查输入是否为有效拼音（能否分解为合法音节）
// 用于中英自动混输：无效拼音直接上屏英文
bool is_valid_pinyin(const std::string &raw) {
    if (raw.empty()) return false;
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
    int n = raw.size();
    std::vector<int> dp(n + 1, -1);
    dp[n] = 0;
    for (int i = n - 1; i >= 0; --i) {
        for (int len = 6; len >= 1; --len) {
            if (i + len > n) continue;
            if (syllables.count(raw.substr(i, len)) && dp[i + len] >= 0) {
                dp[i] = len;
                break;
            }
        }
    }
    return dp[0] >= 0;
}

// v0.9.112: 全拼→简拼动态记忆。
// 用户输入完整全拼并选词后，把「全拼首字母缩写=词」记忆到 abbr 文件。
// 只记忆真正的全拼（is_valid_pinyin 为 true），简拼/混合输入不记忆。
// preedit 是 libime 的原始 preedit（音节间以 ' 分隔，如 zhong'guo），
// 用它切分音节，取每音节首字母组成简拼。
inline void try_remember_abbr_from_full(const std::string &preedit,
                                        const std::string &word) {
    if (preedit.empty() || word.empty()) return;
    std::string compact;
    for (char c : preedit) {
        if (c != '\'' && c != ' ') compact.push_back(c);
    }
    // 仅全拼才记忆（合法音节串且非单音节——单音节简拼就是它自己，无意义）
    if (!is_valid_pinyin(compact) || compact.size() < 2) return;
    // 用 ' 切分音节；若 preedit 无 ' 则退化为整体（不切分，直接不记忆）
    std::vector<std::string> syllables;
    std::string cur;
    for (char c : preedit) {
        if (c == '\'') {
            if (!cur.empty()) { syllables.push_back(cur); cur.clear(); }
        } else if (c != ' ') {
            cur.push_back(c);
        }
    }
    if (!cur.empty()) syllables.push_back(cur);
    if (syllables.size() < 2) return;
    std::string abbr;
    for (const auto &s : syllables) {
        if (!s.empty()) abbr.push_back(s[0]);
    }
    if (abbr.size() < 2) return;
    pinyin_ime::remember_abbr(abbr, word);
}

// ---------------- PinyinIMEState ----------------

PinyinIMEState::PinyinIMEState(PinyinIMEEngine *engine, InputContext *ic)
    : engine_(engine), inputContext_(ic), context_(engine->ime()) {
    for (int i = 0; i < 9; ++i) {
        selectionKeys_.emplace_back(static_cast<KeySym>(FcitxKey_1 + i));
    }
    // 双拼：从引擎配置初始化运行时开关
    shuangpinEnabled_ = engine_->config().enableShuangpin.value();
    if (shuangpinEnabled_) {
        context_.setUseShuangpin(true);
    }
}

size_t PinyinIMEState::pageSize() const {
    return std::max<size_t>(1, static_cast<size_t>(
                                   engine_->config().pageSize.value()));
}

void PinyinIMEState::commitText(const std::string &text) {
    inputContext_->inputPanel().reset();
    inputContext_->updatePreedit();
    inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
    inputContext_->commitString(text);
}

void PinyinIMEState::reset() {
    context_.clear();
    rawInput_.clear();
    page_ = 0;
    boostSwap_ = 0;
    boostInsertWord_.clear();
    maxPage_ = 0;
    candFocus_ = 0;
    englishMode_ = false;
    engBuf_.clear();
    vMode_ = false;
    vModeSub_.clear();
    vModePage_ = 0;
    inputContext_->inputPanel().reset();
    inputContext_->updatePreedit();
    inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
}

// v0.9.115: 可点击候选词 —— 鼠标点击时回调 selectCandidate(显示索引)，
// 使「点哪个词上屏哪个词」与数字键/空格行为一致。
// 官方 DisplayOnlyCandidateWord 的 select() 是空实现，点击无反应。
class PinyinCandidateWord : public CandidateWord {
public:
    PinyinCandidateWord(Text text, PinyinIMEState *state, size_t dispIdx)
        : CandidateWord(std::move(text)), state_(state), dispIdx_(dispIdx) {}

    void select(InputContext *) const override {
        state_->selectCandidate(dispIdx_);
    }

private:
    PinyinIMEState *state_;
    size_t dispIdx_;
};

void PinyinIMEState::updateUI() {

    // 表情选择态：预编辑显示页签，候选列出当前页表情（数字选字在 keyEvent 处理）。
    if (emojiMode_) {
        const auto &list = emoji_list();
        const size_t ps = pageSize();
        const size_t maxPage = list.empty() ? 0 : (list.size() - 1) / ps;
        if (emojiPage_ > maxPage) {
            emojiPage_ = maxPage;
        }
        Text preeditText("表情[" + std::to_string(emojiPage_ + 1) + "/" +
                         std::to_string(maxPage + 1) + "]");
        auto candidateList = std::make_unique<CommonCandidateList>();
        const size_t begin = emojiPage_ * ps;
        const size_t end = std::min<size_t>(list.size(), begin + ps);
        for (size_t i = begin; i < end; ++i) {
            candidateList->append<DisplayOnlyCandidateWord>(
                Text(list[i].glyph + " " + list[i].label));
        }
        candidateList->setPageSize(static_cast<int>(ps));
        candidateList->setSelectionKey(selectionKeys_);
        candidateList->setCursorIncludeUnselected(true);
        // setCursorIndex(0)：fcitx5 5.1.7 的 CommonCandidateList 无此 setter；新建列表光标默认在第 0 位

        inputContext_->inputPanel().reset();
        inputContext_->inputPanel().setPreedit(preeditText);
        inputContext_->inputPanel().setCandidateList(std::move(candidateList));
        inputContext_->updatePreedit();
        inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
        return;
    }

    // 英文态：预编辑显示英文缓冲（不出现候选），整行由回车一次性上屏。
    if (englishMode_) {
        Text preeditText("英文: " + engBuf_);
        inputContext_->inputPanel().reset();
        inputContext_->inputPanel().setPreedit(preeditText);
        inputContext_->updatePreedit();
        inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
        return;
    }

    // v 模式（搜狗式符号输入）：预编辑显示提示，候选列出符号组。
    if (vMode_) {
        std::string preeditStr = "v" + vModeSub_;
        if (vModeSub_.empty()) {
            preeditStr = "v[1圈码 2星号 3箭头 4数学 5希腊 6罗马 7单位 8括号 9点线 d日期 f分数]";
        }
        Text preeditText(preeditStr);
        auto candidateList = std::make_unique<CommonCandidateList>();
        const size_t ps = pageSize();
        if (!vModeSub_.empty()) {
            auto [symbols, label] = pinyin_ime::v_mode_lookup(vModeSub_);
            if (!symbols.empty()) {
                const size_t maxPage = (symbols.size() - 1) / ps;
                if (vModePage_ > maxPage) vModePage_ = maxPage;
                const size_t begin = vModePage_ * ps;
                const size_t end = std::min<size_t>(symbols.size(), begin + ps);
                for (size_t i = begin; i < end; ++i) {
                    candidateList->append<DisplayOnlyCandidateWord>(
                        Text(symbols[i]));
                }
                candidateList->setPageSize(static_cast<int>(ps));
                candidateList->setSelectionKey(selectionKeys_);
                candidateList->setCursorIncludeUnselected(true);
            }
        }
        inputContext_->inputPanel().reset();
        inputContext_->inputPanel().setPreedit(preeditText);
        if (!vModeSub_.empty()) {
            inputContext_->inputPanel().setCandidateList(std::move(candidateList));
        }
        inputContext_->updatePreedit();
        inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
        return;
    }

    // 预编辑（当前敲的拼音）+ 候选光标位置（逐词分段）
    auto [pre, cursorPos] = context_.preeditWithCursor();
    Text preeditText;
    if (!pre.empty()) {
        preeditText.append(pre);
        preeditText.setCursor(static_cast<int>(cursorPos));
    }

    // 候选（当前页）——仅在确实有候选时设置；空列表配 cursorIndex=0 会让
    // classicui 渲染时 candidateFromIndex(0) abort（实机崩溃，虚拟桌面实测发现）。
    const auto &cands = context_.candidatesToCursor();
    const size_t ps = pageSize();
    if (page_ > maxPage_) {
        page_ = maxPage_;
    }
    const size_t begin = page_ * ps;
    const size_t end = std::min<size_t>(cands.size(), begin + ps);

    // ── 简拼 boost（与 CLI 一致）──────────────────────────────
    // 若 preedit 匹配简拼预设（如 zg→中国），把预设词移到首位显示。
    // boostSwap_ 记录实际候选索引，selectCandidate 做索引映射。
    boostSwap_ = 0;
    boostInsertWord_.clear();
    if (page_ == 0 && end > begin + 1) {
        std::string preeditStr = context_.preedit();
        // 去掉隔音符和空格
        std::string compact;
        for (char c : preeditStr) {
            if (c != '\'' && c != ' ') compact.push_back(c);
        }
        std::string pword = pinyin_ime::lookup_pinyin_boost(compact);
        if (!pword.empty()) {
            // 在当前页候选中查找预设词
            for (size_t i = begin + 1; i < end; ++i) {
                if (cands[i].toString() == pword) {
                    boostSwap_ = i;
                    break;
                }
            }
            // 预设词不在当前页候选中：尝试在全局候选中查找
            if (boostSwap_ == 0) {
                for (size_t i = 1; i < cands.size(); ++i) {
                    if (cands[i].toString() == pword) {
                        boostSwap_ = i;
                        break;
                    }
                }
            }
        }
        // v0.9.51: 预设词不在候选列表中：插入首位（与 CLI boost_abbreviation 一致）
        if (boostSwap_ == 0 && !pword.empty()) {
            if (pword != compact) boostInsertWord_ = pword;
        }
        // v0.9.114: 完整匹配未命中时，尝试通用元音尾词组合（如 jrmsha→家人们好啊）
        if (boostSwap_ == 0 && boostInsertWord_.empty()) {
            std::string combined = pinyin_ime::vowel_tail_combined(compact);
            if (!combined.empty() && combined != compact) boostInsertWord_ = combined;
        }
        // v0.9.46: 完整匹配失败时，尝试后缀匹配
        if (boostSwap_ == 0) {
            std::vector<std::string> cand_strs;
            for (size_t i = 0; i < cands.size(); ++i)
                cand_strs.push_back(cands[i].toString());
            boostSwap_ = pinyin_ime::lookup_suffix_boost_index(compact, cand_strs);
        }
    }

    inputContext_->inputPanel().reset();
    inputContext_->inputPanel().setPreedit(preeditText);
    if (end > begin) {
        auto candidateList = std::make_unique<CommonCandidateList>();
        const size_t pageBase = page_ * ps;
        size_t dispK = 0;
        // v0.9.115: 鼠标点击候选上屏——用 PinyinCandidateWord（select() 回调
        // selectCandidate(显示索引)）替代 DisplayOnlyCandidateWord（select() 空实现，
        // 点击无效）。显示索引约定与数字键/空格一致：page*pageSize + 页内显示位。
        if (!boostInsertWord_.empty() && page_ == 0) {
            candidateList->append<PinyinCandidateWord>(Text(boostInsertWord_),
                                                       this, pageBase + dispK);
            ++dispK;
        }
        for (size_t i = begin; i < end; ++i) {
            size_t displayIdx = i;
            // 简拼 boost：交换候选 0 和 boostSwap_ 的显示位置
            if (boostSwap_ > 0 && page_ == 0) {
                if (i == begin) displayIdx = boostSwap_;
                else if (i == boostSwap_) displayIdx = begin;
            }
            // v0.9.111: 移除首选候选的拼音标注（用户反馈第一项后面跟着完整全拼）
            Text candText(cands[displayIdx].toString());
            candidateList->append<PinyinCandidateWord>(candText, this,
                                                       pageBase + dispK);
            ++dispK;
        }
        candidateList->setPageSize(static_cast<int>(ps));
        candidateList->setSelectionKey(selectionKeys_);
        candidateList->setCursorIncludeUnselected(true);
        // 候选焦点视觉高亮：把 classicui 的「高亮光标」对齐到 candFocus_（页内 0..8），
        // 让 Ctrl+←/→ 移动焦点后，候选窗里高亮的就是即将上屏的那个候选（第 15 轮只做了
        // 上屏语义、视觉仍固定在第 1 个）。candFocus_ 已在 keyEvent 钳制到 < end-begin，
        // 且 end>begin 时才走到这里，故索引必有效。翻页/新拼音/退格已把 candFocus_ 归零。
        // fcitx5 5.1.7 的 CommonCandidateList 无 setCursorIndex，用 nextCandidate() 逐步移动
        for (unsigned ci = 0; ci < candFocus_ && ci < 9; ++ci)
            candidateList->nextCandidate();
        inputContext_->inputPanel().setCandidateList(std::move(candidateList));
    }
    inputContext_->updatePreedit();
    inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void PinyinIMEState::refreshUI() {
    const auto &cands = context_.candidatesToCursor();
    const size_t ps = pageSize();
    maxPage_ = cands.empty() ? 0 : (cands.size() - 1) / ps;
    if (page_ > maxPage_) {
        page_ = maxPage_;
    }
    updateUI();
}

void PinyinIMEState::selectCandidate(size_t idx) {
    const auto &cands = context_.candidatesToCursor();
    if (context_.empty() || idx >= cands.size()) {
        return;
    }
    // v0.9.112: 提前保存原始 preedit（含 ' 分隔音节），用于选词后记忆全拼→简拼
    const std::string rawPreedit = context_.preedit();
    // v0.9.51: boost 插入词处理：用户选首位且 boostInsertWord_ 非空时，直接提交预设词
    if (!boostInsertWord_.empty() && page_ == 0 && idx == 0) {
        std::string pinyin;
        for (char c : context_.preedit()) {
            if (c != '\'' && c != ' ') pinyin.push_back(c);
        }
        undoStack_.emplace_back(boostInsertWord_, pinyin);
        if (undoStack_.size() > 32) undoStack_.erase(undoStack_.begin());
        context_.clear();
        rawInput_.clear();
        commitText(boostInsertWord_);
        boostInsertWord_.clear();
        refreshUI();
        return;
    }
    // 简拼 boost 索引映射：显示位置 0↔boostSwap_ 交换
    if (boostSwap_ > 0 && page_ == 0) {
        if (idx == page_ * pageSize()) idx = boostSwap_;
        else if (idx == boostSwap_) idx = page_ * pageSize();
    }
    // 误上屏撤回：先收集恢复拼音（必须在此刻——selectCandidatesToCursor 之后
    // preedit() 会变化/清空，撤回时才能把拼音放回候选态）。
    std::string pinyin;
    for (char c : context_.preedit()) {
        if (c != '\'' && c != ' ') {
            pinyin.push_back(c);
        }
    }
    context_.selectCandidatesToCursor(idx);
    if (context_.selected()) {
        const std::string text = context_.sentence();
        if (!text.empty()) {
            // v0.9.112: 用户输入全拼并选词成功 → 记忆「全拼首字母=词」
            try_remember_abbr_from_full(rawPreedit, text);
            // 与 CLI commit() 语义一致——仅「候选整句上屏」进栈
            // （英文/标点/表情不动），上限 32 条防膨胀。
            undoStack_.emplace_back(text, pinyin);
            if (undoStack_.size() > 32) {
                undoStack_.erase(undoStack_.begin());
            }
            context_.learn();
            commitText(text);
        }
        reset();
    } else {
        // 只选了一个分段、还有后续：继续让候选指向下一段。
        context_.learn();
        refreshUI();
    }
    engine_->saveUserData();
}

void PinyinIMEState::flipPage(int dir) {
    if (dir < 0) {
        if (page_ > 0) {
            --page_;
        }
    } else {
        if (page_ < maxPage_) {
            ++page_;
        }
    }
    candFocus_ = 0; // 翻页后焦点回到页首
    updateUI();
}

void PinyinIMEState::commitPunctuation(const std::string &fullwidth) {
    // 打拼音中：先上屏首选；空闲：直接上屏标点。与 CLI 的 select_and_commit 语义一致。
    if (!context_.empty()) {
        selectCandidate(0);
    }
    commitText(fullwidth);
    if (!context_.empty()) {
        refreshUI();
    }
}

void PinyinIMEState::undoLastCommit() {
    // 正在输入拼音时不撤回（避免误伤当前编辑）；无撤回历史直接忽略。
    if (!context_.empty() || undoStack_.empty()) {
        return;
    }
    const auto [text, pinyin] = undoStack_.back();
    undoStack_.pop_back();
    // 1) 向前台应用转发 BackSpace × text 字符数，删掉刚上屏的文本。
    //    （依赖应用支持退格删除；xterm/常见编辑器都支持。）
    for (size_t i = 0; i < utf8Len(text); ++i) {
        inputContext_->forwardKey(Key(FcitxKey_BackSpace, KeyState::NoState));
    }
    // 2) 把拼音恢复回候选态（与 CLI 撤回语义一致，用户可继续编辑/重上屏）。
    //    应用消歧，使撤回后预编辑与上屏前一致。
    context_.clear();
        rawInput_.clear();
    page_ = 0;
    maxPage_ = 0;
    {
        std::string disambig = disambiguate_pinyin(pinyin);
        for (char c : disambig) {
            if ((c >= 'a' && c <= 'z') || c == '\'') {
                context_.type(std::string(1, c));
            }
        }
    }
    refreshUI();
}

// ---------------- PinyinIMEEngine ----------------

PinyinIMEEngine::PinyinIMEEngine(Instance *instance)
    : instance_(instance) {
    // 关键顺序：必须先 reloadConfig()，再 registerProperty()。
    // registerProperty 会立即为「已存在」的 InputContext 调用 factory（见
    // pinyinime.h 中 factory_ 的 lambda），factory 内部调用 ensureDataLoaded()
    // 加载词典/LM。若此时配置尚未读取，DataDir/UserDataDir/EnableFuzzy 等
    // 会沿用编译默认值，用户在 conf/pinyinime.conf 里的设置不生效
    // （v0.9.101 及以前的实际缺陷：XIM server 先建 IC，addon 后加载）。
    //
    // fcitx5 的 addon 用户配置（conf/pinyinime.conf）默认由 DBus 模块 SetConfig
    // 载入；无 DBus 构建（如 headless Xvfb 实测）不会走该路径，故显式读取一次，
    // 使 EnableCorrect/DataDir/UserDataDir 等配置独立于 DBus 生效。
    reloadConfig();

    // FactoryFor 不会自动注册，必须显式注册到 InputContextManager，否则
    // slot_ = -1、propertyFor() 越界读垃圾指针。
    // （对照官方 keyboard addon 的 registerProperty("keyboardState", ...)）
    instance_->inputContextManager().registerProperty("pinyinimeState",
                                                      &factory_);
}

PinyinIMEEngine::~PinyinIMEEngine() { saveUserData(); }

bool PinyinIMEEngine::ensureDataLoaded() {
    if (ime_) {
        return true;
    }
    const std::string dataDir = config().dataDir.value();
    const std::string lmPath = findData(
        {dataDir + "/libime/zh_CN.lm", dataDir + "/zh_CN.lm",
         "/usr/lib/libime/zh_CN.lm", "/usr/lib64/libime/zh_CN.lm",
         "/usr/lib/x86_64-linux-gnu/libime/zh_CN.lm",
         "/usr/lib/aarch64-linux-gnu/libime/zh_CN.lm",
         "/usr/share/libime/zh_CN.lm", "/usr/share/libime/sc.lm"},
        "language model (zh_CN.lm)");
    const std::string dictPath = findData(
        {dataDir + "/sc.dict", dataDir + "/libime/sc.dict",
         "/usr/share/libime/sc.dict", "/usr/lib/libime/sc.dict",
         "/usr/lib64/libime/sc.dict",
         "/usr/lib/x86_64-linux-gnu/libime/sc.dict",
         "/usr/lib/aarch64-linux-gnu/libime/sc.dict"},
        "system dict (sc.dict)");
    if (lmPath.empty() || dictPath.empty()) {
        FCITX_WARN() << "pinyinime: data not found, pinyin engine disabled. "
                        "安装 libime 数据包，或在 conf 里设置 DataDir。";
        return false;
    }

    ime_ = std::make_unique<libime::PinyinIME>(
        std::make_unique<libime::PinyinDictionary>(),
        std::make_unique<libime::UserLanguageModel>(lmPath.c_str()));
    ime_->dict()->load(libime::PinyinDictionary::SystemDict, dictPath.c_str(),
                       libime::PinyinDictFormat::Binary);
    ime_->setNBest(
        static_cast<size_t>(std::max(1, config().nbest.value())));
    // v0.9.90: 音节分隔显示 — Pinyin 模式在音节间加空格，用户能看到引擎如何切分
    ime_->setPreeditMode(libime::PinyinPreeditMode::Pinyin);
    if (config().enableFuzzy.value()) {
        ime_->setFuzzyFlags(buildFuzzyFlags(config()));
    }

    // 双拼 profile（与 CLI setShuangpinProfile 一致）
    if (config().enableShuangpin.value()) {
        auto sp = shuangpinProfileFrom(config().shuangpinScheme.value());
        ime_->setShuangpinProfile(std::make_shared<libime::ShuangpinProfile>(sp));
    }

    // 用户词库 / 自学习历史持久化
    const std::string userDir = expandHome(config().userDataDir.value());
    std::error_code ec;
    std::filesystem::create_directories(userDir, ec);
    userHistPath_ = userDir + "/user.history";
    userDictPath_ = userDir + "/user.dict";
    if (fileExists(userHistPath_)) {
        std::ifstream fin(userHistPath_);
        if (fin) {
            ime_->model()->load(fin);
        }
    }
    if (fileExists(userDictPath_)) {
        ime_->dict()->load(libime::PinyinDictionary::UserDict,
                           userDictPath_.c_str(),
                           libime::PinyinDictFormat::Binary);
    }
    FCITX_INFO() << "pinyinime: loaded lm=" << lmPath << " dict=" << dictPath;
    return true;
}

void PinyinIMEEngine::saveUserData() {
    if (!ime_ || userHistPath_.empty()) {
        return;
    }
    {
        std::ofstream fout(userHistPath_, std::ios::trunc);
        if (fout) {
            ime_->model()->save(fout);
        }
    }
    ime_->dict()->save(libime::PinyinDictionary::UserDict,
                       userDictPath_.c_str(), libime::PinyinDictFormat::Binary);
}

void PinyinIMEEngine::reloadConfig() {
    readAsIni(config_, kConfFile);
    // 注意：不在此处调用 ensureDataLoaded()，让引擎延迟到首次按键时创建，
    // 确保 setConfig 设置的 EnableFuzzy/EnableCorrect 在引擎创建时已生效。
    // 构造函数中本方法先于 registerProperty() 执行，保证 factory 触发的
    // ensureDataLoaded() 读到的是上述已加载的配置。
    if (ime_) {
        ime_->setNBest(
            static_cast<size_t>(std::max(1, config().nbest.value())));
        // 模糊音热更新（配置变更后即时生效）
        if (config().enableFuzzy.value()) {
            ime_->setFuzzyFlags(buildFuzzyFlags(config()));
        } else {
            ime_->setFuzzyFlags(libime::PinyinFuzzyFlags());
        }
        // 双拼 profile 热更新（配置变更后即时生效）
        if (config().enableShuangpin.value()) {
            auto sp = shuangpinProfileFrom(config().shuangpinScheme.value());
            ime_->setShuangpinProfile(std::make_shared<libime::ShuangpinProfile>(sp));
        }
    }
}

std::vector<InputMethodEntry> PinyinIMEEngine::listInputMethods() {
    std::vector<InputMethodEntry> result;
    result.push_back(std::move(
        InputMethodEntry("pinyinime", "Pinyin IME", "zh_CN", "pinyinime")
            .setNativeName("拼音")
            .setLabel("拼")
            .setIcon("input-keyboard")
            .setConfigurable(true)));
    return result;
}

void PinyinIMEEngine::keyEvent(const InputMethodEntry &entry, KeyEvent &event) {
    FCITX_UNUSED(entry);
    if (event.isRelease()) {
        return;
    }
    // 懒加载：fcitx5 启动时不一定调用 reloadConfig，首次按键前确保引擎与数据就绪。
    ensureDataLoaded();
    if (!ime_) {
        // 数据缺失（DataDir 未找到 zh_CN.lm/sc.dict）：不消费，交给应用/其它 IM。
        return;
    }
    auto *ic = event.inputContext();
    auto *state = ic->propertyFor(&factory_);
    if (!state) {
        // 引擎数据未就绪（DataDir 缺失）：不消费，交给应用/其它 IM。
        return;
    }
    auto &ctx = state->context_;

    // Ctrl-E：进入/退出 表情选择（必须在「组合键交给框架」守卫之前拦截；
    // 其余 Ctrl / Super / Alt 组合键仍交给框架）。
    if (event.key().check(FcitxKey_E, KeyState::Ctrl) ||
        event.key().check(FcitxKey_e, KeyState::Ctrl)) {
        state->emojiMode_ = !state->emojiMode_;
        if (state->emojiMode_) {
            state->emojiPage_ = 0;
        }
        state->refreshUI();
        event.filterAndAccept();
        return;
    }

    // 误上屏撤回（Ctrl-Z）：与 Ctrl-E 一样在「组合键交给框架」守卫之前拦截。
    if (event.key().check(FcitxKey_Z, KeyState::Ctrl) ||
        event.key().check(FcitxKey_z, KeyState::Ctrl)) {
        state->undoLastCommit();
        event.filterAndAccept();
        return;
    }

    // 双拼运行时切换（Ctrl+Shift+S）：全拼↔双拼即时切换，不影响已输入内容。
    if ((event.key().check(FcitxKey_S, KeyState::Ctrl_Shift) ||
         event.key().check(FcitxKey_s, KeyState::Ctrl_Shift))) {
        state->shuangpinEnabled_ = !state->shuangpinEnabled_;
        ctx.setUseShuangpin(state->shuangpinEnabled_);
        state->refreshUI();
        event.filterAndAccept();
        return;
    }

    // 候选焦点移动（Ctrl+←/→）：只在有拼音候选时拦截；空闲时透传给应用
    // （编辑器里 Ctrl+←/→ 是「按词移动文本光标」，不能吃掉）。
    if ((event.key().check(FcitxKey_Left, KeyState::Ctrl) ||
         event.key().check(FcitxKey_Right, KeyState::Ctrl)) &&
        !ctx.empty()) {
        const auto &ccands = ctx.candidatesToCursor();
        if (!ccands.empty()) {
            const size_t begin = state->page_ * state->pageSize();
            const size_t end = std::min<size_t>(ccands.size(), begin + state->pageSize());
            if (end > begin) {
                if (event.key().check(FcitxKey_Left, KeyState::Ctrl)) {
                    state->candFocus_ = state->candFocus_ > 0 ? state->candFocus_ - 1 : 0;
                } else {
                    state->candFocus_ =
                        std::min<size_t>(state->candFocus_ + 1, end - begin - 1);
                }
            }
        }
        state->refreshUI();
        event.filterAndAccept();
        return;
    }

    // 组合键（Ctrl / Super / Alt）交给框架，本引擎不处理（Shift 不算，需用于标点）。
    if (event.key().states().testAny(
            KeyStates{KeyState::Ctrl, KeyState::Super, KeyState::Alt})) {
        return;
    }

    const KeySym sym = event.key().sym();
    const bool shifted = event.key().states().test(KeyState::Shift);

    // ---- 中英切换（Tab）----
    if (event.key().check(FcitxKey_Tab)) {
        if (state->englishMode_) {
            // 英文 → 中文：英文缓冲保留，仅切回（与 CLI 一致）。
            state->englishMode_ = false;
        } else {
            // 中文 → 英文：清缓冲进入英文态（拼音仍保留在 context_，切回即恢复）。
            state->englishMode_ = true;
            state->engBuf_.clear();
        }
        state->refreshUI();
        event.filterAndAccept();
        return;
    }

    // ---- 英文态 ----
    if (state->englishMode_) {
        if (event.key().check(FcitxKey_Escape)) {
            // Esc：清缓冲并切回中文
            state->engBuf_.clear();
            state->englishMode_ = false;
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_BackSpace)) {
            if (!state->engBuf_.empty()) {
                state->engBuf_.pop_back();
            }
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_Return)) {
            // 回车：整行上屏
            if (!state->engBuf_.empty()) {
                state->commitText(state->engBuf_);
                state->engBuf_.clear();
            }
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        const std::string ascii = asciiFromKey(sym, shifted);
        if (!ascii.empty()) {
            state->engBuf_ += ascii;
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        // 英文态不认识的键：透传给应用。
        return;
    }

    // ---- 中文态 ----
    // 表情选择态：1-9 选表情、[,]/[.] 翻页、Esc 退出（Ctrl-E 已在上方守卫前处理）。
    if (state->emojiMode_) {
        const auto &list = emoji_list();
        const size_t ps = state->pageSize();
        if (event.key().check(FcitxKey_Escape)) {
            state->emojiMode_ = false;
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_comma) ||
            event.key().check(FcitxKey_bracketleft)) {
            if (state->emojiPage_ > 0) {
                --state->emojiPage_;
            }
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_period) ||
            event.key().check(FcitxKey_bracketright)) {
            const size_t maxPage = list.empty() ? 0 : (list.size() - 1) / ps;
            if (state->emojiPage_ < maxPage) {
                ++state->emojiPage_;
            }
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        if (sym >= FcitxKey_1 && sym <= FcitxKey_9) {
            const size_t idx = state->emojiPage_ * ps +
                               static_cast<size_t>(sym - FcitxKey_1);
            if (idx < list.size()) {
                state->commitText(list[idx].glyph);
                state->emojiMode_ = false;
            }
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        // 表情态的其他键：吃掉，不交给应用。
        event.filterAndAccept();
        return;
    }

    // ---- v 模式（搜狗式符号输入）----
    // 入口：空闲时按 v 进入 v 模式（不传给解码器）
    if (!state->vMode_ && ctx.empty() && event.key().check(FcitxKey_v)) {
        state->vMode_ = true;
        state->vModeSub_.clear();
        state->vModePage_ = 0;
        state->refreshUI();
        event.filterAndAccept();
        return;
    }
    // v 模式中的按键处理
    if (state->vMode_) {
        // Esc：退出 v 模式
        if (event.key().check(FcitxKey_Escape)) {
            state->vMode_ = false;
            state->vModeSub_.clear();
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        // Backspace：删子输入或退出
        if (event.key().check(FcitxKey_BackSpace)) {
            if (!state->vModeSub_.empty()) {
                state->vModeSub_.pop_back();
                state->vModePage_ = 0;
            } else {
                state->vMode_ = false;
            }
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        // 翻页
        if (event.key().check(FcitxKey_comma) || event.key().check(FcitxKey_bracketleft) ||
            event.key().check(FcitxKey_minus) || event.key().check(FcitxKey_Page_Up)) {
            if (state->vModePage_ > 0) --state->vModePage_;
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_period) || event.key().check(FcitxKey_bracketright) ||
            event.key().check(FcitxKey_equal) || event.key().check(FcitxKey_Page_Down)) {
            ++state->vModePage_;
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        // 数字 1-9：选符号（如果已有子输入）或设置子输入
        if (sym >= FcitxKey_1 && sym <= FcitxKey_9) {
            if (state->vModeSub_.empty()) {
                // 第一次输入：设置符号组
                state->vModeSub_ = std::string(1, '0' + (sym - FcitxKey_0));
                state->vModePage_ = 0;
                state->refreshUI();
                event.filterAndAccept();
                return;
            }
            // 已有子输入：选符号
            auto [symbols, label] = pinyin_ime::v_mode_lookup(state->vModeSub_);
            if (!symbols.empty()) {
                const size_t ps = state->pageSize();
                const size_t idx = state->vModePage_ * ps + static_cast<size_t>(sym - FcitxKey_1);
                if (idx < symbols.size()) {
                    state->commitText(symbols[idx]);
                    state->vMode_ = false;
                    state->vModeSub_.clear();
                }
            }
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        // 字母 d/f：设置符号组
        if (sym == FcitxKey_d || sym == FcitxKey_f) {
            if (state->vModeSub_.empty()) {
                state->vModeSub_ = std::string(1, static_cast<char>(sym));
                state->vModePage_ = 0;
                state->refreshUI();
                event.filterAndAccept();
                return;
            }
        }
        // 空格：选第一个符号
        if (event.key().check(FcitxKey_space) && !state->vModeSub_.empty()) {
            auto [symbols, label] = pinyin_ime::v_mode_lookup(state->vModeSub_);
            if (!symbols.empty()) {
                const size_t ps = state->pageSize();
                const size_t idx = state->vModePage_ * ps;
                if (idx < symbols.size()) {
                    state->commitText(symbols[idx]);
                    state->vMode_ = false;
                    state->vModeSub_.clear();
                }
            }
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        // 回车：上屏原始 v 输入
        if (event.key().check(FcitxKey_Return)) {
            std::string raw = "v" + state->vModeSub_;
            state->commitText(raw);
            state->vMode_ = false;
            state->vModeSub_.clear();
            state->refreshUI();
            event.filterAndAccept();
            return;
        }
        // v 模式其他键：吃掉
        event.filterAndAccept();
        return;
    }

    // 退格：删一个拼音字符
    if (event.key().check(FcitxKey_BackSpace)) {
        if (!ctx.empty()) {
            ctx.backspace();
            state->candFocus_ = 0;
            state->refreshUI();
            event.filterAndAccept();
        }
        return;
    }

    // Esc：清空预编辑
    if (event.key().check(FcitxKey_Escape)) {
        if (!ctx.empty()) {
            state->reset();
            event.filterAndAccept();
        }
        return;
    }

    // 左右箭头：候选光标按词边界移动（逐词分段）；无拼音时不消费，交给应用移动文本光标
    if (event.key().check(FcitxKey_Left) ||
        event.key().check(FcitxKey_Right)) {
        if (!ctx.empty()) {
            if (event.key().check(FcitxKey_Left)) {
                int b = ctx.pinyinBeforeCursor();
                if (b >= 0) {
                    state->page_ = 0;
                    ctx.setCursor(static_cast<size_t>(b));
                }
            } else {
                int a = ctx.pinyinAfterCursor();
                if (a >= 0) {
                    state->page_ = 0;
                    ctx.setCursor(static_cast<size_t>(a));
                }
            }
            state->refreshUI();
            event.filterAndAccept();
        }
        return;
    }

    // Home/End：候选光标跳句首/句尾；无拼音时不消费，交给应用移动文本光标
    if (event.key().check(FcitxKey_Home) ||
        event.key().check(FcitxKey_End)) {
        if (!ctx.empty()) {
            state->page_ = 0;
            if (event.key().check(FcitxKey_Home)) {
                ctx.setCursor(0);
            } else {
                ctx.setCursor(static_cast<size_t>(ctx.size()));
            }
            state->refreshUI();
            event.filterAndAccept();
        }
        return;
    }

    // PageUp/PageDown：候选翻页（与 - = , . [ ] 并列）；无拼音时不消费，交给应用/其它 IM
    if (event.key().check(FcitxKey_Page_Up) ||
        event.key().check(FcitxKey_Page_Down)) {
        if (!ctx.empty()) {
            state->flipPage(
                            event.key().check(FcitxKey_Page_Up) ? -1 : 1);
            event.filterAndAccept();
        }
        return;
    }

    // v0.9.115: 回车：上屏「用户原始输入」（字母 + 隔音符，去掉空格），不选中文候选；
    // 从 ctx.preedit() 取（始终与当前输入一致，退格后也准确），空则回退 rawInput_。
    // 无拼音时不消费（交给应用换行）。
    if (event.key().check(FcitxKey_Return)) {
        if (!ctx.empty()) {
            std::string raw;
            for (char c : ctx.preedit()) {
                if ((c >= 'a' && c <= 'z') || c == '\'') raw.push_back(c);
            }
            if (raw.empty()) raw = state->rawInput_;
            state->commitText(raw);
            state->reset();
            event.filterAndAccept();
        }
        return;
    }

    // 空格：上屏「焦点候选」（默认焦点 0 = 首选，与旧行为一致）
    if (event.key().check(FcitxKey_space)) {
        if (!ctx.empty()) {
            // v0.9.109: 英文检测（含双写辅音）— 中文拼音音节不含任何双写辅音
            // （ll/oo/ee/tt/ss/pp/mm/nn 等），而 hello/good/look/apple 等常见英文
            // 单词含双写。命中即判定为英文输入，跳过 M3 纠错（避免把 hello 误
            // 纠成 henli 再选中生僻候选），直接交给下方混输分支上屏英文。
            std::string compactRaw;
            compactRaw.reserve(state->rawInput_.size());
            for (char c : state->rawInput_) {
                if (c >= 'a' && c <= 'z') compactRaw.push_back(c);
            }
            // v0.9.113: 若命中 boost 预设（已知中文简拼，如 yyds=永远的神），
            // 不判为英文（yy 双写在 yyds/yy 中是合法简拼，不是英文双写）。
            // 注意：英文直通（hello→hello）返回值等于输入，不算 knownAbbr。
            const bool knownAbbr = !compactRaw.empty() &&
                [&compactRaw]() {
                    const auto &w = pinyin_ime::lookup_pinyin_boost(compactRaw);
                    return !w.empty() && w != compactRaw;
                }();
            const bool englishDoubled =
                !compactRaw.empty() && !knownAbbr && !is_valid_pinyin(compactRaw) &&
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
            // M3 纠错（EnableCorrect）：全拼打错时解码前整串纠正。should_correct
            // 门控（含重错全拼扩展）保证简拼/混合输入不误伤（与 CLI --correct 语义一致）。
            // 英文输入（含双写辅音）跳过纠错。
            if (config().enableCorrect.value() && !englishDoubled) {
                std::string cont;
                for (char c : ctx.preedit()) {
                    if (c != '\'' && c != ' ') {
                        cont.push_back(c);
                    }
                }
                if (!cont.empty() && pinyin_corrector::should_correct(cont)) {
                    std::string fixed = pinyin_corrector::correct_to_string(cont);
                    if (!fixed.empty() && fixed != cont) {
                        state->page_ = 0;
                        state->candFocus_ = 0;
                        ctx.clear();
                        for (char c : fixed) {
                            ctx.type(std::string(1, c));
                        }
                        // 同步 rawInput_ 为纠正后的紧凑串（去 ' / 空格）：纠错已
                        // 确认这是「疑似全拼打错」，中英混输判断不能再用纠错前的
                        // 原文（如 zhing 5 字符 >4 且非法拼音，会在纠错成功后仍被
                        // 原样当英文上屏，丢失拼音意图）。
                        state->rawInput_.clear();
                        for (char c : fixed) {
                            if (c != '\'' && c != ' ') {
                                state->rawInput_.push_back(c);
                            }
                        }
                    }
                }
            }
            // v0.9.110: 中英自动混输 — 关键修复：只要当前有中文候选，就优先选候选，
            // 不把原始输入当英文上屏。
            // 此前条件 `candidates().empty() || size>4` 会误伤「含简拼的混合输入」
            // （如 woshizg、nihaozg：is_valid_pinyin 因 zg 不是合法音节而判 false，
            // 但 libime 实际能给出“我是中国/你好中国”等候选），导致按空格把
            // 整个拼音串上屏。现在：
            //   - englishDoubled（含双写辅音）→ 明确英文，上屏英文；
            //   - 否则仅当「无任何候选」且「非合法拼音」才上屏英文；
            //   - 只要候选非空（无论紧凑拼音是否合法）→ 选候选。

            // 搜狗式快捷输入：日期/时间（date/rq/time/sj/week/xq/now/dt）
            {
                std::string dtResult = pinyin_ime::datetime_lookup(compactRaw);
                if (!dtResult.empty()) {
                    state->commitText(dtResult);
                    state->reset();
                    event.filterAndAccept();
                    return;
                }
            }
            // 搜狗式计算器：输入为算式时，结果作为首选上屏
            if (pinyin_ime::is_calculator_input(compactRaw)) {
                std::string calcResult = pinyin_ime::calculator_eval(compactRaw);
                if (!calcResult.empty()) {
                    state->commitText(compactRaw + "=" + calcResult);
                    state->reset();
                    event.filterAndAccept();
                    return;
                }
            }

            if (englishDoubled ||
                (!compactRaw.empty() &&
                 ctx.candidates().empty() &&
                 !is_valid_pinyin(compactRaw))) {
                state->commitText(state->rawInput_);
                state->reset();
                event.filterAndAccept();
                return;
            }
            state->selectCandidate(
                                   state->page_ * state->pageSize() + state->candFocus_);
            event.filterAndAccept();
        }
        return;
    }

    // 数字 1-9：选当前页第 N 个候选（全局索引 = page*pageSize + (N-1)）
    if (sym >= FcitxKey_1 && sym <= FcitxKey_9) {
        if (!ctx.empty()) {
            const size_t idx =
                state->page_ * state->pageSize() +
                static_cast<size_t>(sym - FcitxKey_1);
            state->selectCandidate(idx);
            event.filterAndAccept();
        }
        return;
    }

    // ---- 翻页 / 中文标点 / 引号智能配对 ----
    const std::string ascii = asciiFromKey(sym, shifted);
    const std::string fp = fullwidthPunct(ascii);

    // 引号 "：开“/闭”交替（空闲上屏；打拼音中先上屏首选再出引号）
    if (ascii == "\"") {
        if (!ctx.empty()) {
            state->selectCandidate(0);
        }
        state->commitText(state->quoteOpen_ ? "”" : "“");
        state->quoteOpen_ = !state->quoteOpen_;
        state->refreshUI();
        event.filterAndAccept();
        return;
    }

    const bool prevKey = (ascii == "-" || ascii == "," || ascii == "[");
    const bool nextKey = (ascii == "=" || ascii == "." || ascii == "]");

    if (prevKey || nextKey) {
        if (!ctx.empty()) {
            // 打拼音中：翻页
            state->flipPage( prevKey ? -1 : 1);
            event.filterAndAccept();
            return;
        }
        if (!fp.empty()) {
            // 空闲：, . [ ] 上屏全角标点；- = 无标点映射则透传
            state->commitPunctuation(fp);
            event.filterAndAccept();
            return;
        }
        return; // 空闲 - = ：不消费，透传给应用
    }

    if (!fp.empty()) {
        // 其余标点（; : ? ! ( ) \）：空闲直接上屏，打拼音中先上屏首选再出标点
        state->commitPunctuation(fp);
        event.filterAndAccept();
        return;
    }

    // 单引号：空闲→智能配对‘/’；打拼音中→仍是隔音符（xi'an）。
    // 注：shift+apostrophe 已由上面的 asciiFromKey 折叠为 '"'，此处仅处理非 shift 的 '。
    if (sym == FcitxKey_apostrophe && !shifted) {
        if (ctx.empty()) {
            state->commitText(state->singleQuoteOpen_ ? "’" : "‘");
            state->singleQuoteOpen_ = !state->singleQuoteOpen_;
            state->refreshUI();
        } else {
            ctx.type("'");
            state->refreshUI();
        }
        event.filterAndAccept();
        return;
    }

    // 拼音字母（a-z）或隔音符 ' ：送入解码
    if (event.key().isLAZ() || event.key().check(FcitxKey_apostrophe)) {
        const std::string ch =
            sym == FcitxKey_apostrophe ? "'" : Key::keySymToUTF8(sym);
        if (!ch.empty()) {
            ctx.type(ch);
            state->rawInput_ += ch;
            // 拼音消歧：检查当前预编辑是否需要 xian→xi'an 拆分
            // 获取当前预编辑（去除空格/隔音符），应用消歧
            std::string rawPreedit;
            for (char c : ctx.preedit()) {
                if (c >= 'a' && c <= 'z') {
                    rawPreedit.push_back(c);
                }
            }
            std::string disambig = disambiguate_pinyin(rawPreedit);
            if (disambig != rawPreedit) {
                // 需要消歧：清空并重新输入拆分后的拼音
                ctx.clear();
                state->page_ = 0;
                for (char c : disambig) {
                    ctx.type(std::string(1, c));
                }
            }
            state->candFocus_ = 0; // 新拼音改变候选，焦点回到首选
            state->refreshUI();
            event.filterAndAccept();
        }
        return;
    }

    // 其余键不处理（交给应用）。
}

void PinyinIMEEngine::reset(const InputMethodEntry &entry,
                            InputContextEvent &event) {
    FCITX_UNUSED(entry);
    auto *state = event.inputContext()->propertyFor(&factory_);
    state->reset();
}

void PinyinIMEEngine::deactivate(const InputMethodEntry &entry,
                                 InputContextEvent &event) {
    FCITX_UNUSED(entry);
    saveUserData();
    auto *state = event.inputContext()->propertyFor(&factory_);
    state->reset();
    // 失焦后撤回没有意义（文本已落在别的应用窗口），清空撤回栈防误伤。
    state->undoStack_.clear();
}

class PinyinIMEFactory : public AddonFactory {
    AddonInstance *create(AddonManager *manager) override {
        return new PinyinIMEEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY(fcitx::PinyinIMEFactory)  // 5.1.7 经典宏只带工厂类一个参数