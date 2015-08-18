cmake_minimum_required(VERSION 3.2)
project(example)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -std=c++11 -Wall")

set(SOURCE_FILES example.cpp)

add_executable(example ${SOURCE_FILES})

target_link_libraries(example pthread)
