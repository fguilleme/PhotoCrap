include(FetchContent)

# Use the same CPU runtime for the desktop builds and the packaged application.
set(ort_version "1.23.2")
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ort_cpu)
if(APPLE)
    list(LENGTH CMAKE_OSX_ARCHITECTURES ort_arch_count)
    if(ort_arch_count GREATER 1)
        set(ort_platform osx-universal2)
        set(ort_sha 49ae8e3a66ccb18d98ad3fe7f5906b6d7887df8a5edd40f49eb2b14e20885809)
    else()
        if(CMAKE_OSX_ARCHITECTURES)
            set(ort_cpu "${CMAKE_OSX_ARCHITECTURES}")
        endif()
        if(ort_cpu STREQUAL "arm64")
            set(ort_platform osx-arm64)
            set(ort_sha b4d513ab2b26f088c66891dbbc1408166708773d7cc4163de7bdca0e9bbb7856)
        else()
            set(ort_platform osx-x86_64)
            set(ort_sha d10359e16347b57d9959f7e80a225a5b4a66ed7d7e007274a15cae86836485a6)
        endif()
    endif()
elseif(WIN32)
    if(CMAKE_GENERATOR_PLATFORM)
        string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" ort_cpu)
    endif()
    if(ort_cpu MATCHES "^(arm64|aarch64)(,|$)")
        set(ort_platform win-arm64)
        set(ort_sha 1cfe88b6435df3b5fb0e9f6bd7d6f5df1e887b6174de7f6e2a47bab956f3f168)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ort_platform win-x64)
        set(ort_sha 0b38df9af21834e41e73d602d90db5cb06dbd1ca618948b8f1d66d607ac9f3cd)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(ort_cpu MATCHES "^(arm64|aarch64)$")
        set(ort_platform linux-aarch64)
        set(ort_sha 7c63c73560ed76b1fac6cff8204ffe34fe180e70d6582b5332ec094810241e5c)
    elseif(ort_cpu MATCHES "^(amd64|x86_64)$")
        set(ort_platform linux-x64)
        set(ort_sha 1fa4dcaef22f6f7d5cd81b28c2800414350c10116f5fdd46a2160082551c5f9b)
    endif()
endif()
if(NOT ort_platform)
    return()
endif()

set(ort_suffix tgz)
if(WIN32)
    set(ort_suffix zip)
endif()
FetchContent_Declare(photocrap_onnx
    URL "https://github.com/microsoft/onnxruntime/releases/download/v${ort_version}/onnxruntime-${ort_platform}-${ort_version}.${ort_suffix}"
    URL_HASH "SHA256=${ort_sha}"
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    TLS_VERIFY TRUE
)
FetchContent_MakeAvailable(photocrap_onnx)
set(ort_lib "${photocrap_onnx_SOURCE_DIR}/lib")
add_library(photocrap_onnx_runtime SHARED IMPORTED GLOBAL)
set_target_properties(photocrap_onnx_runtime PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${photocrap_onnx_SOURCE_DIR}/include")
if(WIN32)
    set_target_properties(photocrap_onnx_runtime PROPERTIES
        IMPORTED_LOCATION "${ort_lib}/onnxruntime.dll"
        IMPORTED_IMPLIB "${ort_lib}/onnxruntime.lib")
    # The application and CTest executables share the default runtime directory.
    add_custom_target(photocrap_onnx_dll
        COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_FILE_DIR:PhotoCrap>"
        COMMAND ${CMAKE_COMMAND} -E copy_if_different
            "$<TARGET_FILE:photocrap_onnx_runtime>" "$<TARGET_FILE_DIR:PhotoCrap>/onnxruntime.dll"
        VERBATIM)
    add_dependencies(photocrap_core photocrap_onnx_dll)
elseif(APPLE)
    set_target_properties(photocrap_onnx_runtime PROPERTIES
        IMPORTED_LOCATION "${ort_lib}/libonnxruntime.${ort_version}.dylib")
else()
    set_target_properties(photocrap_onnx_runtime PROPERTIES
        IMPORTED_LOCATION "${ort_lib}/libonnxruntime.so.${ort_version}"
        IMPORTED_SONAME "libonnxruntime.so.1")
endif()
target_link_libraries(photocrap_core PRIVATE photocrap_onnx_runtime)
target_compile_definitions(photocrap_core PRIVATE PHOTOCRAP_LOCAL_SURFACE)

function(photocrap_install_onnx_runtime)
    if(NOT TARGET photocrap_onnx_runtime)
        return()
    endif()
    if(APPLE)
        add_custom_command(TARGET PhotoCrap POST_BUILD
            COMMAND ${CMAKE_COMMAND} -E make_directory "$<TARGET_BUNDLE_DIR:PhotoCrap>/Contents/Frameworks"
            COMMAND ${CMAKE_COMMAND} -E copy_if_different "$<TARGET_FILE:photocrap_onnx_runtime>"
                "$<TARGET_BUNDLE_DIR:PhotoCrap>/Contents/Frameworks"
            COMMAND /usr/bin/codesign --force --sign - --timestamp=none
                "$<TARGET_BUNDLE_DIR:PhotoCrap>/Contents/Frameworks/$<TARGET_FILE_NAME:photocrap_onnx_runtime>"
            VERBATIM)
        set_property(TARGET PhotoCrap APPEND PROPERTY BUILD_RPATH "@executable_path/../Frameworks")
        set_property(TARGET PhotoCrap APPEND PROPERTY INSTALL_RPATH "@executable_path/../Frameworks")
        install(FILES "$<TARGET_FILE:photocrap_onnx_runtime>"
            DESTINATION "PhotoCrap.app/Contents/Frameworks")
        set(notices "PhotoCrap.app/Contents/Resources/third-party")
    elseif(WIN32)
        install(FILES "$<TARGET_FILE:photocrap_onnx_runtime>" DESTINATION "${CMAKE_INSTALL_BINDIR}")
        set(notices "third-party")
    else()
        install(FILES "$<TARGET_FILE:photocrap_onnx_runtime>"
            DESTINATION "${CMAKE_INSTALL_LIBDIR}" RENAME libonnxruntime.so.1)
        set_property(TARGET PhotoCrap APPEND PROPERTY INSTALL_RPATH "$ORIGIN/../${CMAKE_INSTALL_LIBDIR}")
        set(notices "${CMAKE_INSTALL_DATADIR}/PhotoCrap/third-party")
    endif()
    install(FILES "${photocrap_onnx_SOURCE_DIR}/LICENSE" "${photocrap_onnx_SOURCE_DIR}/ThirdPartyNotices.txt"
        DESTINATION "${notices}/onnxruntime")
endfunction()
