cmake_minimum_required(VERSION 3.16)
project(mgui C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Choose exactly one:
#   cmake -S . -B build -DGUI_PLATFORM=WINDOWS
#   cmake -S . -B build -DGUI_PLATFORM=MACOS
#   cmake -S . -B build -DGUI_PLATFORM=LINUX
#
# If omitted, the host platform is selected automatically.
if(NOT DEFINED GUI_PLATFORM)
    if(WIN32)
        set(GUI_PLATFORM WINDOWS)
    elseif(APPLE)
        set(GUI_PLATFORM MACOS)
    else()
        set(GUI_PLATFORM LINUX)
    endif()
endif()

message(STATUS "GUI_PLATFORM = ${GUI_PLATFORM}")

if(DEFINED GUI_APP)
    if(GUI_APP STREQUAL "CAMERA")
        add_executable(mgui
            apps/camera/src/main.cpp
            apps/camera/src/camApi.cpp
            src/gui.c
        )
        set_source_files_properties(apps/camera/src/main.c PROPERTIES LANGUAGE CXX)
    else()
        message(FATAL_ERROR "Unknown GUI_APP: ${GUI_APP}")
    endif()
else()
    add_executable(mgui
        src/main.c
        src/gui.c
    )
endif()
if(GUI_PLATFORM STREQUAL "WINDOWS")
    target_sources(mgui PRIVATE src/platform_windows.c)
    target_compile_definitions(mgui PRIVATE PLATFORM=PLATFORM_WINDOWS)
    target_link_libraries(mgui PRIVATE user32 gdi32 comctl32)

elseif(GUI_PLATFORM STREQUAL "MACOS")
    enable_language(OBJC)
    target_sources(mgui PRIVATE src/platform_macos.m)
    target_compile_definitions(mgui PRIVATE PLATFORM=PLATFORM_MACOS)
    target_link_libraries(mgui PRIVATE "-framework Cocoa")

elseif(GUI_PLATFORM STREQUAL "LINUX")
    find_package(PkgConfig REQUIRED)
    pkg_check_modules(GTK4 REQUIRED gtk4)
    target_sources(mgui PRIVATE src/platform_linux.c)
    target_compile_definitions(mgui PRIVATE PLATFORM=PLATFORM_LINUX)
    target_include_directories(mgui PRIVATE ${GTK4_INCLUDE_DIRS})
    target_link_directories(mgui PRIVATE ${GTK4_LIBRARY_DIRS})
    target_compile_options(mgui PRIVATE ${GTK4_CFLAGS_OTHER})
    target_link_libraries(mgui PRIVATE ${GTK4_LIBRARIES})

else()
    message(FATAL_ERROR "Unknown GUI_PLATFORM: ${GUI_PLATFORM}")
endif()

target_include_directories(mgui PRIVATE include 
    apps/camera/include
)
