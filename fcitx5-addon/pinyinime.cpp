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
    inputContext_->inputPanel().reset();
    inputContext_->updatePreedit();
    inputContext_->updateUserInterface(UserInterfaceComponent::InputPanel);
}

void PinyinIMEState::updateUI(const InputMethodEntry &entry) {
    FCITX_UNUSED(entry);

    // 预编辑（当前敲的拼音）
    const std::string pre = context_.preedit();
    Text preeditText;
    if (!pre.empty()) {
        preeditText.append(pre);
    }

    // 候选（当前页）
    auto candidateList = std::make_unique<CommonCandidateList>();
    const auto &cands = context_.candidatesToCursor();
    const size_t ps = pageSize();
    if (page_ > maxPage_) {
        page_ = maxPage_;
    }
    const size_t begin = page_ * ps;
    const size_t end = std::min<size_t>(cands.size(), begin + ps);
    for (size_t i = begin; i < end; ++i) {
        // 仅展示；数字选字在 keyEvent 里自行处理（与官方 pinyin 一致）。
        candidateList->append<DisplayOnlyCandidateWord>(Text(cands[i].toString()));
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

// ---------------- PinyinIMEEngine ----------------

PinyinIMEEngine::PinyinIMEEngine(Instance *instance) : instance_(instance) {}

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

    auto *ic = event.inputContext();
    auto *state = ic->propertyFor(&factory_);
    auto &ctx = state->context_;

    // 组合键（Ctrl / Super / Alt）交给框架，本引擎不处理。
    if (event.key().states().testAny(
            KeyStates{KeyState::Ctrl, KeyState::Super, KeyState::Alt})) {
        return;
    }

    const KeySym sym = event.key().sym();

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

    // 翻页：- / , 上一页，= / . 下一页
    if (event.key().check(FcitxKey_minus) ||
        event.key().check(FcitxKey_comma)) {
        if (!ctx.empty()) {
            state->flipPage(entry, -1);
            event.filterAndAccept();
        }
        return;
    }
    if (event.key().check(FcitxKey_equal) ||
        event.key().check(FcitxKey_period)) {
        if (!ctx.empty()) {
            state->flipPage(entry, 1);
            event.filterAndAccept();
        }
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

    // 其余键不处理（交给应用）；标点/中英切换等体验项见 README「未来优化方向」。
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