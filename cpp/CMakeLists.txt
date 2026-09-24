cmake_minimum_required(VERSION 3.16)
project(libime_bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(LibIMEPinyin REQUIRED)

add_executable(libime_bench libime_bench.cpp)
target_link_libraries(libime_bench PRIVATE LibIME::Pinyin LibIME::Core)
