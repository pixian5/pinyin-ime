/*
 * pinyin-ime — Fcitx5 addon（输入法引擎）
 *
 * 把 libime 的拼音解码（PinyinIME + PinyinContext）接进 Fcitx5 框架，注册一个
 * 「pinyinime」输入法。这是本项目从「命令行输入法」走向「系统级输入法」的落地件。
 *
 * 架构（对照 fcitx5 官方 keyboard addon 的成熟骨架）：
 *   PinyinIMEEngine : InputMethodEngine   —— 每个 addon 一个，持共享的
 *       libime::PinyinIME（词典 + 语言模型，跨输入上下文共享）。
 *   PinyinIMEState  : InputContextProperty —— 每个输入上下文一个，持独立的
 *       libime::PinyinContext（候选 / 预编辑 / 选中状态）。
 *
 * 许可：跟随项目与 fcitx5 / libime（LGPL-2.1-or-later）。
 */
#ifndef _FCITX_MODULES_PINYINIME_PINYINIME_H_
#define _FCITX_MODULES_PINYINIME_PINYINIME_H_

#include <fcitx-config/configuration.h>
#include <fcitx-config/iniparser.h>
#include <fcitx-config/option.h>
#include <fcitx-utils/i18n.h>
#include <fcitx-utils/key.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinime.h>
#include <libime/pinyin/shuangpinprofile.h>

namespace fcitx {

FCITX_CONFIGURATION(
    PinyinIMEConfig,
    Option<int> nbest{this, "NBest", _("Number of candidates"), 45};
    Option<int> pageSize{this, "PageSize", _("Candidates per page"), 9};
    Option<std::string> dataDir{this, "DataDir", _("Data directory"),
                                "/root/.local/pinyin-ime/share/libime"};
    Option<std::string> userDataDir{
        this, "UserDataDir", _("User data directory"),
        "~/.local/share/pinyin-ime"};
    Option<bool> enableFuzzy{this, "EnableFuzzy", _("Enable fuzzy pinyin"),
                             false};
    Option<bool> enableCorrect{
        this, "EnableCorrect", _("Enable M3 spelling correction"), false};
    Option<bool> enableShuangpin{
        this, "EnableShuangpin", _("Enable double pinyin (shuangpin)"), false};
    Option<std::string> shuangpinScheme{
        this, "ShuangpinScheme", _("Double pinyin scheme (xiaohe/ziranma/ms/ziguang/abc/zhongwenzhixing/pinyinjiajia/gb)"),
        "xiaohe"};);

class PinyinIMEEngine;

class PinyinIMEState : public InputContextProperty {
public:
    PinyinIMEState(PinyinIMEEngine *engine, InputContext *inputContext);

    // 清空当前输入状态并清掉 UI。
    void reset();
    // 重建候选（更新 page_/maxPage_），并刷新 preedit + candidate UI。
    void refreshUI(const InputMethodEntry &entry);
    // 选中第 idx（全局索引，已含翻页偏移）个候选；整句选完即上屏。
    void selectCandidate(const InputMethodEntry &entry, size_t idx);
    // 翻页：dir=-1 上一页 / +1 下一页。
    void flipPage(const InputMethodEntry &entry, int dir);

    PinyinIMEEngine *engine_;
    InputContext *inputContext_;
    libime::PinyinContext context_;
    size_t page_ = 0;
    size_t maxPage_ = 0;
    KeyList selectionKeys_;
    // 候选焦点（页内 0..pageSize-1）：Ctrl+←/→ 移动，空格上屏焦点候选（默认 0=首选）。
    size_t candFocus_ = 0;
    // 简拼 boost：候选 boostSwap_ 与 0 交换显示（0=不交换），与 CLI 行为一致
    size_t boostSwap_ = 0;
    // boost 插入：预设词不在候选列表时，插入首位显示（空=不插入）
    std::string boostInsertWord_;

    // 中英切换（Tab）/ 中文标点（双/单引号智能配对）会话状态。
    bool englishMode_ = false;
    bool quoteOpen_ = false;
    bool singleQuoteOpen_ = false;
    std::string engBuf_;
    // 表情选择（Ctrl-E）：1-9 选表情，[,]/[.] 翻页，Esc/Ctrl-E 返回中文。
    bool emojiMode_ = false;
    // 双拼运行时开关（Ctrl+Shift+S 切换，初始值来自配置）
    bool shuangpinEnabled_ = false;
    size_t emojiPage_ = 0;
    // 误上屏撤回栈：(上屏文本, 拼音串)，最近在上；deactivate（失焦）时清空。
    std::vector<std::pair<std::string, std::string>> undoStack_;

    size_t pageSize() const;
    // 上屏一段已提交文本（整句 / 英文整行 / 标点），清面板并 commitString。
    void commitText(const std::string &text);
    // 上屏中文标点：打拼音中先上屏首选，空闲直接上屏。
    void commitPunctuation(const InputMethodEntry &entry,
                           const std::string &fullwidth);
    // 误上屏撤回（Ctrl-Z）：通知应用删除刚上屏的文本、恢复拼音重新编辑。
    void undoLastCommit(const InputMethodEntry &entry);

private:
    void updateUI(const InputMethodEntry &entry);
};

class PinyinIMEEngine final : public InputMethodEngine {
public:
    PinyinIMEEngine(Instance *instance);
    ~PinyinIMEEngine() override;

    void keyEvent(const InputMethodEntry &entry, KeyEvent &keyEvent) override;
    std::vector<InputMethodEntry> listInputMethods() override;
    void deactivate(const InputMethodEntry &entry,
                    InputContextEvent &event) override;
    void reset(const InputMethodEntry &entry, InputContextEvent &event) override;

    const PinyinIMEConfig &config() const { return config_; }
    const Configuration *getConfig() const override { return &config_; }
    void setConfig(const RawConfig &config) override {
        config_.load(config, true);
        safeSaveAsIni(config_, "conf/pinyinime.conf");
        reloadConfig();
    }
    void reloadConfig() override;

    libime::PinyinIME *ime() { return ime_.get(); }
    // 用户词库 / 自学习历史落盘（destructor 与 deactivate 时调用）。
    void saveUserData();

    auto factory() { return &factory_; }

private:
    bool ensureDataLoaded();

    Instance *instance_;
    PinyinIMEConfig config_;
    std::unique_ptr<libime::PinyinIME> ime_;
    std::string userHistPath_, userDictPath_;
    FactoryFor<PinyinIMEState> factory_{
        [this](InputContext &ic) {
            // 关键：InputContext 创建时会为所有已注册 factory 建 property，
            // 此刻必须保证引擎数据已加载（否则 PinyinIMEState 用空引擎构造、
            // 后续 keyEvent 访问 context_ 即崩溃）。
            ensureDataLoaded();
            return ime_ ? new PinyinIMEState(this, &ic) : nullptr;
        }};
};

} // namespace fcitx

#endif // _FCITX_MODULES_PINYINIME_PINYINIME_H_