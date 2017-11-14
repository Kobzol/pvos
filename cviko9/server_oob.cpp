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
#include <sys/time.h>

#include "../cviko8/util.h"

void get_peer_addr(int fd, char* host, unsigned short& port)
{
    sockaddr_in address{};
    uint address_size = sizeof(address);

    int result = getpeername(fd, (sockaddr*) &address, &address_size);
    strcpy(host, inet_ntoa(address.sin_addr));
    port = ntohs(address.sin_port);
}

std::string clientName(int client)
{
    char clientHost[256];
    unsigned short clientPort;
    get_peer_addr(client, clientHost, clientPort);

    return std::string(clientHost) + ":" + std::to_string(clientPort);
}

int chat_send_all(int client, const std::vector<int>& clients)
{
    char buffer[256];
    ssize_t len = read(client, buffer, sizeof(buffer) - 1);
    if (len <= 0) return -1;
    buffer[len] = '\0';

    auto name = clientName(client);
    printf("%s sends %s", name.c_str(), buffer);

    bool close = std::string(buffer).find("close") != std::string::npos;

    std::string msg = name;
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
void oob_send_all(char msg, const std::vector<int>& clients)
{
    for (auto& client: clients)
    {
        send(client, &msg, 1, MSG_OOB);
    }
}
std::vector<int> pollFdToFd(const std::vector<pollfd>& clients)
{
    std::vector<int> fds;
    for (auto& c: clients)
    {
        fds.push_back(c.fd);
    }
    return fds;
}

ssize_t tm_to_ms(timeval* tm)
{
    return tm->tv_sec * 1000 + tm->tv_usec / 1000;
}
timeval ms_to_tm(ssize_t time)
{
    timeval t{};
    t.tv_sec = time / 1000;
    time %= 1000;
    t.tv_usec = time;
    return t;
}

#define TIMEOUT_MS 10000

void server_body_select(sockaddr_in addr, int fd)
{
    timeval lasttime{};
    gettimeofday(&lasttime, nullptr);

    std::vector<int> clients;

    socklen_t len;
    while (true)
    {
        fd_set readset{};
        fd_set exceptset{};
        FD_ZERO(&readset);
        FD_ZERO(&exceptset);
        FD_SET(fd, &readset);

        int largest = fd;
        for (auto client: clients)
        {
            FD_SET(client, &readset);
            FD_SET(client, &exceptset);
            largest = std::max(client, largest);
        }

        timeval currtime{};
        gettimeofday(&currtime, nullptr);
        timeval result{};
        timersub(&currtime, &lasttime, &result);

        int left = TIMEOUT_MS - tm_to_ms(&result);
        if (left < 0)
        {
            gettimeofday(&lasttime, nullptr);
            result = ms_to_tm(TIMEOUT_MS);
            printf("Sending time request to clients\n");
            oob_send_all('T', clients);
        }

        int ret = select(largest + 1, &readset, nullptr, &exceptset, &result);
        if (ret < 0)
        {
            printf("Server error\n");
            return;
        }
        else if (ret == 0) continue;

        gettimeofday(&lasttime, nullptr);

        if (FD_ISSET(fd, &readset))
        {
            int client = accept(fd, (sockaddr*) &addr, &len);
            if (client < 0)
            {
                perror("accept");
                return;
            }

            printf("client connected\n");

            oob_send_all('N', clients);

            clients.push_back(client);
        }

        std::vector<int> toRemove;
        for (auto client: clients)
        {
            if (FD_ISSET(client, &exceptset))
            {
                char oob;
                recv(client, &oob, 1, MSG_OOB);
                if (oob == 'L')
                {
                    std::string list = "Clients: ";
                    for (auto& c: clients)
                    {
                        list += clientName(c) + ", ";
                    }
                    list += "\n";

                    for (auto& c: clients)
                    {
                        write(c, list.c_str(), list.size());
                    }
                }
            }
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
    timeval lasttime{};
    gettimeofday(&lasttime, nullptr);

    std::vector<pollfd> clients;
    clients.push_back(pollfd{});
    clients.back().fd = fd;
    clients.back().events = POLLIN;

    socklen_t len;
    while (true)
    {
        timeval currtime{};
        gettimeofday(&currtime, nullptr);
        timeval result{};
        timersub(&currtime, &lasttime, &result);

        int left = TIMEOUT_MS - tm_to_ms(&result);
        if (left < 0)
        {
            gettimeofday(&lasttime, nullptr);
            left = TIMEOUT_MS;
            printf("Sending time request to clients\n");
            oob_send_all('T', pollFdToFd(clients));
        }

        int ret = poll(clients.data(), clients.size(), left);
        if (ret < 0)
        {
            printf("Server error\n");
            return;
        }
        else if (ret == 0) continue;

        gettimeofday(&lasttime, nullptr);

        if ((clients[0].revents & POLLIN) != 0)
        {
            int client = accept(fd, (sockaddr*) &addr, &len);
            if (client < 0)
            {
                perror("accept");
                return;
            }

            printf("client connected\n");

            oob_send_all('N', pollFdToFd(clients));

            clients.push_back(pollfd{});
            clients.back().fd = client;
            clients.back().events = POLLIN | POLLHUP | POLLPRI;
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
            if ((client.revents & POLLPRI) != 0)
            {
                char oob;
                recv(client.fd, &oob, 1, MSG_OOB);
                if (oob == 'L')
                {
                    std::string list = "Clients: ";
                    for (auto& c: clients)
                    {
                        list += clientName(c.fd) + ", ";
                    }
                    list += "\n";

                    for (auto& c: clients)
                    {
                        write(c.fd, list.c_str(), list.size());
                    }
                }
            }
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
