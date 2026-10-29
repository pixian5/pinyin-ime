cmake_minimum_required(VERSION 3.16)
project(libime_bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LibIMEPinyin REQUIRED)

# Fcitx5Utils library (from prebuilt runtime)
set(FCITX5_UTILS_LIB "/tmp/pinyin-ime-deploy-v0.9.125/runtime/opt/fcitx5-x11/lib/libFcitx5Utils.so.2")

add_executable(libime_bench libime_bench.cpp)
target_link_libraries(libime_bench PRIVATE LibIME::Pinyin LibIME::Core ${FCITX5_UTILS_LIB})

# M1：可运行输入法（命令行前端，交互 + 行模式）
add_executable(pinyin_ime_cli pinyin_ime_cli.cpp pinyin_corrector.cpp)
target_link_libraries(pinyin_ime_cli PRIVATE LibIME::Pinyin LibIME::Core ${FCITX5_UTILS_LIB})

# M3 纠错 C++ 移植对拍自检（与 correction/fix_pinyin.py 同协议，无 libime 依赖）
add_executable(pinyin_corrector_test pinyin_corrector.cpp pinyin_corrector_test.cpp)

# v0.9.123 搜狗式快速输入单元测试（无 libime 依赖，纯函数）
add_executable(quick_input_test quick_input_test.cpp)

# Install targets
install(TARGETS pinyin_ime_cli DESTINATION bin)
install(TARGETS pinyin_corrector_test DESTINATION bin)
install(TARGETS quick_input_test DESTINATION bin)
install(TARGETS libime_bench DESTINATION bin)
