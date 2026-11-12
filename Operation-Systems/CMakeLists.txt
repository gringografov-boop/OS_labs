cmake_minimum_required(VERSION 3.16)
project(OperationSystems LANGUAGES CXX)
add_subdirectory(common/code/os)
add_subdirectory(lab2)

# BEGIN lab3-mt integration
if(NOT TARGET lab3)
    add_subdirectory(lab3)
endif()
# END lab3-mt integration
