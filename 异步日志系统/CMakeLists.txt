cmake_minimum_required(VERSION 3.15)
project(ThreadLogger)

set(CMAKE_CXX_STANDARD 11)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

add_compile_definitions(_CRT_SECURE_NO_WARNINGS)
find_package(Threads REQUIRED)

add_executable(ThreadLogger logger.cpp)
target_link_libraries(ThreadLogger PRIVATE Threads::Threads)

if(NOT CMAKE_BUILD_TYPE)
    set(CMAKE_BUILD_TYPE Release)
endif()
