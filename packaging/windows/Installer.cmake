if(CPACK_GENERATOR STREQUAL "NSIS")
    # Read at packaging time so CMake preserves the NSIS variables and makensis
    # can compile the generated script without resolving a source-tree path.
    file(READ "${CMAKE_CURRENT_LIST_DIR}/Projects.nsh" project_registration)
    set(CPACK_NSIS_EXTRA_INSTALL_COMMANDS "${project_registration}
!insertmacro RegisterPhotoCrapProjects")
    set(CPACK_NSIS_EXTRA_UNINSTALL_COMMANDS "!insertmacro UnregisterPhotoCrapProjects")
endif()
