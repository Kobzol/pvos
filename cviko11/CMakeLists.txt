find_package(OpenSSL REQUIRED)

add_executable(cviko11_server cviko11_server.cpp)
target_link_libraries(cviko11_server ssl crypto)

add_executable(cviko11_client cviko11_client.cpp)
target_link_libraries(cviko11_client ssl crypto)
