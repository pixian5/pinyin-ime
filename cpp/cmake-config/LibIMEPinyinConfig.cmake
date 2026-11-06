# Stub config for LibIMEPinyin
set(LibIMEPinyin_FOUND TRUE)
set(LIBIME_INCLUDE_DIR "/root/workspace/libime-src/src")
set(LIBIME_LIBRARY_DIR "/root/workspace/pinyin-ime/pinyin-ime-s3/v0.9.171/lib")
set(FCITX5_INCLUDE_DIR "/root/workspace/fcitx5-src/src/lib")
set(STUBS_DIR "/root/workspace/pinyin-ime/cpp/stubs")

if(NOT TARGET LibIME::Pinyin)
    add_library(LibIME::Pinyin SHARED IMPORTED)
    set_target_properties(LibIME::Pinyin PROPERTIES
        IMPORTED_LOCATION "${LIBIME_LIBRARY_DIR}/libIMEPinyin.so.1.1.17"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBIME_INCLUDE_DIR};${FCITX5_INCLUDE_DIR};${STUBS_DIR}"
    )
endif()

if(NOT TARGET LibIME::Core)
    add_library(LibIME::Core SHARED IMPORTED)
    set_target_properties(LibIME::Core PROPERTIES
        IMPORTED_LOCATION "${LIBIME_LIBRARY_DIR}/libIMECore.so.1.1.17"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBIME_INCLUDE_DIR};${FCITX5_INCLUDE_DIR};${STUBS_DIR}"
    )
endif()
