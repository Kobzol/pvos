#include <unistd.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <cstdio>
#include <cstdlib>

sockaddr_un createUnixAddr(const char* path)
{
    sockaddr_un addr{};
    addr.sun_family = AF_UNIX;
    strcpy(addr.sun_path, path);
    return addr;
}

void unix_socket()
{
    const char* path = "/tmp/pvos";

    int pid = fork();
    if (pid == 0)
    {
        int fd = socket(AF_UNIX, SOCK_STREAM, 0);
        if (fd < 0)
        {
            perror("client socket error");
            exit(1);
        }

        sockaddr_un addr = createUnixAddr(path);
        if (connect(fd, (const sockaddr*) &addr, sizeof(addr)) < 0)
        {
            perror("client connect error");
            exit(1);
        }
        printf("client connected\n");

        const char* msg = "hello";
        write(fd, msg, strlen(msg));
        close(fd);

        exit(0);
    }

    int fd = socket(AF_UNIX, SOCK_STREAM, 0);
    if (fd < 0)
    {
        perror("socket");
        exit(1);
    }

    sockaddr_un addr = createUnixAddr(path);
    unlink(addr.sun_path);
    if (bind(fd, (const sockaddr*) &addr, sizeof(addr)) < 0)
    {
        perror("bind");
        exit(1);
    }

    if (listen(fd, 1) < 0)
    {
        perror("listen");
        exit(1);
    }

    socklen_t len = sizeof(addr);
    int client = accept(fd, (sockaddr*) &addr, &len);
    if (client < 0)
    {
        perror("accept");
        exit(1);
    }
    else printf("server received connection from %s\n", addr.sun_path);

    char buf[256];
    ssize_t count = read(client, buf, sizeof(buf));
    buf[count] = '\0';
    printf("server received: %s\n", buf);

    close(client);
    close(fd);
}

int main()
{
    unix_socket();

    return 0;
}
