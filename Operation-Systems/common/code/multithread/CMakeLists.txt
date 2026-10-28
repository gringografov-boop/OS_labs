cmake_minimum_required(VERSION 3.10)

# Put directory name to COMPONENT_NAME variable
get_filename_component(COMPONENT_NAME ${CMAKE_CURRENT_SOURCE_DIR} NAME)

# Set component name
project(${COMPONENT_NAME})



# Set optimization + debug info flagsв
if(CMAKE_BUILD_TYPE MATCHES Debug)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -O0 -g3")
endif()
if(CMAKE_BUILD_TYPE MATCHES Release)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Os -g0")
endif()


# Output out system
if (CMAKE_SYSTEM_NAME STREQUAL "Linux")
  message("Build for Linux")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -Wall -Wextra -fsanitize=address -g -pthread")
endif ()


if (CMAKE_SYSTEM_NAME STREQUAL "Windows")
  message("Build for Windows")
  set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} /Wall /WX /fsanitize=address /Zi")
  # Add win-only source files
  set(SOURCE_LIB 
    mt_win.cpp
  )
endif ()

set(SOURCE_LIB "${SOURCE_LIB} thread.cpp")

# Make lib
add_library(${COMPONENT_NAME} STATIC ${SOURCE_LIB})

set(
    Includes
    ${CMAKE_CURRENT_SOURCE_DIR}/..
)

target_include_directories(${COMPONENT_NAME} PUBLIC ${Includes})
target_link_libraries(${COMPONENT_NAME} PRIVATE common)