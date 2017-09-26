#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>
#include <csignal>
#include <cerrno>
#include <fcntl.h>
#include <cstring>
#include <vector>
#include <bits/siginfo.h>
#include <atomic>


void sigint_handler(int num, siginfo_t* info, void* data)
{
    printf("Received signal %d\n", num);
}
void signals()
{
    /*struct sigaction action{};
    action.sa_sigaction = sigint_handler;
    action.sa_flags = SA_SIGINFO | SA_RESTART;
    sigemptyset(&action.sa_mask);           // mask - které signály zpracovávat
    sigaction(SIGALRM, &action, nullptr);

    alarm(2);
    sleep(5);

    char c;
    ssize_t error = read(STDIN_FILENO, &c, sizeof(c));

    printf("%ld %d\n", error, errno);*/
}
void io()
{
    /*setbuf(stdout, nullptr);
    setvbuf(stdout, nullptr, _IONBF, 0);

    while (true)
    {
        printf("ahoj ");
        //fflush(stdout);
        usleep(10 * 1000);
    }*/
}

void uloha1_cb(int num, siginfo_t* info, void* data)
{
    printf("SIGINT\n");
}
void uloha1()
{
    struct sigaction action{};
    action.sa_sigaction = uloha1_cb;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGINT, &action, nullptr);

    int x;
    printf("%d\n", scanf("%d\n", &x));
    printf("%s\n", strerror(errno));
    printf("end\n");
}

std::vector<int> children;
std::atomic<int> signalsSent{0};
std::atomic<int> signalsReceived{0};
void uloha2_cb(int num, siginfo_t* info, void* data)
{
    pid_t pid;
    int status;
    while ((pid = waitpid(-1, &status, WNOHANG)) > 0)
    //pid = waitpid(-1, &status, WNOHANG);
    {
        signalsReceived++;
        if (WIFEXITED(status)) // exit
        {
            //printf("%d exited\n", pid);
        }
        else
        {
            //printf("%d crashed with %d\n", pid, WTERMSIG(status)); // termination status, 11
        }
    }
}
void uloha2_child_cb(int num, siginfo_t* info, void* data)
{
    if (num == SIGUSR1)
    {
        //printf("%d exiting\n", getpid());
        exit(0);
    }
    else if (num == SIGUSR2)
    {
        //printf("%d crashing\n", getpid());
        int* p = nullptr;
        *p = 5;
    }
}
void uloha2_child()
{
    struct sigaction action{};
    action.sa_sigaction = uloha2_child_cb;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGUSR1, &action, nullptr);
    sigaction(SIGUSR2, &action, nullptr);

    getchar();
}
void uloha2()
{
    struct sigaction action{};
    action.sa_sigaction = uloha2_cb;
    action.sa_flags = SA_SIGINFO;
    sigemptyset(&action.sa_mask);
    sigaction(SIGCHLD, &action, nullptr);

    size_t iter = 0;
    while (true)
    {
        pid_t pid = fork();
        if (pid == 0) uloha2_child();

        children.push_back(pid);

        int num = rand() % 10;
        if (num < 3)
        {
            int process = rand() % children.size();
            kill(children[process], rand() % 2 == 0 ? SIGUSR1 : SIGUSR2);
            signalsSent++;
            children.erase(children.begin() + process);
        }

        if (iter % 1000 == 0)
        {
            printf("%lu\n", signalsSent - signalsReceived);
        }

        usleep(500);
        iter++;
    }
}

int main()
{
    //uloha1();
    uloha2();

    return 0;
}
