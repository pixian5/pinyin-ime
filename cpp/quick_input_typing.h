// 输入过程辅助模块 —— quick_input.h 的片段（v0.9.178 拆出）。
// 注意：本文件不是独立头文件，必须在 quick_input.h 的 namespace pinyin_ime 内 include；
// 拆分只为缩小 quick_input.h 体积、明确模块归属，不改变任何调用方代码。

// ── 拼音分隔符智能插入：搜狗式分词辅助 ──────────────────────
// 输入长拼音串时自动插入分隔符提示，如 "nihaozhongguo" → "ni'hao'zhong'guo"
inline std::string pinyin_segment_hint(const std::string &input) {
    if (input.empty() || input.size() < 4) return {};
    
    // 拼音声母表
    static const std::vector<std::string> initials = {
        "zh", "ch", "sh", "b", "p", "m", "f", "d", "t", "n",
        "l", "g", "k", "h", "j", "q", "x", "r", "z", "c", "s",
        "y", "w"
    };
    // 拼音韵母表（简化）
    static const std::vector<std::string> finals = {
        "a", "o", "e", "i", "u", "v",
        "ai", "ei", "ui", "ao", "ou", "iu", "ie", "ve",
        "an", "en", "in", "un", "vn",
        "ang", "eng", "ing", "ong", "iong",
        "ia", "ua", "uo", "uai", "uei", "iao", "iou",
        "ian", "uan", "van", "iang", "uang", "ueng",
    };
    
    std::string result;
    size_t i = 0;
    bool first = true;
    
    while (i < input.size()) {
        if (!first) result += "'";
        
        // 尝试匹配声母
        std::string matched_initial;
        for (const auto &init : initials) {
            if (i + init.size() <= input.size() &&
                input.substr(i, init.size()) == init) {
                if (init.size() > matched_initial.size()) {
                    matched_initial = init;
                }
            }
        }
        
        size_t start = i;
        i += matched_initial.size();
        
        // 尝试匹配韵母
        std::string matched_final;
        for (const auto &fin : finals) {
            if (i + fin.size() <= input.size() &&
                input.substr(i, fin.size()) == fin) {
                if (fin.size() > matched_final.size()) {
                    matched_final = fin;
                }
            }
        }
        i += matched_final.size();
        
        if (i == start) {
            // 无法匹配，跳过一个字符
            result += input.substr(start, 1);
            i = start + 1;
        } else {
            result += input.substr(start, i - start);
        }
        first = false;
    }
    
    // 只有当插入了分隔符时才返回
    if (result.find("'") != std::string::npos) {
        return result;
    }
    return {};
}

// ── 候选词频排序：根据使用频率调整候选顺序 ──────────────────────
// 搜狗式：高频词排前面
struct FreqEntry {
    std::string word;
    int freq;
};

inline const std::vector<FreqEntry>& default_freq_table() {
    static const std::vector<FreqEntry> table = {
        {"你好", 100}, {"中国", 95}, {"我们", 90}, {"他们", 85},
        {"什么", 80}, {"这个", 75}, {"可以", 70}, {"没有", 65},
        {"现在", 60}, {"时候", 55}, {"知道", 50}, {"觉得", 48},
        {"谢谢", 45}, {"再见", 42}, {"北京", 40}, {"上海", 38},
        {"输入法", 35}, {"手机", 32}, {"电脑", 30}, {"工作", 28},
        {"学习", 26}, {"生活", 24}, {"时间", 22}, {"问题", 20},
        {"方式", 18}, {"地方", 16}, {"电话", 14}, {"地址", 12},
    };
    return table;
}

inline int get_word_freq(const std::string &word) {
    for (const auto &entry : default_freq_table()) {
        if (entry.word == word) return entry.freq;
    }
    return 0;
}

// ── 拼音输入提示：输入过程中显示拼音分词 ──────────────────────
// 搜狗式：输入 "nihao" 时显示 "ni hao" 分词提示
inline std::string pinyin_display_format(const std::string &input) {
    if (input.empty()) return {};
    auto segmented = pinyin_segment_hint(input);
    if (!segmented.empty()) return segmented;
    return input;
}

// ── 长句输入优化：自动断句 ──────────────────────
// 搜狗式：输入很长的拼音串时，自动按词断句
inline std::vector<std::string> long_sentence_split(const std::string &input) {
    if (input.size() < 6) return {};
    
    auto segmented = pinyin_segment_hint(input);
    if (segmented.empty()) return {};
    
    // 按分隔符分割
    std::vector<std::string> parts;
    size_t start = 0;
    while (start < segmented.size()) {
        size_t pos = segmented.find("'", start);
        if (pos == std::string::npos) {
            parts.push_back(segmented.substr(start));
            break;
        }
        parts.push_back(segmented.substr(start, pos - start));
        start = pos + 1;
    }
    
    if (parts.size() < 2) return {};
    return parts;
}

// ── v0.9.127 统一快速输入入口（扩展） ──────────────────────
inline std::vector<std::string> quick_candidates_v3(const std::string &input) {
    std::vector<std::string> result = quick_candidates_v2(input);
    
    // 网址/邮箱智能识别
    auto urls = smart_url_lookup(input);
    for (auto &u : urls) result.push_back(std::move(u));
    
    // 去重
    std::vector<std::string> unique;
    for (auto &r : result) {
        bool dup = false;
        for (auto &u : unique) {
            if (u == r) { dup = true; break; }
        }
        if (!dup) unique.push_back(std::move(r));
    }
    
    if (unique.size() > 20) unique.resize(20);
    return unique;
}



// ── v0.9.128 用户自定义快捷词配置文件 ──────────────────────
// 格式：每行 缩写=展开文本，# 开头为注释
// 示例：
//   addr=北京市海淀区中关村南大街
//   todo=【待办事项】
//   main=int main(int argc, char** argv) { return 0; }
inline std::vector<std::pair<std::string, std::string>>
load_custom_phrases_from_file(const std::string &path) {
    std::vector<std::pair<std::string, std::string>> result;
    FILE *fp = std::fopen(path.c_str(), "r");
    if (!fp) return result;
    char line[1024];
    while (std::fgets(line, sizeof(line), fp)) {
        std::string s(line);
        while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
        if (s.empty() || s[0] == '#') continue;
        auto pos = s.find('=');
        if (pos == std::string::npos || pos == 0) continue;
        std::string abbr = s.substr(0, pos);
        std::string text = s.substr(pos + 1);
        if (!abbr.empty() && !text.empty()) {
            result.emplace_back(std::move(abbr), std::move(text));
        }
    }
    std::fclose(fp);
    return result;
}

// 合并内置自定义快捷词和用户配置文件
inline std::vector<std::pair<std::string, std::string>>
merge_custom_phrases(const std::string &user_file = "") {
    std::vector<std::pair<std::string, std::string>> result;
    for (const auto &p : builtin_custom_phrases()) {
        result.emplace_back(p.abbr, p.phrase);
    }
    if (!user_file.empty()) {
        auto user = load_custom_phrases_from_file(user_file);
        for (auto &u : user) {
            bool found = false;
            for (auto &r : result) {
                if (r.first == u.first) { r.second = u.second; found = true; break; }
            }
            if (!found) result.push_back(u);
        }
    }
    return result;
}

// 用合并后的快捷词表查找
inline std::vector<std::string>
custom_phrase_lookup_ext(const std::string &input,
                          const std::string &user_file = "") {
    auto phrases = merge_custom_phrases(user_file);
    std::vector<std::string> result;
    for (auto &p : phrases) {
        if (p.first == input) result.push_back(p.second);
    }
    for (auto &p : phrases) {
        if (p.first.size() > input.size() &&
            p.first.compare(0, input.size(), input) == 0) {
            result.push_back(p.second);
        }
    }
    return result;
}

// ── v0.9.128 Tab 英文自动补全 ──────────────────────────────
inline std::vector<std::string>
english_autocomplete(const std::string &prefix) {
    if (prefix.empty() || prefix.size() < 2) return {};
    static const std::vector<std::string> words = {
        "about","above","access","across","action","active","add","address",
        "after","again","agent","agree","alert","all","allow","almost",
        "alone","along","already","also","always","among","amount","analyze",
        "angle","animal","another","answer","any","anyone","anything","appear",
        "apple","apply","area","array","arrow","article","ask","assert",
        "asset","assign","async","atom","attach","audio","auth","author",
        "auto","available","await","back","balance","base","basic","batch",
        "battery","beam","bear","beat","begin","behind","being","below",
        "best","better","between","binary","bind","block","blog","blue",
        "body","bool","boost","boot","border","both","bottom","box",
        "branch","break","bridge","brief","bring","broad","browser","buffer",
        "bug","build","busy","button","byte","cache","call","cancel",
        "case","cast","catch","category","center","central","chain","change",
        "channel","char","chart","check","child","choice","choose","chunk",
        "class","clean","clear","click","client","close","cloud","cluster",
        "code","collect","color","column","combine","command","comment","common",
        "compile","complete","compress","computer","config","connect","console","const",
        "contact","content","context","continue","control","convert","copy","core",
        "correct","cost","count","country","course","cover","create","cross",
        "css","csv","ctrl","cube","current","cursor","custom","cycle",
        "daily","data","database","date","day","debug","decimal","default",
        "defer","define","delay","delete","demand","depth","desc","design",
        "detail","detect","device","dict","difference","digital","direct","directory",
        "disable","disk","display","distance","divide","document","domain","done",
        "double","down","download","draft","draw","driver","drop","dump",
        "during","each","early","earth","easy","echo","edge","edit",
        "effect","effort","either","element","else","email","empty","enable",
        "engine","english","ensure","enter","entity","entry","env","equal",
        "error","escape","event","every","exact","example","except","exclude",
        "exec","exist","exit","expand","expect","expire","explain","export",
        "extend","extra","face","fact","factor","factory","fail","fair",
        "false","family","fast","fault","feature","feed","feel","field",
        "file","fill","filter","final","find","fine","finish","fire",
        "first","fish","fix","flag","flash","flat","float","floor",
        "flow","fluid","focus","follow","font","food","foot","force",
        "form","format","forward","found","frame","free","fresh","friend",
        "front","full","function","future","game","gate","general","generate",
        "get","give","global","goal","gold","good","grade","graph",
        "great","green","grid","group","grow","guard","guess","guide",
        "hack","hair","half","hall","hand","handle","hang","happen",
        "happy","hard","hash","have","head","health","hear","heart",
        "heat","hello","help","here","hero","hide","high","hill",
        "hint","history","hit","hold","home","honor","hook","hope",
        "host","hour","house","html","http","human","icon","idea",
        "image","import","include","income","increase","index","info","init",
        "input","insert","inside","install","instance","integer","intent","internal",
        "internet","into","invalid","issue","item","join","json","jump",
        "just","keep","key","keyboard","kind","king","label","language",
        "large","last","late","later","launch","layer","layout","lazy",
        "lead","learn","least","leave","left","legal","length","less",
        "level","library","license","life","light","like","limit","line",
        "link","linux","list","listen","little","live","load","local",
        "location","lock","logic","login","logout","long","look","loop",
        "lose","loss","lower","machine","main","make","manage","many",
        "mark","market","master","match","math","matter","maybe","mean",
        "media","meet","member","memory","menu","merge","message","method",
        "middle","might","mind","mine","minor","minute","miss","mode",
        "model","modern","module","moment","money","month","more","most",
        "move","movie","much","music","must","name","native","nature",
        "near","need","network","new","news","next","nice","night",
        "node","none","normal","north","note","nothing","notice","now",
        "null","number","object","obtain","occur","offer","office","often",
        "old","once","one","online","only","open","operate","option",
        "order","origin","other","out","output","outside","over","own",
        "owner","pack","page","pair","panel","paper","parent","part",
        "pass","past","path","pattern","pause","pay","peak","perform",
        "period","person","phone","photo","physics","pick","picture","piece",
        "place","plan","plant","play","please","plus","point","policy",
        "pool","poor","port","position","positive","possible","post","power",
        "practice","prefer","prepare","present","press","pretty","price","primary",
        "print","private","prize","problem","process","produce","product","program",
        "project","promise","proof","proper","property","protect","protocol","proxy",
        "public","pull","push","python","quality","query","question","queue",
        "quick","quiet","quit","quote","race","radio","rain","raise",
        "random","range","rank","rapid","rate","raw","read","real",
        "reason","receive","recent","record","red","reduce","refer","refresh",
        "region","register","reject","relate","release","reload","remain","remember",
        "remove","render","repair","repeat","replace","reply","report","request",
        "require","research","resource","response","rest","result","return","reveal",
        "review","reward","rich","right","ring","rise","risk","river",
        "road","robot","rock","role","roll","room","root","round",
        "route","row","ruby","rule","run","rust","safe","save",
        "scale","scan","scene","school","scope","score","screen","script",
        "search","season","seat","second","secret","section","secure","seed",
        "seek","seem","select","self","sell","send","sense","server",
        "service","session","setting","setup","seven","shadow","shape","share",
        "sheet","shell","shift","ship","short","should","show","shut",
        "side","sign","signal","silent","silver","simple","since","single",
        "site","size","skill","skin","sky","sleep","slow","small",
        "smart","smile","social","soft","software","solution","some","song",
        "soon","sort","sound","source","space","speak","special","speed",
        "spend","spirit","split","sport","spread","spring","square","stable",
        "staff","stage","stand","standard","start","state","static","station",
        "stay","step","still","stock","stop","store","storm","story",
        "stream","street","strict","string","strong","struct","student","study",
        "style","subject","submit","success","such","sudden","sugar","suggest",
        "summer","super","supply","support","sure","surface","surprise","swap",
        "switch","symbol","system","table","tag","tail","take","talk",
        "tall","task","teach","team","tech","technology","tell","temp",
        "template","term","test","text","than","thank","that","their",
        "them","then","there","these","they","thing","think","third",
        "this","those","though","thread","three","through","throw","time",
        "tiny","title","today","together","tone","tool","top","topic",
        "total","touch","tough","tour","tower","town","track","trade",
        "traffic","train","transfer","transform","translate","transport","travel","tree",
        "trend","trial","trick","trim","true","trust","try","tube",
        "turn","type","under","union","unique","unit","universe","unless",
        "until","update","upgrade","upper","use","user","using","util",
        "valid","value","variable","vector","version","very","video","view",
        "virtual","visible","visit","visual","voice","volume","vote","wait",
        "wake","walk","wall","want","warm","warn","watch","water",
        "wave","way","weak","wear","week","weight","well","west",
        "what","when","where","which","while","white","who","whole",
        "why","wide","width","window","winter","wire","wise","wish",
        "with","within","without","woman","wonder","wood","word","work",
        "world","worry","worse","worst","worth","would","write","wrong",
        "xml","year","yellow","yes","yet","you","young","your",
        "zero","zone"
    };
    std::vector<std::string> result;
    for (auto &w : words) {
        if (w.size() >= prefix.size() &&
            w.compare(0, prefix.size(), prefix) == 0) {
            result.push_back(w);
        }
        if (result.size() >= 10) break;
    }
    return result;
}

// ── v0.9.128 统一快速输入入口（v4 扩展） ──────────────────
inline std::vector<std::string> quick_candidates_v4(const std::string &input,
                                                     const std::string &user_file = "") {
    auto result = quick_candidates_v3(input);
    // 用户自定义快捷词
    auto custom = custom_phrase_lookup_ext(input, user_file);
    for (auto &c : custom) result.push_back(std::move(c));
    // 去重
    std::vector<std::string> unique;
    for (auto &r : result) {
        bool dup = false;
        for (auto &u : unique) { if (u == r) { dup = true; break; } }
        if (!dup) unique.push_back(std::move(r));
    }
    if (unique.size() > 20) unique.resize(20);
    return unique;
}

// ── v0.9.129 常用句式模板 ──────────────────────────────────
// 输入句式缩写，展开为常用句子
inline std::vector<std::pair<std::string, std::string>>
builtin_sentence_templates() {
    return {
        {"xw",      "请问您需要什么帮助？"},
        {"xw2",     "您好，请问有什么可以帮您的？"},
        {"gz",      "感谢您的关注与支持！"},
        {"gz2",     "非常感谢您的配合，期待下次合作。"},
        {"qh",      "请稍等，我正在为您处理。"},
        {"qh2",     "请确认信息无误后提交。"},
        {"yy",      "已为您处理完毕，请查收。"},
        {"yy2",     "已收到您的反馈，我们会尽快处理。"},
        {"bj",      "备注：请仔细核对以上信息。"},
        {"yj",      "以上意见仅供参考，具体以实际情况为准。"},
        {"td",      "【待办】"},
        {"td2",     "【已完成】"},
        {"td3",     "【进行中】"},
        {"td4",     "【已取消】"},
        {"yx",      "【优先级：高】"},
        {"yx2",     "【优先级：中】"},
        {"yx3",     "【优先级：低】"},
        {"ly",      "祝您工作顺利，生活愉快！"},
        {"ly2",     "祝您身体健康，万事如意！"},
        {"ly3",     "祝您旅途愉快，一路顺风！"},
        {"em",      "我的邮箱是"},
        {"ph",      "我的电话是"},
        {"ad",      "我的地址是"},
        {"nm",      "我的名字是"},
        {"sig",     "此致\n敬礼"},
        {"sig2",    "Best regards,"},
        {"sig3",    "Thanks and Best Regards,"},
        {"mt",      "会议时间："},
        {"mt2",     "会议地点："},
        {"mt3",     "参会人员："},
        {"mt4",     "会议主题："},
        {"rp",      "报告人："},
        {"rp2",     "报告时间："},
        {"rp3",     "报告地点："},
    };
}

inline std::vector<std::string>
sentence_template_lookup(const std::string &input) {
    auto templates = builtin_sentence_templates();
    std::vector<std::string> result;
    for (auto &t : templates) {
        if (t.first == input) result.push_back(t.second);
    }
    for (auto &t : templates) {
        if (t.first.size() > input.size() &&
            t.first.compare(0, input.size(), input) == 0) {
            result.push_back(t.second);
        }
    }
    return result;
}

// ── v0.9.129 最近输入记忆 ──────────────────────────────────
// 记录最近上屏的文本，用于快速重复输入
struct RecentHistory {
    std::vector<std::string> items;
    size_t max_size = 20;

    void add(const std::string &text) {
        if (text.empty()) return;
        // 去重：如果已存在，移到最前
        for (auto it = items.begin(); it != items.end(); ++it) {
            if (*it == text) { items.erase(it); break; }
        }
        items.insert(items.begin(), text);
        if (items.size() > max_size) items.resize(max_size);
    }

    std::vector<std::string> lookup(const std::string &prefix = "") const {
        std::vector<std::string> result;
        for (auto &item : items) {
            if (prefix.empty() ||
                (item.size() >= prefix.size() &&
                 item.compare(0, prefix.size(), prefix) == 0)) {
                result.push_back(item);
            }
            if (result.size() >= 10) break;
        }
        return result;
    }

    bool save_to_file(const std::string &path) const {
        FILE *fp = std::fopen(path.c_str(), "w");
        if (!fp) return false;
        for (auto &item : items) {
            std::fprintf(fp, "%s\n", item.c_str());
        }
        std::fclose(fp);
        return true;
    }

    bool load_from_file(const std::string &path) {
        FILE *fp = std::fopen(path.c_str(), "r");
        if (!fp) return false;
        char line[1024];
        items.clear();
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (!s.empty()) items.push_back(s);
            if (items.size() >= max_size) break;
        }
        std::fclose(fp);
        return true;
    }
};

// ── v0.9.129 智能选词（基于上下文） ──────────────────────
// 根据上一个上屏的词，调整当前候选排序
inline std::vector<std::string>
context_rank_candidates(const std::vector<std::string> &candidates,
                         const std::string &last_committed) {
    if (candidates.empty() || last_committed.empty()) return candidates;

    // 上下文关联表：某些词后面更可能跟某些词（v0.9.145 扩展至 72 条）
    static const std::vector<std::pair<std::string, std::string>> context_rules = {
        // 原有 36 条
        {"你", "好"}, {"你", "们"}, {"我", "们"}, {"我", "的"},
        {"他", "们"}, {"这", "个"}, {"那", "个"}, {"什", "么"},
        {"为", "什"}, {"因", "为"}, {"所", "以"}, {"但", "是"},
        {"如", "果"}, {"虽", "然"}, {"还", "是"}, {"或", "者"},
        {"一", "个"}, {"两", "个"}, {"三", "个"}, {"四", "个"},
        {"中", "国"}, {"北", "京"}, {"上", "海"}, {"广", "州"},
        {"学", "生"}, {"老", "师"}, {"朋", "友"}, {"同", "学"},
        {"工", "作"}, {"学", "习"}, {"生", "活"}, {"时", "间"},
        {"今", "天"}, {"明", "天"}, {"昨", "天"}, {"前", "天"},
        {"上", "午"}, {"下", "午"}, {"晚", "上"}, {"早", "上"},
        // v0.9.145 新增 36 条
        {"请", "问"}, {"谢", "谢"}, {"不", "用"}, {"不", "是"},
        {"可", "以"}, {"能", "够"}, {"应", "该"}, {"需", "要"},
        {"已", "经"}, {"正", "在"}, {"将", "会"}, {"可", "能"},
        {"问", "题"}, {"方", "法"}, {"结", "果"}, {"原", "因"},
        {"程", "序"}, {"代", "码"}, {"系", "统"}, {"网", "络"},
        {"数", "据"}, {"技", "术"}, {"开", "发"}, {"测", "试"},
        {"用", "户"}, {"服", "务"}, {"安", "全"}, {"性", "能"},
        {"手", "机"}, {"电", "脑"}, {"网", "站"}, {"软", "件"},
        {"人", "工"}, {"智", "能"}, {"机", "器"}, {"深", "度"},
    };

    auto result = candidates;
    // 如果上一个词在关联表中，把关联词排到前面
    for (auto &rule : context_rules) {
        if (last_committed == rule.first) {
            for (size_t i = 1; i < result.size(); ++i) {
                if (result[i].find(rule.second) != std::string::npos) {
                    std::swap(result[0], result[i]);
                    break;
                }
            }
            break;
        }
    }
    return result;
}

// ── v0.9.130 用户词频学习 ──────────────────────────────────
// 记录 拼音→中文 的上屏频率，用于动态调整候选排序
struct UserFreqEntry {
    std::string pinyin;
    std::string chinese;
    int count = 0;
    long last_used = 0;  // 时间戳
};

struct UserFreqTable {
    std::vector<UserFreqEntry> entries;
    size_t max_size = 500;

    void add(const std::string &pinyin, const std::string &chinese) {
        if (pinyin.empty() || chinese.empty()) return;
        // 查找已有条目
        for (auto &e : entries) {
            if (e.pinyin == pinyin && e.chinese == chinese) {
                e.count++;
                e.last_used = std::time(nullptr);
                return;
            }
        }
        // 新条目
        entries.push_back({pinyin, chinese, 1, std::time(nullptr)});
        // 超过上限：淘汰最低频率的
        if (entries.size() > max_size) {
            size_t min_idx = 0;
            for (size_t i = 1; i < entries.size(); ++i) {
                if (entries[i].count < entries[min_idx].count) min_idx = i;
            }
            entries.erase(entries.begin() + min_idx);
        }
    }

    // 查找某拼音的所有频率条目，按频率降序排列
    std::vector<std::pair<std::string, int>>
    lookup(const std::string &pinyin) const {
        std::vector<std::pair<std::string, int>> result;
        for (auto &e : entries) {
            if (e.pinyin == pinyin) {
                result.emplace_back(e.chinese, e.count);
            }
        }
        std::sort(result.begin(), result.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });
        return result;
    }

    // 根据频率表重排候选列表
    std::vector<std::string>
    rank_candidates(const std::vector<std::string> &candidates,
                    const std::string &pinyin) const {
        if (candidates.empty()) return candidates;
        auto freq = lookup(pinyin);
        if (freq.empty()) return candidates;

        auto result = candidates;
        // 按频率从高到低，把匹配的候选移到前面
        size_t insert_pos = 0;
        for (auto &f : freq) {
            for (size_t i = insert_pos; i < result.size(); ++i) {
                if (result[i] == f.first) {
                    std::swap(result[insert_pos], result[i]);
                    insert_pos++;
                    break;
                }
            }
        }
        return result;
    }

    // 获取所有高频词（用于统计面板）
    std::vector<std::pair<std::string, int>>
    top_entries(size_t n = 20) const {
        std::vector<std::pair<std::string, int>> result;
        for (auto &e : entries) {
            result.emplace_back(e.chinese, e.count);
        }
        std::sort(result.begin(), result.end(),
                  [](const auto &a, const auto &b) { return a.second > b.second; });
        if (result.size() > n) result.resize(n);
        return result;
    }

    bool save_to_file(const std::string &path) const {
        FILE *fp = std::fopen(path.c_str(), "w");
        if (!fp) return false;
        std::fprintf(fp, "# pinyin<TAB>chinese<TAB>count<TAB>last_used\n");
        for (auto &e : entries) {
            std::fprintf(fp, "%s\t%s\t%d\t%ld\n",
                         e.pinyin.c_str(), e.chinese.c_str(), e.count, e.last_used);
        }
        std::fclose(fp);
        return true;
    }

    bool load_from_file(const std::string &path) {
        FILE *fp = std::fopen(path.c_str(), "r");
        if (!fp) return false;
        char line[1024];
        entries.clear();
        while (std::fgets(line, sizeof(line), fp)) {
            std::string s(line);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.empty() || s[0] == '#') continue;
            // 分割 tab
            std::vector<std::string> parts;
            size_t start = 0;
            for (size_t i = 0; i <= s.size(); ++i) {
                if (i == s.size() || s[i] == '\t') {
                    parts.push_back(s.substr(start, i - start));
                    start = i + 1;
                }
            }
            if (parts.size() >= 3) {
                UserFreqEntry e;
                e.pinyin = parts[0];
                e.chinese = parts[1];
                e.count = std::atoi(parts[2].c_str());
                if (parts.size() >= 4) e.last_used = std::atol(parts[3].c_str());
                if (!e.pinyin.empty() && !e.chinese.empty()) entries.push_back(e);
            }
            if (entries.size() >= max_size) break;
        }
        std::fclose(fp);
        return true;
    }
};
