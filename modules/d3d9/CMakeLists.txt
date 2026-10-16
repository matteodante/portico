# Portico D3D9: Direct3D 9 (fixed function) on OpenGL ES 3.
#   Browser (Emscripten): WebGL2 on the page's #canvas, the program on the page's main thread.
#   Native (Windows):     ANGLE's GLES3 in WebGL-compatibility mode, the same code as in the browser, for debugging
#                         and for comparing with the system's Direct3D 9. ANGLE comes from PORTICO_ANGLE_DIR
#                         (include/, lib/, bin/) or from vcpkg's "angle" port (find_package(unofficial-angle)).

set(PORTICO_D3D9_SOURCES
    src/crash_log.cpp
    src/d3d9.cpp
    src/device.cpp
    src/device_draw.cpp
    src/ffp.cpp
    src/formats.cpp
    src/gl_context.cpp
    src/gl_context_web.cpp
    src/iids.cpp
    src/log.cpp
    src/resources.cpp
    src/stats.cpp)

add_library(portico_d3d9 STATIC ${PORTICO_D3D9_SOURCES})
add_library(portico::d3d9 ALIAS portico_d3d9)
target_include_directories(portico_d3d9 PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/include")
target_compile_features(portico_d3d9 PUBLIC cxx_std_17)
target_compile_definitions(portico_d3d9 PRIVATE GL_GLEXT_PROTOTYPES WIN32_LEAN_AND_MEAN)

if(EMSCRIPTEN)
    target_link_libraries(portico_d3d9 PUBLIC portico_win32_headers)
    # Present waits for the next animation frame through JSPI (src/gl_context_web.cpp).
    target_link_options(portico_d3d9 INTERFACE -sMIN_WEBGL_VERSION=2 -sMAX_WEBGL_VERSION=2 -sJSPI)
    return()
endif()

set(PORTICO_ANGLE_DIR "" CACHE PATH "ANGLE for the native build: a folder with include/, lib/ and bin/")
if(PORTICO_ANGLE_DIR)
    if(NOT EXISTS "${PORTICO_ANGLE_DIR}/include/EGL/egl.h")
        message(FATAL_ERROR "PORTICO_ANGLE_DIR (${PORTICO_ANGLE_DIR}) has no include/EGL/egl.h")
    endif()
    target_include_directories(portico_d3d9 PRIVATE "${PORTICO_ANGLE_DIR}/include")
    target_link_libraries(portico_d3d9 PUBLIC "${PORTICO_ANGLE_DIR}/lib/libEGL.lib" "${PORTICO_ANGLE_DIR}/lib/libGLESv2.lib")
    set(PORTICO_ANGLE_BIN "${PORTICO_ANGLE_DIR}/bin" CACHE INTERNAL "")
else()
    find_package(unofficial-angle CONFIG REQUIRED)
    target_link_libraries(portico_d3d9 PUBLIC unofficial::angle::libEGL unofficial::angle::libGLESv2)
endif()
target_compile_definitions(portico_d3d9 PRIVATE EGL_EGLEXT_PROTOTYPES)
target_link_libraries(portico_d3d9 PUBLIC dbghelp dxguid)
if(MSVC)
    target_compile_options(portico_d3d9 PRIVATE /W3 /EHsc)
endif()
