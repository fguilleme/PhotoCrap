include(ExternalProject)
include(FetchContent)

FetchContent_Declare(photocrap_jpeg_sources
    URL https://github.com/libjpeg-turbo/libjpeg-turbo/releases/download/3.2.0/libjpeg-turbo-3.2.0.tar.gz
    URL_HASH SHA256=6f30092cef9fb839779646608f4ee14ae3cbac989c47fa05e841b0841f09878e
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR photocrap-no-upstream-build
)
FetchContent_MakeAvailable(photocrap_jpeg_sources)
set(jpeg_build "${CMAKE_CURRENT_BINARY_DIR}/jpeg-export")
file(MAKE_DIRECTORY "${jpeg_build}")
if(MSVC)
    set(jpeg_archive "jpeg-static.lib")
else()
    set(jpeg_archive "${CMAKE_STATIC_LIBRARY_PREFIX}jpeg${CMAKE_STATIC_LIBRARY_SUFFIX}")
endif()
get_property(jpeg_multiconfig GLOBAL PROPERTY GENERATOR_IS_MULTI_CONFIG)
if(jpeg_multiconfig)
    set(jpeg_byproduct "${jpeg_build}/lib/$<CONFIG>/${jpeg_archive}")
else()
    set(jpeg_byproduct "${jpeg_build}/lib/${jpeg_archive}")
endif()
string(REPLACE ";" "|" jpeg_archs "${CMAKE_OSX_ARCHITECTURES}")
ExternalProject_Add(photocrap_jpeg_build
    SOURCE_DIR "${photocrap_jpeg_sources_SOURCE_DIR}"
    BINARY_DIR "${jpeg_build}"
    LIST_SEPARATOR |
    CMAKE_ARGS
        "-DCMAKE_BUILD_TYPE:STRING=${CMAKE_BUILD_TYPE}"
        "-DCMAKE_C_COMPILER:FILEPATH=${CMAKE_C_COMPILER}"
        "-DCMAKE_OSX_ARCHITECTURES:STRING=${jpeg_archs}"
        "-DCMAKE_OSX_DEPLOYMENT_TARGET:STRING=${CMAKE_OSX_DEPLOYMENT_TARGET}"
        "-DCMAKE_ARCHIVE_OUTPUT_DIRECTORY:PATH=${jpeg_build}/lib"
        -DCMAKE_C_VISIBILITY_PRESET=hidden
        -DENABLE_SHARED=OFF -DENABLE_STATIC=ON -DWITH_TOOLS=OFF -DWITH_TURBOJPEG=OFF
    BUILD_COMMAND "${CMAKE_COMMAND}" --build <BINARY_DIR> --config $<CONFIG> --target jpeg-static
    BUILD_BYPRODUCTS "${jpeg_byproduct}"
    INSTALL_COMMAND ""
)
add_library(photocrap_jpeg STATIC IMPORTED GLOBAL)
add_dependencies(photocrap_jpeg photocrap_jpeg_build)
target_include_directories(photocrap_jpeg INTERFACE
    "${photocrap_jpeg_sources_SOURCE_DIR}/src" "${jpeg_build}")
if(jpeg_multiconfig)
    foreach(config IN LISTS CMAKE_CONFIGURATION_TYPES)
        string(TOUPPER "${config}" upper)
        set_property(TARGET photocrap_jpeg PROPERTY "IMPORTED_LOCATION_${upper}"
            "${jpeg_build}/lib/${config}/${jpeg_archive}")
    endforeach()
else()
    set_property(TARGET photocrap_jpeg PROPERTY IMPORTED_LOCATION "${jpeg_byproduct}")
endif()

add_library(photocrap_jpeg_codec STATIC
    src/storage/JpegExportCodec.c src/storage/JpegExportCodec.h)
target_link_libraries(photocrap_jpeg_codec PRIVATE photocrap_jpeg)
set_target_properties(photocrap_jpeg_codec PROPERTIES AUTOMOC OFF AUTOUIC OFF
    C_VISIBILITY_PRESET hidden POSITION_INDEPENDENT_CODE ON)

function(photocrap_install_jpeg_license)
    if(APPLE)
        set(notices "PhotoCrap.app/Contents/Resources/third-party")
    elseif(WIN32)
        set(notices "third-party")
    else()
        set(notices "share/PhotoCrap/third-party")
    endif()
    install(FILES "${photocrap_jpeg_sources_SOURCE_DIR}/LICENSE.md"
                  "${photocrap_jpeg_sources_SOURCE_DIR}/README.ijg"
            DESTINATION "${notices}/libjpeg-turbo-3.2.0")
endfunction()
