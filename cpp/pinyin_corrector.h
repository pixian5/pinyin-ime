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

// 判断当前输入是否「值得走纠错」（比 is_full_pinyin 更宽的保守门控）：
//   ① 零编辑可切分（真全拼，含音节内打错如 senme——本就是合法音节串）→ true；
//   ② 否则仅当「疑似全拼打错」才 true：长度 >= 3、含元音、纠正结果与输入等长
//      （纯替换型，不做插入/删除）、替换数 1~2 处、纠正后 <= 2 音节。
// 目的：放开第 7 轮声明的边界（zhing→zheng 这类『零编辑切分失败』的重错全拼），
// 同时不误伤简拼（zg/bjdx：无元音或长度过短）、双拼与混合输入（bjdaxue：纠正后
// 音节数 > 2 或不等长）。
bool should_correct(const std::string &pinyin);

// 便捷封装：纠正后串成 `'` 分隔字符串（供解码前端）；失败返回空串。
std::string correct_to_string(const std::string &pinyin, int maxDist = 2);

} // namespace pinyin_corrector