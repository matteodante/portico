# Microsoft's Direct3D 9 tutorials 1-5 (DirectX SDK samples, MIT licence), built unchanged against Portico.
#
#   PORTICO_SAMPLES_FIXED_TIME=<ms>   timeGetTime() returns this constant: every frame is the same (for the
#                                     screenshot tests). Done with a forced include, so the sources stay unchanged.
#   PORTICO_SAMPLES_SYSTEM_D3D9=ON    (Windows) link the system's d3d9.lib instead of Portico: the reference renders.

set(PORTICO_SAMPLES_FIXED_TIME "" CACHE STRING "Freeze timeGetTime() at this value (ms) in the samples")
option(PORTICO_SAMPLES_SYSTEM_D3D9 "Windows: build the samples against the system's Direct3D 9 (reference renders)" OFF)

function(portico_sample name source)
    add_executable(${name} ${source})
    target_compile_definitions(${name} PRIVATE UNICODE _UNICODE)
    target_link_libraries(${name} PRIVATE portico::d3dx portico::win32 portico::win32_main)
    if(PORTICO_SAMPLES_SYSTEM_D3D9 AND NOT EMSCRIPTEN)
        target_link_libraries(${name} PRIVATE d3d9)
        set_target_properties(${name} PROPERTIES OUTPUT_NAME "${name}_d3d9")
    else()
        target_link_libraries(${name} PRIVATE portico::d3d9)
    endif()
    if(NOT PORTICO_SAMPLES_FIXED_TIME STREQUAL "")
        target_compile_definitions(${name} PRIVATE PORTICO_FIXED_TIME=${PORTICO_SAMPLES_FIXED_TIME})
        if(MSVC)
            target_compile_options(${name} PRIVATE "/FI${CMAKE_CURRENT_SOURCE_DIR}/fixed_time.h")
        else()
            target_compile_options(${name} PRIVATE -include "${CMAKE_CURRENT_SOURCE_DIR}/fixed_time.h")
        endif()
    endif()
    if(EMSCRIPTEN)
        # The tutorials write <Windows.h>; the header is windows.h (the same file on a case-insensitive disk).
        target_compile_options(${name} PRIVATE -Wno-nonportable-include-path)
        set_target_properties(${name} PROPERTIES SUFFIX ".html")
        target_link_options(${name} PRIVATE "--shell-file=${CMAKE_CURRENT_SOURCE_DIR}/shell.html"
            -sALLOW_MEMORY_GROWTH=1 -sEXIT_RUNTIME=0 -sENVIRONMENT=web --profiling-funcs)
        set_property(TARGET ${name} APPEND PROPERTY LINK_DEPENDS "${CMAKE_CURRENT_SOURCE_DIR}/shell.html")
    else()
        set_target_properties(${name} PROPERTIES WIN32_EXECUTABLE ON)
        if(MSVC)
            target_compile_options(${name} PRIVATE /W3)
        endif()
        if(PORTICO_ANGLE_BIN AND NOT PORTICO_SAMPLES_SYSTEM_D3D9)
            file(GLOB angle_dlls "${PORTICO_ANGLE_BIN}/*.dll")  # libEGL, libGLESv2 and what they need (zlib)
            add_custom_command(TARGET ${name} POST_BUILD
                COMMAND ${CMAKE_COMMAND} -E copy_if_different ${angle_dlls} "$<TARGET_FILE_DIR:${name}>")
        endif()
    endif()
endfunction()

portico_sample(tut01_createdevice Tut01_CreateDevice/CreateDevice.cpp)
portico_sample(tut02_vertices Tut02_Vertices/Vertices.cpp)
portico_sample(tut03_matrices Tut03_Matrices/Matrices.cpp)
portico_sample(tut04_lights Tut04_Lights/Lights.cpp)
portico_sample(tut05_textures Tut05_Textures/Textures.cpp)

# The texture of tutorial 5: next to the executable natively, in the program's file system in the browser.
if(EMSCRIPTEN)
    target_link_options(tut05_textures PRIVATE "--preload-file=${CMAKE_CURRENT_SOURCE_DIR}/Tut05_Textures/banana.bmp@banana.bmp")
else()
    add_custom_command(TARGET tut05_textures POST_BUILD
        COMMAND ${CMAKE_COMMAND} -E copy_if_different "${CMAKE_CURRENT_SOURCE_DIR}/Tut05_Textures/banana.bmp"
            "$<TARGET_FILE_DIR:tut05_textures>")
endif()
