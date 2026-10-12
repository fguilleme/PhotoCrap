include(FetchContent)
enable_language(C)

# Pin and verify upstream sources; link statically so release packages need no
# system LibRaw installation. Build only the libraries, without upstream tools.
FetchContent_Declare(photocrap_libraw
    URL https://codeload.github.com/LibRaw/LibRaw/tar.gz/refs/tags/0.22.2
    URL_HASH SHA256=627928088300ecde6ca91ffd202e189203f04ad61ad12f0fe9dc57b9a7a0fb3c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR photocrap-no-upstream-build
)
FetchContent_Declare(photocrap_zlib
    URL https://codeload.github.com/madler/zlib/tar.gz/refs/tags/v1.3.1
    URL_HASH SHA256=17e88863f3600672ab49182f217281b6fc4d3c762bde361935e436a95214d05c
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR photocrap-no-upstream-build
)
FetchContent_MakeAvailable(photocrap_libraw photocrap_zlib)

set(photocrap_zlib_sources adler32.c compress.c crc32.c deflate.c gzclose.c gzlib.c gzread.c
    gzwrite.c infback.c inffast.c inflate.c inftrees.c trees.c uncompr.c zutil.c)
list(TRANSFORM photocrap_zlib_sources PREPEND "${photocrap_zlib_SOURCE_DIR}/")
add_library(photocrap_raw_zlib STATIC ${photocrap_zlib_sources})
target_include_directories(photocrap_raw_zlib SYSTEM PUBLIC "${photocrap_zlib_SOURCE_DIR}")
set_target_properties(photocrap_raw_zlib PROPERTIES AUTOMOC OFF AUTOUIC OFF POSITION_INDEPENDENT_CODE ON)

if(UNIX)
    target_compile_definitions(photocrap_raw_zlib PRIVATE HAVE_UNISTD_H)
endif()

file(GLOB_RECURSE photocrap_libraw_sources CONFIGURE_DEPENDS "${photocrap_libraw_SOURCE_DIR}/src/*.cpp")
list(FILTER photocrap_libraw_sources EXCLUDE REGEX "_ph\\.cpp$")
add_library(photocrap_libraw STATIC ${photocrap_libraw_sources})
target_include_directories(photocrap_libraw SYSTEM PUBLIC "${photocrap_libraw_SOURCE_DIR}")
target_compile_definitions(photocrap_libraw PUBLIC LIBRAW_NODLL PRIVATE USE_ZLIB)
target_link_libraries(photocrap_libraw PRIVATE photocrap_raw_zlib)
if(WIN32)
    target_link_libraries(photocrap_libraw PRIVATE ws2_32)
endif()
set_target_properties(photocrap_libraw PROPERTIES CXX_STANDARD 17 AUTOMOC OFF AUTOUIC OFF POSITION_INDEPENDENT_CODE ON)

function(photocrap_install_raw_sources)
    # LibRaw is distributed under CDDL 1.0. Include its unmodified source and notices
    # with each standalone application, alongside zlib's source and licence.
    if(APPLE)
        set(photocrap_raw_notices "PhotoCrap.app/Contents/Resources/third-party")
    elseif(WIN32)
        set(photocrap_raw_notices "third-party")
    else()
        set(photocrap_raw_notices "share/PhotoCrap/third-party")
    endif()
    install(DIRECTORY "${photocrap_libraw_SOURCE_DIR}/" DESTINATION "${photocrap_raw_notices}/LibRaw-0.22.2")
    install(DIRECTORY "${photocrap_zlib_SOURCE_DIR}/" DESTINATION "${photocrap_raw_notices}/zlib-1.3.1")

endfunction()
