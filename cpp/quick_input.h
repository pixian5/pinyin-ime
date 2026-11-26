// quick_input.h — 搜狗式快速输入功能（CLI 与 Fcitx5 addon 共用）
//
// 功能：
//   1. v 模式（符号输入）：v1→①②③  v2→★☆  v3→箭头  v4→数学  v5→希腊
//      v6→罗马数字  v7→单位符号  v8→括号  v9→点线  vd→日期  vf→分数
//      va→拼音字母  vb→方括号  vc→棋牌  ve→自然  vg→几何  vh→扩展箭头
//      vi→音乐  vk→特殊  vl→制表线  vm→扩展数学  vn→扩展数字
//      vs→上下标  vt→技术  vu→扩展单位  vw→天气  vx→星座  vy→生肖  vz→杂项
//   2. 日期/时间快速输入：date/rq→2026年10月22日  time/sj→14:30:25  week/xq→星期四
//      year/yn→2026年  month/yue→10月  ts→20261022  hour→14时
//   3. 简易计算器：输入算式→结果作为候选
//   4. 数字转中文大写：123→壹佰贰拾叁
//   5. 货币转换：123rmb→壹佰贰拾叁元  123$→$123.00  123usd→123.00美元
//   6. 单位换算：1km→1千米=1000米  1kg→1千克=1000克  1mile→1.609千米
//   7. 百分比转中文：50p→百分之五十
//
// 设计原则：纯函数、无副作用、不依赖外部文件；与 fcitx5 / libime 解耦。
#pragma once

#include <ctime>
#include <string>
#include <vector>
#include <cmath>
#include <cstdlib>
#include <functional>
#include <set>
#include <map>
#include <stdexcept>
#include <algorithm>
#include <sstream>
#include <fstream>
#include <chrono>
#include <unordered_map>
#include <filesystem>
#include <sys/stat.h>
#include <cstdio>
#include <unordered_set>
#include <sys/stat.h>
#include <cstdio>
#include <sys/stat.h>
#include <cstdio>

namespace pinyin_ime {

#include "quick_input_core.h"  // 快速输入基础（v0.9.178 拆出）

#include "quick_input_modes.h"  // 字母类输入模式（v0.9.178 拆出）

#include "quick_input_typing.h"  // 输入过程辅助（v0.9.178 拆出）

// ── v0.9.131 皮肤/主题切换 ────────────────────────────────
struct Theme {
    std::string name;
    std::string preedit_color;    // 拼音预编辑区颜色
    std::string candidate_color;  // 候选词颜色
    std::string focus_color;      // 焦点候选颜色
    std::string index_color;      // 序号颜色
    std::string hint_color;       // 提示信息颜色
    std::string reset;            // 重置颜色
};

inline Theme get_theme(const std::string &name) {
    if (name == "dark" || name == "暗色") {
        return {"dark", "\033[36m", "\033[37m", "\033[1;33m",
                "\033[90m", "\033[90m", "\033[0m"};
    } else if (name == "high" || name == "高对比") {
        return {"high", "\033[1;37m", "\033[1;37m", "\033[1;31m",
                "\033[1;36m", "\033[1;35m", "\033[0m"};
    } else if (name == "retro" || name == "复古") {
        return {"retro", "\033[33m", "\033[32m", "\033[1;33m",
                "\033[35m", "\033[36m", "\033[0m"};
    } else if (name == "matrix" || name == "黑客") {
        return {"matrix", "\033[32m", "\033[32m", "\033[1;32m",
                "\033[90m", "\033[32m", "\033[0m"};
    } else if (name == "ocean" || name == "海洋") {
        return {"ocean", "\033[34m", "\033[36m", "\033[1;34m",
                "\033[90m", "\033[35m", "\033[0m"};
    } else if (name == "solar" || name == "日光") {
        return {"solar", "\033[33m", "\033[37m", "\033[1;31m",
                "\033[33m", "\033[35m", "\033[0m"};
    }
    // 默认主题
    return {"default", "", "", "", "", "", ""};
}

inline std::vector<std::string> list_themes() {
    return {"default", "dark", "high", "retro", "matrix", "ocean", "solar"};
}

#include "quick_input_cloud.h"  // 云拼音与模糊音（v0.9.178 拆出）

#include "quick_input_clipboard.h"  // 剪贴板历史（v0.9.178 拆出）

#include "quick_input_sentence.h"  // 长句切分与短语管理（v0.9.178 拆出）

#include "quick_input_stats.h"  // 输入统计（v0.9.178 拆出）

#include "quick_input_rank.h"  // 纠错与候选排序（v0.9.178 拆出）

#include "quick_input_ui.h"  // 表情云同步与候选窗（v0.9.178 拆出）

#include "quick_input_boost.h"  // 搜狗式增强入口（v0.9.178 拆出）

} // namespace pinyin_ime
