cmake_minimum_required(VERSION 3.16)
if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(lab2_variant5 LANGUAGES CXX)
endif()
if(NOT TARGET os)
    add_subdirectory("${CMAKE_CURRENT_LIST_DIR}/../common/code/os" "${CMAKE_CURRENT_BINARY_DIR}/os")
endif()
add_library(lab2_logic STATIC code/common.cpp)

target_compile_features(lab2_logic PUBLIC cxx_std_17)

target_include_directories(lab2_logic PUBLIC
    "${CMAKE_CURRENT_SOURCE_DIR}/code/include"
)

add_executable(lab2_parent code/parent/main.cpp)
add_executable(lab2_child code/child/main.cpp)

target_link_libraries(lab2_parent PRIVATE os lab2_logic)
target_link_libraries(lab2_child PRIVATE os lab2_logic)
foreach(target lab2_parent lab2_child)
    set_target_properties(${target} PROPERTIES
        RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin/lab2")
    foreach(config Debug Release RelWithDebInfo MinSizeRel)
        string(TOUPPER "${config}" upper)
        set_target_properties(${target} PROPERTIES
            RUNTIME_OUTPUT_DIRECTORY_${upper} "${CMAKE_BINARY_DIR}/bin/lab2")
    endforeach()
endforeach()
if(MINGW)
    target_link_options(lab2_child PRIVATE -municode)
endif()
foreach(target lab2_parent lab2_child lab2_logic)
    if(MSVC)
        target_compile_options(${target} PRIVATE /W4 /permissive- /utf-8)
    else()
        target_compile_options(${target} PRIVATE -Wall -Wextra -Wpedantic)
    endif()
endforeach()
