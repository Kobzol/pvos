set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -pthread")

add_executable(cviko7 cviko7.cpp message_queue.h)
target_link_libraries(cviko7 rt)
