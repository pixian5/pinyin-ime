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
#include <fcitx-utils/event.h>
#include <fcitx/event.h>
#include <fcitx/inputcontextproperty.h>
#include <fcitx/inputmethodengine.h>
#include <fcitx/instance.h>

#include <memory>
#include <mutex>
#include <string>
#include <unordered_set>
#include <utility>
#include <vector>

#include <libime/core/userlanguagemodel.h>
#include <libime/pinyin/pinyincontext.h>
#include <libime/pinyin/pinyindictionary.h>
#include <libime/pinyin/pinyinime.h>
#include <libime/pinyin/shuangpinprofile.h>

// v0.9.175: 云拼音客户端（独立编译单元，逐步从 quick_input.h 万能头里拆出来）
#include "cloud_pinyin.h"
#include "char_lm.h"

namespace fcitx {

FCITX_CONFIGURATION(
    PinyinIMEConfig,
    Option<int> nbest{this, "NBest", _("Number of candidates"), 45};
    Option<int> pageSize{this, "PageSize", _("Candidates per page"), 5};
    Option<std::string> dataDir{this, "DataDir", _("Data directory"),
                                "/root/.local/pinyin-ime/share/libime"};
    Option<std::string> userDataDir{
        this, "UserDataDir", _("User data directory"),
        "~/.local/share/pinyin-ime"};
    Option<bool> enableFuzzy{this, "EnableFuzzy", _("Enable fuzzy pinyin"),
                             false};
    Option<bool> fuzzyZ_ZH{this, "FuzzyZ_ZH", _("z <-> zh"), false};
    Option<bool> fuzzyC_CH{this, "FuzzyC_CH", _("c <-> ch"), false};
    Option<bool> fuzzyS_SH{this, "FuzzyS_SH", _("s <-> sh"), false};
    Option<bool> fuzzyL_N{this, "FuzzyL_N", _("l <-> n"), false};
    Option<bool> fuzzyL_R{this, "FuzzyL_R", _("l <-> r"), false};
    Option<bool> fuzzyF_H{this, "FuzzyF_H", _("f <-> h"), false};
    Option<bool> fuzzyAN_ANG{this, "FuzzyAN_ANG", _("an <-> ang"), false};
    Option<bool> fuzzyEN_ENG{this, "FuzzyEN_ENG", _("en <-> eng"), false};
    Option<bool> fuzzyIN_ING{this, "FuzzyIN_ING", _("in <-> ing"), false};
    Option<bool> fuzzyIAN_IANG{this, "FuzzyIAN_IANG", _("ian <-> iang"), false};
    Option<bool> fuzzyU_OU{this, "FuzzyU_OU", _("u <-> ou"), false};
    Option<bool> fuzzyUAN_UANG{this, "FuzzyUAN_UANG", _("uan <-> uang"), false};
    Option<bool> fuzzyV_U{this, "FuzzyV_U", _("v <-> u"), false};
    Option<bool> fuzzyVE_UE{this, "FuzzyVE_UE", _("ve <-> ue"), false};
    Option<bool> enableCorrect{
        this, "EnableCorrect", _("Enable M3 spelling correction"), false};
    Option<bool> enableShuangpin{
        this, "EnableShuangpin", _("Enable double pinyin (shuangpin)"), false};
    Option<std::string> shuangpinScheme{
        this, "ShuangpinScheme", _("Double pinyin scheme (xiaohe/ziranma/ms/ziguang/abc/zhongwenzhixing/pinyinjiajia/gb)"),
        "xiaohe"};
    // v0.9.175: 云拼音（默认关闭，接口协议见 cpp/cloud_pinyin.h）
    Option<bool> cloudPinyin{this, "CloudPinyin", _("Enable cloud pinyin"), false};
    Option<std::string> cloudEndpoint{
        this, "CloudEndpoint", _("Cloud pinyin endpoint (full URL)"), ""};
    Option<std::string> cloudApiKey{this, "CloudApiKey",
                                    _("Cloud pinyin API key (Bearer)"), ""};
    Option<int> cloudTimeoutMs{this, "CloudTimeoutMs",
                               _("Cloud pinyin timeout (ms)"), 1500};);

class PinyinIMEEngine;

class PinyinIMEState : public InputContextProperty {
public:
    PinyinIMEState(PinyinIMEEngine *engine, InputContext *inputContext);

    // 清空当前输入状态并清掉 UI。
    void reset();
    // 重建候选（更新 page_/maxPage_），并刷新 preedit + candidate UI。
    void refreshUI();
    // 选中第 idx（全局索引，已含翻页偏移）个候选；整句选完即上屏。
    void selectCandidate(size_t idx);
    // 翻页：dir=-1 上一页 / +1 下一页。
    void flipPage(int dir);

    PinyinIMEEngine *engine_;
    InputContext *inputContext_;
    libime::PinyinContext context_;
    size_t page_ = 0;
    size_t maxPage_ = 0;
    // v0.9.175 云候选：第 0 页尾部预留槽位数，及云候选在索引空间里的起点
    size_t cloudShift_ = 0;
    size_t cloudIdxBase_ = SIZE_MAX;

    // 本地候选在索引空间里的起点/本页容量（第 0 页少 cloudShift_ 个）
    size_t localBegin() const {
        const size_t ps = pageSize();
        return page_ == 0 ? 0 : (ps - cloudShift_) + (page_ - 1) * ps;
    }
    size_t localPageCapacity() const {
        const size_t ps = pageSize();
        return page_ == 0 ? ps - cloudShift_ : ps;
    }
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
    std::string rawInput_;  // 原始输入（未纠错），用于中英自动混输判断
    // 表情选择（Ctrl-E）：1-9 选表情，[,]/[.] 翻页，Esc/Ctrl-E 返回中文。
    bool emojiMode_ = false;
    // 双拼运行时开关（Ctrl+Shift+S 切换，初始值来自配置）
    bool shuangpinEnabled_ = false;
    size_t emojiPage_ = 0;
    int emojiCategory_ = 0;      // v0.9.161: 表情分类
    bool emojiSearchMode_ = false; // v0.9.161: 表情搜索
    std::string emojiSearchBuf_;   // v0.9.161: 搜索缓冲
    std::string skinName_;          // v0.9.162: 皮肤名称
    // v 模式（搜狗式符号输入）：输入 v 后跟数字/字母进入符号选择。
    bool vMode_ = false;
    std::string vModeSub_;    // v 后的子输入（如 "1"）
    size_t vModePage_ = 0;    // v 模式翻页
    // 误上屏撤回栈：(上屏文本, 拼音串)，最近在上；deactivate（失焦）时清空。
    std::vector<std::pair<std::string, std::string>> undoStack_;

    size_t pageSize() const;
    // v0.9.197：字符 trigram 重排后的「显示位置 → 原始候选索引」映射。
    // 空/未启用时保持 libime 原顺序；启用时 selectCandidate 必须先做一次映射。
    std::vector<size_t> displayToRaw_;
    bool rerankActive_ = false;
    // v0.9.175 云拼音：当前拼音串对应的云候选（只在第 0 页追加到本地候选之后）
    std::vector<std::string> cloudWords_;
    // 触发一次后台云查询（同一拼音在引擎层去重，不会重复发请求）
    void kickCloudQuery();
    // 上屏一段已提交文本（整句 / 英文整行 / 标点），清面板并 commitString。
    void commitText(const std::string &text);
    // 上屏中文标点：打拼音中先上屏首选，空闲直接上屏。
    void commitPunctuation(const std::string &fullwidth);
    // 误上屏撤回（Ctrl-Z）：通知应用删除刚上屏的文本、恢复拼音重新编辑。
    void undoLastCommit();

private:
    void updateUI();
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
    pinyin_ime::CharLM *charLM() { return charLM_.get(); }
    // v0.9.197：重排池大小与 libime 的 nbest 保持一致（至少 10 个整句候选）
    size_t rerankPool() const {
        return std::max<size_t>(10, config().nbest.value());
    }
    // 用户词库 / 自学习历史落盘（destructor 与 deactivate 时调用）。
    void saveUserData();

    auto factory() { return &factory_; }
    Instance *instance() { return instance_; }
    // v0.9.175 云拼音客户端（配置启用时非空；线程共享，查询在后台线程执行）
    std::shared_ptr<pinyinime::CloudPinyinClient> cloudClient() { return cloud_; }
    // 标记/查询某拼音是否已有后台请求在飞（线程安全）
    bool cloudMarkInFlight(const std::string &py);
    void cloudClearInFlight(const std::string &py);
    // v0.9.192：云候选回填后「投递到主循环」的 EventSource。
    // fcitx 的 addPostEvent 返回 unique_ptr，析构即注销事件；以前直接丢弃返回值，
    // 结果是回调永远不触发——云候选到了也不刷新候选窗。这里由 engine 持有，
    // 后台线程写入、主线程事件循环执行，故加锁。
    void keepCloudRefreshSource(std::unique_ptr<fcitx::EventSource> source);

private:
    bool ensureDataLoaded();

    Instance *instance_;
    std::mutex cloudSourceMutex_;
    std::unique_ptr<fcitx::EventSource> cloudRefreshSource_;
    PinyinIMEConfig config_;
    std::unique_ptr<libime::PinyinIME> ime_;
    std::unique_ptr<pinyin_ime::CharLM> charLM_;
    std::shared_ptr<pinyinime::CloudPinyinClient> cloud_;
    std::mutex cloudMutex_;
    std::unordered_set<std::string> cloudInFlight_;
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