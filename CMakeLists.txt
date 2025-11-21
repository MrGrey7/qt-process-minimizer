cmake_minimum_required(VERSION 3.16)

project(ProcessMinimizer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

# Standard Qt6 Project Setup (Sets up deployment defaults)
find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets Test)
qt_standard_project_setup()

# --- MAIN APP ---
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    processpickerdialog.cpp processpickerdialog.h processpickerdialog.ui
    hotkeyeventfilter.cpp hotkeyeventfilter.h
    utils.cpp utils.h   # <--- Added Utils
    win32utils.h
    resources.qrc
)

if(WIN32)
    list(APPEND PROJECT_SOURCES appicon.rc)
endif()

qt_add_executable(ProcessMinimizer WIN32 ${PROJECT_SOURCES})

target_link_libraries(ProcessMinimizer PRIVATE
    Qt6::Core Qt6::Gui Qt6::Widgets
)

if(WIN32)
    target_link_libraries(ProcessMinimizer PRIVATE
        user32 kernel32 psapi shell32 shlwapi advapi32
    )
endif()

# --- DEPLOYMENT (The Magic Part) ---
# This generates a script that runs windeployqt when you install
qt_generate_deploy_app_script(
    TARGET ProcessMinimizer
    OUTPUT_SCRIPT deploy_script
    NO_UNSUPPORTED_PLATFORM_ERROR
)

# Tell CMake to install the exe to the "./bin" folder
install(TARGETS ProcessMinimizer
    RUNTIME DESTINATION bin
    BUNDLE DESTINATION bin
)

# Tell CMake to run the deployment script (copying DLLs) after install
install(SCRIPT ${deploy_script})

# --- TESTS ---
enable_testing()

add_executable(tst_keymapping tests/tst_keymapping.cpp utils.cpp)
target_link_libraries(tst_keymapping PRIVATE Qt6::Core Qt6::Test)

if(WIN32)
    # Utils needs user32 for VK constants
    target_link_libraries(tst_keymapping PRIVATE user32)
endif()

add_test(NAME KeyMappingTest COMMAND tst_keymapping)
