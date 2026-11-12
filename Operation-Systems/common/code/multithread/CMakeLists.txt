cmake_minimum_required(VERSION 3.16)

if(CMAKE_SOURCE_DIR STREQUAL CMAKE_CURRENT_SOURCE_DIR)
    project(multithread_project LANGUAGES CXX)
endif()

if(CMAKE_SYSTEM_NAME STREQUAL "Linux")
    set(THREADS_PREFER_PTHREAD_FLAG TRUE)
    find_package(Threads REQUIRED)
    add_library(multithread STATIC thread.cpp mt_posix.cpp mt_sync_posix.cpp)
    target_link_libraries(multithread PUBLIC Threads::Threads)
elseif(CMAKE_SYSTEM_NAME STREQUAL "Windows")
    # Original Windows implementation remains in the repository.
    # It is not repaired or tested by this Linux-only lab3 adaptation.
    add_library(multithread STATIC thread.cpp mt_win.cpp)
else()
    message(FATAL_ERROR "multithread: unsupported operating system")
endif()

target_compile_features(multithread PUBLIC cxx_std_17)
target_include_directories(multithread PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}/..")
if(TARGET common)
    target_link_libraries(multithread PRIVATE common)
endif()
