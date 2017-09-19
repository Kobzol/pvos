#include <unistd.h>
#include <cstdio>
#include <cstdlib>
#include <sys/wait.h>

void child()
{
    srand((unsigned int) getpid());
    usleep((unsigned int)(100000 * (rand() % 50)));

    if (rand() % 10 > 7)
    {
        printf("child crash\n");
        int* ptr = nullptr;
        *ptr = 5;
    }

    int ret = rand() % 2;
    printf("child exits with code %d\n", ret);

    exit(ret);
}

int main()
{
    while (true)
    {
        pid_t pid = fork();
        if (pid == 0) child();

        int collected = 0;
        while (true)
        {
            int status;
            pid_t childpid = waitpid(-1, &status, WNOHANG);
            if (childpid < 1) break;

            if (WIFEXITED(status))
            {
                printf("exited child %d with status %d\n", childpid, WEXITSTATUS(status));
            }
            else printf("child %d crashed with signal %d\n", childpid, WTERMSIG(status));

            collected++;
        }

        if (collected > 0)
        {
            printf("collected %d processes\n", collected);
        }

        usleep(1000 * 10);
    }

    return 0;
}
