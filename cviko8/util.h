#pragma once

#include <sys/socket.h>
#include <netinet/in.h>

inline sockaddr_in create_address(in_addr addr, unsigned short port)
{
    sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_port = htons(port);
    address.sin_addr = addr;

    return address;
}

inline sockaddr_in create_address(unsigned short port)
{
    in_addr addr_any = { INADDR_ANY };

    return create_address(addr_any, port);
}
