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
libime::PinyinFuzzyFlags commonFuzzyFlags() {
    using libime::PinyinFuzzyFlag;
    return {PinyinFuzzyFlag::C_CH,         PinyinFuzzyFlag::F_H,
            PinyinFuzzyFlag::L_N,          PinyinFuzzyFlag::S_SH,
            PinyinFuzzyFlag::Z_ZH,         PinyinFuzzyFlag::L_R,
            PinyinFuzzyFlag::AN_ANG,       PinyinFuzzyFlag::EN_ENG,
            PinyinFuzzyFlag::IAN_IANG,     PinyinFuzzyFlag::IN_ING,
            PinyinFuzzyFlag::U_OU,         PinyinFuzzyFlag::UAN_UANG,
            PinyinFuzzyFlag::V_U,          PinyinFuzzyFlag::VE_UE,
            PinyinFuzzyFlag::CommonTypo,   PinyinFuzzyFlag::AdvancedTypo,
            PinyinFuzzyFlag::Correction};
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

    if (name == "gb") return libime::ShuangpinBuiltinProfile::GB;

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
    inputContext_->inputPanel().reset();
    inputContext_->updatePreedit();
    inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void PinyinIMEState::updateUI(const InputMethodEntry &entry) {
    FCITX_UNUSED(entry);

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
        candidateList->setCursorIndex(0);

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
        // v0.9.51: boost 插入词显示在首位
        if (!boostInsertWord_.empty() && page_ == 0) {
            candidateList->append<DisplayOnlyCandidateWord>(Text(boostInsertWord_));
        }
        for (size_t i = begin; i < end; ++i) {
            size_t displayIdx = i;
            // 简拼 boost：交换候选 0 和 boostSwap_ 的显示位置
            if (boostSwap_ > 0 && page_ == 0) {
                if (i == begin) displayIdx = boostSwap_;
                else if (i == boostSwap_) displayIdx = begin;
            }
            // v0.9.90: 首选候选显示拼音标注
            Text candText(cands[displayIdx].toString());
            if (i == begin && page_ == 0) {
                std::string candPy = context_.candidateFullPinyin(displayIdx);
                if (!candPy.empty()) {
                    candText.append(" " + candPy);
                }
            }
            // 仅展示；数字选字在 keyEvent 里自行处理（与官方 pinyin 一致）。
            candidateList->append<DisplayOnlyCandidateWord>(candText);
        }
        candidateList->setPageSize(static_cast<int>(ps));
        candidateList->setSelectionKey(selectionKeys_);
        candidateList->setCursorIncludeUnselected(true);
        // 候选焦点视觉高亮：把 classicui 的「高亮光标」对齐到 candFocus_（页内 0..8），
        // 让 Ctrl+←/→ 移动焦点后，候选窗里高亮的就是即将上屏的那个候选（第 15 轮只做了
        // 上屏语义、视觉仍固定在第 1 个）。candFocus_ 已在 keyEvent 钳制到 < end-begin，
        // 且 end>begin 时才走到这里，故索引必有效。翻页/新拼音/退格已把 candFocus_ 归零。
        candidateList->setCursorIndex(static_cast<int>(candFocus_));
        inputContext_->inputPanel().setCandidateList(std::move(candidateList));
    }
    inputContext_->updatePreedit();
    inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void PinyinIMEState::refreshUI(const InputMethodEntry &entry) {
    const auto &cands = context_.candidatesToCursor();
    const size_t ps = pageSize();
    maxPage_ = cands.empty() ? 0 : (cands.size() - 1) / ps;
    if (page_ > maxPage_) {
        page_ = maxPage_;
    }
    updateUI(entry);
}

void PinyinIMEState::selectCandidate(const InputMethodEntry &entry, size_t idx) {
    const auto &cands = context_.candidatesToCursor();
    if (context_.empty() || idx >= cands.size()) {
        return;
    }
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
        refreshUI(entry);
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
        refreshUI(entry);
    }
    engine_->saveUserData();
}

void PinyinIMEState::flipPage(const InputMethodEntry &entry, int dir) {
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
    updateUI(entry);
}

void PinyinIMEState::commitPunctuation(const InputMethodEntry &entry,
                                       const std::string &fullwidth) {
    // 打拼音中：先上屏首选；空闲：直接上屏标点。与 CLI 的 select_and_commit 语义一致。
    if (!context_.empty()) {
        selectCandidate(entry, 0);
    }
    commitText(fullwidth);
    if (!context_.empty()) {
        refreshUI(entry);
    }
}

void PinyinIMEState::undoLastCommit(const InputMethodEntry &entry) {
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
    refreshUI(entry);
}

// ---------------- PinyinIMEEngine ----------------

PinyinIMEEngine::PinyinIMEEngine(Instance *instance)
    : instance_(instance) {
    // 关键：FactoryFor 不会自动注册，必须在构造函数里显式注册到
    // InputContextManager，否则 slot_ = -1、propertyFor() 越界读垃圾指针。
    // （对照官方 keyboard addon 的 registerProperty("keyboardState", ...)）
    instance_->inputContextManager().registerProperty("pinyinimeState",
                                                      &factory_);
    // fcitx5 的 addon 用户配置（conf/pinyinime.conf）默认由 DBus 模块 SetConfig
    // 载入；无 DBus 构建（如 headless Xvfb 实测）不会走该路径，故显式读取一次，
    // 使 EnableCorrect/DataDir/UserDataDir 等配置独立于 DBus 生效。
    reloadConfig();
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
        ime_->setFuzzyFlags(commonFuzzyFlags());
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
    if (ime_) {
        ime_->setNBest(
            static_cast<size_t>(std::max(1, config().nbest.value())));
        // 模糊音热更新（配置变更后即时生效）
        if (config().enableFuzzy.value()) {
            ime_->setFuzzyFlags(commonFuzzyFlags());
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
        state->refreshUI(entry);
        event.filterAndAccept();
        return;
    }

    // 误上屏撤回（Ctrl-Z）：与 Ctrl-E 一样在「组合键交给框架」守卫之前拦截。
    if (event.key().check(FcitxKey_Z, KeyState::Ctrl) ||
        event.key().check(FcitxKey_z, KeyState::Ctrl)) {
        state->undoLastCommit(entry);
        event.filterAndAccept();
        return;
    }

    // 双拼运行时切换（Ctrl+Shift+S）：全拼↔双拼即时切换，不影响已输入内容。
    if ((event.key().check(FcitxKey_S, KeyState::Ctrl_Shift) ||
         event.key().check(FcitxKey_s, KeyState::Ctrl_Shift))) {
        state->shuangpinEnabled_ = !state->shuangpinEnabled_;
        ctx.setUseShuangpin(state->shuangpinEnabled_);
        state->refreshUI(entry);
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
        state->refreshUI(entry);
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
        state->refreshUI(entry);
        event.filterAndAccept();
        return;
    }

    // ---- 英文态 ----
    if (state->englishMode_) {
        if (event.key().check(FcitxKey_Escape)) {
            // Esc：清缓冲并切回中文
            state->engBuf_.clear();
            state->englishMode_ = false;
            state->refreshUI(entry);
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_BackSpace)) {
            if (!state->engBuf_.empty()) {
                state->engBuf_.pop_back();
            }
            state->refreshUI(entry);
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_Return)) {
            // 回车：整行上屏
            if (!state->engBuf_.empty()) {
                state->commitText(state->engBuf_);
                state->engBuf_.clear();
            }
            state->refreshUI(entry);
            event.filterAndAccept();
            return;
        }
        const std::string ascii = asciiFromKey(sym, shifted);
        if (!ascii.empty()) {
            state->engBuf_ += ascii;
            state->refreshUI(entry);
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
            state->refreshUI(entry);
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_comma) ||
            event.key().check(FcitxKey_bracketleft)) {
            if (state->emojiPage_ > 0) {
                --state->emojiPage_;
            }
            state->refreshUI(entry);
            event.filterAndAccept();
            return;
        }
        if (event.key().check(FcitxKey_period) ||
            event.key().check(FcitxKey_bracketright)) {
            const size_t maxPage = list.empty() ? 0 : (list.size() - 1) / ps;
            if (state->emojiPage_ < maxPage) {
                ++state->emojiPage_;
            }
            state->refreshUI(entry);
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
            state->refreshUI(entry);
            event.filterAndAccept();
            return;
        }
        // 表情态的其他键：吃掉，不交给应用。
        event.filterAndAccept();
        return;
    }

    // 退格：删一个拼音字符
    if (event.key().check(FcitxKey_BackSpace)) {
        if (!ctx.empty()) {
            ctx.backspace();
            state->candFocus_ = 0;
            state->refreshUI(entry);
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
            state->refreshUI(entry);
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
            state->refreshUI(entry);
            event.filterAndAccept();
        }
        return;
    }

    // PageUp/PageDown：候选翻页（与 - = , . [ ] 并列）；无拼音时不消费，交给应用/其它 IM
    if (event.key().check(FcitxKey_Page_Up) ||
        event.key().check(FcitxKey_Page_Down)) {
        if (!ctx.empty()) {
            state->flipPage(entry,
                            event.key().check(FcitxKey_Page_Up) ? -1 : 1);
            event.filterAndAccept();
        }
        return;
    }

    // 空格 / 回车：上屏「焦点候选」（默认焦点 0 = 首选，与旧行为一致）
    if (event.key().check(FcitxKey_space) ||
        event.key().check(FcitxKey_Return)) {
        if (!ctx.empty()) {
            // M3 纠错（EnableCorrect）：全拼打错时解码前整串纠正。should_correct
            // 门控（含重错全拼扩展）保证简拼/混合输入不误伤（与 CLI --correct 语义一致）。
            if (config().enableCorrect.value()) {
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
                    }
                }
            }
            // v0.9.100: 中英自动混输 — 用原始输入（未纠错）判断
            // compactPy 是纠错后的 preedit（如 hello→henli），会误判为有效拼音
            // rawInput_ 是用户实际键入的字符，用它判断是否英文
            if (!state->rawInput_.empty() &&
                !is_valid_pinyin(state->rawInput_) &&
                (ctx.candidates().empty() || state->rawInput_.size() > 4)) {
                state->commitText(state->rawInput_);
                state->reset();
                event.filterAndAccept();
                return;
            }
            state->selectCandidate(entry,
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
            state->selectCandidate(entry, idx);
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
            state->selectCandidate(entry, 0);
        }
        state->commitText(state->quoteOpen_ ? "”" : "“");
        state->quoteOpen_ = !state->quoteOpen_;
        state->refreshUI(entry);
        event.filterAndAccept();
        return;
    }

    const bool prevKey = (ascii == "-" || ascii == "," || ascii == "[");
    const bool nextKey = (ascii == "=" || ascii == "." || ascii == "]");

    if (prevKey || nextKey) {
        if (!ctx.empty()) {
            // 打拼音中：翻页
            state->flipPage(entry, prevKey ? -1 : 1);
            event.filterAndAccept();
            return;
        }
        if (!fp.empty()) {
            // 空闲：, . [ ] 上屏全角标点；- = 无标点映射则透传
            state->commitPunctuation(entry, fp);
            event.filterAndAccept();
            return;
        }
        return; // 空闲 - = ：不消费，透传给应用
    }

    if (!fp.empty()) {
        // 其余标点（; : ? ! ( ) \）：空闲直接上屏，打拼音中先上屏首选再出标点
        state->commitPunctuation(entry, fp);
        event.filterAndAccept();
        return;
    }

    // 单引号：空闲→智能配对‘/’；打拼音中→仍是隔音符（xi'an）。
    // 注：shift+apostrophe 已由上面的 asciiFromKey 折叠为 '"'，此处仅处理非 shift 的 '。
    if (sym == FcitxKey_apostrophe && !shifted) {
        if (ctx.empty()) {
            state->commitText(state->singleQuoteOpen_ ? "’" : "‘");
            state->singleQuoteOpen_ = !state->singleQuoteOpen_;
            state->refreshUI(entry);
        } else {
            ctx.type("'");
            state->refreshUI(entry);
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
            state->refreshUI(entry);
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

FCITX_ADDON_FACTORY_V2(pinyinime, fcitx::PinyinIMEFactory);