cmake_minimum_required(VERSION 3.16)
project(libime_bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LibIMEPinyin REQUIRED)

add_executable(libime_bench libime_bench.cpp)
target_link_libraries(libime_bench PRIVATE LibIME::Pinyin LibIME::Core)

# M1：可运行输入法（命令行前端，交互 + 行模式）
add_executable(pinyin_ime_cli pinyin_ime_cli.cpp)
target_link_libraries(pinyin_ime_cli PRIVATE LibIME::Pinyin LibIME::Core)
