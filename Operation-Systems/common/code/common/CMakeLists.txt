cmake_minimum_required(VERSION 3.10)

# Put directory name to COMPONENT_NAME variable
get_filename_component(COMPONENT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Set component name
project(${COMPONENT_NAME})


# Set warnings + ASan flags
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" -Wall -Wextra -Werror --fsanitize=address)

# Set optimization + debug info flags
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
endif()
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -g0")
endif()


# Add source files
set(SOURCE_LIB 
    errors.cpp
)

# Make lib
add_library(${COMPONENT_NAME} STATIC ${SOURCE_LIB})


# Output out system
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message("Build for Linux")
endif ()


if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  message("Build for Windows")
endif ()