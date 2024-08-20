cmake_minimum_required(VERSION 3.10)
project(rbp)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_FLAGS_RELEASE "-O3")
set(CMAKE_CXX_FLAGS_DEBUG "-g -O3")

include_directories(${CMAKE_CURRENT_SOURCE_DIR}/../CPS/include)

file(GLOB SOURCES
    "edge.h"
    "examples_mrf.h"
    "message.h"
    "mrf.h"
    "relaxed-rbp.h"
    "residual_bp.h"
    "utils.h"
    "heap_rbp.h"
    "bucket_rbp.h"
    "edge.cpp"
    "examples_mrf.cpp"
    "main.cpp"
    "message.cpp"
    "mrf.cpp"
    "residual_bp.cpp"
    "relaxed-rbp.cpp"
    "heap_rbp.cpp"
    "bucket_rbp.cpp"
)

add_executable(rbp ${SOURCES})
target_link_libraries(rbp pthread ${Boost_LIBRARIES})