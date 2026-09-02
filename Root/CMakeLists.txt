cmake_minimum_required(VERSION 3.16)
#"C:\Qt\6.11.1\msvc2022_64\bin\windeployqt6.exe" "C:\Users\yiyan\Desktop\CalibrationStage\Root\build\Desktop_Qt_6_11_1_MSVC2022_64bit-Release\CalibrationStage.exe"
project(CalibrationStage VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_COMPONENTS Widgets Charts)
if(NOT EMSCRIPTEN)
    list(APPEND QT_COMPONENTS SerialPort)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${QT_COMPONENTS})
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${QT_COMPONENTS})

set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp
    mainwindow.h
    mainwindow.ui
    pico.cpp
    pico.h
    fg.cpp
    fg.h
    vmx.cpp
    vmx.h
    filer.cpp
    filer.h
    device.h
    device.cpp
    scan.h
    scan.cpp
)

if(${QT_VERSION_MAJOR} GREATER_EQUAL 6)
    qt_add_executable(CalibrationStage
        MANUAL_FINALIZATION
        ${PROJECT_SOURCES}
    )
# Define target properties for Android with Qt 6 as:
#    set_property(TARGET CalibrationStage APPEND PROPERTY QT_ANDROID_PACKAGE_SOURCE_DIR
#                 ${CMAKE_CURRENT_SOURCE_DIR}/android)
# For more information, see https://doc.qt.io/qt-6/qt-add-executable.html#target-creation
else()
    if(ANDROID)
        add_library(CalibrationStage SHARED
            ${PROJECT_SOURCES}
        )
# Define properties for Android with Qt 5 after find_package() calls as:
#    set(ANDROID_PACKAGE_SOURCE_DIR "${CMAKE_CURRENT_SOURCE_DIR}/android")
    else()
        add_executable(CalibrationStage
            ${PROJECT_SOURCES}
        )
    endif()
endif()


if(NOT EMSCRIPTEN)
    set(PROJECT_LIB_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/lib")
    set(PROJECT_INC_ROOT "${CMAKE_CURRENT_SOURCE_DIR}/inc")

    set(PICOSDK_PS5000A_LIBRARY "${PROJECT_LIB_ROOT}/ps5000a.lib")
    if(NOT EXISTS "${PICOSDK_PS5000A_LIBRARY}")
        message(FATAL_ERROR "Could not find ps5000a.lib in ${PROJECT_LIB_ROOT}")
    endif()

    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(IVI_VISA_LIBRARY_NAME visa64)
    else()
        set(IVI_VISA_LIBRARY_NAME visa32)
    endif()

    set(IVI_VISA_LIBRARY "${PROJECT_LIB_ROOT}/${IVI_VISA_LIBRARY_NAME}.lib")
    if(NOT EXISTS "${IVI_VISA_LIBRARY}")
        message(FATAL_ERROR "Could not find ${IVI_VISA_LIBRARY_NAME}.lib in ${PROJECT_LIB_ROOT}")
    endif()

    target_include_directories(CalibrationStage PRIVATE
        "${PROJECT_INC_ROOT}"
    )
endif()

target_link_libraries(CalibrationStage PRIVATE
    Qt${QT_VERSION_MAJOR}::Widgets
    Qt${QT_VERSION_MAJOR}::Charts
)

if(NOT EMSCRIPTEN)
    target_link_libraries(CalibrationStage PRIVATE
        Qt${QT_VERSION_MAJOR}::SerialPort
        "${PICOSDK_PS5000A_LIBRARY}"
        "${IVI_VISA_LIBRARY}"
    )
endif()


# Qt for iOS sets MACOSX_BUNDLE_GUI_IDENTIFIER automatically since Qt 6.1.
# If you are developing for iOS or macOS you should consider setting an
# explicit, fixed bundle identifier manually though.
if(${QT_VERSION} VERSION_LESS 6.1.0)
  set(BUNDLE_ID_OPTION MACOSX_BUNDLE_GUI_IDENTIFIER com.example.CalibrationStage)
endif()
set_target_properties(CalibrationStage PROPERTIES
    ${BUNDLE_ID_OPTION}
    MACOSX_BUNDLE_BUNDLE_VERSION ${PROJECT_VERSION}
    MACOSX_BUNDLE_SHORT_VERSION_STRING ${PROJECT_VERSION_MAJOR}.${PROJECT_VERSION_MINOR}
    MACOSX_BUNDLE TRUE
    WIN32_EXECUTABLE TRUE
)

include(GNUInstallDirs)
install(TARGETS CalibrationStage
    BUNDLE DESTINATION .
    LIBRARY DESTINATION ${CMAKE_INSTALL_LIBDIR}
    RUNTIME DESTINATION ${CMAKE_INSTALL_BINDIR}
)

if(QT_VERSION_MAJOR EQUAL 6)
    qt_finalize_executable(CalibrationStage)
endif()

include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(CalibrationStageTests
        tests/tst_scanutilities.cpp
        filer.cpp
        filer.h
    )
    target_link_libraries(CalibrationStageTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        Qt${QT_VERSION_MAJOR}::Widgets
    )
    if(NOT EMSCRIPTEN)
        target_link_libraries(CalibrationStageTests PRIVATE
            Qt${QT_VERSION_MAJOR}::SerialPort
        )
    endif()
    target_include_directories(CalibrationStageTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
    )
    add_test(NAME CalibrationStageTests COMMAND CalibrationStageTests)
endif()
