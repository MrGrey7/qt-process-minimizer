cmake_minimum_required(VERSION 3.16)

project(ProcessMinimizer VERSION 1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# --- Qt6 Configuration ---
set(CMAKE_AUTOMOC ON)
set(CMAKE_AUTORCC ON)
set(CMAKE_AUTOUIC ON)

find_package(Qt6 REQUIRED COMPONENTS Core Gui Widgets)

# --- Source Files ---
set(PROJECT_SOURCES
    main.cpp
    mainwindow.cpp mainwindow.h mainwindow.ui
    processpickerdialog.cpp processpickerdialog.h processpickerdialog.ui
    hotkeyeventfilter.cpp hotkeyeventfilter.h
    win32utils.h
    resources.qrc
)

# --- Windows Specifics ---
if(WIN32)
    # Add the RC file for the application icon
    list(APPEND PROJECT_SOURCES appicon.rc)
endif()

# --- Executable Definition ---
# WIN32_EXECUTABLE ON prevents the cmd console from popping up
add_executable(ProcessMinimizer WIN32 ${PROJECT_SOURCES})

# --- Linking ---
target_link_libraries(ProcessMinimizer PRIVATE
    Qt6::Core
    Qt6::Gui
    Qt6::Widgets
)

if(WIN32)
    # Explicitly link Windows System Libraries used in your code
    target_link_libraries(ProcessMinimizer PRIVATE
        user32      # RegisterHotKey, EnumWindows, ShowWindow
        kernel32    # CreateToolhelp32Snapshot, GetModuleFileNameW
        psapi       # GetModuleBaseName, QueryFullProcessImageName
        shell32     # SHGetFileInfo (for Icons)
        shlwapi     # Path handling
        advapi32    # RegOpenKeyExW (Registry manipulation)
    )
endif()