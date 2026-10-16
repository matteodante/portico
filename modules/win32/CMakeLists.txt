# Portico win32: the Windows API subset for programs written for Windows (Emscripten builds). Natively the targets
# exist too, empty or as thin forwards, so programs link the same targets everywhere.
#
#   portico::win32_headers   windows.h and friends, and the Direct3D 9 headers (MinGW-w64, submodule)
#   portico::win32           windows, messages, timers and system calls over the browser
#   portico::win32_main      main() that calls the program's wWinMain or WinMain

add_library(portico_win32_headers INTERFACE)
add_library(portico::win32_headers ALIAS portico_win32_headers)

if(NOT EMSCRIPTEN)
    # Windows: the real headers and libraries.
    add_library(portico_win32 INTERFACE)
    target_link_libraries(portico_win32 INTERFACE winmm)
    add_library(portico_win32_main INTERFACE)
    add_library(portico::win32 ALIAS portico_win32)
    add_library(portico::win32_main ALIAS portico_win32_main)
    return()
endif()

if(NOT EXISTS "${PROJECT_SOURCE_DIR}/external/mingw-directx-headers/d3d9.h")
    message(FATAL_ERROR "external/mingw-directx-headers is missing: git submodule update --init")
endif()

target_include_directories(portico_win32_headers INTERFACE "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_definitions(portico_win32_headers INTERFACE NOMINMAX)
# Windows file names are case-insensitive and programs write <Windows.h>; so does a case-insensitive host.
if(NOT CMAKE_HOST_WIN32 AND NOT CMAKE_HOST_APPLE)
    file(WRITE "${CMAKE_CURRENT_BINARY_DIR}/case/Windows.h" "#include <windows.h>\n")
    target_include_directories(portico_win32_headers INTERFACE "${CMAKE_CURRENT_BINARY_DIR}/case")
endif()

add_library(portico_win32 STATIC src/window.cpp)
add_library(portico::win32 ALIAS portico_win32)
target_link_libraries(portico_win32 PUBLIC portico_win32_headers)
# GetMessage and Sleep give the thread back to the browser through JSPI. Programs written for Windows expect the
# 1 MB stack a Windows thread starts with (Emscripten's default is 64 KB).
target_link_options(portico_win32 INTERFACE -sJSPI -sSTACK_SIZE=1MB)

add_library(portico_win32_main OBJECT src/main.cpp)
add_library(portico::win32_main ALIAS portico_win32_main)
target_link_libraries(portico_win32_main PUBLIC portico_win32)
