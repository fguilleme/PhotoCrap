include(FetchContent)

FetchContent_Declare(photocrap_webp
    URL https://storage.googleapis.com/downloads.webmproject.org/releases/webp/libwebp-1.6.0.tar.gz
    URL_HASH SHA256=e4ab7009bf0629fd11982d4c2aa83964cf244cffba7347ecd39019a9e38c4564
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR photocrap-no-upstream-build
)
FetchContent_MakeAvailable(photocrap_webp)

function(photocrap_add_webp_encoder)
    set(BUILD_SHARED_LIBS OFF)
    set(CMAKE_C_VISIBILITY_PRESET hidden)
    foreach(option ANIM_UTILS CWEBP DWEBP GIF2WEBP IMG2WEBP VWEBP WEBPINFO LIBWEBPMUX WEBPMUX EXTRAS)
        set(WEBP_BUILD_${option} OFF)
    endforeach()
    add_subdirectory("${photocrap_webp_SOURCE_DIR}" "${photocrap_webp_BINARY_DIR}" EXCLUDE_FROM_ALL)
endfunction()
photocrap_add_webp_encoder()

function(photocrap_install_webp_license)
    if(APPLE)
        set(notices "PhotoCrap.app/Contents/Resources/third-party")
    elseif(WIN32)
        set(notices "third-party")
    else()
        set(notices "share/PhotoCrap/third-party")
    endif()
    install(FILES "${photocrap_webp_SOURCE_DIR}/COPYING" "${photocrap_webp_SOURCE_DIR}/PATENTS"
            DESTINATION "${notices}/libwebp-1.6.0")
endfunction()
