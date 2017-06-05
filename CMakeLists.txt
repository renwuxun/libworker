cmake_minimum_required(VERSION 2.8)
project(worker)


set(CMAKE_C_FLAGS_DEBUG, "${CMAKE_C_FLAGS_DEBUG}")
set(CMAKE_CXX_FLAGS_DEBUG, "${CMAKE_CXX_FLAGS_DEBUG}")
set(CMAKE_C_FLAGS_RELEASE "${CMAKE_C_FLAGS_RELEASE} -O2 -Wall")
set(CMAKE_CXX_FLAGS_RELEASE "${CMAKE_CXX_FLAGS_RELEASE} -O2 -Wall")


add_subdirectory(src)


add_executable(testworker test.c)

target_link_libraries(testworker worker)