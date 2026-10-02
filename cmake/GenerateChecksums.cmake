if(NOT DEFINED PACKAGE_DIR)
    message(FATAL_ERROR "PACKAGE_DIR is required")
endif()

file(GLOB package_files
    "${PACKAGE_DIR}/*.dmg"
    "${PACKAGE_DIR}/*.exe"
    "${PACKAGE_DIR}/*.zip"
    "${PACKAGE_DIR}/*.AppImage"
)

if(NOT package_files)
    message(FATAL_ERROR "No packages found in ${PACKAGE_DIR}")
endif()

foreach(package_file IN LISTS package_files)
    file(SHA256 "${package_file}" package_sha256)
    cmake_path(GET package_file FILENAME package_name)
    file(WRITE "${package_file}.sha256" "${package_sha256}  ${package_name}\n")
endforeach()
