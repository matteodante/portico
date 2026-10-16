# Unit tests (run with ctest; in Emscripten builds under Node.js).
add_executable(test_d3dx_math test_d3dx_math.cpp)
target_link_libraries(test_d3dx_math PRIVATE portico::d3dx)
if(EMSCRIPTEN)
    target_link_options(test_d3dx_math PRIVATE -sENVIRONMENT=node -sEXIT_RUNTIME=1)
endif()
add_test(NAME d3dx_math COMMAND test_d3dx_math)
