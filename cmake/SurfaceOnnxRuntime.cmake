include(FetchContent)

# Use the same CPU runtime for the desktop builds and the packaged application.
set(ort_version "1.23.2")
string(TOLOWER "${CMAKE_SYSTEM_PROCESSOR}" ort_cpu)
if(APPLE)
    list(LENGTH CMAKE_OSX_ARCHITECTURES ort_arch_count)
    if(ort_arch_count GREATER 1)
        set(ort_platform osx-universal2)
    else()
        if(CMAKE_OSX_ARCHITECTURES)
            set(ort_cpu "${CMAKE_OSX_ARCHITECTURES}")
        endif()
        if(ort_cpu STREQUAL "arm64")
            set(ort_platform osx-arm64)
        else()
            set(ort_platform osx-x64)
        endif()
    endif()
elseif(WIN32)
    if(CMAKE_GENERATOR_PLATFORM)
        string(TOLOWER "${CMAKE_GENERATOR_PLATFORM}" ort_cpu)
    endif()
    if(ort_cpu MATCHES "^(arm64|aarch64)(,|$)")
        set(ort_platform win-arm64)
    elseif(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(ort_platform win-x64)
    endif()
elseif(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    if(ort_cpu MATCHES "^(arm64|aarch64)$")
        set(ort_platform linux-arm64)
    elseif(ort_cpu MATCHES "^(amd64|x86_64)$")
        set(ort_platform linux-x64)
    endif()
endif()
if(NOT ort_platform)
    return()
endif()

# The official NuGet package contains the C/C++ headers and CPU libraries for
# all desktop architectures, plus the notices shipped with the application.
FetchContent_Declare(photocrap_onnx
    URL "https://api.nuget.org/v3-flatcontainer/microsoft.ml.onnxruntime/${ort_version}/microsoft.ml.onnxruntime.${ort_version}.nupkg"
    URL_HASH SHA256=25fe172f7fcdf34f2b5b02c8b997f6b88abc282956849d4af4335af23d0c4e4a
    DOWNLOAD_EXTRACT_TIMESTAMP FALSE
    TLS_VERIFY TRUE
)
FetchContent_MakeAvailable(photocrap_onnx)
set(ort_runtimes "${photocrap_onnx_SOURCE_DIR}/runtimes")
set(ort_lib "${ort_runtimes}/${ort_platform}/native")
add_library(photocrap_onnx_runtime SHARED IMPORTED GLOBAL)
set_target_properties(photocrap_onnx_runtime PROPERTIES
    INTERFACE_INCLUDE_DIRECTORIES "${photocrap_onnx_SOURCE_DIR}/build/native/include")
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
else()
    # NuGet omits the version in the filename; use the name the dynamic loader
    # expects in both the build tree and the application package.
    set(ort_stage "${CMAKE_CURRENT_BINARY_DIR}/onnxruntime/${ort_platform}")
    file(MAKE_DIRECTORY "${ort_stage}")
    if(APPLE)
        set(ort_binary "${ort_stage}/libonnxruntime.${ort_version}.dylib")
        if(ort_platform STREQUAL "osx-universal2")
            find_program(ort_lipo lipo REQUIRED)
            execute_process(COMMAND "${ort_lipo}" -create
                "${ort_runtimes}/osx-arm64/native/libonnxruntime.dylib"
                "${ort_runtimes}/osx-x64/native/libonnxruntime.dylib"
                -output "${ort_binary}" COMMAND_ERROR_IS_FATAL ANY)
        else()
            file(COPY_FILE "${ort_lib}/libonnxruntime.dylib" "${ort_binary}" ONLY_IF_DIFFERENT)
        endif()
    else()
        set(ort_binary "${ort_stage}/libonnxruntime.so.1")
        file(COPY_FILE "${ort_lib}/libonnxruntime.so" "${ort_binary}" ONLY_IF_DIFFERENT)
        set_target_properties(photocrap_onnx_runtime PROPERTIES IMPORTED_SONAME "libonnxruntime.so.1")
    endif()
    set_target_properties(photocrap_onnx_runtime PROPERTIES IMPORTED_LOCATION "${ort_binary}")
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
