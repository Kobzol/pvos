cmake_minimum_required(VERSION 3.8)

set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(cviko1 cviko1.cpp)
add_executable(thread_total thread_total.cpp)
add_executable(thread_current thread_current.cpp)
