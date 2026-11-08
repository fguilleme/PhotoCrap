include(FetchContent)
FetchContent_Declare(photocrap_zstd
    URL https://codeload.github.com/facebook/zstd/tar.gz/refs/tags/v1.5.7
    URL_HASH SHA256=37d7284556b20954e56e1ca85b80226768902e2edabd3b649e9e72c0c9012ee3
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR photocrap-no-upstream-build)
FetchContent_MakeAvailable(photocrap_zstd)
file(GLOB zstd_sources CONFIGURE_DEPENDS
    "${photocrap_zstd_SOURCE_DIR}/lib/common/*.c"
    "${photocrap_zstd_SOURCE_DIR}/lib/compress/*.c"
    "${photocrap_zstd_SOURCE_DIR}/lib/decompress/*.c")
add_library(photocrap_zstd STATIC ${zstd_sources})
target_include_directories(photocrap_zstd SYSTEM PUBLIC "${photocrap_zstd_SOURCE_DIR}/lib")
target_compile_definitions(photocrap_zstd PRIVATE ZSTD_DISABLE_ASM=1)
set_target_properties(photocrap_zstd PROPERTIES AUTOMOC OFF AUTOUIC OFF
    POSITION_INDEPENDENT_CODE ON C_VISIBILITY_PRESET hidden)
if(APPLE)
    set(zstd_notices "PhotoCrap.app/Contents/Resources/third-party")
elseif(WIN32)
    set(zstd_notices "third-party")
else()
    set(zstd_notices "share/PhotoCrap/third-party")
endif()
install(FILES "${photocrap_zstd_SOURCE_DIR}/LICENSE" DESTINATION "${zstd_notices}/zstd-1.5.7")
