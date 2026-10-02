if(NOT DEFINED DEPLOY_ROOT OR NOT IS_DIRECTORY "${DEPLOY_ROOT}")
    message(FATAL_ERROR "DEPLOY_ROOT must point to the installed PhotoCrap tree")
endif()
if(NOT DEFINED DEPLOY_PLATFORM)
    message(FATAL_ERROR "DEPLOY_PLATFORM must be Linux, Windows or macOS")
endif()

function(require_deployed_file description)
    set(matches "")
    foreach(pattern IN LISTS ARGN)
        file(GLOB_RECURSE found LIST_DIRECTORIES false "${DEPLOY_ROOT}/${pattern}")
        list(APPEND matches ${found})
    endforeach()
    if(NOT matches)
        message(FATAL_ERROR "Standalone package is missing ${description} in ${DEPLOY_ROOT}")
    endif()
    list(GET matches 0 first_match)
    file(RELATIVE_PATH relative_match "${DEPLOY_ROOT}" "${first_match}")
    message(STATUS "Found ${description}: ${relative_match}")
endfunction()

if(DEPLOY_PLATFORM STREQUAL "macOS")
    require_deployed_file("the application executable" "PhotoCrap.app/Contents/MacOS/PhotoCrap")
    require_deployed_file("Qt Core" "PhotoCrap.app/Contents/Frameworks/QtCore.framework/*/QtCore")
    require_deployed_file("the Cocoa platform plugin" "PhotoCrap.app/Contents/PlugIns/platforms/libqcocoa.dylib")
    execute_process(
        COMMAND /usr/bin/codesign --verify --deep --strict --verbose=2
                "${DEPLOY_ROOT}/PhotoCrap.app"
        RESULT_VARIABLE codesign_result
        ERROR_VARIABLE codesign_error
    )
    if(NOT codesign_result EQUAL 0)
        message(FATAL_ERROR "The macOS application signature is invalid: ${codesign_error}")
    endif()
    message(STATUS "The macOS application signature is valid")
elseif(DEPLOY_PLATFORM STREQUAL "Windows")
    require_deployed_file("the application executable" "*/PhotoCrap.exe" "PhotoCrap.exe")
    require_deployed_file("Qt Core" "*/Qt6Core.dll" "Qt6Core.dll")
    require_deployed_file("the Windows platform plugin" "*/qwindows.dll" "qwindows.dll")
elseif(DEPLOY_PLATFORM STREQUAL "Linux")
    require_deployed_file("the application executable" "*/PhotoCrap" "PhotoCrap")
    require_deployed_file("Qt Core" "*/libQt6Core.so*")
    require_deployed_file("the XCB platform plugin" "*/libqxcb.so")
else()
    message(FATAL_ERROR "Unsupported DEPLOY_PLATFORM: ${DEPLOY_PLATFORM}")
endif()

require_deployed_file("the QtQuick QML module" "qml/QtQuick/qmldir"
                      "PhotoCrap.app/Contents/Resources/qml/QtQuick/qmldir")
require_deployed_file("the Qt Quick Controls QML module" "qml/QtQuick/Controls/qmldir"
                      "PhotoCrap.app/Contents/Resources/qml/QtQuick/Controls/qmldir")
message(STATUS "PhotoCrap deployment contains Qt runtime libraries, plugins and QML modules")
