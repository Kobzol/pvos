#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/socket.h>
#include <netinet/in.h>
#include <vector>
#include <algorithm>
#include <cstring>
#include <fcntl.h>
#include <arpa/inet.h>
#include <poll.h>

#include "util.h"

void get_peer_addr(int fd, char* host, unsigned short& port)
{
    sockaddr_in address{};
    uint address_size = sizeof(address);

    int result = getpeername(fd, (sockaddr*) &address, &address_size);
    strcpy(host, inet_ntoa(address.sin_addr));
    port = ntohs(address.sin_port);
}

int chat_send_all(int client, const std::vector<int>& clients)
{
    char clientHost[256];
    unsigned short clientPort;
    get_peer_addr(client, clientHost, clientPort);

    char buffer[256];
    ssize_t len = read(client, buffer, sizeof(buffer) - 1);
    if (len <= 0) return -1;
    buffer[len] = '\0';

    printf("%s:%d sends %s\n", clientHost, clientPort, buffer);

    bool close = std::string(buffer).find("close") != std::string::npos;

    std::string msg(clientHost);
    msg += ":";
    msg += std::to_string(clientPort);
    msg += " says: ";
    msg += buffer;

    for (auto remote: clients)
    {
        if (close || client != remote)
        {
            write(remote, msg.c_str(), msg.size());
        }
    }

    return 0;
}

void server_body_select(sockaddr_in addr, int fd)
{
    std::vector<int> clients;

    socklen_t len;
    while (true)
    {
        fd_set readset{};
        FD_ZERO(&readset);
        FD_SET(fd, &readset);

        int largest = fd;
        for (auto client: clients)
        {
            FD_SET(client, &readset);
            largest = std::max(client, largest);
        }

        timeval timeout{};
        timeout.tv_sec = 1;
        timeout.tv_usec = 0;
        int ret = select(largest + 1, &readset, nullptr, nullptr, &timeout);
        if (ret < 0)
        {
            printf("Server error\n");
            return;
        }
        else if (ret == 0) continue;

        if (FD_ISSET(fd, &readset))
        {
            int client = accept(fd, (sockaddr*) &addr, &len);
            if (client < 0)
            {
                perror("accept");
                return;
            }

            printf("client connected\n");

            fcntl(client, F_SETFL, fcntl(client, F_GETFL) | O_NONBLOCK);
            clients.push_back(client);
        }

        std::vector<int> toRemove;
        for (auto client: clients)
        {
            if (FD_ISSET(client, &readset))
            {
                if (chat_send_all(client, clients) < 0)
                {
                    toRemove.push_back(client);
                    printf("client disconnected\n");
                }
            }
        }
        for (auto client: toRemove)
        {
            clients.erase(std::find(clients.begin(), clients.end(), client));
        }
    }
}
void server_body_poll(sockaddr_in addr, int fd)
{
    std::vector<pollfd> clients;
    clients.push_back(pollfd{});
    clients.back().fd = fd;
    clients.back().events = POLLIN;

    socklen_t len;
    while (true)
    {
        int ret = poll(clients.data(), clients.size(), 1000);
        if (ret < 0)
        {
            printf("Server error\n");
            return;
        }
        else if (ret == 0) continue;

        if ((clients[0].revents & POLLIN) != 0)
        {
            int client = accept(fd, (sockaddr*) &addr, &len);
            if (client < 0)
            {
                perror("accept");
                return;
            }

            printf("client connected\n");

            fcntl(client, F_SETFL, fcntl(client, F_GETFL) | O_NONBLOCK);
            clients.push_back(pollfd{});
            clients.back().fd = client;
            clients.back().events = POLLIN | POLLHUP;
        }

        std::vector<int> fds;
        for (int i = 1; i < clients.size(); i++)
        {
            fds.push_back(clients[i].fd);
        }

        std::vector<pollfd> nextClients = { clients[0] };
        for (int i = 1; i < clients.size(); i++)
        {
            auto& client = clients[i];
            if ((client.revents & POLLIN) != 0)
            {
                if (chat_send_all(client.fd, fds) < 0)
                {
                    printf("client disconnected\n");
                }
                else if ((client.revents & POLLHUP) == 0)
                {
                    nextClients.push_back(client);
                }
            }
            else if ((client.revents & POLLHUP) == 0)
            {
                nextClients.push_back(client);
            }
        }
        clients = std::move(nextClients);
    }
}

void server_fork(char** argv)
{
    unsigned short port = atoi(argv[1]);

    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        exit(1);
    }

    int opt = 1;
    if (setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &opt, sizeof(opt)) < 0)
    {
        perror("setsockopt");
        exit(1);
    }

    sockaddr_in addr = create_address(port);
    if (bind(fd, (const sockaddr*) &addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    if (listen(fd, 10) < 0)
    {
        perror("listen");
        exit(1);
    }

    fcntl(fd, F_SETFL, fcntl(fd, F_GETFL) | O_NONBLOCK);
    //server_body_select(addr, fd);
    server_body_poll(addr, fd);

    close(fd);
}

int main(int argc, char** argv)
{
    if (argc < 2) return 1;

    server_fork(argv);

    return 0;
}
