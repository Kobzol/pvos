#include <cstdio>
#include <pthread.h>
#include <cerrno>
#include <sys/resource.h>

void* thread_fn(void* arg)
{
    getchar();
}

int main()
{
    int counter = 0;
    while (true)
    {
        pthread_t tid;
        int ret = pthread_create(&tid, nullptr, thread_fn, nullptr);

        if (ret == 0)
        {
            counter++;
        }
        else if (ret == EAGAIN)
        {
            printf("no more threads can be created, %d was the limit\n", counter);

            rlimit limit;
            getrlimit(RLIMIT_NPROC, &limit);
            printf("system soft limits: %ld (%ld)\n", limit.rlim_cur, limit.rlim_max);

            return 0;
        }
    }
}
