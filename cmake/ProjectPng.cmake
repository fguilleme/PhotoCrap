include(FetchContent)

FetchContent_Declare(photocrap_png
    URL https://codeload.github.com/pnggroup/libpng/tar.gz/refs/tags/v1.6.59
    URL_HASH SHA256=2540302a1844ad2b2b501977abecfa850f265f97b78f065a712ab4074a89f5b5
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR photocrap-no-upstream-build
)
FetchContent_MakeAvailable(photocrap_png)

configure_file("${photocrap_png_SOURCE_DIR}/scripts/pnglibconf.h.prebuilt"
               "${CMAKE_CURRENT_BINARY_DIR}/project-png/pnglibconf.h" COPYONLY)
set(photocrap_png_sources png.c pngerror.c pngget.c pngmem.c pngpread.c pngread.c pngrio.c
    pngrtran.c pngrutil.c pngset.c pngtrans.c pngwio.c pngwrite.c pngwtran.c pngwutil.c)
list(TRANSFORM photocrap_png_sources PREPEND "${photocrap_png_SOURCE_DIR}/")
add_library(photocrap_png STATIC ${photocrap_png_sources})
target_include_directories(photocrap_png SYSTEM PUBLIC
    "${photocrap_png_SOURCE_DIR}" "${CMAKE_CURRENT_BINARY_DIR}/project-png")
target_compile_definitions(photocrap_png PRIVATE PNG_ARM_NEON_OPT=0 PNG_INTEL_SSE_OPT=0)
target_link_libraries(photocrap_png PRIVATE photocrap_raw_zlib)
set_target_properties(photocrap_png PROPERTIES AUTOMOC OFF AUTOUIC OFF
    C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)

add_library(photocrap_png_codec STATIC
    src/storage/ProjectPngCodec.c src/storage/ProjectPngCodec.h)
target_link_libraries(photocrap_png_codec PRIVATE photocrap_png)
set_target_properties(photocrap_png_codec PROPERTIES AUTOMOC OFF AUTOUIC OFF
    C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)

function(photocrap_install_png_license)
    if(APPLE)
        set(notices "PhotoCrap.app/Contents/Resources/third-party")
    elseif(WIN32)
        set(notices "third-party")
    else()
        set(notices "share/PhotoCrap/third-party")
    endif()
    install(FILES "${photocrap_png_SOURCE_DIR}/LICENSE" DESTINATION "${notices}/libpng-1.6.59")
endfunction()
