cmake_minimum_required(VERSION 3.16)

project(OnPointPhotoEditor VERSION 1.0.0 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

option(ONPOINT_PORTABLE_DEPLOY "Bundle Qt runtime libraries for a portable archive" OFF)

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS Core Widgets PrintSupport Concurrent)
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Core Widgets PrintSupport Concurrent)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_standard_project_setup()
endif()

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    onpoint_resources.qrc
    image_tools.cpp
    image_tools.h
    red_eye_tools.h
    red_eye_tools.cpp
    redeyeselectionwidget.h
    redeyeselectionwidget.cpp
    redeyedialog.h
    redeyedialog.cpp
    overexposure_tools.h
    overexposure_tools.cpp
    overexposureselectionwidget.h
    overexposureselectionwidget.cpp
    overexposurerecoveryworkspace.h
    overexposurerecoveryworkspace.cpp
    bright_area_tools.h
    bright_area_tools.cpp
    brightareaselectionwidget.h
    brightareaselectionwidget.cpp
    toolpreviewwidget.h
    toolpreviewwidget.cpp
    white_balance_tools.h
    white_balance_tools.cpp
    whitebalancedialog.h
    whitebalancedialog.cpp
    noise_reduction_tools.h
    noise_reduction_tools.cpp
    noisereductiondialog.h
    noisereductiondialog.cpp
    focus_tools.h
    focus_tools.cpp
    focusdialog.h
    focusdialog.cpp
    appsettings.h
    appsettings.cpp
    preferencesdialog.h
    preferencesdialog.cpp
    imageoutputservice.h
    imageoutputservice.cpp
    imageoutputdialog.h
    imageoutputdialog.cpp
    imageimportservice.h
    imageimportservice.cpp
    printservice.h
    printservice.cpp
    printpreviewwidget.h
    printpreviewwidget.cpp
    printlayoutdialog.h
    printlayoutdialog.cpp
    systemprofile.h
    systemprofile.cpp
    imageprocessingruntime.h
    imageprocessingruntime.cpp
    openclprocessingbackend.h
    openclprocessingbackend.cpp
    openclspatialprocessingbackend.h
    openclspatialprocessingbackend.cpp
    optimizersplashwindow.h
    optimizersplashwindow.cpp
    optimizeroptionsdialog.h
    optimizeroptionsdialog.cpp
    startupcoordinator.h
    startupcoordinator.cpp

)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(OnPointPhotoEditor
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
else()
    if(ANDROID)
        add_library(OnPointPhotoEditor SHARED
            ${PROJECT_SOURCES}
        )
    else()
        add_executable(OnPointPhotoEditor
            ${PROJECT_SOURCES}
        )
    endif()
endif()

target_compile_definitions(OnPointPhotoEditor PRIVATE
    ONPOINT_APP_VERSION="${PROJECT_VERSION}"
    ONPOINT_DESKTOP_FILE_ID="com.onpoint.OnPointPhotoEditor"
)

target_link_libraries(OnPointPhotoEditor PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::PrintSupport
    Qt${QT_VERSION_MAJOR}::Concurrent
)

if(${QT_VERSION} VERSION_LESS 6.1.0)
    set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.OnPointPhotoEditor)
endif()

set_target_properties(OnPointPhotoEditor PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)

install(TARGETS OnPointPhotoEditor
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(UNIX AND NOT APPLE)
    install(FILES
        packaging/linux/com.onpoint.OnPointPhotoEditor.desktop
        DESTINATION ${CMAKE_INSTALL_DATADIR}/applications
    )

    install(FILES
        packaging/linux/com.onpoint.OnPointPhotoEditor.png
        DESTINATION ${CMAKE_INSTALL_DATADIR}/icons/hicolor/512x512/apps
    )

    install(FILES
        packaging/linux/com.onpoint.OnPointPhotoEditor.metainfo.xml
        DESTINATION ${CMAKE_INSTALL_DATADIR}/metainfo
    )
endif()

install(FILES
    LICENSE
    README.md
    CHANGELOG.md
    DESTINATION ${CMAKE_INSTALL_DOCDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(OnPointPhotoEditor)

    if(ONPOINT_PORTABLE_DEPLOY)
        qt_generate_deploy_app_script(
            TARGET OnPointPhotoEditor
            OUTPUT_SCRIPT onpoint_deploy_script
            NO_TRANSLATIONS
            NO_UNSUPPORTED_PLATFORM_ERROR
        )
        install(SCRIPT ${onpoint_deploy_script})
    endif()
endif()

set(CPACK_PACKAGE_NAME "onpoint-photo-editor")
set(CPACK_PACKAGE_VENDOR "Gary Scott")
set(CPACK_PACKAGE_CONTACT "Gary Scott")
set(CPACK_PACKAGE_DESCRIPTION_SUMMARY "Precision photo correction and restoration for Linux")
set(CPACK_PACKAGE_DESCRIPTION "On Point Photo Editor is a Linux-first Qt photo correction and restoration application with multithreaded CPU processing and optional OpenCL GPU acceleration.")
set(CPACK_PACKAGE_VERSION ${PROJECT_VERSION})
set(CPACK_PACKAGE_FILE_NAME "onpoint-photo-editor-${PROJECT_VERSION}-rc1-${CMAKE_SYSTEM_PROCESSOR}")
set(CPACK_RESOURCE_FILE_LICENSE "${CMAKE_CURRENT_SOURCE_DIR}/LICENSE")
set(CPACK_RESOURCE_FILE_README "${CMAKE_CURRENT_SOURCE_DIR}/README.md")
set(CPACK_STRIP_FILES TRUE)
set(CPACK_GENERATOR "DEB;RPM;TGZ")

set(CPACK_DEBIAN_PACKAGE_NAME "onpoint-photo-editor")
set(CPACK_DEBIAN_FILE_NAME DEB-DEFAULT)
set(CPACK_DEBIAN_PACKAGE_MAINTAINER "Gary Scott")
set(CPACK_DEBIAN_PACKAGE_SECTION "graphics")
set(CPACK_DEBIAN_PACKAGE_PRIORITY "optional")
set(CPACK_DEBIAN_PACKAGE_RELEASE "0~rc1")
set(CPACK_DEBIAN_PACKAGE_SHLIBDEPS ON)
set(CPACK_DEBIAN_PACKAGE_RECOMMENDS "ocl-icd-libopencl1")

set(CPACK_RPM_PACKAGE_NAME "onpoint-photo-editor")
set(CPACK_RPM_FILE_NAME RPM-DEFAULT)
set(CPACK_RPM_PACKAGE_LICENSE "GPL-3.0-or-later")
set(CPACK_RPM_PACKAGE_RELEASE "0.rc1")
set(CPACK_RPM_PACKAGE_GROUP "Applications/Multimedia")
set(CPACK_RPM_PACKAGE_AUTOREQ ON)
set(CPACK_RPM_PACKAGE_REQUIRES "qt6-qtbase, qt6-qtbase-gui")

include(CPack)
