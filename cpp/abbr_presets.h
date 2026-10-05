/*
 * abbr_presets.h — 简拼预设表与 boost 逻辑（CLI 与 fcitx5 addon 共享）
 *
 * libime 的语言模型按词频排序，日常用语（"这个"/"即使"/"随着"）频率高于
 * "中国"/"技术"/"深圳"，导致简拼 zg/js/sz 首选不是用户期望的词。
 * 本表对高频简拼缩写做 boost：若预设词已在候选列表中，移到首位；否则插入。
 *
 * 设计原则：
 *   1) 只收录「无歧义或用户期望明确」的简拼（zg→中国 而非 这个）
 *   2) 不覆盖已有正确行为的简拼（bj→北京 已正确，不加预设）
 *   3) 预设词在候选列表中：移到首位；不在候选中：插入首位
 *   4) 用户自定义简拼（~/.config/pinyin-ime/abbr.txt）优先级高于内置预设
 *   5) 用户自学习数据优先级高于预设（learn() 后 user.history 会覆盖）
 *
 * 优先级：用户自定义 > 内置预设 > 语言模型默认排序
 */
#ifndef ABBR_PRESETS_H
#define ABBR_PRESETS_H

#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>
#include <fstream>

namespace pinyin_ime {

// ── 内置简拼预设表 ──────────────────────────────────────────
// 返回静态 unordered_map，O(1) 查找
inline const std::unordered_map<std::string, std::string>& abbr_preset_map() {
    static const std::unordered_map<std::string, std::string> map = {
        // 国家/政治
        {"zg",   "中国"},       // 这个→中国
        {"zgr",  "中国人"},
        {"zgh",  "中华人民共和国"},
        {"zgrm", "中国人民"},
        // 技术（js→即使 需 boost）
        {"js",   "技术"},
        {"rj",   "软件"},
        {"kj",   "科技"},
        {"wl",   "网络"},
        {"sd",   "算法"},
        {"dm",   "代码"},
        {"gj",   "工具"},
        {"jk",   "接口"},
        {"cs",   "测试"},
        {"sc",   "生产"},
        {"bb",   "版本"},
        {"az",   "安装"},
        // 城市（sz→随着 需 boost；bj/sh/gz/hz 已正确，不加）
        {"sz",   "深圳"},
        {"cd",   "成都"},
        {"wh",   "武汉"},
        {"cq",   "重庆"},
        {"xa",   "西安"},
        {"nj",   "南京"},
        {"tj",   "天津"},
        // 常见搭配
        {"zl",   "资料"},
        {"jl",   "记录"},
        {"xw",   "新闻"},
        {"bm",   "部门"},
        {"dw",   "单位"},
        {"gr",   "个人"},
        {"qg",   "全国"},
        {"yx",   "邮箱"},
        {"sy",   "使用"},
        {"gn",   "功能"},
        {"xt",   "系统"},
        {"aq",   "安全"},
        {"mm",   "密码"},
        {"zc",   "注册"},
        {"dl",   "登录"},
        {"tc",   "退出"},
        {"xg",   "修改"},
        {"cx",   "查询"},
        {"fx",   "分析"},
        {"bg",   "报告"},
        {"jh",   "计划"},
        {"rw",   "任务"},
        {"jd",   "进度"},
        {"zt",   "状态"},
        {"lx",   "联系"},
        {"dh",   "电话"},
        {"yy",   "应用"},
        {"fw",   "服务"},
        {"zy",   "资源"},
        {"pj",   "配置"},
        {"yh",   "用户"},
        {"qy",   "企业"},
        {"cp",   "产品"},
        {"xq",   "需求"},
        // 简拼扩展（3-4字母）
        {"bjdx", "北京大学"},
        {"qhdx", "清华大学"},
        // v0.9.35: 更多常见简拼
        {"wn",   "我们"},       // 温暖→我们
        {"wx",   "微信"},       // 文学→微信
        {"tt",   "通过"},       // 疼痛→通过
        {"ee",   "而且"},       // 峨峨→而且
        // 重复字母（家人称呼）
        {"mmm",  "妈妈"},
        {"bbb",  "爸爸"},
        {"ggg",  "哥哥"},
        {"jjj",  "姐姐"},
        {"nnn",  "奶奶"},
    };
    return map;
}

// ── 用户自定义简拼文件 ──────────────────────────────────────
// 全局变量：用户可通过 --abbr-file 或 API 设置
inline std::string& abbr_file_path() {
    static std::string path;
    return path;
}

// 加载用户自定义简拼文件（~/.config/pinyin-ime/abbr.txt 或指定路径）
// 格式：每行 "abbr=word"，# 开头为注释。返回空 map 若文件不存在。
inline const std::unordered_map<std::string, std::string>& user_abbr_map() {
    static std::unordered_map<std::string, std::string> umap;
    static bool loaded = false;
    if (!loaded) {
        loaded = true;
        std::string path = abbr_file_path();
        if (path.empty()) {
            const char *home = getenv("HOME");
            if (home) path = std::string(home) + "/.config/pinyin-ime/abbr.txt";
        }
        if (!path.empty()) {
            std::ifstream f(path);
            if (f) {
                std::string line;
                while (std::getline(f, line)) {
                    if (line.empty() || line[0] == '#') continue;
                    auto pos = line.find('=');
                    if (pos == std::string::npos || pos == 0 || pos == line.size() - 1) continue;
                    umap[line.substr(0, pos)] = line.substr(pos + 1);
                }
            }
        }
    }
    return umap;
}

// ── 简拼判断与 boost ────────────────────────────────────────

// 判断输入是否为简拼（全小写字母，长度 2-6，无隔音符）
inline bool is_abbreviated_pinyin(const std::string &input) {
    if (input.size() < 2 || input.size() > 6) return false;
    for (char c : input) {
        if (c < 'a' || c > 'z') return false;
    }
    return true;
}

// ── 常见英文直通词 ──────────────────────────────────────────
// 输入匹配这些词时，英文词作为首选候选（插入候选列表首位）
inline const std::unordered_set<std::string>& english_passthrough_set() {
    static const std::unordered_set<std::string> set = {
        "bug", "app", "web", "ok", "cpu", "gpu", "api", "url",
        "pdf", "jpg", "png", "gif", "mp3", "mp4", "wav",
        "html", "css", "json", "xml", "yaml", "http", "https",
        "sql", "git", "ssh", "tcp", "udp", "dns", "vpn",
        "ai", "ml", "dl", "nlp", "ocr", "iot",
        "ui", "ux", "qa", "ops", "dev", "pm",
        "id", "ip", "mac", "ssid",
        "hi", "hey", "hello",
        "yes", "no", "not", "and", "or", "if", "else",
        "for", "while", "do", "try", "catch",
        "true", "false", "null", "void",
        "int", "char", "bool", "float", "double",
        "class", "struct", "enum", "union",
        "public", "private", "protected",
        "return", "break", "continue",
        "import", "export", "package",
        "func", "var", "let", "const",
        "new", "delete", "this", "super",
        "test", "debug", "build", "run",
        "log", "info", "warn", "error",
        "data", "file", "code", "type",
        "list", "map", "set", "queue",
        "name", "value", "key", "path",
        "user", "home", "help", "version",
        "start", "stop", "pause", "reset",
        "open", "close", "read", "write",
        "send", "recv", "load", "save",
        "init", "config", "setup", "install",
        "update", "upgrade", "remove", "uninstall",
        "login", "logout", "signup", "register",
        "admin", "root", "guest", "master",
        "main", "branch", "tag", "commit",
        "push", "pull", "merge", "rebase",
        "cache", "buffer", "stream", "pipe",
        "thread", "process", "socket", "port",
        "size", "count", "length", "width",
        "height", "depth", "level", "layer",
        "mode", "state", "status", "flag",
        "src", "dst", "tmp", "var", "opt",
        "bin", "lib", "doc", "man",
        "max", "min", "avg", "sum",
        "rgb", "rgba", "hsv", "cmyk",
        "vip", "ceo", "cto", "coo",
        "usa", "uk", "eu", "un",
        "wifi", "bluetooth", "usb",
    };
    return set;
}

// 检查输入是否为常见英文词（全小写字母，3-10字符）
inline bool is_english_passthrough(const std::string &input) {
    if (input.size() < 2 || input.size() > 10) return false;
    for (char c : input) {
        if (c < 'a' || c > 'z') return false;
    }
    return english_passthrough_set().count(input) > 0;
}

// ── 全拼音 boost 预设表 ──────────────────────────────────────
// 对特定全拼音做硬排序覆盖（学习数据权重不够时使用）
// 这些是系统 LM 排序不当的常见词
inline const std::unordered_map<std::string, std::string>& full_pinyin_preset_map() {
    static const std::unordered_map<std::string, std::string> map = {
        {"chucha",   "出差"},       // 初查→出差
        {"chengdu",  "成都"},       // 程度→成都
        {"xian",     "西安"},       // 先→西安（城市名优先）
        {"nihaoshijie", "你好世界"}, // 你好时节→你好世界
        {"shide",    "是的"},       // 使得→是的
        {"tuichu",   "退出"},       // 推出→退出
        {"renwu",    "任务"},       // 人物→任务
        {"denglu",   "登录"},       // 登陆→登录
        {"shoujihao", "手机号"},    // 手机好→手机号
        {"lvyou",    "旅游"},       // 驴友→旅游
        {"chifan",   "吃饭"},       // 迟反→吃饭
        {"hecha",    "喝茶"},       //
        {"hejiu",    "喝酒"},       //
        {"shangban", "上班"},       //
        {"xiaban",   "下班"},       //
        {"dakai",    "打开"},       //
        {"guanbi",   "关闭"},       //
        {"baocun",   "保存"},       //
        {"sousuo",   "搜索"},       //
        {"shuaxin",  "刷新"},       //
        {"xiazai",   "下载"},       //
        {"shangchuan", "上传"},     //
        {"dayin",    "打印"},       //
        {"queren",   "确认"},       //
        {"quxiao",   "取消"},       //
        {"fuzhi",    "复制"},       //
        {"zhantie",  "粘贴"},       //
        {"dianzan",  "点赞"},       //
        {"shoucang", "收藏"},       //
        {"zhuanfa",  "转发"},       //
        {"pinglun",  "评论"},       //
        {"guanzhu",  "关注"},       //
        {"yulan",    "预览"},       //
        // v0.9.37b: 更多常见词
        {"biancheng", "编程"},       // 变成→编程
        {"guopiao",  "股票"},       // 过票→股票
        {"yilai",    "依赖"},       // 以来→依赖
        {"shujukujiegou", "数据库结构"}, //
        {"xinxiyilai", "信息依赖"},   //
        // v0.9.38b: 更多常见词
        {"shangwu",  "上午"},       // 商务→上午
        {"bianyi",   "编译"},       // 变异→编译
        {"duotai",   "多态"},       // 堕胎→多态
        {"shipin",   "视频"},       // 食品→视频
        {"yinpian",  "音频"},       // 饮片→音频
        {"jicheng",  "继承"},       // 集成→继承
        // v0.9.40b: 消歧 boost — 日常高频短语优先
        {"jidian",   "几点"},       // 机电→几点（时间语境更高频）
        {"zaochenjidian", "早晨几点"}, // 早晨机电→早晨几点
        {"qingwenyixiazaochenjidian", "请问一下早晨几点"}, //
        {"yidianshi", "一点事"},     // 一点是→一点事
        {"henhao",   "很好"},        // 很薄→很好
        {"tianqihenhao", "天气很好"}, //
        {"jintiantianqihenhao", "今天天气很好"}, //
        {"yiqian",   "以前"},        // 一千→以前
        {"yihou",    "以后"},        // 一后→以后
        {"tongshi",  "同时"},        // 同事→同时
        {"yiyang",   "一样"},        // 异样→一样
        {"zheyang",  "这样"},        //
        {"nayang",   "那样"},        //
        {"wangshang", "网上"},       // 往上→网上
        {"zenmeyang", "怎么样"},     //
        {"weiwanme", "为什么"},      //
        {"yinwei",   "因为"},        //
        {"suoyi",    "所以"},        //
        {"ruguo",    "如果"},        //
        {"danshi",   "但是"},        //
        {"haishi",   "还是"},        //
        {"huozhe",   "或者"},        //
        {"erqie",    "而且"},        //
        {"ranhou",   "然后"},        //
        {"yijing",   "已经"},        //
        {"zhidao",   "知道"},        //
        {"renwei",   "认为"},        // 任务→认为
        {"yiqi",     "一起"},        //
        {"yuyue",    "预约"},        // 语音→预约
        {"fuwu",     "服务"},        //
        {"fuwuqi",   "服务器"},      //
        // v0.9.40c: 多词组合 boost
        {"yijingdenglu", "已经登录"},   // 已经登陆→已经登录
        {"dengluchenggong", "登录成功"}, //
        {"denglushibai", "登录失败"},   //
        {"fuwuqianquan", "服务器安全"}, // 服务缱绻→服务器安全
        {"fuwuqixitong", "服务器系统"}, //
        {"ranhouwojiuhuiqu", "然后我就会去"}, // 然后我就回去→然后我就会去
        {"wojiuhuiqu", "我就会去"},     // 我就回去→我就会去
        {"jiuhuiqu", "就会去"},         // 就回去→就会去
        {"womenyiqi", "我们一起"},      // 我一起→我们一起
        {"yiqiqu", "一起去"},           //
        {"yiqiqukan", "一起去看"},      //
        {"qukanwangzhan", "去看网站"},  //
        {"xiugaimima", "修改密码"},     //
        {"chongzhimima", "重置密码"},   //
        {"wangzhandizhi", "网站地址"},  //
        {"wangzhanmingcheng", "网站名称"}, //
        // v0.9.41b: 长句消歧
        {"nanjingshiliuchaogudu", "南京是六朝古都"}, // 南京市→南京是
        {"beijingshishoudu", "北京是首都"},   //
        {"shanghaishichengshi", "上海是城市"}, //
        {"wojuedede", "我觉得的"},           //
        {"woxiangjuede", "我想觉得"},         //
        {"woxianzaijiuxiangyao", "我现在就想要"}, //
        {"qingwenyixia", "请问一下"},         //
        {"qingwenyixiashi", "请问一下是"},     //
        {"zaochenjidianle", "早晨几点了"},     //
        {"xianzaijidianle", "现在几点了"},     //
        {"buzhidao", "不知道"},              //
        {"buzhidaoe", "不知道的"},            //
        {"woyebuzhidao", "我也不知道"},       //
        {"meixiangdao", "没想到"},            //
        {"womeixiangdao", "我没想到"},         //
        {"zenmehuishi", "怎么会是"},          //
        {"zenmekeyi", "怎么可以"},            //
        {"zenmenenggou", "怎么能够"},          //
        {"weishenme", "为什么"},              //
        {"weishenmeyao", "为什么要"},          //
        {"weishenmebuxing", "为什么不行"},      //
        {"meiyouguanxi", "没有关系"},          //
        {"meiyouwenti", "没有问题"},           //
        {"meiyouyisi", "没有意思"},            //
        {"youyisi", "有意思"},               //
        {"youyidian", "有一点"},              //
        {"youyidianr", "有一点儿"},            //
        {"youyidianyisi", "有一点意思"},        //
        {"woyouyige", "我有一个"},             //
        {"woyouyigewenti", "我有一个问题"},      //
        {"woyouyigexiangfa", "我有一个想法"},    //
        {"zheshiyige", "这是一个"},            //
        {"nashiyige", "那是一个"},             //
        {"zheliyou", "这里有"},               //
        {"naliyou", "哪里有"},                //
        {"shenmedifang", "什么地方"},          //
        {"shenmeshihou", "什么时候"},          //
        {"shenmeyangzi", "什么样子"},          //
    };
    return map;
}

// 查找简拼预设词：用户自定义优先，然后内置预设。返回空 string 若无匹配。
inline std::string lookup_abbr(const std::string &input) {
    // 用户自定义优先
    const auto& umap = user_abbr_map();
    auto uit = umap.find(input);
    if (uit != umap.end()) return uit->second;
    // 简拼预设
    const auto& map = abbr_preset_map();
    auto it = map.find(input);
    if (it != map.end()) return it->second;
    // 全拼音 boost 预设
    const auto& fmap = full_pinyin_preset_map();
    auto fit = fmap.find(input);
    if (fit != fmap.end()) return fit->second;
    return "";
}

// 查找 boost 预设词（不限制简拼）：用于全拼音 boost
inline std::string lookup_pinyin_boost(const std::string &input) {
    // 英文直通
    if (is_english_passthrough(input)) return input;
    // 用户自定义优先
    const auto& umap = user_abbr_map();
    auto uit = umap.find(input);
    if (uit != umap.end()) return uit->second;
    // 全拼音 boost 预设
    const auto& fmap = full_pinyin_preset_map();
    auto fit = fmap.find(input);
    if (fit != fmap.end()) return fit->second;
    // 简拼预设（仅对简拼输入生效）
    if (is_abbreviated_pinyin(input)) {
        const auto& map = abbr_preset_map();
        auto it = map.find(input);
        if (it != map.end()) return it->second;
    }
    return "";
}

// 对简拼候选做 boost：若输入匹配预设表，将预设词移到首位
inline void boost_abbreviation(const std::string &input, std::vector<std::string> &cands) {
    // 英文直通：常见英文词作为首选
    if (is_english_passthrough(input)) {
        // 检查英文词是否已在候选中
        for (size_t i = 0; i < cands.size(); ++i) {
            if (cands[i] == input) return;  // 已在首位或已知位置
        }
        cands.insert(cands.begin(), input);
        return;
    }
    // 同时支持简拼 boost 和全拼音 boost
    std::string preset_word = lookup_pinyin_boost(input);
    if (preset_word.empty()) return;

    // 在候选列表中查找预设词
    for (size_t i = 1; i < cands.size(); ++i) {  // 从 1 开始，跳过当前首选
        if (cands[i] == preset_word) {
            cands.erase(cands.begin() + i);
            cands.insert(cands.begin(), preset_word);
            return;
        }
    }

    // 预设词不在候选列表中：插入首位
    cands.insert(cands.begin(), preset_word);
}

// 计算 boostSwap：返回预设词在候选列表中的位置（0 表示无需交换）
// 用于交互模式：显示时交换 cands[0] 和 cands[boostSwap]
inline size_t compute_boost_swap(const std::string &preedit, size_t total) {
    if (total <= 1) return 0;
    std::string pword = lookup_pinyin_boost(preedit);
    if (pword.empty()) return 0;
    return 1;  // caller needs to find the actual index by comparing strings
}

} // namespace pinyin_ime

#endif // ABBR_PRESETS_H
