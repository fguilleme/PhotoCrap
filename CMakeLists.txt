cmake_minimum_required(VERSION 3.24)
project(PhotoCrap VERSION 0.14.0 LANGUAGES CXX)
if(APPLE)
    enable_language(OBJCXX)
endif()

set(CMAKE_EXPORT_COMPILE_COMMANDS ON)
set(CMAKE_CXX_STANDARD 20)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
include(CTest)
include(cmake/RawDependencies.cmake)

set(PHOTOCRAP_CXX_SOURCES
    src/RawImageReader.cpp
    src/ToolInput.cpp
    src/CanvasItem.cpp
    src/Document.cpp
    src/DocumentAdjustments.cpp
    src/DocumentCompositor.cpp
    src/DocumentEditing.cpp
    src/DocumentFilters.cpp
    src/DocumentFrequency.cpp
    src/DocumentIO.cpp
    src/DocumentLayers.cpp
    src/DocumentPreview.cpp
    src/storage/ScratchImageCache.cpp
    src/storage/TiledImage.cpp
    src/tools/BrushTool.cpp
    src/tools/PenTool.cpp
    src/tools/PenPath.cpp
    src/tools/FillTool.cpp
    src/tools/MagicEraserTool.cpp
    src/tools/ShapeTools.cpp
    src/tools/RetouchTools.cpp
    src/tools/SelectionTools.cpp
    src/selection/SelectionCommands.cpp
    src/selection/SelectionGeometry.cpp
    src/selection/SelectionCoverage.cpp
    src/selection/ColorSelection.cpp
    src/tools/SmartSelectionTools.cpp
    src/tools/StampTools.cpp
    src/tools/SmudgeTool.cpp
    src/tools/ColorReplacementTool.cpp
    src/tools/MixerBrushTool.cpp
    src/tools/RedEyeTool.cpp
    src/tools/HealingBrushTool.cpp
    src/tools/SpotHealingTool.cpp
    src/tools/PatchTool.cpp
    src/tools/RegionHealing.cpp
    src/tools/ContentAwareMoveTool.cpp
    src/tools/ContentFill.cpp
    src/tools/RemoveTool.cpp
    src/tools/HistoryBrushTool.cpp
    src/tools/ArtHistoryBrushTool.cpp
    src/tools/ToolInteraction.cpp
    src/tools/ToolController.cpp
    src/tools/ToolSettings.cpp
    src/tools/EyedropperTools.cpp
    src/tools/CropTools.cpp
    src/tools/SliceTools.cpp
    src/filters/BlackWhiteFilter.cpp
    src/filters/BrightnessContrastFilter.cpp
    src/filters/ChannelMixerFilter.cpp
    src/filters/ColorFilter.cpp
    src/filters/ColorLookupFilter.cpp
    src/filters/CurvesFilter.cpp
    src/filters/GradientMapFilter.cpp
    src/filters/LightFilter.cpp
    src/filters/LevelsFilter.cpp
    src/filters/PhotoFilterFilter.cpp
    src/filters/PosterizeFilter.cpp
    src/filters/SelectiveColorFilter.cpp
    src/filters/ThresholdFilter.cpp
    src/LanguageManager.cpp
    src/RecentFilesManager.cpp
    src/ShortcutManager.cpp
    src/ShortcutCatalog.h
    src/WorkspaceSettings.cpp
    src/main.cpp
)
set(PHOTOCRAP_FORMAT_SOURCES
    src/storage/ScratchImageCache.h
    src/storage/TiledImage.h
    tests/ScratchImageCacheTest.cpp
    tests/TiledImageTest.cpp
    src/ImagePixels.h
    src/DocumentStorage.h
    src/RawImageReader.h
    src/ToolInput.h
    src/MacToolCursor.h
    src/MacToolCursor.mm
    ${PHOTOCRAP_CXX_SOURCES}
    src/CanvasItem.h
    src/Document.h
    src/ImageUtils.h
    src/tools/RegionHealing.h
    src/tools/ContentFill.h
    src/tools/PenPath.h
    src/selection/SelectionGeometry.h
    src/selection/SelectionCoverage.h
    src/filters/BlackWhiteFilter.h
    src/filters/BrightnessContrastFilter.h
    src/filters/ChannelMixerFilter.h
    src/filters/ColorFilter.h
    src/filters/ColorLookupFilter.h
    src/filters/CurvesFilter.h
    src/filters/GradientMapFilter.h
    src/filters/LightFilter.h
    src/filters/LevelsFilter.h
    src/filters/PhotoFilterFilter.h
    src/filters/PosterizeFilter.h
    src/filters/SelectiveColorFilter.h
    src/filters/ThresholdFilter.h
    src/LanguageManager.h
    tests/MacApplicationMenuTest.h
    tests/MacApplicationMenuTest.mm
    src/RecentFilesManager.h
    src/ShortcutManager.h
    src/WorkspaceSettings.h
    tests/DocumentTest.cpp
    tests/RawTestImage.h
    tests/UiSmoke.h
    tests/SelectionTest.cpp
    tests/SelectionUiSmoke.h
    tests/FileDialogSmoke.h
)

find_program(CLANG_FORMAT_EXECUTABLE NAMES clang-format)
if(CLANG_FORMAT_EXECUTABLE)
    add_custom_target(clang-format
        COMMAND ${CLANG_FORMAT_EXECUTABLE} -i ${PHOTOCRAP_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Formatting C++ sources"
        VERBATIM
    )
    add_custom_target(clang-format-check
        COMMAND ${CLANG_FORMAT_EXECUTABLE} --dry-run --Werror ${PHOTOCRAP_FORMAT_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Checking C++ source formatting"
        VERBATIM
    )
else()
    message(STATUS "clang-format not found; formatting targets are disabled")
endif()

find_program(CLANG_TIDY_EXECUTABLE NAMES clang-tidy)
if(CLANG_TIDY_EXECUTABLE AND CMAKE_GENERATOR MATCHES "Ninja|Makefiles")
    set(PHOTOCRAP_TIDY_SOURCES ${PHOTOCRAP_CXX_SOURCES})
    set(PHOTOCRAP_TIDY_ARGUMENTS
        -p=${CMAKE_CURRENT_BINARY_DIR}
        --config-file=${CMAKE_CURRENT_SOURCE_DIR}/.clang-tidy
    )
    if(APPLE)
        execute_process(
            COMMAND xcrun --show-sdk-path
            OUTPUT_VARIABLE PHOTOCRAP_MACOS_SDK
            OUTPUT_STRIP_TRAILING_WHITESPACE
            ERROR_QUIET
        )
        if(EXISTS "${PHOTOCRAP_MACOS_SDK}/usr/include/c++/v1")
            list(APPEND PHOTOCRAP_TIDY_ARGUMENTS
                --extra-arg-before=-isystem
                --extra-arg-before=${PHOTOCRAP_MACOS_SDK}/usr/include/c++/v1
                --extra-arg-before=-isysroot
                --extra-arg-before=${PHOTOCRAP_MACOS_SDK}
            )
        endif()
    endif()
    if(BUILD_TESTING)
        list(APPEND PHOTOCRAP_TIDY_SOURCES tests/DocumentTest.cpp)
    endif()
    add_custom_target(clang-tidy
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            ${PHOTOCRAP_TIDY_ARGUMENTS}
            ${PHOTOCRAP_TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Running clang-tidy"
        VERBATIM
    )
    add_custom_target(clang-tidy-fix
        COMMAND ${CLANG_TIDY_EXECUTABLE}
            ${PHOTOCRAP_TIDY_ARGUMENTS}
            --fix
            ${PHOTOCRAP_TIDY_SOURCES}
        WORKING_DIRECTORY ${CMAKE_CURRENT_SOURCE_DIR}
        COMMENT "Applying safe clang-tidy fixes"
        VERBATIM
    )
elseif(CLANG_TIDY_EXECUTABLE)
    message(STATUS "clang-tidy target requires a Ninja or Makefiles generator")
else()
    message(STATUS "clang-tidy not found; lint target is disabled")
endif()

find_package(Qt6 6.5 REQUIRED COMPONENTS Core Gui Qml Quick QuickControls2 Concurrent LinguistTools)
qt_standard_project_setup(REQUIRES 6.5)
if(QT_KNOWN_POLICY_QTP0004)
    qt_policy(SET QTP0004 NEW)
endif()

qt_add_library(photocrap_core STATIC
    src/storage/ScratchImageCache.cpp src/storage/ScratchImageCache.h
    src/storage/TiledImage.cpp src/storage/TiledImage.h
    src/tools/PenTool.cpp
    src/tools/PenPath.cpp src/tools/PenPath.h
    src/Document.cpp
    src/Document.h
    src/DocumentAdjustments.cpp
    src/DocumentCompositor.cpp
    src/DocumentEditing.cpp
    src/DocumentFilters.cpp
    src/DocumentFrequency.cpp
    src/DocumentIO.cpp
    src/RawImageReader.cpp
    src/RawImageReader.h
    src/DocumentLayers.cpp
    src/DocumentPreview.cpp
    src/tools/BrushTool.cpp
    src/tools/FillTool.cpp
    src/tools/MagicEraserTool.cpp
    src/tools/ShapeTools.cpp
    src/tools/RetouchTools.cpp
    src/tools/SelectionTools.cpp
    src/selection/SelectionCommands.cpp
    src/selection/SelectionGeometry.cpp
    src/selection/SelectionCoverage.cpp
    src/selection/ColorSelection.cpp
    src/tools/SmartSelectionTools.cpp
    src/tools/StampTools.cpp
    src/tools/SmudgeTool.cpp
    src/tools/ColorReplacementTool.cpp
    src/tools/MixerBrushTool.cpp
    src/tools/RedEyeTool.cpp
    src/tools/HealingBrushTool.cpp
    src/tools/SpotHealingTool.cpp
    src/tools/PatchTool.cpp
    src/tools/RegionHealing.cpp
    src/tools/ContentAwareMoveTool.cpp
    src/tools/ContentFill.cpp
    src/tools/RemoveTool.cpp
    src/tools/HistoryBrushTool.cpp
    src/tools/ArtHistoryBrushTool.cpp
    src/tools/ToolInteraction.cpp
    src/tools/ToolController.cpp
    src/tools/ToolSettings.cpp
    src/tools/EyedropperTools.cpp
    src/tools/CropTools.cpp
    src/tools/SliceTools.cpp
    src/ImageUtils.h
    src/filters/BlackWhiteFilter.cpp
    src/filters/BlackWhiteFilter.h
    src/filters/BrightnessContrastFilter.cpp
    src/filters/BrightnessContrastFilter.h
    src/filters/ChannelMixerFilter.cpp
    src/filters/ChannelMixerFilter.h
    src/filters/ColorFilter.cpp
    src/filters/ColorFilter.h
    src/filters/ColorLookupFilter.cpp
    src/filters/ColorLookupFilter.h
    src/filters/CurvesFilter.cpp
    src/filters/CurvesFilter.h
    src/filters/GradientMapFilter.cpp
    src/filters/GradientMapFilter.h
    src/filters/LightFilter.cpp
    src/filters/LightFilter.h
    src/filters/LevelsFilter.cpp
    src/filters/LevelsFilter.h
    src/filters/PhotoFilterFilter.cpp
    src/filters/PhotoFilterFilter.h
    src/filters/PosterizeFilter.cpp
    src/filters/PosterizeFilter.h
    src/filters/SelectiveColorFilter.cpp
    src/filters/SelectiveColorFilter.h
    src/filters/ThresholdFilter.cpp
    src/filters/ThresholdFilter.h
)
target_link_libraries(photocrap_core PUBLIC Qt6::Core Qt6::Gui Qt6::Qml Qt6::Concurrent)
target_link_libraries(photocrap_core PRIVATE photocrap_libraw)
target_include_directories(photocrap_core PUBLIC src)
if(APPLE)
    target_compile_definitions(photocrap_core PRIVATE PHOTOCRAP_USE_ACCELERATE)
    target_link_libraries(photocrap_core PRIVATE "-framework Accelerate")
endif()

qt_add_executable(PhotoCrap
    src/Application.h
    src/main.cpp
    src/ToolInput.cpp src/ToolInput.h
    src/CanvasItem.cpp src/CanvasItem.h
    src/ShortcutManager.cpp src/ShortcutManager.h
    src/LanguageManager.cpp src/LanguageManager.h
    src/RecentFilesManager.cpp src/RecentFilesManager.h
    src/WorkspaceSettings.cpp src/WorkspaceSettings.h
)
if(APPLE)
    target_sources(PhotoCrap PRIVATE
        src/MacToolCursor.h src/MacToolCursor.mm
        packaging/macos/PhotoCrap.icns
    )
    target_link_libraries(PhotoCrap PRIVATE "-framework AppKit")
    set_source_files_properties(packaging/macos/PhotoCrap.icns PROPERTIES
        MACOSX_PACKAGE_LOCATION "Resources"
    )
elseif(WIN32)
    target_sources(PhotoCrap PRIVATE packaging/windows/PhotoCrap.rc)
endif()
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
target_compile_definitions(PhotoCrap PRIVATE PHOTOCRAP_VERSION="${PROJECT_VERSION}")

qt_add_qml_module(PhotoCrap
    URI PhotoCrap VERSION 1.0
    OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/qml/PhotoCrap"
    IMPORT_PATH ${photocrap_qml_import_paths}
    QML_FILES
        qml/Main.qml
        qml/ScratchCachePreferences.qml
        qml/DialogActionButton.qml
        qml/DockablePanel.qml
        qml/IconButton.qml
        qml/ToolIcon.qml
        qml/MixerBrushOptions.qml
        qml/RedEyeOptions.qml
        qml/HealingBrushOptions.qml
        qml/SpotHealingOptions.qml
        qml/PatchOptions.qml
        qml/PenOptions.qml
        qml/ColorSelectionPreview.qml
        qml/ColorSelectionDialog.qml
        qml/SelectionMenu.qml
        qml/SelectionModifyDialog.qml
        qml/PenOverlay.qml
        qml/ContentAwareMoveOptions.qml
        qml/RemoveOptions.qml
        qml/HistoryBrushOptions.qml
        qml/ArtHistoryBrushOptions.qml
        qml/EyedropperOptions.qml
        qml/EyedropperButton.qml
        qml/RulerOptions.qml
        qml/CropOptions.qml
        qml/CropOverlay.qml
        qml/ScreenOverlayCanvas.qml
        qml/SliceOptions.qml
        qml/SliceOverlay.qml
        qml/InformationPanel.qml
        qml/PanelTitle.qml
        qml/TrashIcon.qml
        qml/ContextMenuItem.qml
        qml/LayerContextMenu.qml
        qml/MaskContextMenu.qml
        qml/DarkMenu.qml
        qml/DarkMenuItem.qml
        qml/DarkSlider.qml
        qml/DarkSpinBox.qml
        qml/CurvesEditor.qml
        qml/ColorEditor.qml
        qml/ColorLookupEditor.qml
        qml/ColorBalanceEditor.qml
        qml/BlackWhiteEditor.qml
        qml/ChannelMixerEditor.qml
        qml/GradientMapEditor.qml
        qml/FrequencySeparationEditor.qml
        qml/LightEditor.qml
        qml/LayerPropertiesActions.qml
        qml/LevelsEditor.qml
        qml/PhotoFilterEditor.qml
        qml/PosterizeEditor.qml
        qml/SelectiveColorEditor.qml
        qml/ThresholdEditor.qml
)
qt_add_translations(PhotoCrap
    TS_FILES translations/PhotoCrap_en.ts
    RESOURCE_PREFIX "/i18n"
)
target_link_libraries(PhotoCrap PRIVATE photocrap_core Qt6::Quick Qt6::Qml Qt6::QuickControls2)
set_target_properties(PhotoCrap PROPERTIES
    MACOSX_BUNDLE TRUE WIN32_EXECUTABLE TRUE
    MACOSX_BUNDLE_GUI_IDENTIFIER org.photocrap.editor
    MACOSX_BUNDLE_BUNDLE_NAME "PhotoCrap"
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION}
    MACOSX_BUNDLE_ICON_FILE "PhotoCrap.icns"
)
if(MSVC)
    target_compile_options(photocrap_core PRIVATE /W4 /utf-8)
    target_compile_options(PhotoCrap PRIVATE /utf-8)
else()
    target_compile_options(photocrap_core PRIVATE -Wall -Wextra -Wpedantic)
endif()

if(BUILD_TESTING)
    find_package(Qt6 REQUIRED COMPONENTS Test)
    target_sources(PhotoCrap PRIVATE tests/UiSmoke.h)
    if(APPLE)
        target_sources(PhotoCrap PRIVATE tests/MacApplicationMenuTest.h tests/MacApplicationMenuTest.mm
            tests/MacToolCursorTest.h tests/MacToolCursorTest.mm)
        add_test(NAME macos_tool_cursors COMMAND PhotoCrap --smoke-test --native-cursor-test)
        add_test(NAME macos_pen_input COMMAND PhotoCrap --smoke-test --native-pen-test)
        set_tests_properties(macos_pen_input PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=cocoa;QT_QUICK_BACKEND=software"
            TIMEOUT 30
        )
        set_tests_properties(macos_tool_cursors PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=cocoa;QT_QUICK_BACKEND=software"
            TIMEOUT 30
        )
        add_test(NAME macos_preferences COMMAND PhotoCrap --smoke-test --native-menu-test)
        set_tests_properties(macos_preferences PROPERTIES
            ENVIRONMENT "QT_QPA_PLATFORM=cocoa;QT_QUICK_BACKEND=software"
            TIMEOUT 30
        )
    endif()
    target_compile_definitions(PhotoCrap PRIVATE PHOTOCRAP_TESTING)
    target_link_libraries(PhotoCrap PRIVATE Qt6::Test)
    qt_add_executable(photocrap_tests
        tests/DocumentTest.cpp
        src/ShortcutManager.cpp src/ShortcutManager.h
        src/RecentFilesManager.cpp src/RecentFilesManager.h
        src/WorkspaceSettings.cpp src/WorkspaceSettings.h
    )
    target_link_libraries(photocrap_tests PRIVATE photocrap_core Qt6::Test)
    add_test(NAME document COMMAND photocrap_tests)
    qt_add_executable(photocrap_scratch_tests tests/ScratchImageCacheTest.cpp)
    target_link_libraries(photocrap_scratch_tests PRIVATE photocrap_core Qt6::Test)
    add_test(NAME scratch_cache COMMAND photocrap_scratch_tests)
    set_tests_properties(scratch_cache PROPERTIES TIMEOUT 30)
    qt_add_executable(photocrap_tiled_tests tests/TiledImageTest.cpp)
    target_link_libraries(photocrap_tiled_tests PRIVATE photocrap_core Qt6::Test)
    add_test(NAME tiled_image COMMAND photocrap_tiled_tests)
    set_tests_properties(tiled_image PROPERTIES TIMEOUT 30)
    qt_add_executable(photocrap_pen_tests tests/PenToolTest.cpp)
    target_link_libraries(photocrap_pen_tests PRIVATE photocrap_core Qt6::Test)
    add_test(NAME pen_tool COMMAND photocrap_pen_tests)
    set_tests_properties(pen_tool PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 30)
    qt_add_executable(photocrap_selection_tests tests/SelectionTest.cpp)
    target_link_libraries(photocrap_selection_tests PRIVATE photocrap_core Qt6::Test)
    add_test(NAME selection COMMAND photocrap_selection_tests)
    set_tests_properties(selection PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 45)
    add_test(NAME file_dialog_fallback COMMAND PhotoCrap --smoke-test --file-dialog-test)
    set_tests_properties(file_dialog_fallback PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software" TIMEOUT 25)
    add_test(NAME qml_smoke COMMAND PhotoCrap --smoke-test)
    add_test(NAME shutdown_discard COMMAND PhotoCrap --smoke-test --shutdown-test)
    add_test(NAME shutdown_save COMMAND PhotoCrap --smoke-test --shutdown-test --shutdown-save)
    add_test(NAME shutdown_clean COMMAND PhotoCrap --smoke-test --shutdown-test --shutdown-clean)
    set_tests_properties(document qml_smoke shutdown_discard shutdown_save shutdown_clean PROPERTIES
        ENVIRONMENT "QT_QPA_PLATFORM=offscreen;QT_QUICK_BACKEND=software"
    )
    # The engine suite includes multiple independent full-resolution async
    # deadlines; its aggregate timeout must be greater than their sum.
    set_tests_properties(document PROPERTIES TIMEOUT 180)
    set_tests_properties(qml_smoke PROPERTIES TIMEOUT 60)
    set_tests_properties(shutdown_discard shutdown_save shutdown_clean PROPERTIES TIMEOUT 15)
endif()

include(GNUInstallDirs)
install(TARGETS PhotoCrap BUNDLE DESTINATION . RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR})
qt_generate_deploy_qml_app_script(TARGET PhotoCrap OUTPUT_SCRIPT deploy_script NO_UNSUPPORTED_PLATFORM_ERROR)
install(SCRIPT ${deploy_script})
photocrap_install_raw_sources()
if(APPLE)
    # Qt's deployment step adds frameworks, plugins and QML resources after
    # the executable has received its linker signature. Seal the completed
    # bundle so macOS does not terminate it with CODESIGNING/Invalid Page.
    install(CODE [=[
        set(photocrap_app "$ENV{DESTDIR}${CMAKE_INSTALL_PREFIX}/PhotoCrap.app")
        execute_process(
            COMMAND /usr/bin/codesign --force --deep --sign - "${photocrap_app}"
            RESULT_VARIABLE photocrap_codesign_result
            ERROR_VARIABLE photocrap_codesign_error
        )
        if(NOT photocrap_codesign_result EQUAL 0)
            message(FATAL_ERROR
                "Failed to seal ${photocrap_app}: ${photocrap_codesign_error}")
        endif()
    ]=])
endif()
set(CPACK_PACKAGE_NAME "PhotoCrap")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_VENDOR "PhotoCrap")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Cross-platform Qt 6 image editor")
set(CPACK_PACKAGE_FILE_NAME
    "PhotoCrap-${PROJECT_VERSION}-${CMAKE_SYSTEM_NAME}-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_PACKAGE_CHECKSUM SHA256)
if(APPLE)
    # Produces a drag-and-drop DMG containing the fully deployed .app bundle.
    set(CPACK_GENERATOR "DragNDrop")
elseif(WIN32)
    # Publish both a conventional NSIS installer and a portable ZIP. Both
    # contain PhotoCrap.exe, Qt DLLs, plugins and QML imports.
    set(CPACK_GENERATOR "NSIS;ZIP")
    set(CPACK_NSIS_DISPLAY_NAME "PhotoCrap")
    set(CPACK_NSIS_PACKAGE_NAME "PhotoCrap")
    set(CPACK_NSIS_MUI_ICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/PhotoCrap.ico")
    set(CPACK_NSIS_MUI_UNIICON "${CMAKE_CURRENT_SOURCE_DIR}/packaging/windows/PhotoCrap.ico")
    set(CPACK_NSIS_INSTALL_ROOT "$PROGRAMFILES64")
    set(CPACK_NSIS_ENABLE_UNINSTALL_BEFORE_INSTALL ON)
    set(CPACK_NSIS_MODIFY_PATH OFF)
    set(CPACK_PACKAGE_EXECUTABLES "PhotoCrap" "PhotoCrap")
    set(CPACK_CREATE_DESKTOP_LINKS "PhotoCrap")
else()
    # Portable Linux tree with Qt libraries and plugins beside the executable.
    set(CPACK_GENERATOR "TGZ")
endif()
include(CPack)
