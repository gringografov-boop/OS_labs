cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(lab3 LANGUAGES CXX)
endif()

if(NOT CMAKE_SYSTEM_NAME STREQUAL "Linux")
    message(STATUS "lab3 skipped: this adaptation currently requires Linux/WSL")
    return()
endif()

# Use the real common/code/multithread component, not a private replacement.
if(NOT TARGET multithread)
    add_subdirectory("${CMAKE_CURRENT_SOURCE_DIR}/../common/code/multithread"
                     "${CMAKE_CURRENT_BINARY_DIR}/_deps/multithread")
endif()

add_executable(lab3
    code/main.cpp
    code/common.cpp
    code/matrix.cpp
    code/filters.cpp
)
target_compile_features(lab3 PRIVATE cxx_std_17)
target_include_directories(lab3 PRIVATE "${CMAKE_CURRENT_SOURCE_DIR}/code")
target_link_libraries(lab3 PRIVATE multithread)
target_compile_options(lab3 PRIVATE -Wall -Wextra -Wpedantic)
set_target_properties(lab3 PROPERTIES
    RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lab3")
foreach(config DEBUG RELEASE RELWITHDEBINFO MINSIZEREL)
    set_target_properties(lab3 PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY_${config} "${CMAKE_BINARY_DIR}/bin/lab3")
endforeach()

option(LAB3_BUILD_TESTS "Build Linux mt API tests" OFF)
if(LAB3_BUILD_TESTS)
    add_executable(lab3_mt_test tests/test_mt.cpp)
    target_link_libraries(lab3_mt_test PRIVATE multithread)
    target_compile_features(lab3_mt_test PRIVATE cxx_std_17)
endif()
