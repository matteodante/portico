cmake_minimum_required(VERSION 3.21)

project(Portico
    VERSION 0.1.0
    DESCRIPTION "Run Direct3D 9 C++ programs in the browser, from source, without rewriting them"
    HOMEPAGE_URL "https://github.com/matteodante/portico"
    LANGUAGES C CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(PORTICO_BUILD_SAMPLES "Build the samples (Microsoft's DirectX SDK tutorials 1-5)" ${PROJECT_IS_TOP_LEVEL})
option(PORTICO_BUILD_TESTS "Build the tests" ${PROJECT_IS_TOP_LEVEL})

if(NOT EMSCRIPTEN AND NOT WIN32)
    message(FATAL_ERROR "Portico builds with Emscripten (the browser) or natively on Windows (ANGLE)")
endif()

add_subdirectory(modules/win32)
add_subdirectory(modules/d3d9)
add_subdirectory(modules/d3dx)

if(PORTICO_BUILD_SAMPLES)
    add_subdirectory(samples)
endif()

if(PORTICO_BUILD_TESTS)
    enable_testing()
    add_subdirectory(tests)
endif()
