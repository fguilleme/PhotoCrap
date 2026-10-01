cmake_minimum_required(VERSION 3.24)
project(PhotoCrap VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2)
qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_library(photocrap_core STATIC src/Document.cpp src/Document.h)
target_link_libraries(photocrap_core PUBLIC Qt6::Core Qt6::Gui)
target_include_directories(photocrap_core PUBLIC src)

qt_add_executable(PhotoCrap
    src/main.cpp
    src/CanvasItem.cpp src/CanvasItem.h
    src/ShortcutManager.cpp src/ShortcutManager.h
)
# Homebrew exposes QML plugins through relative symlinks. Scan the real module
# directory so Qt's deploy step copies binaries, not links into the Cellar.
set(photocrap_qml_import_paths "")
if(APPLE)
    file(REAL_PATH "${Qt6Qml_DIR}/Qt6QmlConfig.cmake" photocrap_qml_config)
    get_filename_component(photocrap_qml_config_dir "${photocrap_qml_config}" DIRECTORY)
    get_filename_component(photocrap_qml_prefix "${photocrap_qml_config_dir}/../../.." ABSOLUTE)
    if(EXISTS "${photocrap_qml_prefix}/share/qt/qml/QtQuick/qmldir")
        list(APPEND photocrap_qml_import_paths "${photocrap_qml_prefix}/share/qt/qml")
    endif()
endif()
qt_add_qml_module(PhotoCrap
    URI PhotoCrap VERSION 1.0
    IMPORT_PATH ${photocrap_qml_import_paths}
    QML_FILES qml/Main.qml qml/IconButton.qml qml/PanelTitle.qml qml/ContextMenuItem.qml
)
target_link_libraries(PhotoCrap PRIVATE photocrap_core Qt6::Quick Qt6::Qml Qt6::QuickControls2)
set_target_properties(PhotoCrap PROPERTIES
    MACOSX_BUNDLE TRUE WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER org.photocrap.editor
    MACOSX_BUNDLE_BUNDLE_NAME "PhotoCrap"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
)
if(MSVC)
    target_compile_options(photocrap_core PRIVATE /W4 /utf-8)
    target_compile_options(PhotoCrap PRIVATE /utf-8)
else()
    target_compile_options(photocrap_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

include(CTest)
if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    target_sources(PhotoCrap PRIVATE tests/UiSmoke.h)
    target_compile_definitions(PhotoCrap PRIVATE PHOTOCRAP_TESTING)
    target_link_libraries(PhotoCrap PRIVATE Qt6::Test)
    qt_add_executable(photocrap_tests
        tests/DocumentTest.cpp
        src/ShortcutManager.cpp src/ShortcutManager.h
    )
    target_link_libraries(photocrap_tests PRIVATE photocrap_core Qt6::Test)
    add_test(NAME document COMMAND photocrap_tests)
    add_test(NAME qml_smoke COMMAND PhotoCrap --smoke-test)
    set_tests_properties(document qml_smoke PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 45)
endif()

include(GNUInstallDirs)
install(TARGETS PhotoCrap BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
qt_generate_deploy_qml_app_script(TARGET PhotoCrap OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
set(CPACK_PACKAGE_NAME "PhotoCrap")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_GENERATOR "ZIP")
include(CPack)
