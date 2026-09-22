cmake_minimum_required(VERSION 3.16)
project(CalibrationStageGUI VERSION 0.1 LANGUAGES CXX)

set(CMAKE_AUTOUIC ON)
set(CMAKE_AUTOMOC ON)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(QT_COMPONENTS Widgets Charts)
if(NOT EMSCRIPTEN)
    list(APPEND QT_COMPONENTS SerialPort)
endif()

find_package(QT NAMES Qt6 Qt5 REQUIRED COMPONENTS ${QT_COMPONENTS})
find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS ${QT_COMPONENTS})

set(QT_LIBRARIES)
foreach(component IN LISTS QT_COMPONENTS)
    list(APPEND QT_LIBRARIES Qt${QT_VERSION_MAJOR}::${component})
endforeach()

# The application and tests share the same UI and scan implementation.
set(SHARED_SOURCES
    mainwindow.cpp mainwindow.h mainwindow.ui
    scan.cpp scan.h
    filer.cpp filer.h
    device.cpp device.h
    pico.h fg.h vmx.h
)
set(APP_SOURCES
    main.cpp main.h
    pico.cpp fg.cpp vmx.cpp
    ${SHARED_SOURCES}
)

if(QT_VERSION_MAJOR GREATER_EQUAL 6)
    qt_add_executable(CalibrationStage MANUAL_FINALIZATION ${APP_SOURCES})
elseif(ANDROID)
    add_library(CalibrationStage SHARED ${APP_SOURCES})
else()
    add_executable(CalibrationStage ${APP_SOURCES})
endif()

target_link_libraries(CalibrationStage PRIVATE ${QT_LIBRARIES})

if(EMSCRIPTEN)
    target_compile_definitions(CalibrationStage PRIVATE QT_NO_DEBUG_OUTPUT)
else()
    if(CMAKE_SIZEOF_VOID_P EQUAL 8)
        set(IVI_VISA_LIBRARY_NAME visa64)
    else()
        set(IVI_VISA_LIBRARY_NAME visa32)
    endif()
    set(HARDWARE_LIBRARIES
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/ps5000a.lib"
        "${CMAKE_CURRENT_SOURCE_DIR}/lib/${IVI_VISA_LIBRARY_NAME}.lib"
    )
    foreach(library IN LISTS HARDWARE_LIBRARIES)
        if(NOT EXISTS "${library}")
            message(FATAL_ERROR "Could not find instrument library: ${library}")
        endif()
    endforeach()
    target_include_directories(CalibrationStage PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/inc")
    target_link_libraries(CalibrationStage PRIVATE ${HARDWARE_LIBRARIES})
endif()

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

option(BUILD_TESTING "Build scan lifecycle tests (requires Qt Test)" OFF)
include(CTest)
if(BUILD_TESTING AND NOT EMSCRIPTEN)
    find_package(Qt${QT_VERSION_MAJOR} REQUIRED COMPONENTS Test)
    add_executable(CalibrationStageTests
        tests/scan_lifecycle_tests.cpp
        tests/scan_lifecycle_tests.h
        tests/device_stubs.cpp
        ${SHARED_SOURCES}
    )
    target_link_libraries(CalibrationStageTests PRIVATE
        Qt${QT_VERSION_MAJOR}::Test
        ${QT_LIBRARIES}
    )
    target_include_directories(CalibrationStageTests PRIVATE
        "${CMAKE_CURRENT_SOURCE_DIR}"
        "${CMAKE_CURRENT_SOURCE_DIR}/inc"
    )
    add_test(NAME CalibrationStageTests COMMAND CalibrationStageTests)
    set_tests_properties(CalibrationStageTests PROPERTIES ENVIRONMENT "QT_QPA_PLATFORM=offscreen")
endif()
