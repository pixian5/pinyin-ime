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

// 常见「好用度」模糊音集合（与 cpp/pinyin_ime_cli.cpp 保持一致）。
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

} // namespace

// ---------------- PinyinIMEState ----------------

PinyinIMEState::PinyinIMEState(PinyinIMEEngine *engine, InputContext *ic)
    : engine_(engine), inputContext_(ic), context_(engine->ime()) {
    for (int i = 0; i < 9; ++i) {
        selectionKeys_.emplace_back(static_cast<KeySym>(FcitxKey_1 + i));
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
    page_ = 0;
    maxPage_ = 0;
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

    inputContext_->inputPanel().reset();
    inputContext_->inputPanel().setPreedit(preeditText);
    if (end > begin) {
        auto candidateList = std::make_unique<CommonCandidateList>();
        for (size_t i = begin; i < end; ++i) {
            // 仅展示；数字选字在 keyEvent 里自行处理（与官方 pinyin 一致）。
            candidateList->append<DisplayOnlyCandidateWord>(
                Text(cands[i].toString()));
        }
        candidateList->setPageSize(static_cast<int>(ps));
        candidateList->setSelectionKey(selectionKeys_);
        candidateList->setCursorIncludeUnselected(true);
        candidateList->setCursorIndex(0);
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
    context_.selectCandidatesToCursor(idx);
    if (context_.selected()) {
        const std::string text = context_.sentence();
        if (!text.empty()) {
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

// ---------------- PinyinIMEEngine ----------------

PinyinIMEEngine::PinyinIMEEngine(Instance *instance)
    : instance_(instance) {
    // 关键：FactoryFor 不会自动注册，必须在构造函数里显式注册到
    // InputContextManager，否则 slot_ = -1、propertyFor() 越界读垃圾指针。
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
    if (config().enableFuzzy.value()) {
        ime_->setFuzzyFlags(commonFuzzyFlags());
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
    ensureDataLoaded();
    if (ime_) {
        ime_->setNBest(
            static_cast<size_t>(std::max(1, config().nbest.value())));
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

    // 空格 / 回车：上屏首选
    if (event.key().check(FcitxKey_space) ||
        event.key().check(FcitxKey_Return)) {
        if (!ctx.empty()) {
            state->selectCandidate(entry, 0);
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
}

class PinyinIMEFactory : public AddonFactory {
    AddonInstance *create(AddonManager *manager) override {
        return new PinyinIMEEngine(manager->instance());
    }
};

} // namespace fcitx

FCITX_ADDON_FACTORY_V2(pinyinime, fcitx::PinyinIMEFactory);