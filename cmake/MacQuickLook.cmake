if(NOT APPLE)
    return()
endif()

add_library(photocrap_quicklook_reader STATIC src/mac/quicklook/PCRPreviewReader.mm)
set_target_properties(photocrap_quicklook_reader PROPERTIES AUTOMOC OFF AUTOUIC OFF)
target_include_directories(photocrap_quicklook_reader PUBLIC src src/mac/quicklook)
target_compile_options(photocrap_quicklook_reader PRIVATE -fobjc-arc -fapplication-extension)
target_link_libraries(photocrap_quicklook_reader PUBLIC "-framework Foundation" "-framework CoreGraphics" "-framework ImageIO")

function(photocrap_quicklook target suffix point principal source attributes framework)
    set(PCR_NAME "${target}")
    set(PCR_ID "org.photocrap.editor.quicklook.${suffix}")
    set(PCR_POINT "com.apple.quicklook.${point}")
    set(PCR_CLASS "${principal}")
    set(PCR_ATTRIBUTES "${attributes}")
    configure_file(packaging/macos/QuickLook-Info.plist.in "quicklook/${target}-Info.plist" @ONLY)
    add_executable(${target} MACOSX_BUNDLE "${source}")
    set_target_properties(${target} PROPERTIES
        AUTOMOC OFF AUTOUIC OFF BUNDLE_EXTENSION appex
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}/quicklook"
        MACOSX_BUNDLE_INFO_PLIST "${CMAKE_CURRENT_BINARY_DIR}/quicklook/${target}-Info.plist")
    target_compile_options(${target} PRIVATE -fobjc-arc -fapplication-extension)
    target_link_options(${target} PRIVATE -Wl,-e,_NSExtensionMain)
    target_link_libraries(${target} PRIVATE photocrap_quicklook_reader "-framework ${framework}")
    add_custom_command(TARGET ${target} POST_BUILD
        COMMAND /usr/bin/codesign --force --sign - --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/QuickLook.entitlements" "$<TARGET_BUNDLE_DIR:${target}>"
        VERBATIM)
    add_dependencies(PhotoCrap ${target})
    set_property(TARGET PhotoCrap APPEND PROPERTY LINK_DEPENDS "$<TARGET_FILE:${target}>")
    add_custom_command(TARGET PhotoCrap POST_BUILD
        COMMAND /usr/bin/codesign --force --sign - --entitlements "${CMAKE_CURRENT_SOURCE_DIR}/packaging/macos/QuickLook.entitlements" "$<TARGET_BUNDLE_DIR:${target}>"
        COMMAND ${CMAKE_COMMAND} -E copy_directory "$<TARGET_BUNDLE_DIR:${target}>" "$<TARGET_BUNDLE_DIR:PhotoCrap>/Contents/PlugIns/${target}.appex"
        VERBATIM)
endfunction()

photocrap_quicklook(PhotoCrapPreview preview preview PCRPreviewProvider
    src/mac/quicklook/PreviewProvider.mm "<key>QLIsDataBasedPreview</key><true/>" QuickLookUI)
photocrap_quicklook(PhotoCrapThumbnail thumbnail thumbnail PCRThumbnailProvider
    src/mac/quicklook/ThumbnailProvider.mm "<key>QLThumbnailMinimumDimension</key><integer>0</integer>" QuickLookThumbnailing)
add_custom_command(TARGET PhotoCrap POST_BUILD
    COMMAND /usr/bin/codesign --force --sign - "$<TARGET_BUNDLE_DIR:PhotoCrap>" VERBATIM)

if(BUILD_TESTING)
    add_executable(photocrap_quicklook_tests tests/MacQuickLookTest.mm)
    set_target_properties(photocrap_quicklook_tests PROPERTIES AUTOMOC OFF AUTOUIC OFF)
    target_compile_options(photocrap_quicklook_tests PRIVATE -fobjc-arc)
    target_link_libraries(photocrap_quicklook_tests PRIVATE photocrap_quicklook_reader "-framework UniformTypeIdentifiers" "-framework QuickLookThumbnailing")
    add_test(NAME macos_quicklook_reader COMMAND photocrap_quicklook_tests)
    set_tests_properties(macos_quicklook_reader PROPERTIES TIMEOUT 20)
endif()
