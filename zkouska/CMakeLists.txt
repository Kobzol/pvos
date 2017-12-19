find_package(OpenSSL REQUIRED)

add_executable(zkouska zkouska.cpp reader.h)
target_link_libraries(zkouska ssl crypto)

add_executable(zkouska_client zkouska_client.cpp)
target_link_libraries(zkouska_client ssl crypto)
