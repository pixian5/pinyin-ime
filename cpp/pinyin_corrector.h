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

// 判断是否为「零编辑可完整切分」的全拼输入（简拼/双拼/混合输入返回 false）。
// 供解码前端使用：只有真全拼才值得做纠错，避免把简拼误纠成其他全拼。
bool is_full_pinyin(const std::string &pinyin);

// 便捷封装：纠正后串成 `'` 分隔字符串（供解码前端）；失败返回空串。
std::string correct_to_string(const std::string &pinyin, int maxDist = 2);

} // namespace pinyin_corrector