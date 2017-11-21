#include <unistd.h>
#include <sys/uio.h>
#include <sys/socket.h>
#include <cstdio>
#include <cstdlib>
#include <sys/un.h>
#include <cerrno>
#include <wait.h>
#include <poll.h>
#include <vector>
#include <netinet/in.h>
#include <arpa/inet.h>
#include <string>

#include "util.h"

#define WRAP_ERROR(ret)\
    if ((ret) < 0)\
    {\
        printf("Error: %s\n", strerror(errno));\
    }

void vector_io()
{
    iovec iov[2] = {
            {(void*) "a", 1},
            {(void*) "b", 1}
    };

    writev(STDOUT_FILENO, iov, 2);
}

sockaddr_un createUnixAddr(const char* path)
{
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    return addr;
}

void sendfd(int client, int fd, int workerid)
{
    msghdr header{};
    header.msg_name = nullptr;
    header.msg_namelen = 0;
    header.msg_flags = 0;
    pid_t pid = workerid;

    iovec iodata = {
        &pid, sizeof(pid)
    };
    header.msg_iov = &iodata;
    header.msg_iovlen = 1;

    char data[CMSG_SPACE(sizeof(fd))];
    header.msg_control = &data;
    header.msg_controllen = sizeof(data);

    cmsghdr* ch = CMSG_FIRSTHDR(&header);
    ch->cmsg_len = CMSG_LEN(sizeof(fd));
    ch->cmsg_type = SCM_RIGHTS;
    ch->cmsg_level = SOL_SOCKET;
    memcpy(CMSG_DATA(ch), &fd, sizeof(fd));

    WRAP_ERROR(sendmsg(client, &header, 0));
}
std::pair<int, int> recvfd(int server, int workerid)
{
    pid_t pid;
    iovec iodata = { &pid, sizeof(pid) };
    msghdr header{};
    header.msg_iov = &iodata;
    header.msg_iovlen = 1;

    char data[CMSG_SPACE(sizeof(int))];
    header.msg_control = data;
    header.msg_controllen = sizeof(data);

    pollfd pfd{};
    pfd.fd = server;
    pfd.events = POLLIN;
    poll(&pfd, 1, -1);

    WRAP_ERROR(recvmsg(server, &header, 0));

    cmsghdr* ch = CMSG_FIRSTHDR(&header);
    return std::make_pair(pid, *(int*) CMSG_DATA(ch));
}
/*void unix_sendfd()
{
    int pair[2];
    WRAP_ERROR(socketpair(AF_UNIX, SOCK_STREAM, 0, pair));

    printf("Server: %d\n", getpid());

    if (fork() == 0)
    {
        printf("Client: %d\n", getpid());

        int fd = recvfd(pair[1]);
        printf("Client received: %d\n", fd);

        auto* msg = "Ahoj rodici!";
        write(fd, msg, strlen(msg));

        exit(0);
    }

    int pipes[2];
    WRAP_ERROR(pipe(pipes));

    sendfd(pair[0], pipes[1]);

    char buf[128];
    ssize_t len = read(pipes[0], buf, sizeof(buf));
    if (len > 0)
    {
        buf[len] = '\0';
        printf("Server received: %s\n", buf);
    }

    wait(nullptr);
}*/

void get_peer_addr(int fd, char* host, unsigned short& port)
{
    sockaddr_in address{};
    uint address_size = sizeof(address);

    int result = getpeername(fd, (sockaddr*) &address, &address_size);
    strcpy(host, inet_ntoa(address.sin_addr));
    port = ntohs(address.sin_port);
}

int chat_send_all(int client, const std::vector<int>& clients, bool even)
{
    char clientHost[256];
    unsigned short clientPort;
    get_peer_addr(client, clientHost, clientPort);

    char buffer[256];
    ssize_t len = read(client, buffer, sizeof(buffer) - 1);
    if (len <= 0) return -1;
    buffer[len] = '\0';

    printf("worker %d: %s:%d sends %s\n", even, clientHost, clientPort, buffer);

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

void server_body_poll(int fd, int workerid)
{
    printf("Starting worker %d, server fd: %d\n", workerid, fd);

    std::vector<pollfd> clients;
    clients.push_back(pollfd{});
    clients.back().fd = fd;
    clients.back().events = POLLIN;

    while (true)
    {
        int ret = poll(clients.data(), clients.size(), 20000);
        if (ret < 0)
        {
            printf("worker %d: server error\n", workerid);
            return;
        }
        else if (ret == 0) continue;

        if ((clients[0].revents & POLLIN) != 0)
        {
            auto data = recvfd(fd, workerid);
            int id = data.first;
            int client = data.second;

            bool accepted = (id == workerid);
            printf("worker %d: client connected, accepted: %d, fd: %d\n", workerid, accepted, client);

            clients.push_back(pollfd{});
            clients.back().fd = client;
            clients.back().events = POLLHUP;

            if (accepted)
            {
                clients.back().events |= POLLIN;
            }
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
                if (chat_send_all(client.fd, fds, workerid) < 0)
                {
                    printf("worker %d: client disconnected\n", workerid);
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

    int sockets[2][2];
    for (int i = 0; i < 2; i++)
    {
        WRAP_ERROR(socketpair(AF_UNIX, SOCK_STREAM, 0, sockets[i]));
    }

    for (int i = 0; i < 2; i++)
    {
        pid_t pid = fork();
        if (pid == 0)
        {
            server_body_poll(sockets[i][1], i);
            exit(0);
        }
    }

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

    int workerid = 0;
    while (true)
    {
        int client = accept(fd, nullptr, nullptr);
        WRAP_ERROR(client);

        for (int i = 0; i < 2; i++)
        {
            sendfd(sockets[i][0], client, workerid);
        }
        close(client);
        workerid = 1 - workerid;
    }

    close(fd);
}

int main(int argc, char** argv)
{
    if (argc < 2) return 1;

    server_fork(argv);

    return 0;
}
