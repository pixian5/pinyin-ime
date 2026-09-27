// pinyin_corrector_test.cpp — C++ 纠错移植与 Python 口径的对拍自检。
// stdin 每行一段连续全拼 → 输出纠正后的 ' 分隔音节串（与 correction/fix_pinyin.py 同协议）。
#include "pinyin_corrector.h"

#include <iostream>
#include <string>

int main() {
    std::string line;
    while (std::getline(std::cin, line)) {
        if (line.empty()) continue;
        std::string r = pinyin_corrector::correct_to_string(line);
        std::cout << (r.empty() ? line : r) << "\n";
    }
    return 0;
}