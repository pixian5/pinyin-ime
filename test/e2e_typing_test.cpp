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
        config.setValueByPath("UserDataDir", getenv("PINYINIME_USER_DATA") ? getenv("PINYINIME_USER_DATA") : "/root/.local/share/pinyin-ime");
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
        // --- Extended test cases (v0.9.9) ---
        {"shurufa", "输入法", "technical term"},
        {"ruanjian", "软件", "common noun"},
        {"kaifazhe", "开发者", "common noun"},
        {"jisuanji", "计算机", "technical term"},
        {"wangluo", "网络", "common noun"},
        {"shujuku", "数据库", "technical term"},
        {"zaixian", "在线", "common phrase"},
        {"xitong", "系统", "common noun"},
        {"anquan", "安全", "common noun"},
        {"mima", "密码", "common noun"},
        {"denglu", "登录", "common phrase"},
        {"zhuce", "注册", "common phrase"},
        {"shoujihao", "手机号", "common phrase"},
        {"dianziyoujian", "电子邮件", "technical term"},
        {"wangzhan", "网站", "common noun"},
        {"yunduan", "云端", "technical term"},
        {"rengongzhineng", "人工智能", "technical term"},
        {"jiqixuexi", "机器学习", "technical term"},
        {"shenduxuexi", "深度学习", "technical term"},
        {"jiankangma", "健康码", "common phrase"},
        // --- More sentences ---
        {"woyaochifan", "我要吃饭", "sentence"},
        {"qinggeiwohuidianhua", "请给我回电话", "sentence"},
        {"mingtianjian", "明天见", "common phrase"},
        {"zhuyuandankuaile", "祝元旦快乐", "greeting"},
        // --- More fuzzy/correct ---
        {"zongguoren", "中国人", "fuzzy+correct: zong→zhong"},
        {"sengri", "生日", "correct: sengri→shengri"},
        {"zaoan", "早安", "greeting"},
        {"wanan", "晚安", "greeting"},
        // --- Edge cases: single syllables ---
        {"a", "啊", "single vowel"},
        {"o", "哦", "single vowel"},
        {"e", "恶", "single vowel"},
        {"ai", "爱", "single diphthong"},
        {"yi", "一", "single syllable"},
        {"wu", "无", "single syllable"},
        {"yu", "与", "single syllable"},
        {"yue", "月", "single syllable"},
        {"yuan", "元", "single syllable"},
        {"ying", "应", "single syllable"},
        {"yong", "用", "single syllable"},
        // --- Compound words ---
        {"aimei", "爱美", "compound"},
        {"aixin", "爱心", "compound"},
        {"wenhou", "问候", "compound"},
        {"wenhua", "文化", "compound"},
        {"kexue", "科学", "compound"},
        {"jiaoyu", "教育", "compound"},
        {"yiliao", "医疗", "compound"},
        {"zhengzhi", "政治", "compound"},
        // --- Common daily phrases ---
        {"shangban", "上班", "daily"},
        {"xiaban", "下班", "daily"},
        {"shuijiao", "睡觉", "daily"},
        {"xiuxi", "休息", "daily"},
        {"gouwu", "购物", "daily"},
        {"yundong", "运动", "daily"},
        // --- Longer sentences ---
        {"woxiangqubeijinglvyou", "我想去北京旅游", "long sentence"},
        {"mingtianyaoqichuang", "明天要起床", "sentence"},
        {"jintiantianqihenhao", "今天天气很好", "sentence"},
        // --- Tech terms extended ---
        {"yunjisuan", "云计算", "tech term"},
        {"dashuju", "大数据", "tech term"},
        {"wulianwang", "物联网", "tech term"},
        {"qukuailian", "区块链", "tech term"},
        // --- Disambiguation (xian→xi'an) ---
        // --- v0.9.26 扩展：城市名 ---
        {"shanghai", "上海", "city name"},
        {"guangzhou", "广州", "city name"},
        {"shenzhen", "深圳", "city name"},
        {"hangzhou", "杭州", "city name"},
        {"chengdu", "成都", "city name"},
        {"tianjin", "天津", "city name"},
        {"chongqing", "重庆", "city name"},
        {"wuhan", "武汉", "city name"},
        {"nanjing", "南京", "city name"},
        // --- v0.9.26 扩展：文化/体育 ---
        {"yinyue", "音乐", "culture"},
        {"dianying", "电影", "culture"},
        {"youxi", "游戏", "culture"},
        {"tiyu", "体育", "culture"},
        {"xinwen", "新闻", "culture"},
        // --- v0.9.26 扩展：科技/经济 ---
        {"keji", "科技", "tech term"},
        {"chuangxin", "创新", "tech term"},
        {"fazhan", "发展", "tech term"},
        {"jinrong", "金融", "economy"},
        {"shangye", "商业", "economy"},
        {"nongye", "农业", "economy"},
        {"gongye", "工业", "economy"},
        // --- v0.9.26 扩展：地标/文化 ---
        {"taishan", "泰山", "landmark"},
        {"changcheng", "长城", "landmark"},
        {"gugong", "故宫", "landmark"},
        {"zhonghua", "中华", "culture"},
        {"renmin", "人民", "common noun"},
        // --- v0.9.26 扩展：更多句子 ---
        {"woxiangqulvyou", "我想去旅游", "sentence"},
        {"woshixuesheng", "我是学生", "sentence"},
        {"zhongguorenmin", "中国人民", "phrase"},
        {"shurufashijian", "输入法时间", "phrase"},
        {"mubiao", "目标", "common noun"},
        {"hexin", "核心", "common noun"},

        {"xianbeijing", "西安北京", "disambiguation"},
        {"xianyou", "西安有", "disambiguation"},
        {"xianchengshi", "西安城市", "disambiguation"},
        // --- Disambiguation: should NOT split ---
        {"xiandai", "现代", "no-split"},
        {"xianshi", "显示", "no-split"},
        {"xianzai", "现在", "no-split"},

        // --- v0.9.28: Extended test cases ---
        // Common daily phrases
        {"chifanlema", "吃饭了吗", "daily"},
        {"xiexieni", "谢谢你", "daily"},
        {"duibuqi", "对不起", "daily"},
        {"meiguanxi", "没关系", "daily"},
        {"bukeqi", "不客气", "daily"},
        {"zaoshanghao", "早上好", "daily"},
        {"zenmeyang", "怎么样", "daily"},
        // Work/study
        {"kaishigongzuo", "开始工作", "work"},
        {"wanchengrenwu", "完成任务", "work"},
        {"gongzuobaogao", "工作报告", "work"},
        {"xuexijihua", "学习计划", "study"},
        {"xuexijindu", "学习进度", "study"},
        {"kaoshichengji", "考试成绩", "study"},
        // Tech terms
        {"ruanjiankaifa", "软件开发", "tech"},
        {"chengxusheji", "程序设计", "tech"},
        {"wangluoanquan", "网络安全", "tech"},
        {"shujukujishu", "数据库技术", "tech"},
        // Cities
        {"shanghaishi", "上海市", "city"},
        {"guangzhoushi", "广州市", "city"},
        {"shenzhenshi", "深圳市", "city"},
        // Sentences
        {"jintiantianqihenhao", "今天天气很好", "sentence"},
        {"woxiangqubeijinglvyou", "我想去北京旅游", "sentence"},
        // Disambiguation
        {"xiangqu", "想去", "disambiguation"},
        {"xiangchi", "想吃", "disambiguation"},
        {"xiangshuo", "想说", "disambiguation"},

        // --- v0.9.30: Expanded learning data tests ---
        // Idioms
        {"yixinyiyi", "一心一意", "idiom"},
        {"quanliyifu", "全力以赴", "idiom"},
        {"shishiqiushi", "实事求是", "idiom"},
        {"shunqiziran", "顺其自然", "idiom"},
        {"xunxujianjin", "循序渐进", "idiom"},
        // Questions
        {"shenmeshihou", "什么时候", "question"},
        {"weishenme", "为什么", "question"},
        {"zainali", "在哪里", "question"},
        {"shide", "是的", "affirm"},
        {"haode", "好的", "affirm"},
        // Time
        {"jintian", "今天", "time"},
        {"mingtian", "明天", "time"},
        {"zuotian", "昨天", "time"},
        {"xianzai", "现在", "time"},
        {"meitian", "每天", "time"},
        // Weather
        {"tianqiyubao", "天气预报", "weather"},
        {"xiayu", "下雨", "weather"},
        {"qingtian", "晴天", "weather"},
        {"yintian", "阴天", "weather"},
        // Food
        {"huoguo", "火锅", "food"},
        {"shaokao", "烧烤", "food"},
        {"miantiao", "面条", "food"},
        {"jiaozi", "饺子", "food"},
        {"waimai", "外卖", "food"},
        // Transport
        {"gongjiaoche", "公交车", "transport"},
        {"ditie", "地铁", "transport"},
        {"gaotie", "高铁", "transport"},
        {"feiji", "飞机", "transport"},
        // Emotions
        {"kaixin", "开心", "emotion"},
        {"nanguo", "难过", "emotion"},
        {"shengqi", "生气", "emotion"},
        {"xihuan", "喜欢", "emotion"},
        // Sentences
        {"woxiangshuo", "我想说", "sentence"},
        {"wojuede", "我觉得", "sentence"},
        {"woxiwang", "我希望", "sentence"},
        {"woxihuan", "我喜欢", "sentence"},
        {"wozhidao", "我知道", "sentence"},
        {"wobuzhidao", "我不知道", "sentence"},
        // Tech
        {"jiqixuexi", "机器学习", "tech"},
        {"shenduxuexi", "深度学习", "tech"},
        {"dashuju", "大数据", "tech"},
        {"qukuailian", "区块链", "tech"},
        {"qianduan", "前端", "tech"},
        {"houduan", "后端", "tech"},
        // Shopping
        {"maidongxi", "买东西", "shopping"},
        {"wanggou", "网购", "shopping"},
        {"kuaidi", "快递", "shopping"},
        // Health
        {"yiyuan", "医院", "health"},
        {"yisheng", "医生", "health"},
        // Education
        {"xuexiao", "学校", "education"},
        {"kaoshi", "考试", "education"},
        {"biye", "毕业", "education"},
        // Finance
        {"yinhang", "银行", "finance"},
        {"zhifu", "支付", "finance"},
        {"zhuanzhang", "转账", "finance"},
        // v0.9.53: 扩展测试 — 长句消歧
        {"woshizhongguoren", "我是中国人", "identity"},
        {"woxihuanbiancheng", "我喜欢编程", "hobby"},
        {"woxianzaihenmang", "我现在很忙", "status"},
        {"wozhidaole", "我知道了", "comprehension"},
        {"wobuzhidao", "我不知道", "comprehension"},
        {"woyaohuijia", "我要回家", "intent"},
        {"woyaogongzuo", "我要工作", "intent"},
        {"woyouyigewenti", "我有一个问题", "question"},
        {"zhongguoshiyigedaguo", "中国是一个大国", "statement"},
        {"beijingshishoudu", "北京是首都", "geography"},
        {"woxihuanzhongguocai", "我喜欢中国菜", "preference"},
        {"mingtianyoukongma", "明天有空吗", "question"},
        {"woyaoshangxue", "我要上学", "intent"},
        {"woyoulianggehaizi", "我有两个孩子", "statement"},
        {"dongdeqi", "懂得起", "v0.9.53 boost"},
        // v0.9.53: 扩展测试 — 前缀 boost
        {"woxihuanzhege", "我喜欢这个", "prefix boost"},
        {"womendouzhidao", "我们都知道", "prefix boost"},
        // v0.9.53: 扩展测试 — 日常对话
        {"qingbangzhuwo", "请帮助我", "request"},
        {"xiexienidebangzhu", "谢谢你的帮助", "gratitude"},
        {"woxuyaonidebangzhu", "我需要你的帮助", "request"},
        // v0.9.53: 扩展测试 — 复合句
        {"woxiangqubeijing", "我想去北京", "travel"},
        {"woxihuanzhongguo", "我喜欢中国", "preference"},
        {"tamenxihuanzhongguo", "他们喜欢中国", "preference"},
        {"zhongguorenmin", "中国人民", "entity"},
        {"wozaibeijing", "我在北京", "location"},
        // v0.9.53: 扩展测试 — 时间表达
        {"jintiantianqihenhao", "今天天气很好", "weather"},
        {"xianzaijidian", "现在几点", "time"},
        // v0.9.53: 扩展测试 — 情感
        {"woaini", "我爱你", "emotion"},
        {"wohenkaixin", "我很开心", "emotion"},
        {"wohenshangxin", "我很伤心", "emotion"},
        // v0.9.56: 扩展用例 (248→310+)
        // 办公/工作
        {"woyaokaihui", "我要开会", "work"},
        {"jintianyouhuiyi", "今天有会议", "work"},
        {"qingfasongyoujian", "请发送邮件", "work"},
        {"woyaobaoxiao", "我要报销", "work"},
        {"xiaban", "下班", "work"},
        {"shangban", "上班", "work"},
        {"jiaqitian", "假期天", "work"},
        {"qingjiabuxing", "请假不行", "work"},
        // 教育
        {"woyaoshangxue", "我要上学", "education"},
        {"zuoyewancheng", "作业完成", "education"},
        {"kaoshidefen", "考试得分", "education"},
        {"laoshibuzhi", "老师布置", "education"},
        {"xueshengxuexi", "学生学习", "education"},
        {"ketangbi", "课堂比", "education"},
        // 技术
        {"ruanjiankaifa", "软件开发", "tech"},
        {"chengxuyuan", "程序员", "tech"},
        {"jisuanji", "计算机", "tech"},
        {"wangluolianjie", "网络连接", "tech"},
        {"shujukufuwu", "数据库服务", "tech"},
        {"yunduan", "云端", "tech"},
        {"rengongzhineng", "人工智能", "tech"},
        // 健康
        {"shentibuhao", "身体不好", "health"},
        {"duochishuiguo", "多吃水果", "health"},
        {"shaoyan", "瘙痒", "health"},
        {"duanlian", "锻炼", "health"},
        {"zaoqi", "早起", "health"},
        // 购物
        {"woyaomaidongxi", "我要买东西", "shopping"},
        {"qingjiezhang", "清洁章", "shopping"},
        {"duoshaoqian", "多少钱", "shopping"},
        {"biyixia", "比一下", "shopping"},
        // 交流
        {"qingdianhua", "请电话", "communication"},
        {"wozhao", "我找", "communication"},
        {"qingdengyixia", "请等一下", "communication"},
        {"meitingqing", "没听清", "communication"},
        {"zaishuoyibian", "再说一遍", "communication"},
        // 日常生活
        {"qichuang", "起床", "daily"},
        {"xiyifu", "洗衣服", "daily"},
        {"zuofan", "做饭", "daily"},
        {"dasao", "打扫", "daily"},
        {"guangjie", "逛街", "daily"},
        {"zhumian", "助眠", "daily"},
        // 天气/季节
        {"xiayu", "下雨", "weather"},
        {"xue", "学", "weather"},
        {"qingtian", "晴天", "weather"},
        {"yin", "因", "weather"},
        // 时间/日期
        {"zuotian", "昨天", "time"},
        {"mingtian", "明天", "time"},
        {"houtian", "后天", "time"},
        {"jintian", "今天", "time"},
        {"xingqiyi", "星期一", "time"},
        {"xingqiliu", "星期六", "time"},
        {"zhoumo", "周末", "time"},
        // 人物关系
        {"baba", "爸爸", "family"},
        {"mama", "妈妈", "family"},
        {"gege", "哥哥", "family"},
        {"jiejie", "姐姐", "family"},
        {"didi", "弟弟", "family"},
        {"meimei", "妹妹", "family"},
        {"yeye", "爷爷", "family"},
        {"nainai", "奶奶", "family"},
        // 更多消歧
        {"zhandiqilai", "站起来", "boost"},
        {"qianmian", "前面", "boost"},
        {"dongdeqilai", "懂得起来", "boost"},
        // 更多长句
        {"woxianzaiyaochifanle", "我现在要吃饭了", "long"},
        {"tamenxianzaihenmang", "他们现在很忙", "long"},
        {"womingtianyaoshangban", "我明天要上班", "long"},
        {"zhongguoshiyigeweida", "中国是一个伟大", "long"},
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
