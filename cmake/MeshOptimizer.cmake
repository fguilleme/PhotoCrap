include(FetchContent)

FetchContent_Declare(photocrap_meshoptimizer
    URL https://codeload.github.com/zeux/meshoptimizer/tar.gz/refs/tags/v1.3
    URL_HASH SHA256=ca4f47d451201593904b9e3067fa54b65db94bc34ab5173864950862b748a145
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
    SOURCE_SUBDIR photocrap-no-upstream-build
)
FetchContent_MakeAvailable(photocrap_meshoptimizer)

function(photocrap_add_meshoptimizer)
    set(MESHOPT_INSTALL OFF)
    set(MESHOPT_BUILD_SHARED_LIBS OFF)
    add_subdirectory("${photocrap_meshoptimizer_SOURCE_DIR}"
        "${photocrap_meshoptimizer_BINARY_DIR}" EXCLUDE_FROM_ALL)
endfunction()
photocrap_add_meshoptimizer()
