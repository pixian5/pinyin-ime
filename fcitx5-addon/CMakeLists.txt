# pinyin-ime 的 Fcitx5 addon：把 libime 拼音引擎接进 Fcitx5 输入法框架。
# 外部 addon（与 fcitx5 主程序分离编译，产出 libpinyinime.so + pinyinime.conf）。
#
# 沙箱内验证编译：
#   cmake -S fcitx5-addon -B fcitx5-addon/build -G Ninja -DCMAKE_BUILD_TYPE=Release \
#     -DCMAKE_PREFIX_PATH="$OPT/fcitx5;$OPT/libime;$ENV"
#   cmake --build fcitx5-addon/build
#   DESTDIR=$PWD/fcitx5-addon/stage cmake --install fcitx5-addon/build
#
# 实机（Ubuntu 26.04）用法见 README「Fcitx5 addon：实机安装」。
cmake_minimum_required(VERSION 3.16)
project(pinyinime VERSION 0.9.50 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

find_package(Fcitx5Core REQUIRED)
find_package(Fcitx5Config REQUIRED)
find_package(Fcitx5Utils REQUIRED)
find_package(LibIMEPinyin REQUIRED)

set(PINYINIME_VERSION ${PROJECT_VERSION})

add_fcitx5_addon(pinyinime pinyinime.cpp ../cpp/pinyin_corrector.cpp)

target_link_libraries(pinyinime PRIVATE
    Fcitx5::Core
    Fcitx5::Config
    Fcitx5::Utils
    LibIME::Pinyin
    LibIME::Core)

target_include_directories(pinyinime PRIVATE
    ${CMAKE_CURRENT_SOURCE_DIR}
    ${CMAKE_CURRENT_SOURCE_DIR}/../cpp)  # 共享 emoji_list.h

install(TARGETS pinyinime DESTINATION "${FCITX_INSTALL_ADDONDIR}")

# 无翻译版本：直接 configure 出最终 conf，避免依赖 gettext/msgfmt。
configure_file(pinyinime.conf.in pinyinime.conf @ONLY)
install(FILES "${CMAKE_CURRENT_BINARY_DIR}/pinyinime.conf"
        DESTINATION "${FCITX_INSTALL_PKGDATADIR}/addon")