cmake_minimum_required(VERSION 3.16)
project(OperationSystems LANGUAGES CXX)
add_subdirectory(common/code/os)
add_subdirectory(lab2)

# BEGIN lab3-mt integration
if(NOT TARGET lab3)
    add_subdirectory(lab3)
endif()
# END lab3-mt integration

# BEGIN lab4 integration
if(NOT TARGET lab4_parent)
    add_subdirectory(lab4)
endif()
# END lab4 integration

# BEGIN lab5 integration
if(NOT TARGET lab5_static)
    add_subdirectory(lab5)
endif()
# END lab5 integration
