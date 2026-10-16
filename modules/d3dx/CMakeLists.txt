# Portico D3DX: D3DX 9 math, ray tests and texture loading (DDS, TGA, BMP; PNG with zlib), over any IDirect3DDevice9.

add_library(portico_d3dx STATIC src/d3dx9math.cpp src/d3dx9shader.cpp src/d3dx9tex.cpp)
add_library(portico::d3dx ALIAS portico_d3dx)
# Before any other include folder: Portico's d3dx9*.h, never another D3DX's.
target_include_directories(portico_d3dx BEFORE PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(portico_d3dx PUBLIC cxx_std_17)

if(EMSCRIPTEN)
    target_link_libraries(portico_d3dx PUBLIC portico_win32_headers)
    target_compile_options(portico_d3dx PRIVATE -sUSE_ZLIB=1)
    target_link_options(portico_d3dx INTERFACE -sUSE_ZLIB=1)
    target_compile_definitions(portico_d3dx PRIVATE PORTICO_D3DX_PNG)
else()
    target_compile_definitions(portico_d3dx PRIVATE NOMINMAX WIN32_LEAN_AND_MEAN)
    find_package(ZLIB QUIET)
    if(ZLIB_FOUND)
        target_link_libraries(portico_d3dx PRIVATE ZLIB::ZLIB)
        target_compile_definitions(portico_d3dx PRIVATE PORTICO_D3DX_PNG)
    else()
        message(STATUS "Portico D3DX: zlib not found, PNG loading off")
    endif()
endif()
