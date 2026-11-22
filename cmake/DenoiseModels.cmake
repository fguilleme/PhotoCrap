# The archives contain ONNX graphs and the authors' model cards. Pin both
# versions so a saved adjustment renders with the same network after reopening.
include(FetchContent)
function(photocrap_denoise_model package digest license)
    FetchContent_Declare(${package}
        URL "https://github.com/darktable-org/darktable-ai/releases/download/release-5.6.0/${package}.dtmodel"
        URL_HASH "SHA256=${digest}"
        DOWNLOAD_EXTRACT_TIMESTAMP FALSE TLS_VERIFY TRUE)
    FetchContent_MakeAvailable(${package})
    set(files ${ARGN} config.json "${CMAKE_CURRENT_SOURCE_DIR}/packaging/licenses/${license}")
    foreach(entry IN LISTS files)
        get_filename_component(name "${entry}" NAME)
        if(IS_ABSOLUTE "${entry}")
            set(src "${entry}")
        else()
            set(src "${${package}_SOURCE_DIR}/${entry}")
        endif()
        if(APPLE)
            set_source_files_properties("${src}" PROPERTIES
                MACOSX_PACKAGE_LOCATION "Resources/models/${package}")
            target_sources(PhotoCrap PRIVATE "${src}")
        endif()
        # Non-bundled executables (including the test runners).
        file(MAKE_DIRECTORY "${CMAKE_BINARY_DIR}/models/${package}")
        file(COPY_FILE "${src}" "${CMAKE_BINARY_DIR}/models/${package}/${name}" ONLY_IF_DIFFERENT)
        if(NOT APPLE)
            install(FILES "${src}" DESTINATION "${CMAKE_INSTALL_BINDIR}/models/${package}")
        endif()
    endforeach()
endfunction()
photocrap_denoise_model(rawdenoise-nind d71b5f1e727c85a359e6f74dca9e2016c9d8fc3e2f7ac3e9b347d80ceca969af RawNIND.txt model_bayer.onnx model_linear.onnx)
photocrap_denoise_model(denoise-nafnet 4b5c0b9c2650956eaab9a3fc6ffe3e11d4dc4f55b246b7f3a0d2d72161b6980c NAFNet.txt model.onnx)
