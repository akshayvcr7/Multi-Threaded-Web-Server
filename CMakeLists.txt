cmake_minimum_required(VERSION 3.10)

# set the project name
project(Tutorial)

# specify the C++ standard
set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED True)

# Set the output folder where your program will be created
set(CMAKE_BINARY_DIR ${CMAKE_SOURCE_DIR}/bin)
set(EXECUTABLE_OUTPUT_PATH ${CMAKE_BINARY_DIR})

include_directories("${PROJECT_SOURCE_DIR}")

# add the executable
add_executable(av_server ${PROJECT_SOURCE_DIR}/av_server.cpp)

# add the executable
add_executable(av_client ${PROJECT_SOURCE_DIR}/av_client.cpp)