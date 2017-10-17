set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(cviko4 cviko4.cpp)
target_link_libraries(cviko4 rt)
