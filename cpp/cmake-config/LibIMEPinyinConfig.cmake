# Stub config for LibIMEPinyin
set(LibIMEPinyin_FOUND TRUE)
set(LIBIME_INCLUDE_DIR "/root/workspace/libime/src/libime")
set(LIBIME_LIBRARY_DIR "/tmp/pinyin-ime-deploy-v0.9.125/runtime/lib")

# Create imported targets
if(NOT TARGET LibIME::Pinyin)
    add_library(LibIME::Pinyin SHARED IMPORTED)
    set_target_properties(LibIME::Pinyin PROPERTIES
        IMPORTED_LOCATION "${LIBIME_LIBRARY_DIR}/libIMEPinyin.so.1.1.17"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBIME_INCLUDE_DIR}/pinyin;${LIBIME_INCLUDE_DIR}/core"
    )
endif()

if(NOT TARGET LibIME::Core)
    add_library(LibIME::Core SHARED IMPORTED)
    set_target_properties(LibIME::Core PROPERTIES
        IMPORTED_LOCATION "${LIBIME_LIBRARY_DIR}/libIMECore.so.1.1.17"
        INTERFACE_INCLUDE_DIRECTORIES "${LIBIME_INCLUDE_DIR}/core"
    )
endif()
