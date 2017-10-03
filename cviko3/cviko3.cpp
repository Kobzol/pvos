#include <unistd.h>
#include <cstdio>
#include <fcntl.h>
#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <sys/wait.h>
#include <algorithm>
#include <poll.h>
#include <sys/epoll.h>
#include "reader.h"


// 0 - read, 1 - write
#define CHILDREN_COUNT 2


void child(int pipe[2])
{
    close(pipe[0]);

    srand(getpid());
    for (int i = 0; i < 10; i++)
    {
        char buffer[128];
        int waittime = (rand() % 1000);
        sprintf(buffer, "%d waits for %d ms", getpid(), waittime);

        write(pipe[1], buffer, strlen(buffer));

        usleep(waittime * 1000);
    }

    printf("Child %d exiting\n", getpid());

    close(pipe[1]);
    exit(0);
}

void parentDirect(int pipes[CHILDREN_COUNT][2])
{
    char buffer[128];
    while (true)
    {
        for (int i = 0; i < CHILDREN_COUNT; i++)
        {
            auto& p = pipes[i];
            ssize_t len = read(p[0], buffer, sizeof(buffer));
            if (len > 0)
            {
                buffer[len] = '\0';
                printf("Read %lu bytes: %s\n", len, buffer);
            }
            else if (len < 0 && errno == EAGAIN)
            {
                usleep(1000 * 10);
            }
            else return;
        }
    }
}
void parentSelect(int pipes[CHILDREN_COUNT][2])
{
    char buffer[128];
    while (true)
    {
        fd_set readset{};
        FD_ZERO(&readset);
        int largest = -1;

        for (int i = 0; i < CHILDREN_COUNT; i++)
        {
            FD_SET(pipes[i][0], &readset);
            largest = std::max(pipes[i][0], largest);
        }

        timeval timeout{};
        timeout.tv_sec = 1;

        int ret = select(largest + 1, &readset, nullptr, nullptr, &timeout);
        if (ret > 0)
        {
            for (int i = 0; i < CHILDREN_COUNT; i++)
            {
                if (FD_ISSET(pipes[i][0], &readset))
                {
                    ssize_t len = read(pipes[i][0], buffer, sizeof(buffer));
                    if (len > 0)
                    {
                        buffer[len] = '\0';
                        printf("Read %lu bytes: %s\n", len, buffer);
                    }
                    else return;
                }
            }
        }
    }
}
void parentPoll(int pipes[CHILDREN_COUNT][2])
{
    struct pollfd polls[CHILDREN_COUNT];

    for (int i = 0; i < CHILDREN_COUNT; i++)
    {
        polls[i].fd = pipes[i][0];
        polls[i].events = POLLIN;
    }

    char buffer[128];
    while (true)
    {
        int ret = poll(polls, CHILDREN_COUNT, 1000);
        if (ret > 0)
        {
            for (int i = 0; i < CHILDREN_COUNT; i++)
            {
                if ((polls[i].revents & POLLIN) != 0)
                {
                    ssize_t len = read(pipes[i][0], buffer, sizeof(buffer));
                    if (len > 0)
                    {
                        buffer[len] = '\0';
                        printf("Read %lu bytes: %s\n", len, buffer);
                    }
                    else return;
                }
                else if ((polls[i].revents & POLLHUP) != 0) return;
            }
        }
        else return;
    }
}
void parentEpoll(int pipes[CHILDREN_COUNT][2])
{
    int pollingfd = epoll_create(0xCAFE);
    if (pollingfd < 0) exit(1);

    for (int i = 0; i < CHILDREN_COUNT; i++)
    {
        epoll_event ev = { 0 };
        ev.data.ptr = nullptr;
        ev.data.fd = pipes[i][0];
        ev.events = EPOLLIN;
        if (epoll_ctl(pollingfd, EPOLL_CTL_ADD, pipes[i][0], &ev) < 0) exit(1);
    }

    char buffer[128];
    while (true)
    {
        epoll_event pevents[20];
        int ret = epoll_wait(pollingfd, pevents, 1, 1000);
        if (ret > 0)
        {
            for (int i = 0; i < ret; i++)
            {
                if ((pevents[i].events & EPOLLIN) != 0)
                {
                    ssize_t len = read(pevents[i].data.fd, buffer, sizeof(buffer));
                    if (len > 0)
                    {
                        buffer[len] = '\0';
                        printf("Read %lu bytes: %s\n", len, buffer);
                    }
                    else return;
                }
                else if ((pevents[i].events & EPOLLHUP) != 0) return;
            }
        }
        else return;
    }
}

void asyncRead()
{
    int pipes[CHILDREN_COUNT][2];

    for (auto& p: pipes)
    {
        pipe(p);

        int pid = fork();
        if (pid == 0) child(p);
        else
        {
            int fl = fcntl(p[0], F_GETFL);
            fcntl(p[0], F_SETFL, fl | O_NONBLOCK);
            close(p[1]);
        }
    }

    parentEpoll(pipes);

    while (wait(nullptr) > 0);
}

int main()
{
    // tty, stty -F /dev/tty -icanon
    // vypnuti line bufferovani terminalu

    int fl = fcntl(STDIN_FILENO, F_GETFL);
    fcntl(STDIN_FILENO, F_SETFL, fl | O_NONBLOCK);

    Reader reader;
    char buffer[1024];

    try
    {
        //int size = reader.readlineSelect(STDIN_FILENO, buffer, sizeof(buffer), 2000);
        //int size = reader.readlinePoll(STDIN_FILENO, buffer, sizeof(buffer), 2000);
        int size = reader.readlineNonblock(STDIN_FILENO, buffer, sizeof(buffer), 2000);
        printf("Password: %s", buffer);
    }
    catch (const char* err)
    {
        printf("%s\n", err);
    }

    return 0;
}
