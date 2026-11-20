cmake_minimum_required(VERSION 3.16)
project(libime_bench CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# GCC 10 兼容：提供 <source_location> stub（新版 GCC 不需要，但保留不影响）
set(CXX20_STUBS "${CMAKE_CURRENT_SOURCE_DIR}/stubs/cxx20")
if(EXISTS "${CXX20_STUBS}")
  add_compile_options(-isystem${CXX20_STUBS})
endif()

find_package(LibIMEPinyin REQUIRED)
find_package(Fcitx5Utils QUIET)

# Fcitx5Utils：优先用系统库（Ubuntu 24.04 自带 fcitx5 5.1.7）；
# 允许用 -DFCITX5_UTILS_LIB=... 指向 S3 预构建库做交叉环境构建。
# 旧版本曾硬编码 /root/workspace/... 沙箱路径，导致脱离沙箱后无法构建，此处已移除。
if(NOT FCITX5_UTILS_LIB)
  if(TARGET Fcitx5::Utils)
    set(FCITX5_UTILS_LIB Fcitx5::Utils)
  else()
    find_library(FCITX5_UTILS_LIB NAMES Fcitx5Utils)
  endif()
endif()
if(NOT FCITX5_UTILS_LIB)
  message(FATAL_ERROR "未找到 libFcitx5Utils，请安装 libfcitx5utils-dev 或传 -DFCITX5_UTILS_LIB=<path>")
endif()

# 兼容不同 fcitx5 版本的符号依赖
set(CMAKE_EXE_LINKER_FLAGS "${CMAKE_EXE_LINKER_FLAGS} -Wl,--allow-shlib-undefined")

add_executable(libime_bench libime_bench.cpp)
target_link_libraries(libime_bench PRIVATE LibIME::Pinyin LibIME::Core ${FCITX5_UTILS_LIB})

# M1：可运行输入法（命令行前端，交互 + 行模式）
add_executable(pinyin_ime_cli pinyin_ime_cli.cpp pinyin_corrector.cpp)
target_link_libraries(pinyin_ime_cli PRIVATE LibIME::Pinyin LibIME::Core ${FCITX5_UTILS_LIB})

# M3 纠错 C++ 移植对拍自检（与 correction/fix_pinyin.py 同协议，无 libime 依赖）
add_executable(pinyin_corrector_test pinyin_corrector.cpp pinyin_corrector_test.cpp)

# v0.9.123 搜狗式快速输入单元测试（无 libime 依赖，纯函数）
add_executable(quick_input_test quick_input_test.cpp)

# v0.9.175 云拼音客户端（libcurl）+ 自检
find_package(CURL REQUIRED)
add_executable(cloud_pinyin_test cloud_pinyin_test.cpp cloud_pinyin.cpp)
target_link_libraries(cloud_pinyin_test PRIVATE CURL::libcurl)
install(TARGETS cloud_pinyin_test DESTINATION bin)

# Install targets
install(TARGETS pinyin_ime_cli DESTINATION bin)
install(TARGETS pinyin_corrector_test DESTINATION bin)
install(TARGETS quick_input_test DESTINATION bin)
install(TARGETS libime_bench DESTINATION bin)
