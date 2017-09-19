#include <cstdio>
#include <pthread.h>
#include <cerrno>
#include <sys/resource.h>
#include <unistd.h>
#include <cstdlib>

void* thread_fn(void* arg)
{
    usleep(1000 * 1000 * (rand() % 5));
    return nullptr;
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
            if (pthread_detach(tid) != 0)
            {
                printf("detach failed\n");
                return 0;
            }
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
