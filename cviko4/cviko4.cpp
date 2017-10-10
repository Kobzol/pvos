#include <unistd.h>
#include <csignal>
#include <fcntl.h>
#include <cstdio>
#include <bits/siginfo.h>
#include <aio.h>
#include <cstdlib>
#include <string>
#include <wait.h>
#include <pthread.h>
#include <sys/types.h>
#include <syscall.h>
#include <vector>
#include <cstring>

struct IOContext
{
    aiocb block;
    volatile char buffer[1024];
};

static aiocb block;
static volatile char buffer[1024];
void asyncio_handler(int num, siginfo_t* info, void* data)
{
    printf("%d: %s", aio_return(&block), block.aio_buf);
    aio_read(&block);
}
void asyncio_signal()
{
    struct sigaction action{};
    action.sa_sigaction = asyncio_handler;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    sigaction(SIGIO, &action, nullptr);

    block.aio_fildes = STDIN_FILENO;
    block.aio_offset = 0;
    block.aio_buf = buffer;
    block.aio_nbytes = sizeof(buffer);
    block.aio_reqprio = 0;
    block.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
    block.aio_sigevent.sigev_signo = SIGIO;
    block.aio_sigevent.sigev_value.sival_ptr = nullptr;
    block.aio_lio_opcode = 0;

    aio_read(&block);
}
void asyncio_thread(sigval value)
{
    auto block = (aiocb*) value.sival_ptr;
    printf("%d: %s", aio_return(block), block->aio_buf);
    aio_read(block);
}
void sigio_handler(int num, siginfo_t* info, void* data)
{
    char buf[256];
    ssize_t len = read(info->si_fd, buf, sizeof(buf));
    if (len < 0)
    {
        printf("error\n");
    }
    else
    {
        buf[len] = '\0';
        printf("%s", buf);
    }
}

void uloha1_child(int pipe[2])
{
    close(pipe[0]);
    srand(getpid());

    for (int i = 0; i < 10; i++)
    {
        std::string str = "Hello from " + std::to_string(getpid());
        write(pipe[1], str.c_str(), str.size());
        usleep(1000 * (rand() % 1000));
    }

    exit(0);
}
void uloha1_handler(int num, siginfo_t* info, void* data)
{
    char buf[256];
    ssize_t len = read(info->si_fd, buf, sizeof(buf));
    if (len < 0)
    {
        printf("error\n");
    }
    else if (len > 0)
    {
        buf[len] = '\0';
        printf("%s (received from fd %d)\n", buf, info->si_fd);
    }
}
void uloha1()
{
    struct sigaction action{};
    action.sa_sigaction = uloha1_handler;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    sigaction(SIGIO, &action, nullptr);

    int pipes[2][2];
    for (int i = 0; i < 2; i++)
    {
        pipe(pipes[i]);

        int pid = fork();
        if (pid == 0) uloha1_child(pipes[i]);

        close(pipes[i][1]);

        fcntl(pipes[i][0], F_SETFL, fcntl(pipes[i][0], F_GETFL) | O_ASYNC);
        fcntl(pipes[i][0], F_SETOWN, getpid());
        fcntl(pipes[i][0], F_SETSIG, SIGIO);
    }

    while (waitpid(-1, nullptr, WNOHANG) >= 0)
    {
        usleep(100 * 1000);
    }
}

void uloha2_handler(sigval value)
{
    auto block = (aiocb*) value.sival_ptr;
    //printf("%d\n", aio_error(block));
    ssize_t len = aio_return(block);
    char* buf = (char*) block->aio_buf;
    buf[len] = '\0';

    printf("%d bytes (thread %d): %s", len, pthread_self(), block->aio_buf);
    aio_read(block);
}
void uloha2()
{
    volatile char buffer[1024];

    aiocb block;
    std::memset(&block, 0, sizeof(block));
    block.aio_fildes = STDIN_FILENO;
    block.aio_offset = 0;
    block.aio_buf = buffer;
    block.aio_nbytes = sizeof(buffer);
    block.aio_reqprio = 0;
    block.aio_sigevent.sigev_notify = SIGEV_THREAD;
    block.aio_sigevent.sigev_notify_function = uloha2_handler;
    block.aio_sigevent.sigev_notify_attributes = nullptr;
    block.aio_sigevent.sigev_value.sival_ptr = &block;
    block.aio_lio_opcode = 0;

    aio_read(&block);

    while (true)
    {
        sleep(1);
    }
}

void uloha3_child(int pipe[2])
{
    close(pipe[0]);

    for (int i = 0; i < 1000; i++)
    {
        std::string str = "ahoj\n";
        write(pipe[1], str.c_str(), str.size());
        usleep(1000);
    }

    exit(0);
}
void uloha3_handler(sigval value)
{
    auto block = (aiocb*) value.sival_ptr;
    printf("%d bytes (thread %d): %s", aio_return(block), pthread_self(), block->aio_buf);
    aio_read(block);
}
void uloha3()
{
    std::vector<IOContext> contexts;
    contexts.resize(20);

    int pipes[2][2];
    for (int i = 0; i < 2; i++)
    {
        pipe(pipes[i]);

        int pid = fork();
        if (pid == 0) uloha3_child(pipes[i]);

        close(pipes[i][1]);

        aiocb& block = contexts[i].block;
        std::memset(&block, 0, sizeof(block));
        block.aio_fildes = pipes[i][0];
        block.aio_offset = 0;
        block.aio_buf = contexts[i].buffer;
        block.aio_nbytes = sizeof(contexts[i].buffer);
        block.aio_reqprio = 0;
        block.aio_sigevent.sigev_notify = SIGEV_THREAD;
        block.aio_sigevent.sigev_notify_function = uloha3_handler;
        block.aio_sigevent.sigev_notify_attributes = nullptr;
        block.aio_sigevent.sigev_value.sival_ptr = (void*) &(contexts[i].block);
        block.aio_lio_opcode = 0;

        aio_read(&block);
    }

    while (waitpid(-1, nullptr, WNOHANG) >= 0)
    {
        usleep(100 * 1000);
    }
}

void uloha4_handler(int num, siginfo_t* info, void* data)
{
    auto block = (aiocb*) info->si_value.sival_ptr;
    printf("%d bytes: %s\n", aio_return(block), block->aio_buf);
    aio_read(block);
}
void uloha4()
{
    struct sigaction action{};
    action.sa_sigaction = uloha4_handler;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);
    sigaction(SIGIO, &action, nullptr);

    std::vector<IOContext> blocks;
    blocks.resize(2);

    int pipes[2][2];
    for (int i = 0; i < 2; i++)
    {
        pipe(pipes[i]);

        int pid = fork();
        if (pid == 0) uloha1_child(pipes[i]);

        close(pipes[i][1]);

        aiocb& block = blocks[i].block;
        block.aio_fildes = pipes[i][0];
        block.aio_offset = 0;
        block.aio_buf = blocks[i].buffer;
        block.aio_nbytes = sizeof(blocks[i].buffer);
        block.aio_reqprio = 0;
        block.aio_sigevent.sigev_notify = SIGEV_SIGNAL;
        block.aio_sigevent.sigev_signo = SIGIO;
        block.aio_sigevent.sigev_value.sival_ptr = &blocks[i].block;
        block.aio_lio_opcode = 0;

        aio_read(&block);
    }

    while (waitpid(-1, nullptr, WNOHANG) >= 0)
    {
        usleep(100 * 1000);
    }
}

int main()
{
    //uloha1();
    //uloha2();
    //uloha3();
    uloha4();

    return 0;
}
