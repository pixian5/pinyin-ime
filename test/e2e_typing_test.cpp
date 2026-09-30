/*
 * pinyin-ime fcitx5 End-to-End Typing Test (Extended)
 * Full test suite: basic words, long sentences, fuzzy pinyin, error correction
 */
#include <fcitx/instance.h>
#include <fcitx/addonmanager.h>
#include <fcitx/inputcontextmanager.h>
#include <fcitx/inputmethodmanager.h>
#include <fcitx/inputcontext.h>
#include <fcitx/inputpanel.h>
#include <fcitx/candidatelist.h>
#include <fcitx-utils/key.h>
#include <fcitx-utils/log.h>
#include <fcitx-utils/eventdispatcher.h>
#include <fcitx/event.h>
#include <fcitx/addoninstance.h>
#include <fcitx/inputmethodentry.h>
#include <fcitx-config/rawconfig.h>
#include "testfrontend_public.h"

// 注册 keyboard 静态 addon（fcitx5 server 内置，需手动导入）
#include <fcitx/addonfactory.h>
FCITX_DEFINE_STATIC_ADDON_REGISTRY(getStaticAddon)
FCITX_IMPORT_ADDON_FACTORY(getStaticAddon, keyboard);

#include <iostream>
#include <string>
#include <vector>
#include <chrono>
#include <thread>

using namespace fcitx;

static Instance *g_instance = nullptr;

struct TestResult {
    std::string input;
    std::string preedit;
    std::vector<std::string> candidates;
    std::string committed;
    bool pass;
};

void runTests(EventDispatcher *dispatcher) {
    auto &addonMgr = g_instance->addonManager();
    auto testFrontend = addonMgr.addon("testfrontend", true);
    if (!testFrontend) {
        std::cerr << "FAIL: testfrontend not loaded" << std::endl;
        dispatcher->schedule([dispatcher]() { dispatcher->detach(); g_instance->exit(); });
        return;
    }

    // 通过 setConfig API 直接设置 addon 配置（绕过配置文件路径问题）
    auto *pinyinimeAddon = addonMgr.addon("pinyinime", true);
    if (pinyinimeAddon) {
        RawConfig config;
        config.setValueByPath("EnableFuzzy", "True");
        config.setValueByPath("EnableCorrect", "True");
        config.setValueByPath("EnableShuangpin", "False");
        config.setValueByPath("NBest", "9");
        config.setValueByPath("DataDir", "/tmp/libime_data");
        config.setValueByPath("PageSize", "9");
        pinyinimeAddon->setConfig(config);
        std::cout << "[setup] pinyinime config set via setConfig API" << std::endl;
    }

    auto uuid = testFrontend->call<ITestFrontend::createInputContext>("testapp");
    auto *ic = g_instance->inputContextManager().findByUUID(uuid);
    if (!ic) {
        std::cerr << "FAIL: cannot create IC" << std::endl;
        dispatcher->schedule([dispatcher]() { dispatcher->detach(); g_instance->exit(); });
        return;
    }

    // Add pinyinime to the current input method group (required for setCurrentInputMethod)
    auto &imManager = g_instance->inputMethodManager();
    auto group = imManager.currentGroup();
    group.inputMethodList().emplace_back("pinyinime");
    imManager.setGroup(std::move(group));
    g_instance->setCurrentInputMethod(ic, "pinyinime", false);
    std::this_thread::sleep_for(std::chrono::milliseconds(100));

    std::cout << "\n========== pinyin-ime E2E Test Suite ==========\n" << std::endl;

    // Test cases: input → expected first candidate
    struct TestCase { std::string input; std::string expected; std::string desc; };
    std::vector<TestCase> tests = {
        // --- Basic words ---
        {"nihao", "你好", "basic greeting"},
        {"shijie", "世界", "basic word"},
        {"zhongguo", "中国", "country name"},
        {"beijing", "北京", "city name"},
        {"xuesheng", "学生", "common noun"},
        {"laoshi", "老师", "common noun"},
        {"xiexie", "谢谢", "common phrase"},
        {"zaijian", "再见", "common phrase"},
        // --- Longer phrases ---
        {"beijingdaxue", "北京大学", "university name"},
        {"qinghuadaxue", "清华大学", "university name"},
        {"zhongguoren", "中国人", "compound"},
        {"woshizhongguoren", "我是中国人", "sentence"},
        {"nihaozhongguo", "你好中国", "phrase combo"},
        {"woaixuexi", "我爱学习", "sentence"},
        {"jintiandetianqi", "今天的天气", "longer phrase"},
        // --- Common sentences ---
        {"qingwenyixiaruzhushenmefangjian", "请问一下入住什么房间", "long sentence"},
        {"woxiangdingyigefangjian", "我想订一个房间", "sentence"},
        {"qingbangwochaxunyixia", "请帮我查询一下", "sentence"},
        // --- Fuzzy pinyin (平翘舌/前后鼻音/n-l) ---
        {"cifan", "吃饭", "fuzzy: ci→chi"},
        {"zeli", "这里", "fuzzy: ze→zhe"},
        {"lanfang", "南方", "fuzzy: l→n"},
        {"senme", "什么", "fuzzy+correct: senme→shenme→什么"},
        {"zongguo", "中国", "fuzzy+correct: zong→zhong"},
        // --- Error correction (M3 model) ---
        {"shengri", "生日", "correct: shengri"},
        // --- Simplified pinyin ---
        {"bjdx", "北京大学", "simplified: bj→beijing, dx→daxue"},
        {"zg", "中国", "simplified: zg→zhongguo"},
        // --- Additional common phrases ---
        {"woaini", "我爱你", "common phrase"},
        {"mingbai", "明白", "common phrase"},
        {"keyi", "可以", "common phrase"},
        {"meiyou", "没有", "common phrase"},
        {"zhidao", "知道", "common phrase"},
        {"xihuan", "喜欢", "common phrase"},
        {"pengyou", "朋友", "common phrase"},
        {"gongzuo", "工作", "common phrase"},
        // --- Additional sentences ---
        {"woxiangchi", "我想吃", "sentence"},
        {"nihaoa", "你好啊", "sentence with particle"},
        {"jintiannahenkaixin", "今天那很开心", "longer sentence"},
        {"qingwenyixia", "请问一下", "polite phrase"},
        // --- Fuzzy additional ---
        {"sijiao", "视角", "fuzzy: si→shi"},
        {"zeliangge", "这两个", "fuzzy: ze→zhe"},
    };

    int pass = 0, fail = 0;
    std::vector<TestResult> results;

    for (const auto &tc : tests) {
        // Type input
        for (char c : tc.input) {
            testFrontend->call<ITestFrontend::keyEvent>(uuid, Key(std::string(1, c)), false);
        }
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        // Capture state
        TestResult r;
        r.input = tc.input;
        r.preedit = ic->inputPanel().preedit().toString();

        auto candidates = ic->inputPanel().candidateList();
        if (candidates && candidates->size() > 0) {
            for (int i = 0; i < std::min(5, (int)candidates->size()); i++) {
                r.candidates.push_back(candidates->candidate(i).text().toString());
            }
            r.committed = r.candidates[0];
        }

        // Press space to commit
        testFrontend->call<ITestFrontend::keyEvent>(uuid, Key(FcitxKey_space), false);
        std::this_thread::sleep_for(std::chrono::milliseconds(30));

        r.pass = (r.committed == tc.expected);
        if (r.pass) pass++; else fail++;

        // Print result
        std::cout << "[" << (r.pass ? "PASS" : "FAIL") << "] " << tc.desc
                  << ": " << tc.input << " → " << r.committed;
        if (!r.pass) {
            std::cout << " (expected: " << tc.expected << ")";
            if (!r.candidates.empty()) {
                std::cout << "\n  Candidates: ";
                for (size_t i = 0; i < r.candidates.size(); i++) {
                    if (i > 0) std::cout << ", ";
                    std::cout << "[" << i << "]" << r.candidates[i];
                }
            }
        }
        std::cout << std::endl;

        results.push_back(r);
        ic->reset();
    }

    // Summary
    std::cout << "\n========== Summary ==========" << std::endl;
    std::cout << "Total: " << tests.size() << " | Pass: " << pass << " | Fail: " << fail << std::endl;
    std::cout << "Accuracy: " << (100.0 * pass / tests.size()) << "%" << std::endl;

    // List failures
    if (fail > 0) {
        std::cout << "\nFailed cases:" << std::endl;
        for (size_t i = 0; i < results.size(); i++) {
            if (!results[i].pass) {
                std::cout << "  " << results[i].input << " → " << results[i].committed
                          << " (expected: " << tests[i].expected << ")" << std::endl;
            }
        }
    }

    testFrontend->call<ITestFrontend::destroyInputContext>(uuid);
    dispatcher->schedule([dispatcher]() { dispatcher->detach(); g_instance->exit(); });
}

int main() {
    std::cout << "=== pinyin-ime E2E Extended Test ===" << std::endl;

    // Clean user history to avoid prior test runs affecting candidate ranking
    std::filesystem::remove_all("/tmp/fcitx5-test-home/.local/share/pinyin-ime");

    char arg0[] = "fcitx5-test";
    char arg1[] = "--disable=all";
    char arg2[] = "--enable=testfrontend,pinyinime,keyboard,spell,quickphrase,notifications,clipboard,unicode";
    char *argv[] = {arg0, arg1, arg2};

    try {
        Instance instance(3, argv);
        instance.addonManager().registerDefaultLoader(&getStaticAddon());
        g_instance = &instance;

        EventDispatcher dispatcher;
        dispatcher.attach(&instance.eventLoop());
        dispatcher.schedule([&dispatcher]() { runTests(&dispatcher); });

        instance.exec();
    } catch (const InstanceQuietQuit &) {
    } catch (const std::exception &e) {
        std::cerr << "Exception: " << e.what() << std::endl;
        return 1;
    }
    return 0;
}
