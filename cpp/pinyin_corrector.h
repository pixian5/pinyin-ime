// pinyin_corrector.h — M3 纠错模块的 C++ 原生移植（前端）。
//
// 与 correction/corrector.py 的 `correct()` 语义一致：输入连续全拼（可能打错），
// 输出「切分 + 纠错」后的正确音节列表。数据固化在 correction_data.h（生成器
// correction/generate_cpp_data.py），运行时无 Python 依赖，替换行模式每行 fork
// Python 的桥接（correction/fix_pinyin.py）。
#pragma once

#include <optional>
#include <string>
#include <vector>

namespace pinyin_corrector {

// 纠正连续全拼，返回最优音节切分（可能含纠错）。无法纠或空输入返回 nullopt。
std::optional<std::vector<std::string>> correct(const std::string &pinyin,
                                                int maxDist = 2);

// 便捷封装：纠正后串成 `'` 分隔字符串（供解码前端）；失败返回空串。
std::string correct_to_string(const std::string &pinyin, int maxDist = 2);

} // namespace pinyin_corrector